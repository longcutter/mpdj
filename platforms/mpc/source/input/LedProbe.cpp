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
// Env: MPDJ_LED_PROBE_START=n (first number), MPDJ_MIDI_DEV, MPDJ_MIDI_SUBDEV.
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

int fd = -1;

void send3(uint8_t s, uint8_t d1, uint8_t d2)
{
    uint8_t m[3] = { s, d1, d2 };
    if (write(fd, m, 3) != 3)
        printf("led probe: write failed: %s\n", strerror(errno));
}

int relSteps(uint8_t v) { return v < 0x40 ? v : (int)v - 0x80; }
} // namespace

int runLedProbe()
{
    const char* dev = envStr("MPDJ_MIDI_DEV", "/dev/snd/midiC0D0");
    int subdev = atoi(envStr("MPDJ_MIDI_SUBDEV", "1"));
    char ctlPath[64] = "/dev/snd/controlC0";
    if (const char* c = strstr(dev, "midiC"))
        snprintf(ctlPath, sizeof ctlPath, "/dev/snd/controlC%d", atoi(c + 5));
    int ctl = open(ctlPath, O_RDWR | O_CLOEXEC);
    if (ctl >= 0)
        ioctl(ctl, SNDRV_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE, &subdev);
    fd = open(dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (ctl >= 0)
        close(ctl);
    if (fd < 0)
    {
        printf("led probe: can't open %s: %s (is the MPC app still running?)\n", dev, strerror(errno));
        return 1;
    }
    snd_rawmidi_info info{};
    info.stream = SNDRV_RAWMIDI_STREAM_OUTPUT;
    if (ioctl(fd, SNDRV_RAWMIDI_IOCTL_INFO, &info) == 0)
        printf("led probe: %s output sub %u \"%s\"\n", dev, info.subdevice, (const char*)info.subname);

    printf("led probe: encoder = number, encoder push = CC/note, Q-Link 1 = value,\n"
           "           PLAY = sweep, STOP = pause, Ctrl+C = quit. Watch the panel.\n");

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
        ssize_t r = read(fd, buf, sizeof buf);
        for (ssize_t i = 0; i < r; i++)
        {
            uint8_t b = buf[i];
            if (b >= 0xF0)
                continue;
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
            if ((status & 0x0F) != 0)
                continue; // pads (channel 10) ignored
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
            else if (type == 0x90 && data[1] > 0)
            {
                if (data[0] == 111) // encoder push
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
    close(fd);
    printf("led probe: done\n");
    return 0;
}
