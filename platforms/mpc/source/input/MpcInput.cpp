#include "MpcInput.h"

#include "PlatformImpl.h"
#include "eve/FbOutput.h"
#include "core/interfaceDefs.h"
#include "mtSequencer.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sound/asound.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static_assert(sizeof(input_event) == 16, "armhf input_event should be 16 bytes");

namespace
{
const char* envStr(const char* name, const char* def)
{
    const char* v = getenv(name);
    return v && *v ? v : def;
}

bool envFlag(const char* name)
{
    const char* v = getenv(name);
    return v && *v && *v != '0';
}

// ---------------------------------------------------------------- control map
// MPC button note (channel 1) -> Tracker+ button. Edit here to remap.
struct ButtonMap
{
    uint8_t mpcNote;
    int8_t trackerButton;
    const char* name;
};

const ButtonMap kButtonMap[] = {
    { 49, interfaceButtonShift, "SHIFT" },
    { 82, interfaceButtonPlay, "PLAY" },
    { 73, interfaceButtonRec, "REC" },
    { 122, interfaceButtonCopy, "COPY" },
    { 9, interfaceButtonDelete, "ERASE" },
    { 54, interfaceButtonInsert, "+" },
    { 52, interfaceButtonPattern, "MAIN" },
    { 50, interfaceButtonFile, "BROWSE" },
    { 116, interfaceButtonMaster, "TRACK MIX" },
    { 43, interfaceButtonPerformance, "TRACK MUTE" },
    { 42, interfaceButtonSong, "NEXT SEQ" },
    { 123, interfaceButtonConfig, "MENU" },
    { 2, interfaceButtonParams, "PROGRAM EDIT" },
    { 6, interfaceButtonSampleEdit, "SAMPLE EDIT" },
    { 125, interfaceButtonSampleRec, "SAMPLER" },
    { 0, interfaceButtonSampleLoad, "Q-LINK" },
    { 53, interfaceButtonSamplePlay, "TAP TEMPO" },
    { 11, interfaceButtonNote, "NOTE REPEAT" },
    { 39, interfaceButtonInstr, "FULL LEVEL" },
    { 40, interfaceButtonVol, "16 LEVELS" },
    { 41, interfaceButtonFx, "STEP SEQ" },
};

constexpr uint8_t kNoteStop = 81;
constexpr uint8_t kNoteBankA = 35, kNoteBankC = 37;

// MPC pad note (bank A layout) -> pad number 1..16 (1 = bottom left).
int padNumber(uint8_t note)
{
    static const uint8_t kPadNotes[16] = { 37, 36, 42, 82, 40, 38, 46, 44,
                                           48, 47, 45, 43, 49, 55, 51, 53 };
    for (int i = 0; i < 16; i++)
        if (kPadNotes[i] == note)
            return i + 1;
    return 0;
}

// Relative encoder value: 0x01-0x3F forward, 0x41-0x7F backward (two's complement).
int relSteps(uint8_t v)
{
    return v < 0x40 ? v : (int)v - 0x80;
}

constexpr uint8_t kCcMainEncoder = 100;
constexpr uint8_t kCcQLink1 = 16; // Q-Links 1-4 = CC 16-19, top to bottom

// Tracker+ screen buttons sit under 8 label boxes, y 423-478 of 480.
constexpr float kScreenButtonTop = 400.0f;
} // namespace

// ---------------------------------------------------------------- setup
void MpcInput::open(SwitchGroupImpl* b, SwitchGroupImpl* p, EncoderImpl* e, const FbOutput* f)
{
    buttons = b;
    pads = p;
    encoder = e;
    fb = f;
    trace = envFlag("MPDJ_INPUT_TRACE");
    memset(padHeld, -1, sizeof padHeld);
    memset(buttonHeld, -1, sizeof buttonHeld);
    openMidi();
    openTouch();
}

void MpcInput::close()
{
    if (midiFd >= 0)
        ::close(midiFd);
    if (touchFd >= 0)
        ::close(touchFd);
    midiFd = touchFd = -1;
}

void MpcInput::poll()
{
    pollMidi();
    pollTouch();
}

// ---------------------------------------------------------------- controller MIDI
void MpcInput::openMidi()
{
    const char* dev = envStr("MPDJ_MIDI_DEV", "/dev/snd/midiC0D0");
    int subdev = atoi(envStr("MPDJ_MIDI_SUBDEV", "1"));

    // Pick the subdevice the way alsa-lib does: state a preference on the
    // card's control device (kept open while the rawmidi device opens).
    char ctlPath[64] = "/dev/snd/controlC0";
    const char* c = strstr(dev, "midiC");
    if (c)
        snprintf(ctlPath, sizeof ctlPath, "/dev/snd/controlC%d", atoi(c + 5));
    int ctl = ::open(ctlPath, O_RDWR | O_CLOEXEC);
    if (ctl >= 0 && ioctl(ctl, SNDRV_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE, &subdev) < 0)
        printf("input: prefer subdevice %d failed: %s\n", subdev, strerror(errno));

    midiFd = ::open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (midiFd >= 0)
    {
        snd_rawmidi_info info{};
        info.stream = SNDRV_RAWMIDI_STREAM_INPUT;
        if (ioctl(midiFd, SNDRV_RAWMIDI_IOCTL_INFO, &info) == 0)
        {
            printf("input: controller MIDI %s sub %u \"%s\"\n",
                   dev,
                   info.subdevice,
                   (const char*)info.subname);
            if (info.subdevice != (unsigned)subdev)
                printf("input: WARNING wanted subdevice %d (is the MPC app still running?)\n", subdev);
        }
        else
            printf("input: controller MIDI %s\n", dev);
    }
    else
        printf("input: can't open %s: %s (is the MPC app still running?)\n", dev, strerror(errno));
    if (ctl >= 0)
        ::close(ctl);
}

void MpcInput::pollMidi()
{
    if (midiFd < 0)
        return;
    uint8_t buf[256];
    for (;;)
    {
        ssize_t n = read(midiFd, buf, sizeof buf);
        if (n <= 0)
            break;
        for (ssize_t i = 0; i < n; i++)
        {
            uint8_t b = buf[i];
            if (b >= 0xF8)
                continue; // real-time bytes can appear anywhere
            if (b == 0xF0)
            {
                inSysex = true;
                continue;
            }
            if (b == 0xF7)
            {
                inSysex = false;
                continue;
            }
            if (b & 0x80)
            {
                inSysex = false;
                runningStatus = b < 0xF0 ? b : 0;
                dataCount = 0;
                continue;
            }
            if (inSysex || !runningStatus)
                continue;
            data[dataCount++] = b;
            uint8_t type = runningStatus & 0xF0;
            int need = (type == 0xC0 || type == 0xD0) ? 1 : 2;
            if (dataCount == need)
            {
                onMidi(runningStatus, data[0], need > 1 ? data[1] : 0);
                dataCount = 0;
            }
        }
    }
}

void MpcInput::onMidi(uint8_t status, uint8_t d1, uint8_t d2)
{
    uint8_t type = status & 0xF0, ch = status & 0x0F;
    if (trace)
        printf("midi: %02X %02X %02X\n", status, d1, d2);

    bool noteOn = type == 0x90 && d2 > 0;
    bool noteOff = type == 0x80 || (type == 0x90 && d2 == 0);

    if (ch == 9) // pads
    {
        if (noteOn || noteOff)
            onPad(d1, noteOn);
        // poly aftertouch (0xA9) is ignored for now
        return;
    }
    if (ch != 0)
        return;
    if (type == 0xB0)
        onRelative(d1, d2);
    else if (noteOn || noteOff)
    {
        if (d1 >= 84 && d1 <= 87)
            return; // Q-Link touch
        onButton(d1, noteOn);
    }
}

void MpcInput::onButton(uint8_t note, bool down)
{
    if (down && note >= kNoteBankA && note <= kNoteBankC)
    {
        padBank = note - kNoteBankA;
        if (trace)
            printf("input: pad bank %c\n", 'A' + padBank);
        return;
    }

    if (!down)
    {
        if (buttonHeld[note] >= 0)
        {
            buttons->release(buttonHeld[note]);
            buttonHeld[note] = -1;
        }
        return;
    }

    int target = -1;
    if (note == kNoteStop)
    {
        // The Tracker's Play button toggles; STOP only acts while playing.
        if (sequencer.isPlay())
            target = interfaceButtonPlay;
    }
    else
        for (const ButtonMap& m : kButtonMap)
            if (m.mpcNote == note)
            {
                target = m.trackerButton;
                break;
            }

    if (trace)
        printf("input: button note %u -> tracker %d\n", note, target);
    if (target < 0)
        return;
    buttonHeld[note] = (int8_t)target;
    buttons->press(target);
}

void MpcInput::onPad(uint8_t note, bool down)
{
    if (!down)
    {
        if (padHeld[note] >= 0)
        {
            pads->release(padHeld[note]);
            padHeld[note] = -1;
        }
        return;
    }
    int n = padNumber(note);
    if (!n)
        return;
    // MPC pads: 4x4, pad 1 bottom left. Tracker+ pads: 4 rows x 12, index 0 top left.
    int rowFromTop = 3 - (n - 1) / 4;
    int col = padBank * 4 + (n - 1) % 4;
    int idx = rowFromTop * 12 + col;
    if (trace)
        printf("input: pad %d (bank %c) -> tracker pad %d\n", n, 'A' + padBank, idx);
    padHeld[note] = (int8_t)idx;
    pads->press(idx);
}

void MpcInput::tapButton(int trackerButton, int times)
{
    if (times > 4)
        times = 4;
    for (int i = 0; i < times; i++)
    {
        buttons->press(trackerButton);
        buttons->release(trackerButton);
    }
}

void MpcInput::onRelative(uint8_t cc, uint8_t value)
{
    int steps = relSteps(value);
    if (!steps)
        return;
    switch (cc)
    {
        case kCcMainEncoder:
        case kCcQLink1 + 2: // Q-Link 3: jog wheel too, next to the arrows
            encoder->turn(steps);
            break;
        case kCcQLink1 + 0: // Q-Link 1: left / right
            tapButton(steps > 0 ? interfaceButtonRight : interfaceButtonLeft, abs(steps));
            break;
        case kCcQLink1 + 1: // Q-Link 2: up / down (clockwise = down the pattern)
            tapButton(steps > 0 ? interfaceButtonDown : interfaceButtonUp, abs(steps));
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- touchscreen
void MpcInput::openTouch()
{
    const char* dev = envStr("MPDJ_TOUCH_DEV", "/dev/input/event0");
    swapXY = envFlag("MPDJ_TOUCH_SWAP_XY");
    invertX = envFlag("MPDJ_TOUCH_INVERT_X");
    invertY = envFlag("MPDJ_TOUCH_INVERT_Y");

    touchFd = ::open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (touchFd < 0)
    {
        printf("input: no touchscreen (%s: %s)\n", dev, strerror(errno));
        return;
    }
    input_absinfo ax{}, ay{};
    if (ioctl(touchFd, EVIOCGABS(ABS_MT_POSITION_X), &ax) < 0 || ax.maximum <= ax.minimum)
        ioctl(touchFd, EVIOCGABS(ABS_X), &ax);
    if (ioctl(touchFd, EVIOCGABS(ABS_MT_POSITION_Y), &ay) < 0 || ay.maximum <= ay.minimum)
        ioctl(touchFd, EVIOCGABS(ABS_Y), &ay);
    rawMinX = ax.minimum;
    rawMaxX = ax.maximum;
    rawMinY = ay.minimum;
    rawMaxY = ay.maximum;
    char name[64] = "?";
    ioctl(touchFd, EVIOCGNAME(sizeof name), name);
    printf("input: touch %s \"%s\" x %d..%d y %d..%d\n", dev, name, rawMinX, rawMaxX, rawMinY, rawMaxY);
}

void MpcInput::pollTouch()
{
    if (touchFd < 0)
        return;
    input_event ev[32];
    for (;;)
    {
        ssize_t n = read(touchFd, ev, sizeof ev);
        if (n <= 0)
            break;
        for (int i = 0; i < (int)(n / (ssize_t)sizeof(input_event)); i++)
        {
            const input_event& e = ev[i];
            if (e.type == EV_ABS)
            {
                switch (e.code)
                {
                    case ABS_MT_SLOT: slot = e.value; break;
                    case ABS_MT_TRACKING_ID:
                        if (slot == 0)
                            touching = e.value >= 0;
                        break;
                    case ABS_MT_POSITION_X:
                        if (slot == 0)
                            rawX = e.value;
                        break;
                    case ABS_MT_POSITION_Y:
                        if (slot == 0)
                            rawY = e.value;
                        break;
                    case ABS_X: rawX = e.value; break;
                    case ABS_Y: rawY = e.value; break;
                    default: break;
                }
            }
            else if (e.type == EV_KEY && e.code == BTN_TOUCH)
                touching = e.value != 0;
            else if (e.type == EV_SYN && e.code == SYN_REPORT)
                onTouchFrame();
        }
    }
}

void MpcInput::onTouchFrame()
{
    bool down = touching && !wasTouching;
    bool up = !touching && wasTouching;
    wasTouching = touching;

    if (up)
    {
        if (trace)
            printf("touch: up\n");
        if (touchedButton >= 0)
            buttons->release(touchedButton);
        touchedButton = -1;
        return;
    }
    if (!down || !fb || rawMaxX <= rawMinX || rawMaxY <= rawMinY)
        return;

    // raw -> panel pixels (portrait panel) -> landscape screen -> Tracker 800x480
    float u = (float)(rawX - rawMinX) / (float)(rawMaxX - rawMinX);
    float v = (float)(rawY - rawMinY) / (float)(rawMaxY - rawMinY);
    if (swapXY)
    {
        float t = u;
        u = v;
        v = t;
    }
    if (invertX)
        u = 1.0f - u;
    if (invertY)
        v = 1.0f - v;
    int pw = fb->panelWidth(), ph = fb->panelHeight();
    if (pw <= 0 || ph <= 0)
        return;
    int px = (int)(u * (pw - 1) + 0.5f), py = (int)(v * (ph - 1) + 0.5f);
    int sx, sy;
    fb->panelToScreen(px, py, sx, sy);
    float nx, ny;
    fb->toNative((float)sx, (float)sy, nx, ny);

    int target = -1;
    if (nx >= 0 && nx < 800 && ny >= kScreenButtonTop && ny < 480)
        target = interfaceButton0 + (int)(nx / 100.0f);

    if (trace)
        printf("touch: raw (%d, %d) panel (%d, %d) screen (%d, %d) tracker (%.0f, %.0f) -> button %d\n",
               rawX, rawY, px, py, sx, sy, nx, ny, target);
    if (target >= 0)
    {
        touchedButton = target;
        buttons->press(target);
    }
}
