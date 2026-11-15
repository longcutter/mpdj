// MPDJ additions (MIT, see NOTICE.md).
//
// MPDJ_USB_TEST=1: stand-alone probe for milestone 10b (USB audio + MIDI to a
// computer over the USB-B port). It does what the tracker will do later, with
// nothing else running, and prints what it finds:
//
//  1. Builds a configfs gadget "mpdj" on the UDC (first one in /sys/class/udc,
//     ff580000.usb on the One+): UAC2 (44.1 kHz, 16-bit stereo both ways) + MIDI
//     (one port each way). A gadget left by an earlier run is removed first.
//  2. Waits for the gadget's ALSA card (id "UAC2Gadget"), lists its controls
//     (does 5.15's u_audio have the "Playback/Capture Pitch" controls?) and the
//     UDC's state / current_speed.
//  3. Plays a 440 Hz tone at -12 dBFS to the computer. The tone is made on the
//     codec's clock (a thread writing silence to the codec, 128 frames a block,
//     as MPDJ's audio thread does) and handed to the USB side through a ring, so
//     the ring's fill shows how far the two clocks drift apart (ppm).
//  4. Records what the computer plays into the gadget to a WAV (default
//     usb-in.wav in the current folder).
//  5. Every few seconds: frames each way, the ring fill and drift, xruns, the
//     Pitch controls' values (when present), current_speed.
//  6. On Ctrl+C or after MPDJ_USB_TEST_SECONDS (default 600): stops, unbinds and
//     removes the gadget.
//
// Env: MPDJ_USB_TEST_SECONDS, MPDJ_USB_TEST_EVERY (s, default 5),
// MPDJ_USB_TEST_WAV, MPDJ_USB_UDC (UDC name), MPDJ_USB_CONFIGFS (default
// /sys/kernel/config; a plain folder works for a dry run, e.g. under QEMU),
// MPDJ_USB_UDC_DIR (default /sys/class/udc), MPDJ_USB_NO_CODEC=1 (no codec
// thread: the tone is made on the USB side's own pace, no drift figure).
// Stop the MPC app first (it holds the codec).
#include "UsbTest.h"
#include "../audio/AlsaPcm.h"
#include "../audio/AudioProbe.h"
#include "../audio/MpcAudio.h"

#include <atomic>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <sound/asound.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace
{
constexpr unsigned kRate = 44100;
constexpr unsigned kBlock = 128;
constexpr unsigned kRingFrames = kRate * 2; // 2 s
constexpr unsigned kPrefill = kRate / 2;    // the USB side starts at 0.5 s of fill

std::atomic<bool> g_run{ true };
void onSignal(int) { g_run = false; }

const char* envStr(const char* n, const char* d)
{
    const char* v = getenv(n);
    return v && *v ? v : d;
}
int envNum(const char* n, int d)
{
    const char* v = getenv(n);
    return v && *v ? atoi(v) : d;
}

uint64_t nowUs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}

// ---------------------------------------------------------------- configfs
std::string g_gadget;                // .../usb_gadget/mpdj
std::vector<std::string> g_files;    // attribute files we wrote (removed in a dry run)
bool g_dry = false;                  // MPDJ_USB_CONFIGFS isn't the real configfs: print each write

bool writeAttr(const std::string& path, const char* value, bool quiet = false)
{
    FILE* f = fopen(path.c_str(), "w");
    if (!f)
    {
        if (!quiet)
            printf("usb test: write %s: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    bool ok = fputs(value, f) >= 0;
    ok = (fclose(f) == 0) && ok;
    if (!ok && !quiet)
        printf("usb test: write %s = %s: %s\n", path.c_str(), value, strerror(errno));
    g_files.push_back(path);
    if (g_dry && ok)
        printf("usb test: (dry run) %s = %s\n", path.c_str(), value[0] == '\n' ? "(empty)" : value);
    return ok;
}

std::string readLine(const std::string& path)
{
    char buf[128] = "";
    if (FILE* f = fopen(path.c_str(), "r"))
    {
        if (fgets(buf, sizeof buf, f))
            buf[strcspn(buf, "\n")] = 0;
        fclose(f);
    }
    else
        snprintf(buf, sizeof buf, "(%s)", strerror(errno));
    return buf;
}

bool mk(const std::string& path)
{
    if (mkdir(path.c_str(), 0755) == 0 || errno == EEXIST)
        return true;
    printf("usb test: mkdir %s: %s\n", path.c_str(), strerror(errno));
    return false;
}

// Remove the gadget (also one left by an earlier run): unbind, links, dirs.
// On real configfs the attribute files go with their folders; in a dry-run
// folder they are plain files, so remove those first.
void removeGadget(const std::string& g)
{
    struct stat st;
    if (stat(g.c_str(), &st) != 0)
        return;
    writeAttr(g + "/UDC", "\n", true);
    const char* funcs[] = { "uac2.usb0", "midi.usb0" };
    for (const char* fn : funcs)
        unlink((g + "/configs/c.1/" + fn).c_str());
    for (auto& f : g_files)
        if (f.compare(0, g.size(), g) == 0)
            unlink(f.c_str()); // fails on real configfs (attributes can't be removed): fine
    g_files.clear();
    // dry run: also remove files an earlier run left
    for (const char* d : { "/configs/c.1/strings/0x409", "/configs/c.1", "/functions/uac2.usb0",
                           "/functions/midi.usb0", "/strings/0x409", "" })
        if (DIR* dir = opendir((g + d).c_str()))
        {
            while (dirent* e = readdir(dir))
                if (e->d_type == DT_REG)
                    unlink((g + d + "/" + e->d_name).c_str());
            closedir(dir);
        }
    rmdir((g + "/configs/c.1/strings/0x409").c_str());
    rmdir((g + "/configs/c.1/strings").c_str()); // dry run only
    rmdir((g + "/configs/c.1").c_str());
    rmdir((g + "/functions/uac2.usb0").c_str());
    rmdir((g + "/functions/midi.usb0").c_str());
    rmdir((g + "/strings/0x409").c_str());
    rmdir((g + "/configs").c_str());   // dry run only (configfs makes these itself)
    rmdir((g + "/functions").c_str()); // dry run only
    rmdir((g + "/strings").c_str());   // dry run only
    if (rmdir(g.c_str()) != 0)
        printf("usb test: remove gadget %s: %s\n", g.c_str(), strerror(errno));
    else
        printf("usb test: gadget %s removed\n", g.c_str());
}

std::string firstUdc(const char* udcDir)
{
    std::string name;
    if (DIR* d = opendir(udcDir))
    {
        while (dirent* e = readdir(d))
            if (e->d_name[0] != '.')
            {
                name = e->d_name;
                break;
            }
        closedir(d);
    }
    return name;
}

bool buildGadget(const std::string& g, const std::string& udc)
{
    if (!mk(g))
        return false;
    bool ok = true;
    ok &= writeAttr(g + "/idVendor", "0x1d6b");  // Linux Foundation
    ok &= writeAttr(g + "/idProduct", "0x0104"); // Multifunction Composite Gadget
    writeAttr(g + "/bcdDevice", "0x0100");
    writeAttr(g + "/bcdUSB", "0x0200");
    // the "MPDJ" name the computer shows
    mk(g + "/strings");
    if (mk(g + "/strings/0x409"))
    {
        writeAttr(g + "/strings/0x409/manufacturer", "longcutter");
        writeAttr(g + "/strings/0x409/product", "MPDJ");
        writeAttr(g + "/strings/0x409/serialnumber", "0001");
    }
    mk(g + "/configs");
    ok &= mk(g + "/configs/c.1");
    if (mk(g + "/configs/c.1/strings"), mk(g + "/configs/c.1/strings/0x409"))
        writeAttr(g + "/configs/c.1/strings/0x409/configuration", "MPDJ audio + MIDI");
    writeAttr(g + "/configs/c.1/bmAttributes", "0xc0"); // self-powered
    writeAttr(g + "/configs/c.1/MaxPower", "2");
    mk(g + "/functions");
    // UAC2: p_* = MPDJ -> computer, c_* = computer -> MPDJ (names as in 5.15's f_uac2)
    std::string a = g + "/functions/uac2.usb0";
    ok &= mk(a);
    ok &= writeAttr(a + "/p_chmask", "3");
    ok &= writeAttr(a + "/p_srate", "44100");
    ok &= writeAttr(a + "/p_ssize", "2");
    ok &= writeAttr(a + "/c_chmask", "3");
    ok &= writeAttr(a + "/c_srate", "44100");
    ok &= writeAttr(a + "/c_ssize", "2");
    // c_sync left at the kernel's default (async, with a feedback endpoint: what macOS handles best)
    std::string m = g + "/functions/midi.usb0";
    if (mk(m))
    {
        writeAttr(m + "/id", "MPDJ");
        writeAttr(m + "/in_ports", "1");
        writeAttr(m + "/out_ports", "1");
    }
    if (!ok)
        return false;
    if (symlink(a.c_str(), (g + "/configs/c.1/uac2.usb0").c_str()) != 0 && errno != EEXIST)
    {
        printf("usb test: link uac2: %s\n", strerror(errno));
        return false;
    }
    if (symlink(m.c_str(), (g + "/configs/c.1/midi.usb0").c_str()) != 0 && errno != EEXIST)
        printf("usb test: link midi: %s (going on without MIDI)\n", strerror(errno));
    if (udc.empty())
    {
        printf("usb test: no UDC found; the gadget is built but not bound\n");
        return false;
    }
    std::string u = udc + "\n";
    if (!writeAttr(g + "/UDC", u.c_str()))
        return false;
    printf("usb test: gadget bound to %s\n", udc.c_str());
    return true;
}

// ---------------------------------------------------------------- ALSA
int findCardById(const char* want)
{
    for (int c = 0; c < 16; c++)
    {
        char path[64];
        snprintf(path, sizeof path, "/proc/asound/card%d/id", c);
        if (readLine(path) == want)
            return c;
    }
    return -1;
}

// An integer control's value by name on a card, or false
bool readIntControl(int card, const char* name, long* out)
{
    char path[64];
    snprintf(path, sizeof path, "/dev/snd/controlC%d", card);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    snd_ctl_elem_value v = {};
    v.id.iface = SNDRV_CTL_ELEM_IFACE_PCM;
    snprintf((char*)v.id.name, sizeof v.id.name, "%s", name);
    bool ok = ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &v) == 0;
    if (!ok)
    {
        v.id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
        ok = ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &v) == 0;
    }
    close(fd);
    if (ok)
        *out = v.value.integer.value[0];
    return ok;
}

bool openTry(AlsaPcm& pcm, int card, bool capture, const char* what)
{
    static const unsigned periods[][2] = { { 128, 4 }, { 256, 4 }, { 512, 4 }, { 1024, 4 } };
    char err[200] = "";
    for (auto& p : periods)
    {
        if (pcm.open(card, 0, capture, kRate, 2, p[0], p[1], err, sizeof err))
        {
            printf("usb test: %s hw:%d,0 %u Hz, %u x %u frames\n", what, card, pcm.rate,
                   pcm.bufferFrames / pcm.periodFrames, pcm.periodFrames);
            return true;
        }
    }
    printf("usb test: %s hw:%d,0: %s\n", what, card, err);
    return false;
}

// ---------------------------------------------------------------- ring + threads
int16_t g_ring[kRingFrames * 2];
std::atomic<uint64_t> g_written{ 0 }, g_read{ 0 }; // frames into / out of the ring
std::atomic<uint64_t> g_toUsb{ 0 }, g_fromUsb{ 0 }, g_codecFrames{ 0 };
std::atomic<uint32_t> g_usbOutXruns{ 0 }, g_usbInXruns{ 0 }, g_codecXruns{ 0 }, g_underruns{ 0 }, g_overruns{ 0 };
std::atomic<bool> g_usbStarted{ false };
double g_phase = 0;

void makeTone(int16_t* out, unsigned frames)
{
    const double step = 2 * M_PI * 440.0 / kRate;
    const double amp = 32767 * 0.251; // -12 dBFS
    for (unsigned i = 0; i < frames; i++)
    {
        int16_t s = (int16_t)lrint(amp * sin(g_phase));
        g_phase += step;
        if (g_phase > 2 * M_PI)
            g_phase -= 2 * M_PI;
        out[2 * i] = out[2 * i + 1] = s;
    }
}

void ringPush(const int16_t* in, unsigned frames)
{
    uint64_t w = g_written.load(), r = g_read.load();
    if (w - r + frames > kRingFrames)
    {
        g_overruns++;
        return; // the USB side is slower: drop the block (counted)
    }
    for (unsigned i = 0; i < frames; i++)
    {
        unsigned at = (unsigned)((w + i) % kRingFrames);
        g_ring[2 * at] = in[2 * i];
        g_ring[2 * at + 1] = in[2 * i + 1];
    }
    g_written.store(w + frames);
}

unsigned ringPop(int16_t* out, unsigned frames)
{
    uint64_t w = g_written.load(), r = g_read.load();
    unsigned n = (unsigned)(w - r < frames ? w - r : frames);
    for (unsigned i = 0; i < n; i++)
    {
        unsigned at = (unsigned)((r + i) % kRingFrames);
        out[2 * i] = g_ring[2 * at];
        out[2 * i + 1] = g_ring[2 * at + 1];
    }
    if (n < frames)
    {
        memset(out + 2 * n, 0, (frames - n) * 4);
        g_underruns++;
    }
    g_read.store(r + n);
    return n;
}

struct Ctx
{
    AlsaPcm codec, usbOut, usbIn;
    FILE* wav = nullptr;
    uint32_t wavBytes = 0;
    bool noCodec = false;
};

void wavHeader(FILE* f, uint32_t dataBytes)
{
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(kRate);
    u32(kRate * 4);
    u16(4);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(dataBytes);
}

void* codecThread(void* p)
{
    Ctx* c = (Ctx*)p;
    sched_param sp = { 70 };
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    int16_t silence[kBlock * 2] = {}, tone[kBlock * 2];
    while (g_run)
    {
        int r = c->codec.write(silence, kBlock);
        if (r == -EPIPE)
        {
            g_codecXruns++;
            c->codec.prepare();
            continue;
        }
        if (r < 0)
            break;
        g_codecFrames += kBlock;
        makeTone(tone, kBlock);
        ringPush(tone, kBlock);
    }
    return nullptr;
}

void* usbOutThread(void* p)
{
    Ctx* c = (Ctx*)p;
    unsigned period = c->usbOut.periodFrames;
    std::vector<int16_t> buf(period * 2);
    if (!c->noCodec)
        while (g_run && g_written.load() < kPrefill)
            usleep(5000);
    g_usbStarted = true;
    while (g_run)
    {
        if (c->noCodec)
            makeTone(buf.data(), period);
        else
            ringPop(buf.data(), period);
        int r = c->usbOut.write(buf.data(), period);
        if (r == -EPIPE)
        {
            g_usbOutXruns++;
            c->usbOut.prepare();
            continue;
        }
        if (r < 0)
        {
            printf("usb test: write to the computer: %s\n", strerror(-r));
            break;
        }
        g_toUsb += period;
    }
    return nullptr;
}

void* usbInThread(void* p)
{
    Ctx* c = (Ctx*)p;
    unsigned period = c->usbIn.periodFrames;
    std::vector<int16_t> buf(period * 2);
    c->usbIn.start();
    while (g_run)
    {
        int r = c->usbIn.read(buf.data(), period);
        if (r == -EPIPE)
        {
            g_usbInXruns++;
            c->usbIn.prepare();
            c->usbIn.start();
            continue;
        }
        if (r < 0)
        {
            if (g_run)
                printf("usb test: read from the computer: %s\n", strerror(-r));
            break;
        }
        if (c->wav)
        {
            fwrite(buf.data(), 4, r, c->wav);
            c->wavBytes += r * 4;
        }
        g_fromUsb += r;
    }
    return nullptr;
}
} // namespace

int runUsbTest()
{
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    const int seconds = envNum("MPDJ_USB_TEST_SECONDS", 600);
    const int every = envNum("MPDJ_USB_TEST_EVERY", 5);
    const char* udcDir = envStr("MPDJ_USB_UDC_DIR", "/sys/class/udc");
    std::string root = envStr("MPDJ_USB_CONFIGFS", "/sys/kernel/config");
    std::string udc = envStr("MPDJ_USB_UDC", "");
    if (udc.empty())
        udc = firstUdc(udcDir);
    g_gadget = root + "/usb_gadget/mpdj";
    g_dry = root != "/sys/kernel/config";
    printf("usb test: configfs %s, UDC %s (%s/%s/state: %s)\n", root.c_str(), udc.empty() ? "(none)" : udc.c_str(),
           udcDir, udc.c_str(), readLine(std::string(udcDir) + "/" + udc + "/state").c_str());

    struct stat st;
    if (stat((root + "/usb_gadget").c_str(), &st) != 0)
    {
        if (root == "/sys/kernel/config")
        {
            printf("usb test: %s/usb_gadget missing (configfs not mounted?)\n", root.c_str());
            return 1;
        }
        mkdir((root + "/usb_gadget").c_str(), 0755); // dry run folder
    }
    removeGadget(g_gadget); // left by an earlier run
    bool bound = buildGadget(g_gadget, udc);
    if (!bound)
    {
        printf("usb test: the gadget isn't bound; removing it\n");
        removeGadget(g_gadget);
        return 1;
    }

    // the gadget's sound card
    int card = -1;
    for (int i = 0; i < 50 && card < 0 && g_run; i++)
    {
        card = findCardById("UAC2Gadget");
        if (card < 0)
            usleep(100000);
    }
    printf("usb test: UDC state %s, current_speed %s\n", readLine(std::string(udcDir) + "/" + udc + "/state").c_str(),
           readLine(std::string(udcDir) + "/" + udc + "/current_speed").c_str());
    if (card < 0)
    {
        printf("usb test: no UAC2Gadget sound card after 5 s\n");
        removeGadget(g_gadget);
        return 1;
    }
    printf("usb test: gadget sound card %d; its controls:\n", card);
    char cardStr[8];
    snprintf(cardStr, sizeof cardStr, "%d", card);
    setenv("MPDJ_AUDIO_CARD", cardStr, 1);
    AudioProbe::listControls();
    long pitch;
    bool hasPlayPitch = readIntControl(card, "Playback Pitch 1000000", &pitch);
    bool hasCapPitch = readIntControl(card, "Capture Pitch 1000000", &pitch);
    printf("usb test: Playback Pitch control: %s, Capture Pitch control: %s\n", hasPlayPitch ? "yes" : "no",
           hasCapPitch ? "yes" : "no");

    Ctx c;
    c.noCodec = envNum("MPDJ_USB_NO_CODEC", 0) != 0;
    if (!c.noCodec)
    {
        char err[200];
        int codec = MpcAudio::codecCard();
        if (!c.codec.open(codec, 0, false, kRate, 2, kBlock, 3, err, sizeof err))
        {
            printf("usb test: codec hw:%d,0: %s (is the MPC app stopped?); going on without the drift figure\n", codec,
                   err);
            c.noCodec = true;
        }
        else
            printf("usb test: codec hw:%d,0 paces the tone (3 x 128 frames)\n", codec);
    }
    bool out = openTry(c.usbOut, card, false, "to the computer");
    bool in = openTry(c.usbIn, card, true, "from the computer");
    const char* wavPath = envStr("MPDJ_USB_TEST_WAV", "usb-in.wav");
    if (in)
    {
        c.wav = fopen(wavPath, "wb");
        if (c.wav)
            wavHeader(c.wav, 0);
    }

    pthread_t tCodec = 0, tOut = 0, tIn = 0;
    if (!c.noCodec)
        pthread_create(&tCodec, nullptr, codecThread, &c);
    if (out)
        pthread_create(&tOut, nullptr, usbOutThread, &c);
    if (in)
        pthread_create(&tIn, nullptr, usbInThread, &c);
    printf("usb test: running %d s (Ctrl+C stops). On the computer: pick \"MPDJ\" as an input to hear the tone, "
           "as an output to record into %s\n", seconds, wavPath);

    uint64_t t0 = nowUs(), next = t0 + every * 1000000ull;
    int64_t fill0 = -1;
    uint64_t codec0 = 0;
    while (g_run && nowUs() - t0 < (uint64_t)seconds * 1000000ull)
    {
        usleep(100000);
        if (nowUs() < next)
            continue;
        next += every * 1000000ull;
        double t = (nowUs() - t0) / 1e6;
        int64_t fill = (int64_t)(g_written.load() - g_read.load());
        char drift[48] = "";
        if (!c.noCodec && g_usbStarted)
        {
            if (fill0 < 0)
            {
                fill0 = fill;
                codec0 = g_codecFrames.load();
            }
            uint64_t span = g_codecFrames.load() - codec0;
            if (span > kRate)
                snprintf(drift, sizeof drift, ", drift %+.1f ppm", (double)(fill - fill0) * 1e6 / span);
        }
        char pitches[64] = "";
        long pp = 0, cp = 0;
        if (hasPlayPitch && readIntControl(card, "Playback Pitch 1000000", &pp))
            snprintf(pitches, sizeof pitches, ", play pitch %ld", pp);
        if (hasCapPitch && readIntControl(card, "Capture Pitch 1000000", &cp))
            snprintf(pitches + strlen(pitches), sizeof pitches - strlen(pitches), ", capture pitch %ld", cp);
        printf("usb test: %5.0f s: to computer %llu, from computer %llu frames; ring %lld%s; xruns out %u in %u codec %u, "
               "ring under %u over %u%s; speed %s\n",
               t, (unsigned long long)g_toUsb.load(), (unsigned long long)g_fromUsb.load(), (long long)fill, drift,
               g_usbOutXruns.load(), g_usbInXruns.load(), g_codecXruns.load(), g_underruns.load(), g_overruns.load(),
               pitches, readLine(std::string(udcDir) + "/" + udc + "/current_speed").c_str());
        fflush(stdout);
    }

    printf("usb test: stopping\n");
    g_run = false;
    // unbinding ends the USB streams, so blocked reads / writes return
    writeAttr(g_gadget + "/UDC", "\n", true);
    if (tCodec)
        pthread_join(tCodec, nullptr);
    for (pthread_t t : { tOut, tIn })
        if (t)
        {
            timespec until;
            clock_gettime(CLOCK_REALTIME, &until);
            until.tv_sec += 2;
            if (pthread_timedjoin_np(t, nullptr, &until) != 0)
                printf("usb test: a USB thread didn't stop (blocked read / write); exiting anyway\n");
        }
    if (c.wav)
    {
        fseek(c.wav, 0, SEEK_SET);
        wavHeader(c.wav, c.wavBytes);
        fclose(c.wav);
        printf("usb test: %s: %.1f s from the computer\n", wavPath, c.wavBytes / 4.0 / kRate);
    }
    removeGadget(g_gadget);
    return 0;
}
