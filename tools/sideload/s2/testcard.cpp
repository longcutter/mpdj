// MPDJ sideload test S2: a test card that stands in for a takeover sideload.
//
// Draws a 1280x800 landscape test card on the panel (/dev/fb0, rotated 270
// like MPDJ), shows the controller MIDI MPDJ relays on fd 3 (pads light up,
// Q-Links and the main encoder move bars, buttons are named), the touchscreen
// (crosshair), lights the PLAY and REC LEDs through fd 4, and plays a quiet
// sine on the codec (hw:1,0, opened directly like MPDJ does) whose pitch
// follows the main encoder, a semitone per detent.
//
//   STOP    exit cleanly (status 0)
//   ERASE   hang on purpose: ignore SIGTERM and spin, to test MPDJ's
//           SHIFT+MAIN escape (it then needs SIGKILL)
//   SHIFT+MAIN held 2 s: MPDJ stops it from outside
//
// Env: MPDJ_MIDI_IN_FD / MPDJ_MIDI_OUT_FD (set by MPDJ), TESTCARD_SECONDS=N
// (exit by itself after N s, for MPDJ_SIDELOAD_CYCLE), TESTCARD_FB (default
// /dev/fb0; "none" = no screen), TESTCARD_TOUCH (default /dev/input/event0),
// TESTCARD_PPM=file (write the first frame as a landscape PPM and carry on),
// TESTCARD_RUN (shown as the run number), TESTCARD_TONE=0 (no tone).
//
// Build (static, so it runs inside or outside the Debian root):
//   zig c++ -target arm-linux-musleabihf -O2 -static -I../../../platforms/mpc/source/audio \
//     -o testcard testcard.cpp ../../../platforms/mpc/source/audio/AlsaPcm.cpp
#include "AlsaPcm.h"
#include "font.h"

#include <atomic>
#include <math.h>
#include <pthread.h>
#include <sched.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

enum { W = 1280, H = 800 };
static uint32_t frame[W * H]; // 0xRRGGBB, landscape

static volatile sig_atomic_t stopRequested = 0;
static void onTerm(int s) { (void)s; stopRequested = 1; }

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// ---------------------------------------------------------------- drawing
static void fill(int x, int y, int w, int h, uint32_t c)
{
    for (int yy = y < 0 ? 0 : y; yy < y + h && yy < H; yy++)
        for (int xx = x < 0 ? 0 : x; xx < x + w && xx < W; xx++)
            frame[yy * W + xx] = c;
}

static void frameRect(int x, int y, int w, int h, int t, uint32_t c)
{
    fill(x, y, w, t, c);
    fill(x, y + h - t, w, t, c);
    fill(x, y, t, h, c);
    fill(x + w - t, y, t, h, c);
}

static int text(int x, int y, int scale, uint32_t c, const char* fmt, ...)
{
    char s[256];
    va_list a;
    va_start(a, fmt);
    vsnprintf(s, sizeof s, fmt, a);
    va_end(a);
    for (const char* p = s; *p; p++, x += kFontW * scale)
    {
        int ch = (unsigned char)*p;
        if (ch < 32 || ch > 126)
            ch = '?';
        const uint16_t* g = kFont[ch - 32];
        for (int gy = 0; gy < kFontH; gy++)
            for (int gx = 0; gx < kFontW; gx++)
                if (g[gy] >> (15 - gx) & 1)
                    fill(x + gx * scale, y + gy * scale, scale, scale, c);
    }
    return x;
}

// ---------------------------------------------------------------- panel
static int fbFd = -1, fbW, fbH, fbBpp, fbLine, fbPage;
static int rOff = 16, gOff = 8, bOff = 0, rLen = 8, gLen = 8, bLen = 8;
static uint8_t* fbMem;
static size_t fbSize;

static void openPanel(void)
{
    const char* dev = getenv("TESTCARD_FB");
    if (!dev)
        dev = "/dev/fb0";
    if (!strcmp(dev, "none"))
        return;
    fbFd = open(dev, O_RDWR | O_CLOEXEC);
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (fbFd < 0 || ioctl(fbFd, FBIOGET_VSCREENINFO, &v) || ioctl(fbFd, FBIOGET_FSCREENINFO, &f))
    {
        printf("testcard: no panel (%s: %s), drawing nothing\n", dev, strerror(errno));
        if (fbFd >= 0)
            close(fbFd);
        fbFd = -1;
        return;
    }
    fbW = v.xres;
    fbH = v.yres;
    fbBpp = v.bits_per_pixel;
    fbLine = f.line_length;
    fbPage = v.yres ? v.yoffset / v.yres : 0;
    rOff = v.red.offset, gOff = v.green.offset, bOff = v.blue.offset;
    rLen = v.red.length, gLen = v.green.length, bLen = v.blue.length;
    fbSize = f.smem_len;
    fbMem = (uint8_t*)mmap(NULL, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fbFd, 0);
    if ((void*)fbMem == MAP_FAILED)
    {
        fbMem = NULL;
        printf("testcard: mmap failed\n");
        return;
    }
    printf("testcard: panel %s %dx%d %dbpp, page %d\n", dev, fbW, fbH, fbBpp, fbPage);
}

static uint32_t pack(uint32_t c)
{
    uint32_t r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;
    return (r >> (8 - rLen)) << rOff | (g >> (8 - gLen)) << gOff | (b >> (8 - bLen)) << bOff;
}

// Landscape (lx, ly) -> panel (px, py) = (ly, W - 1 - lx), as MPDJ's FbOutput at rotate 270.
static void present(void)
{
    if (!fbMem)
        return;
    uint8_t* base = fbMem + (size_t)fbPage * fbH * fbLine;
    if (fbW == H && fbH == W)
    {
        for (int lx = 0; lx < W; lx++)
        {
            uint8_t* row = base + (size_t)(W - 1 - lx) * fbLine;
            if (fbBpp == 32)
            {
                uint32_t* out = (uint32_t*)row;
                for (int ly = 0; ly < H; ly++)
                    out[ly] = pack(frame[ly * W + lx]) | (0xFFu << 24);
            }
            else
            {
                uint16_t* out = (uint16_t*)row;
                for (int ly = 0; ly < H; ly++)
                    out[ly] = (uint16_t)pack(frame[ly * W + lx]);
            }
        }
    }
    else // some other panel: draw unrotated, clipped
        for (int y = 0; y < H && y < fbH; y++)
            for (int x = 0; x < W && x < fbW; x++)
            {
                uint8_t* row = base + (size_t)y * fbLine;
                if (fbBpp == 32)
                    ((uint32_t*)row)[x] = pack(frame[y * W + x]) | (0xFFu << 24);
                else
                    ((uint16_t*)row)[x] = (uint16_t)pack(frame[y * W + x]);
            }
}

static void writePpm(const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++)
    {
        uint8_t px[3] = { (uint8_t)(frame[i] >> 16), (uint8_t)(frame[i] >> 8), (uint8_t)frame[i] };
        fwrite(px, 3, 1, f);
    }
    fclose(f);
    printf("testcard: wrote %s\n", path);
}

// ---------------------------------------------------------------- inputs
static int midiIn = -1, midiOut = -1, touchFd = -1;

static uint8_t mStatus, mData[2], mCount;
static int msgCount;
static char lastMsg[96] = "(nothing yet)";
static int padVel[17];   // pad 1..16: velocity while held, 0 = up
static int qlink[4], encoderPos;
static int stopPressed, erasePressed;

static int padNumber(int note)
{
    static const uint8_t notes[16] = { 37, 36, 42, 82, 40, 38, 46, 44, 48, 47, 45, 43, 49, 55, 51, 53 };
    for (int i = 0; i < 16; i++)
        if (notes[i] == note)
            return i + 1;
    return 0;
}

static const char* buttonName(int note)
{
    switch (note)
    {
        case 49: return "SHIFT";
        case 73: return "REC";
        case 9: return "ERASE";
        case 54: return "+";
        case 55: return "-";
        case 122: return "COPY";
        case 123: return "MENU";
        case 2: return "PROGRAM EDIT";
        case 116: return "TRACK MIX";
        case 125: return "SAMPLER";
        case 52: return "MAIN";
        case 6: return "SAMPLE EDIT";
        case 50: return "BROWSE";
        case 42: return "NEXT SEQ";
        case 82: return "PLAY";
        case 83: return "PLAY START";
        case 81: return "STOP";
        case 67: return "UNDO";
        case 53: return "TAP TEMPO";
        case 111: return "ENCODER PUSH";
        case 0: return "Q-LINK";
        case 35: return "BANK A";
        case 36: return "BANK B";
        case 37: return "BANK C";
        case 38: return "BANK D";
        case 84: return "Q-LINK 1 touch";
        case 85: return "Q-LINK 2 touch";
        case 86: return "Q-LINK 3 touch";
        case 87: return "Q-LINK 4 touch";
    }
    return "button";
}

static void led(int cc, int value)
{
    if (midiOut < 0)
        return;
    uint8_t m[3] = { 0xB0, (uint8_t)cc, (uint8_t)value };
    if (write(midiOut, m, 3) != 3)
        printf("testcard: LED write failed\n");
}

static void onMidi(uint8_t st, uint8_t d1, uint8_t d2)
{
    int type = st & 0xF0, ch = st & 0x0F;
    int on = type == 0x90 && d2 > 0, off = type == 0x80 || (type == 0x90 && d2 == 0);
    msgCount++;
    if (ch == 9 && (on || off))
    {
        int p = padNumber(d1);
        if (p)
            padVel[p] = on ? d2 : 0;
        snprintf(lastMsg, sizeof lastMsg, "%02X %02X %02X  pad %d %s vel %d", st, d1, d2, p, on ? "down" : "up", d2);
    }
    else if (ch == 9 && type == 0xA0)
    {
        int p = padNumber(d1);
        if (p && padVel[p])
            padVel[p] = d2 ? d2 : 1;
        snprintf(lastMsg, sizeof lastMsg, "%02X %02X %02X  pad %d pressure %d", st, d1, d2, p, d2);
        msgCount--; // pressure floods; don't count it
    }
    else if (ch == 0 && type == 0xB0)
    {
        int steps = d2 < 0x40 ? d2 : d2 - 0x80;
        if (d1 >= 16 && d1 <= 19)
            qlink[d1 - 16] += steps;
        else if (d1 == 100)
            encoderPos += steps;
        snprintf(lastMsg, sizeof lastMsg, "%02X %02X %02X  %s %+d", st, d1, d2,
                 d1 == 100 ? "main encoder" : d1 >= 16 && d1 <= 19 ? "Q-Link" : "CC", steps);
    }
    else if (ch == 0 && (on || off))
    {
        snprintf(lastMsg, sizeof lastMsg, "%02X %02X %02X  %s %s", st, d1, d2, buttonName(d1), on ? "down" : "up");
        if (on && d1 == 81)
            stopPressed = 1;
        if (on && d1 == 9)
            erasePressed = 1;
    }
    else
        snprintf(lastMsg, sizeof lastMsg, "%02X %02X %02X", st, d1, d2);
    if (!(ch == 9 && type == 0xA0))
        printf("testcard: midi %s\n", lastMsg);
}

static void pollMidi(void)
{
    if (midiIn < 0)
        return;
    uint8_t buf[256];
    ssize_t n;
    while ((n = read(midiIn, buf, sizeof buf)) > 0)
        for (ssize_t i = 0; i < n; i++)
        {
            uint8_t b = buf[i];
            if (b >= 0xF8)
                continue;
            if (b & 0x80)
            {
                mStatus = b < 0xF0 ? b : 0;
                mCount = 0;
                continue;
            }
            if (!mStatus)
                continue;
            mData[mCount++] = b;
            int need = ((mStatus & 0xF0) == 0xC0 || (mStatus & 0xF0) == 0xD0) ? 1 : 2;
            if (mCount == need)
            {
                onMidi(mStatus, mData[0], need > 1 ? mData[1] : 0);
                mCount = 0;
            }
        }
}

static int rawX, rawY, touching, touchCount, touchSeen;

static void pollTouch(void)
{
    if (touchFd < 0)
        return;
    struct input_event ev[32];
    ssize_t n;
    static int slot;
    while ((n = read(touchFd, ev, sizeof ev)) > 0)
        for (int i = 0; i < (int)(n / (ssize_t)sizeof ev[0]); i++)
        {
            struct input_event* e = &ev[i];
            if (e->type == EV_ABS)
            {
                if (e->code == ABS_MT_SLOT)
                    slot = e->value;
                else if (slot == 0 && (e->code == ABS_MT_POSITION_X || e->code == ABS_X))
                    rawX = e->value;
                else if (slot == 0 && (e->code == ABS_MT_POSITION_Y || e->code == ABS_Y))
                    rawY = e->value;
                else if (slot == 0 && e->code == ABS_MT_TRACKING_ID)
                    touching = e->value >= 0;
            }
            else if (e->type == EV_KEY && e->code == BTN_TOUCH)
            {
                if (e->value && !touching)
                    touchCount++;
                touching = e->value;
                touchSeen = 1;
            }
        }
}

// ---------------------------------------------------------------- tone
static AlsaPcm pcm;
static pthread_t toneThread;
static std::atomic<bool> toneRunning{ false };
static std::atomic<int> toneXruns{ 0 };
static std::atomic<float> toneHz{ 220.0f };
static char toneLine[128] = "off";

static int codecCard(void)
{
    for (int c = 0; c < 8; c++)
    {
        char path[64], id[32] = "";
        snprintf(path, sizeof path, "/proc/asound/card%d/id", c);
        if (FILE* f = fopen(path, "r"))
        {
            if (fgets(id, sizeof id, f))
                id[strcspn(id, "\n")] = 0;
            fclose(f);
            if (!strcmp(id, "ACVA"))
                return c;
        }
    }
    return 1;
}

static void* toneMain(void*)
{
    enum { kBlock = 128 };
    int16_t buf[kBlock * 2];
    double phase = 0, gain = 0;
    const double target = 0.12; // about -18 dBFS
    while (toneRunning)
    {
        double hz = toneHz.load();
        for (int i = 0; i < kBlock; i++)
        {
            gain += (target - gain) * 0.0005;
            int16_t v = (int16_t)lrint(sin(phase) * gain * 32767);
            buf[2 * i] = buf[2 * i + 1] = v;
            phase += 2 * M_PI * hz / 44100;
            if (phase > 2 * M_PI)
                phase -= 2 * M_PI;
        }
        int w = pcm.write(buf, kBlock);
        if (w == -EPIPE)
        {
            toneXruns++;
            pcm.prepare();
        }
        else if (w < 0)
            break;
    }
    // fade out so the clean exit doesn't click
    for (int b = 0; b < 40; b++)
    {
        for (int i = 0; i < kBlock; i++)
        {
            gain *= 0.995;
            int16_t v = (int16_t)lrint(sin(phase) * gain * 32767);
            buf[2 * i] = buf[2 * i + 1] = v;
            phase += 2 * M_PI * toneHz.load() / 44100;
        }
        if (pcm.write(buf, kBlock) < 0)
            break;
    }
    return NULL;
}

static void startTone(void)
{
    const char* e = getenv("TESTCARD_TONE");
    if (e && !strcmp(e, "0"))
        return;
    int card = codecCard();
    char err[200];
    bool ok = false;
    for (int i = 0; i < 30 && !(ok = pcm.open(card, 0, false, 44100, 2, 128, 3, err, sizeof err)); i++)
        usleep(100000); // MPDJ may still be letting go of it
    if (!ok)
    {
        snprintf(toneLine, sizeof toneLine, "can't open hw:%d,0: %s", card, err);
        printf("testcard: tone: %s\n", toneLine);
        return;
    }
    snprintf(toneLine, sizeof toneLine, "hw:%d,0, %u x %u frames", card, pcm.bufferFrames / pcm.periodFrames, pcm.periodFrames);
    printf("testcard: tone on %s\n", toneLine);
    toneRunning = true;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setinheritsched(&a, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&a, SCHED_FIFO);
    sched_param sp = {};
    sp.sched_priority = 60;
    pthread_attr_setschedparam(&a, &sp);
    if (pthread_create(&toneThread, &a, toneMain, NULL) != 0 && pthread_create(&toneThread, NULL, toneMain, NULL) != 0)
        toneRunning = false;
    pthread_attr_destroy(&a);
}

static void stopTone(void)
{
    if (!toneRunning)
        return;
    toneRunning = false;
    pthread_join(toneThread, NULL);
    pcm.drop();
    pcm.close();
}

// ---------------------------------------------------------------- the card
static void draw(int run, double up, double fps)
{
    const uint32_t bg = 0x101418, fg = 0xE8E8E8, dim = 0x808890, accent = 0xFFB000;
    fill(0, 0, W, H, bg);

    // Edge frame: one colour per side, so a wrong rotation shows at once.
    fill(0, 0, W, 8, 0xFF3030);     // top: red
    fill(W - 8, 0, 8, H, 0x30D030); // right: green
    fill(0, H - 8, W, 8, 0x3070FF); // bottom: blue
    fill(0, 0, 8, H, 0xFFE030);     // left: yellow
    text(W / 2 - 15, 12, 1, 0xFF3030, "TOP");

    // Colour bars
    static const uint32_t bars[8] = { 0xFFFFFF, 0xFFFF00, 0x00FFFF, 0x00FF00, 0xFF00FF, 0xFF0000, 0x0000FF, 0x000000 };
    for (int i = 0; i < 8; i++)
        fill(40 + i * 150, 40, 150, 40, bars[i]);
    // Grey ramp
    for (int x = 0; x < 1200; x++)
    {
        uint32_t g = (uint32_t)(x * 255 / 1199);
        fill(40 + x, 80, 1, 16, g << 16 | g << 8 | g);
    }

    text(40, 110, 3, accent, "MPDJ SIDELOAD TEST CARD");
    text(40, 172, 1, fg, "Run %d   up %.1f s   %.0f fps   pid %d", run, up, fps, (int)getpid());
    text(40, 196, 1, fg, "STOP: exit cleanly    ERASE: hang on purpose    SHIFT+MAIN held 2 s: MPDJ takes over again");
    text(40, 220, 1, dim, "Tone: %s%s   %.1f Hz (main encoder = pitch)   xruns %d", toneRunning ? "" : "(off) ", toneLine,
         toneHz.load(), toneXruns.load());

    // Last MIDI
    text(40, 260, 2, fg, "MIDI from MPDJ: %d messages", msgCount);
    text(40, 300, 2, accent, "%s", midiIn >= 0 ? lastMsg : "no MIDI fd (not started by MPDJ)");

    // Pads 4x4, pad 1 bottom left
    int px0 = 40, py0 = 360, ps = 96, gap = 10;
    text(px0, py0 - 4 - kFontH, 1, dim, "Pads (bank A layout)");
    for (int p = 1; p <= 16; p++)
    {
        int col = (p - 1) % 4, rowFromTop = 3 - (p - 1) / 4;
        int x = px0 + col * (ps + gap), y = py0 + rowFromTop * (ps + gap);
        uint32_t c = 0x30363C;
        if (padVel[p])
        {
            uint32_t v = (uint32_t)(80 + padVel[p] * 175 / 127);
            c = v << 16 | (v * 3 / 5) << 8;
        }
        fill(x, y, ps, ps, c);
        text(x + 6, y + 6, 1, fg, "%d", p);
        if (padVel[p])
            text(x + 6, y + ps - kFontH - 6, 1, fg, "%d", padVel[p]);
    }

    // Q-Links and main encoder
    int qx = 500, qy = 360;
    for (int i = 0; i < 5; i++)
    {
        int v = i < 4 ? qlink[i] : encoderPos;
        int y = qy + i * 56;
        text(qx, y, 1, dim, i < 4 ? "Q-Link %d" : "Encoder", i + 1);
        fill(qx + 110, y, 400, 22, 0x30363C);
        int m = ((v % 400) + 400) % 400;
        fill(qx + 110 + m - 2, y, 5, 22, accent);
        text(qx + 520, y, 1, fg, "%d", v);
    }

    // Touch
    int tx = (int)(W - 1 - (long)rawY * 1279 / 2048), ty = (int)((long)rawX * 799 / 2048);
    text(500, 650, 1, dim, "Touch: %s  raw %d,%d -> screen %d,%d  taps %d", touchFd < 0 ? "no device" : touching ? "DOWN" : "up",
         rawX, rawY, tx, ty, touchCount);
    if (touchSeen)
    {
        uint32_t c = touching ? 0x00FFFF : 0x507080;
        fill(tx - 30, ty - 1, 61, 3, c);
        fill(tx - 1, ty - 30, 3, 61, c);
        frameRect(tx - 12, ty - 12, 25, 25, 2, c);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = onTerm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    const char* e;
    if ((e = getenv("MPDJ_MIDI_IN_FD")) && *e)
    {
        midiIn = atoi(e);
        fcntl(midiIn, F_SETFL, fcntl(midiIn, F_GETFL) | O_NONBLOCK);
    }
    if ((e = getenv("MPDJ_MIDI_OUT_FD")) && *e)
        midiOut = atoi(e);
    const char* touchDev = getenv("TESTCARD_TOUCH");
    touchFd = open(touchDev ? touchDev : "/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    openPanel();
    int run = (e = getenv("TESTCARD_RUN")) ? atoi(e) : 1;
    double limit = (e = getenv("TESTCARD_SECONDS")) ? atof(e) : 0;
    const char* ppm = getenv("TESTCARD_PPM");

    printf("testcard: started, MIDI in fd %d out fd %d, touch %s\n", midiIn, midiOut, touchFd >= 0 ? "open" : "none");
    led(82, 127); // PLAY
    led(73, 127); // REC
    startTone();

    double t0 = now(), lastFrame = 0, fpsT = t0;
    int frames = 0;
    double fps = 0;
    while (!stopRequested)
    {
        pollMidi();
        pollTouch();
        toneHz = (float)(220.0 * pow(2.0, encoderPos / 12.0));
        double t = now();
        if (stopPressed)
        {
            printf("testcard: STOP pressed, exiting\n");
            break;
        }
        if (erasePressed)
        {
            printf("testcard: ERASE pressed, hanging on purpose (ignoring SIGTERM)\n");
            signal(SIGTERM, SIG_IGN);
            signal(SIGINT, SIG_IGN);
            fill(0, H / 2 - 60, W, 120, 0xA00000);
            text(60, H / 2 - 30, 3, 0xFFFFFF, "HUNG ON PURPOSE - hold SHIFT+MAIN");
            present();
            for (volatile unsigned long i = 0;; i++)
                ; // spin
        }
        if (limit > 0 && t - t0 >= limit)
        {
            printf("testcard: %.0f s up, exiting\n", limit);
            break;
        }
        if (t - lastFrame >= 1.0 / 30)
        {
            draw(run, t - t0, fps);
            present();
            if (ppm && frames == 0)
                writePpm(ppm);
            lastFrame = t;
            frames++;
            if (t - fpsT >= 1.0)
            {
                fps = frames / (t - fpsT);
                frames = 0;
                fpsT = t;
            }
        }
        usleep(2000);
    }
    stopTone();
    led(82, 0);
    led(73, 0);
    if (stopRequested)
        printf("testcard: SIGTERM, exiting\n");
    // Leave the panel dark so a slow hand-back shows as black, not a stale card.
    memset(frame, 0, sizeof frame);
    present();
    return 0;
}
