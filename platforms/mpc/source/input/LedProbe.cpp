// MPDJ_LED_PROBE=1: find which controller MIDI messages light which LEDs.
//
// inMusic's open-source release doesn't include the controller protocol, so
// this stand-alone check (run instead of the tracker, MPC app stopped) sends
// one message at a time on the Private port (hw:0,0,1, the one the buttons
// arrive on) and prints it, while you watch the panel:
//   main encoder        number 0..127 (previous one is switched off first)
//   main encoder push   message type: CC (B0 n v) <-> note (90 n v), channel 1
//   Q-Link 1            value 0..127 (brightness / colour), default 127
//   PLAY / STOP         sweep through the numbers by itself (0.7 s each) / pause
//   Ctrl+C              quit (switches everything it lit back off)
// Q-Link 1 single clicks reach the low values TheKikGen reported for button
// LEDs (0 off, 1 dim, 2 = ?, 3 bright).
//
// Pad colours (reported by TheKikGen, untested on the One+): SysEx
//   F0 47 7F 56 65 00 04 <pad 0-15> <R> <G> <B> F7   (0x56 = One+ product ID)
// sent on the Public port (subdevice 0) by default:
//   Q-Links 2 / 3 / 4   red / green / blue 0..127 for the next pad
//   hit a pad           paint it with that colour
//   TAP TEMPO           paint all 16 pads with a test pattern (pad 1 = red, ...)
//   ERASE               all pads off
//   Q-LINK              send pad SysEx on the other port (Public <-> Private)
// At start it sends the identity request F0 7E 00 06 01 F7 on both ports and
// prints any SysEx that comes back (the reply includes the unit's serial
// number: don't paste that part anywhere public).
// Env: MPDJ_LED_PROBE_START=n (first number), MPDJ_MIDI_DEV, MPDJ_MIDI_SUBDEV,
// MPDJ_LED_PROBE_PUBLIC_SUBDEV (default 0).
#include "LedProbe.h"

#include <errno.h>
#include <fcntl.h>
#include <sound/asound.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

extern volatile bool g_running;

namespace
{
const char* envStr(const char* name, const char* def)
{
    const char* v = getenv(name);
    return v && *v ? v : def;
}

uint32_t nowMs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

int fd = -1;    // Private port (buttons, button / Q-Link LEDs)
int fdPub = -1; // Public port (pad colours, identity request)

void send3(uint8_t s, uint8_t d1, uint8_t d2)
{
    uint8_t m[3] = { s, d1, d2 };
    if (write(fd, m, 3) != 3)
        printf("led probe: write failed: %s\n", strerror(errno));
}

bool sendBytes(int f, const uint8_t* m, size_t n, const char* what)
{
    if (f < 0)
        return false;
    ssize_t w = write(f, m, n);
    if (w != (ssize_t)n)
    {
        printf("led probe: %s write failed: %s\n", what, w < 0 ? strerror(errno) : "short write");
        return false;
    }
    return true;
}

// Pad number 1..16 (1 = bottom left) from the pad's note (bank A layout, as MpcInput).
int padNumber(uint8_t note)
{
    static const uint8_t kPadNotes[16] = { 37, 36, 42, 82, 40, 38, 46, 44,
                                           48, 47, 45, 43, 49, 55, 51, 53 };
    for (int i = 0; i < 16; i++)
        if (kPadNotes[i] == note)
            return i + 1;
    return 0;
}

// Collects one SysEx message per port and prints it in hex when it ends.
struct SysexIn
{
    const char* port;
    uint8_t buf[256];
    size_t len = 0;
    bool in = false;

    void feed(uint8_t b)
    {
        if (b == 0xF0)
        {
            in = true;
            len = 0;
        }
        if (!in)
            return;
        if (b >= 0xF8)
            return; // real-time bytes may appear inside SysEx
        if (len < sizeof buf)
            buf[len++] = b;
        if (b == 0xF7 || (b & 0x80 && b != 0xF0))
        {
            in = false;
            printf("led probe: SysEx in on %s (%zu bytes):", port, len);
            for (size_t i = 0; i < len; i++)
                printf(" %02X", buf[i]);
            printf("\n");
            if (len >= 10 && buf[1] == 0x7E && buf[3] == 0x06 && buf[4] == 0x02)
                printf("led probe:   identity reply: maker %02X, family (product ID) %02X %02X, member %02X %02X"
                       " (the rest includes the serial: keep it private)\n",
                       buf[5], buf[6], buf[7], buf[8], buf[9]);
        }
    }
};

int openPort(const char* dev, int subdev)
{
    char ctlPath[64] = "/dev/snd/controlC0";
    if (const char* c = strstr(dev, "midiC"))
        snprintf(ctlPath, sizeof ctlPath, "/dev/snd/controlC%d", atoi(c + 5));
    // The preference applies to opens by this process while the control fd is open.
    int ctl = open(ctlPath, O_RDWR | O_CLOEXEC);
    if (ctl >= 0)
        ioctl(ctl, SNDRV_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE, &subdev);
    int f = open(dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (ctl >= 0)
        close(ctl);
    if (f >= 0)
    {
        snd_rawmidi_info info{};
        info.stream = SNDRV_RAWMIDI_STREAM_OUTPUT;
        if (ioctl(f, SNDRV_RAWMIDI_IOCTL_INFO, &info) == 0)
        {
            printf("led probe: %s output sub %u \"%s\"\n", dev, info.subdevice, (const char*)info.subname);
            if (info.subdevice != (unsigned)subdev)
                printf("led probe: WARNING wanted subdevice %d\n", subdev);
        }
    }
    return f;
}

int relSteps(uint8_t v) { return v < 0x40 ? v : (int)v - 0x80; }
} // namespace

int runLedProbe()
{
    const char* dev = envStr("MPDJ_MIDI_DEV", "/dev/snd/midiC0D0");
    int subdev = atoi(envStr("MPDJ_MIDI_SUBDEV", "1"));
    int pubSubdev = atoi(envStr("MPDJ_LED_PROBE_PUBLIC_SUBDEV", "0"));
    fd = openPort(dev, subdev);
    if (fd < 0)
    {
        printf("led probe: can't open %s: %s (is the MPC app still running?)\n", dev, strerror(errno));
        return 1;
    }
    if (pubSubdev != subdev)
    {
        fdPub = openPort(dev, pubSubdev);
        if (fdPub < 0)
            printf("led probe: can't open the Public port (sub %d): %s; pad SysEx goes to Private\n",
                   pubSubdev, strerror(errno));
    }
    SysexIn sxPriv{ "Private" }, sxPub{ "Public" };

    static const uint8_t kIdentity[] = { 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 };
    if (sendBytes(fdPub, kIdentity, sizeof kIdentity, "identity (Public)"))
        printf("led probe: identity request sent on Public\n");
    if (sendBytes(fd, kIdentity, sizeof kIdentity, "identity (Private)"))
        printf("led probe: identity request sent on Private\n");

    bool padOnPub = fdPub >= 0;
    int rgb[3] = { 127, 0, 0 };
    bool padUsed[2][16] = {};
    auto padColour = [&](int pad, int r, int g, int b, bool quiet) {
        int f = padOnPub ? fdPub : fd;
        uint8_t m[] = { 0xF0, 0x47, 0x7F, 0x56, 0x65, 0x00, 0x04,
                        (uint8_t)pad, (uint8_t)r, (uint8_t)g, (uint8_t)b, 0xF7 };
        if (sendBytes(f, m, sizeof m, padOnPub ? "pad SysEx (Public)" : "pad SysEx (Private)"))
        {
            padUsed[padOnPub][pad] = true;
            if (!quiet)
                printf("led probe: pad %d (index %d) <- R %d G %d B %d on %s:"
                       " F0 47 7F 56 65 00 04 %02X %02X %02X %02X F7\n",
                       pad + 1, pad, r, g, b, padOnPub ? "Public" : "Private", pad, r, g, b);
        }
    };

    printf("led probe: encoder = number, encoder push = CC/note, Q-Link 1 = value,\n"
           "           PLAY = sweep, STOP = pause, Ctrl+C = quit. Watch the panel.\n"
           "           Pads: Q-Links 2/3/4 = R/G/B, hit a pad = paint it, TAP TEMPO = test\n"
           "           pattern on all pads, ERASE = pads off, Q-LINK = other port.\n");

    bool useNote = false;
    int n = atoi(envStr("MPDJ_LED_PROBE_START", "0")) & 127, value = 127;
    bool sweep = false, used[2][128] = {};
    uint32_t lastStep = nowMs();

    auto show = [&](int oldType, int oldN) {
        if (oldN >= 0) // switch the previous one off
            send3(oldType ? 0x90 : 0xB0, (uint8_t)oldN, 0);
        send3(useNote ? 0x90 : 0xB0, (uint8_t)n, (uint8_t)value);
        used[useNote][n] = true;
        printf("led probe: %s %3d (0x%02X) value %d\n", useNote ? "note" : "CC  ", n, n, value);
    };
    show(0, -1);

    uint8_t status = 0, data[2];
    int count = 0;
    while (g_running)
    {
        uint8_t buf[64];
        if (fdPub >= 0)
        {
            ssize_t rp = read(fdPub, buf, sizeof buf);
            for (ssize_t i = 0; i < rp; i++)
                sxPub.feed(buf[i]); // Public port: only SysEx replies matter here
        }
        ssize_t r = read(fd, buf, sizeof buf);
        for (ssize_t i = 0; i < r; i++)
        {
            uint8_t b = buf[i];
            sxPriv.feed(b);
            if (b >= 0xF0 || sxPriv.in)
            {
                if (b == 0xF0 || sxPriv.in)
                    status = 0; // SysEx cancels running status
                continue;
            }
            if (b & 0x80)
            {
                status = b;
                count = 0;
                continue;
            }
            if (!status)
                continue;
            data[count++] = b;
            if (count < 2)
                continue;
            count = 0;
            uint8_t type = status & 0xF0;
            if ((status & 0x0F) == 9) // pads, channel 10
            {
                int p = padNumber(data[0]);
                if (type == 0x90 && data[1] > 0 && p > 0)
                    padColour(p - 1, rgb[0], rgb[1], rgb[2], false);
                continue;
            }
            if ((status & 0x0F) != 0)
                continue;
            int oldType = useNote, oldN = n;
            if (type == 0xB0 && data[0] == 100) // main encoder
            {
                n = (n + relSteps(data[1]) + 128) % 128;
                show(oldType, oldN);
            }
            else if (type == 0xB0 && data[0] == 16) // Q-Link 1
            {
                value += relSteps(data[1]);
                value = value < 0 ? 0 : value > 127 ? 127 : value;
                show(oldType, -1); // same LED, new value
            }
            else if (type == 0xB0 && data[0] >= 17 && data[0] <= 19) // Q-Links 2-4 = R/G/B
            {
                int& c = rgb[data[0] - 17];
                c += relSteps(data[1]);
                c = c < 0 ? 0 : c > 127 ? 127 : c;
                printf("led probe: pad colour R %d G %d B %d\n", rgb[0], rgb[1], rgb[2]);
            }
            else if (type == 0x90 && data[1] > 0)
            {
                if (data[0] == 53) // TAP TEMPO: test pattern
                {
                    static const uint8_t kPattern[16][3] = {
                        { 127, 0, 0 },   { 0, 127, 0 },   { 0, 0, 127 },  { 127, 127, 127 },
                        { 127, 127, 0 }, { 0, 127, 127 }, { 127, 0, 127 }, { 127, 40, 0 },
                        { 64, 0, 0 },    { 0, 64, 0 },    { 0, 0, 64 },   { 64, 64, 64 },
                        { 16, 0, 0 },    { 0, 16, 0 },    { 0, 0, 16 },   { 16, 16, 16 },
                    };
                    for (int p = 0; p < 16; p++)
                        padColour(p, kPattern[p][0], kPattern[p][1], kPattern[p][2], true);
                    printf("led probe: test pattern on %s. Expect, from pad 1 (bottom left), left to right:\n"
                           "  row 1: red, green, blue, white\n"
                           "  row 2: yellow, cyan, magenta, orange\n"
                           "  row 3: half red, half green, half blue, half white\n"
                           "  row 4: dim red, dim green, dim blue, dim white\n",
                           padOnPub ? "Public" : "Private");
                }
                else if (data[0] == 9) // ERASE: pads off
                {
                    for (int p = 0; p < 16; p++)
                        padColour(p, 0, 0, 0, true);
                    printf("led probe: all pads off on %s\n", padOnPub ? "Public" : "Private");
                }
                else if (data[0] == 0) // Q-LINK: other port
                {
                    if (fdPub >= 0)
                        padOnPub = !padOnPub;
                    printf("led probe: pad SysEx now goes to %s\n", padOnPub ? "Public" : "Private");
                }
                else if (data[0] == 111) // encoder push
                {
                    useNote = !useNote;
                    show(oldType, oldN);
                }
                else if (data[0] == 82 || data[0] == 83) // PLAY / PLAY START
                {
                    sweep = true;
                    lastStep = nowMs();
                    printf("led probe: sweeping\n");
                }
                else if (data[0] == 81) // STOP
                {
                    sweep = false;
                    printf("led probe: paused at %s %d\n", useNote ? "note" : "CC", n);
                }
                else
                    printf("led probe: (button note %u pressed)\n", data[0]);
            }
        }
        if (sweep && nowMs() - lastStep >= 700)
        {
            lastStep = nowMs();
            int oldN = n;
            n = (n + 1) % 128;
            show(useNote, oldN);
        }
        usleep(2000);
    }
    for (int t = 0; t < 2; t++)
        for (int i = 0; i < 128; i++)
            if (used[t][i])
                send3(t ? 0x90 : 0xB0, (uint8_t)i, 0);
    for (int port = 0; port < 2; port++)
    {
        padOnPub = port == 1;
        for (int p = 0; p < 16; p++)
            if (padUsed[port][p])
                padColour(p, 0, 0, 0, true);
    }
    if (fdPub >= 0)
        close(fdPub);
    close(fd);
    printf("led probe: done\n");
    return 0;
}
