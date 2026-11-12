#include "MpcInput.h"

#include "PlatformImpl.h"
#include "sideload/Sideload.h"
#include "DisplayDriverImpl.h"
#include "eve/FbOutput.h"
#include "core/interfaceDefs.h"
#include "core/interface.h"
#include "display.h"
#include "list.h"
#include "mtConfig.h"
#include "core/interfacePopups.h"
#include "fileManager.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"
#include "projectEditor/projectEditor.h"
#include "sampleEditor/sampleEditor.h"
#include "samplePlayback/samplePlayback.h"
#include "songEditor/songEditor.h"

#include <algorithm>
#include <errno.h>
#include <math.h>
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
// MPC button note (channel 1) -> what it does. The agreed map lives in the
// project doc claude/mpdj-control-map.tsv; keep the two in step.
enum class Act : uint8_t
{
    Forward,     // press/release the Tracker+ button as-is
    Main,        // Pattern <-> Song
    SampleEdit,  // Sample Playback <-> Sample Editor
    Browse,      // Sample Loader; SHIFT = File
    NextSeq,     // next pattern (queued while playing); SHIFT = Performance (XYFX)
    Play,        // play from the playhead (Pattern screen: from the cursor row)
    PlayStart,   // play from the pattern start (restarts if playing)
    Stop,        // stop
    Minus,       // remove step (Tracker Backspace = Shift+Delete)
    Copy,        // copy; SHIFT+COPY (DELETE label) = remove the selected steps
    Undo,        // undo; SHIFT = redo (Pattern screen)
    TapTempo,    // tap tempo; SHIFT = lock / unlock the tempo
    EncoderPush, // Pattern screen: paste (Tracker Shift+Copy); elsewhere Enter (Mini middle)
    QLinkMode,   // Q-LINK: Q-Link modes 1 (navigate) > 2 (screen buttons 1-4) > 3 (5-8)
};

struct ButtonMap
{
    uint8_t mpcNote;
    Act act;
    int8_t trackerButton; // for Forward / Copy
    const char* name;
};

const ButtonMap kButtonMap[] = {
    { 49, Act::Forward, interfaceButtonShift, "SHIFT" },
    { 73, Act::Forward, interfaceButtonRec, "REC" },
    { 9, Act::Forward, interfaceButtonDelete, "ERASE" },
    { 54, Act::Forward, interfaceButtonInsert, "+" },
    { 55, Act::Minus, -1, "-" },
    { 122, Act::Copy, interfaceButtonCopy, "COPY" },
    { 123, Act::Forward, interfaceButtonConfig, "MENU" },
    { 2, Act::Forward, interfaceButtonParams, "PROGRAM EDIT" },  // again = Automation
    { 116, Act::Forward, interfaceButtonMaster, "TRACK MIX" },   // again = next page
    { 125, Act::Forward, interfaceButtonSampleRec, "SAMPLER" },
    { 52, Act::Main, -1, "MAIN" },
    { 6, Act::SampleEdit, -1, "SAMPLE EDIT" },
    { 50, Act::Browse, -1, "BROWSE" },
    { 42, Act::NextSeq, -1, "NEXT SEQ" },
    { 82, Act::Play, -1, "PLAY" },
    { 83, Act::PlayStart, -1, "PLAY START" },
    { 81, Act::Stop, -1, "STOP" },
    { 67, Act::Undo, -1, "UNDO" },
    { 53, Act::TapTempo, -1, "TAP TEMPO" },
    { 111, Act::EncoderPush, -1, "ENCODER PUSH" },
    { 0, Act::QLinkMode, -1, "Q-LINK" },
    // Not mapped yet: TRACK MUTE, NOTE REPEAT,
    // FULL LEVEL, 16 LEVELS, STEP SEQ, TC, OVER DUB, bank D (pad work, milestone 5)
};

constexpr uint8_t kNoteBankA = 35, kNoteBankC = 37;
constexpr uint8_t kNoteShift = 49;
constexpr uint8_t kNoteMain = 52;
constexpr uint8_t kNoteErase = 9;

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
constexpr int kListJump = 5; // file lists: rows per Q-Link 2 step (Q-Link 4 = one row)
constexpr uint8_t kCcQLink1 = 16; // Q-Links 1-4 = CC 16-19, top to bottom

// Tracker+ screen buttons sit under 8 label boxes, y 423-478 of 480.
constexpr float kScreenButtonTop = 400.0f;
} // namespace

// ---------------------------------------------------------------- setup
void MpcInput::open(SwitchGroupImpl* b, SwitchGroupImpl* p, EncoderImpl* e, DisplayDriverImpl* d)
{
    if (const char* v = getenv("MPDJ_POINT_FINE_MS"))
        pointFineMs = (uint32_t)atoi(v);
    if (const char* v = getenv("MPDJ_POINT_FAST_MS"))
        pointFastMs = (uint32_t)atoi(v);
    buttons = b;
    pads = p;
    encoder = e;
    disp = d;
    fb = d ? &d->output() : nullptr;
    trace = envFlag("MPDJ_INPUT_TRACE");
    qlinkDivider = atoi(envStr("MPDJ_QLINK_DIV", "4"));
    if (qlinkDivider < 1)
        qlinkDivider = 1;
    memset(padHeld, -1, sizeof padHeld);
    memset(buttonHeld, -1, sizeof buttonHeld);
    openMidi();
    openTouch();
    parseQLinkLeds();
    setQLinkLeds();
}

void MpcInput::close()
{
    qlinkMode = 0; // all Q-Link mode LEDs off
    setQLinkLeds();
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
    pollSave();
    pollQLinks();
    if (sideloadComboMs && mpcShift &&
        platform.getTimer()->millis() - sideloadComboMs >= Sideload::kComboHoldMs)
    {
        sideloadComboMs = 0;
        Sideload::request();
    }
}

void MpcInput::suspend()
{
    releaseQLinkButton();
    closeProgramPicker();
    for (int n = 0; n < 128; n++)
    {
        if (buttonHeld[n] >= 0)
            buttons->release(buttonHeld[n]);
        if (padHeld[n] >= 0)
            pads->release(padHeld[n]);
    }
    memset(padHeld, -1, sizeof padHeld);
    memset(buttonHeld, -1, sizeof buttonHeld);
    if (touchedButton >= 0)
        buttons->release(touchedButton);
    touchedButton = -1;
    releaseCoreShift();
    mpcShift = false;
    sideloadComboMs = 0;
    for (bool& t : qlinkTouched)
        t = false;
    int mode = qlinkMode;
    qlinkMode = 0; // Q-Link mode LEDs off
    setQLinkLeds();
    qlinkMode = mode;
    if (touchFd >= 0)
        ::close(touchFd);
    touchFd = -1;
    touching = wasTouching = false;
    runningStatus = dataCount = 0;
    inSysex = false;
}

void MpcInput::resume()
{
    runningStatus = dataCount = 0;
    inSysex = false;
    mpcShift = false;
    openTouch();
    setQLinkLeds();
}

// SHIFT+BROWSE = SAVE (the MPC's printed shift label), from any screen.
// A project that has never been saved needs a name: that goes through the
// File screen's Save As (keyboard). Otherwise save in place here; the File
// screen normally finishes a save (fmSaveEnd) in its update(), so pollSave()
// does that part when the save was started from another screen.
void MpcInput::saveProject()
{
    if (savePending)
        return;
    bool needsName = (mtProject.values.projectNotSavedFlag & 2) ||
                     newFileManager.getCurrentProjectName()[0] == 0 ||
                     !newFileManager.projectExist(newFileManager.getCurrentProjectPath(),
                                                  newFileManager.getCurrentProjectName());
    if (needsName || mtInterface.isFileScreenActive())
    {
        if (!mtInterface.isFileScreenActive())
            tapButton(interfaceButtonFile, 1);
        if (mtInterface.isFileScreenActive())
        {
            projectEditor.afterSaveOperation = cProjectEditor::AfterSaveOperation::noOperationAfterSave;
            projectEditor.saveProject(); // Save As browser if it needs a name
        }
        return;
    }
    if (!newFileManager.saveProject())
    {
        mtPopups.show(0, "Busy - try SAVE again");
        return;
    }
    savePending = true;
    printf("input: saving project \"%s\"\n", newFileManager.getCurrentProjectName());
}

void MpcInput::pollSave()
{
    if (!savePending)
        return;
    uint8_t s = newFileManager.getStatus();
    if (s == fmSaveEnd)
        newFileManager.clearStatus();
    else if (s >= fmError)
    {
        newFileManager.clearStatus();
        savePending = false;
        mtPopups.show(0, "Save failed");
        printf("input: save failed (status %u)\n", s);
        return;
    }
    else if (s != fmIdle)
        return; // still saving
    // fmIdle here also covers a screen that took the end status first
    savePending = false;
    static char text[64];
    snprintf(text, sizeof text, "Saved %s", newFileManager.getCurrentProjectName());
    mtPopups.show(0, text);
    printf("input: project saved\n");
}

// SAMPLE EDIT / PROGRAM EDIT screens: hold SHIFT to pick the program. This
// holds the Tracker's Instr button as well, which opens its instrument list
// popup; the main encoder, Q-Link 4 (and Q-Links 1-2, pads) pick from it, and
// letting go of SHIFT closes it on the new program. Any other MPC button
// closes the list first, so SHIFT combos still work.
bool MpcInput::onProgramScreen()
{
    return mtInterface.isSamplePlayerScreenActive() || mtInterface.isSampleEditorScreenActive() ||
           mtInterface.isInstrumentParamsScreenActive();
}

void MpcInput::closeProgramPicker()
{
    if (!programPicker)
        return;
    programPicker = false;
    buttons->release(interfaceButtonInstr);
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

    // read-write: LEDs are lit by messages sent back on the same port
    midiFd = ::open(dev, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (midiFd < 0)
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
        {
            qlinkTouch(d1 - 84, noteOn); // Q-Link touch
            return;
        }
        onButton(d1, noteOn);
    }
}

bool MpcInput::coreShift()
{
    return buttons->isButtonPressed(interfaceButtonShift);
}

// The Sample Editor's and Sample Recorder's confirm popups ("This action will stop
// the pattern", "Selected sample is too long", ...) clear every binding but screen
// buttons 6 (Cancel / No) and 7 (Yes / Ok), so on the MPC only touch would answer
// them. Recognise that state (no mode button and no Play bound, 6 or 7 bound) and
// map hardware to it (Mike 2026-11-11): ENCODER PUSH, PLAY, PLAY START = Yes / Ok;
// STOP, ERASE = Cancel, or Ok when the popup has nothing else. Returns the screen
// button to press, or -1 when this isn't such a popup or the key has no part in it.
int MpcInput::confirmPopupButton(uint8_t note, int act)
{
    InputProcessor* ip = ModuleBase::ip;
    if (!ip || ip->hasButtonObj(interfaceButtonPlay))
        return -1;
    for (int b = interfaceButton8; b <= interfaceButton22; b++)
        if (ip->hasButtonObj(b))
            return -1;
    bool yes = ip->hasButtonObj(interfaceButton7), no = ip->hasButtonObj(interfaceButton6);
    if (!yes && !no)
        return -1;

    Act a = (Act)act;
    if (a == Act::EncoderPush || a == Act::Play || a == Act::PlayStart)
        return yes ? interfaceButton7 : -1;
    if (a == Act::Stop || note == kNoteErase)
        return no ? interfaceButton6 : interfaceButton7;
    return -1;
}

// Some MPC combos use SHIFT to mean something the Tracker doesn't (SHIFT+BROWSE
// = File): take Shift off the core before pressing, so screens don't see a
// Shift combo. It stays off until SHIFT is pressed again.
void MpcInput::releaseCoreShift()
{
    if (coreShift())
        buttons->release(interfaceButtonShift);
}

// Press and release a Tracker button with Shift held (Shift+Delete, Shift+Copy...).
void MpcInput::tapWithShift(int trackerButton)
{
    bool had = coreShift();
    if (!had)
        buttons->press(interfaceButtonShift);
    buttons->press(trackerButton);
    buttons->release(trackerButton);
    if (!had)
        buttons->release(interfaceButtonShift);
}

void MpcInput::onButton(uint8_t note, bool down)
{
    if (down && fxListOpen())
    {
        // The effect list opened by a tap (step 3): ENCODER PUSH keeps the pick,
        // ERASE / STOP close it unchanged; any other button keeps the pick and
        // then does its own job (as letting go of FX does on the Tracker+)
        if (note == 111)
        {
            closeFxList(true);
            return;
        }
        if (note == 9 || note == 81)
        {
            closeFxList(false);
            return;
        }
        closeFxList(true);
    }
    if (down && note >= kNoteBankA && note <= kNoteBankC)
    {
        padBank = note - kNoteBankA;
        if (trace)
            printf("input: pad bank %c\n", 'A' + padBank);
        return;
    }

    if (!down)
    {
        if (note == kNoteMain || note == kNoteShift)
            sideloadComboMs = 0;
        if (note == kNoteShift)
        {
            mpcShift = false;
            closeProgramPicker();
        }
        if (buttonHeld[note] >= 0)
        {
            buttons->release(buttonHeld[note]);
            buttonHeld[note] = -1;
        }
        return;
    }

    if (note == kNoteMain && mpcShift && Sideload::enabled())
    {
        // SHIFT+MAIN held 2 s hands the MPC to the sideload (MPDJ_SIDELOAD).
        sideloadComboMs = platform.getTimer()->millis();
        if (!sideloadComboMs)
            sideloadComboMs = 1;
        if (trace)
            printf("input: SHIFT+MAIN: hold for the sideload\n");
        return;
    }

    const ButtonMap* m = nullptr;
    for (const ButtonMap& e : kButtonMap)
        if (e.mpcNote == note)
        {
            m = &e;
            break;
        }
    if (trace)
        printf("input: button note %u%s %s\n", note, mpcShift ? " +SHIFT" : "", m ? m->name : "(unmapped)");
    if (!m)
        return;
    if (note != kNoteShift)
        closeProgramPicker();
    if (m->act != Act::QLinkMode && note != kNoteShift)
        releaseQLinkButton();

    int hold = -1; // Tracker button to press now and release with the MPC button
    bool playing = sequencer.isPlay();
    bool onPattern = mtInterface.isPatternScreenActive();

    int popupAnswer = mpcShift ? -1 : confirmPopupButton(note, (int)m->act);
    if (popupAnswer >= 0)
    {
        if (trace)
            printf("input: confirm popup -> screen button %d\n", popupAnswer - interfaceButton0);
        hold = popupAnswer;
    }
    else switch (m->act)
    {
        case Act::Forward:
            if (note == kNoteShift)
                mpcShift = true;
            hold = m->trackerButton;
            break;

        case Act::Main: hold = onPattern ? interfaceButtonSong : interfaceButtonPattern; break;

        case Act::SampleEdit:
            hold = mtInterface.isSamplePlayerScreenActive() ? interfaceButtonSampleEdit
                                                         : interfaceButtonSamplePlay;
            break;

        case Act::Browse:
            if (mpcShift)
            {
                releaseCoreShift();
                saveProject(); // SAVE
            }
            else // Sample Loader <-> File
                hold = mtInterface.isSampleLoaderScreenActive() ? interfaceButtonFile
                                                                : interfaceButtonSampleLoad;
            break;

        case Act::NextSeq:
            if (mpcShift)
            {
                releaseCoreShift();
                hold = interfaceButtonPerformance;
            }
            else if (playing)
            {
                // queue it and arm the switch for the end of this pattern, as the
                // Tracker does when its Pattern button is released
                sequencer.sequencialSwitch_changeNextPattern(1);
                sequencer.sequencialSwitch_SetReady();
            }
            else if (onPattern)
                patternEditor.changeActualPattern(1);
            break;

        case Act::Play:
            if (playing)
                break;
            if (onPattern)
                tapWithShift(interfaceButtonPlay); // Tracker Shift+Play = from the cursor
            else
                tapButton(interfaceButtonPlay, 1);
            break;

        case Act::PlayStart:
            releaseCoreShift(); // Shift+Play would start from the cursor
            if (playing)
                tapButton(interfaceButtonPlay, 1); // stop, then start again
            tapButton(interfaceButtonPlay, 1);
            break;

        case Act::Stop:
            if (playing)
                tapButton(interfaceButtonPlay, 1); // the Tracker's Play toggles
            break;

        case Act::Minus: tapWithShift(interfaceButtonDelete); break;

        case Act::Copy:
            if (mpcShift)
                tapWithShift(interfaceButtonDelete); // DELETE label: remove selected steps
            else
                hold = m->trackerButton;
            break;

        case Act::Undo:
            if (!onPattern)
                break;
            if (mpcShift)
                newFileManager.redoPattern();
            else
                newFileManager.undoPattern();
            patternEditor.refreshPattern();
            patternEditor.showPattern();
            patternEditor.showLength();
            break;

        case Act::TapTempo:
            if (mpcShift)
            {
                songEditor.tempoLocked = !songEditor.tempoLocked;
                mtPopups.show(0, songEditor.tempoLocked ? "Tempo locked" : "Tempo unlocked");
            }
            else
                tapTempo();
            break;

        case Act::EncoderPush:
            if (onPattern)
                tapWithShift(interfaceButtonCopy); // paste
            else
                hold = interfaceButtonMiniMiddle; // the Tracker Mini's Enter: open, load, select, confirm
            break;
        case Act::QLinkMode: cycleQLinkMode(); break;
    }

    if (hold >= 0)
    {
        buttonHeld[note] = (int8_t)hold;
        buttons->press(hold);
    }
    if (note == kNoteShift && onProgramScreen())
    {
        programPicker = true;
        buttons->press(interfaceButtonInstr);
    }
}

// Average the last few taps; a pause of 2 s starts over. Internal clock only.
void MpcInput::tapTempo()
{
    uint32_t now = platform.getTimer()->millis();
    if (tapCount > 0 && now - tapTimes[tapCount - 1] > 2000)
        tapCount = 0;
    if (tapCount == kTaps)
    {
        memmove(tapTimes, tapTimes + 1, sizeof(tapTimes[0]) * (kTaps - 1));
        tapCount--;
    }
    tapTimes[tapCount++] = now;
    if (tapCount < 2 || !sequencer.isSetInternalClock())
        return;
    if (songEditor.tempoLocked)
    {
        mtPopups.show(0, "Tempo locked (SHIFT+TAP TEMPO)");
        return;
    }

    float ms = (float)(tapTimes[tapCount - 1] - tapTimes[0]) / (tapCount - 1);
    if (ms <= 0)
        return;
    float bpm = roundf(600000.0f / ms) / 10.0f;
    if (bpm < Sequencer::MIN_TEMPO)
        bpm = Sequencer::MIN_TEMPO;
    if (bpm > Sequencer::MAX_TEMPO)
        bpm = Sequencer::MAX_TEMPO;
    mtProject.values.globalTempo = bpm;
    newFileManager.setProjectStructChanged();
    if (mtInterface.isSongScreenActive())
        songEditor.showTempoValue();

    static char text[32];
    snprintf(text, sizeof text, "Tempo %.1f", bpm);
    mtPopups.show(0, text);
    if (trace)
        printf("input: tap tempo %.1f BPM (%d taps)\n", bpm, tapCount);
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

// Q-Links are smooth high-resolution encoders that send several messages per
// small twist: count them up and step once per N messages (kQLinkDiv, scaled
// by MPDJ_QLINK_DIV / 4). The count resets when the direction changes, so
// reversing responds at once.
namespace
{
// Q-Link 1 tracks, 2 rows, 3 cell within the track, 4 value (Mike, step 1b);
// [4] = Q-Link 4 picking an FX type, half the value speed (Mike, step 1d)
constexpr int kQLinkDiv[5] = { 11, 8, 11, 6, 12 };
} // namespace

int MpcInput::divideQLink(int knob, int steps, int divIdx)
{
    int div = (kQLinkDiv[divIdx < 0 ? knob : divIdx] * qlinkDivider + 2) / 4;
    if (div < 1)
        div = 1;
    int& acc = qlinkAcc[knob];
    if ((acc > 0 && steps < 0) || (acc < 0 && steps > 0))
        acc = 0;
    acc += steps;
    int out = acc / div;
    acc -= out * div;
    return out;
}

// Pattern screen: select Note / Instrument / FX1 / FX2 (or Volume / Micromove
// in the Mic/Vol view) in the current track. Same state changes as the
// Tracker's Note/Instr/FX buttons, without their side effects (view-mode
// changes, the Mic/Vol toggle on a second Note press, popups while held).
void MpcInput::stepEditParam(int steps)
{
    cPatternEditor& pe = patternEditor;
    int p = (int)pe.editParam + steps;
    p = p < 0 ? 0 : p > (int)cPatternEditor::EditFx2 ? (int)cPatternEditor::EditFx2 : p;
    if (p == (int)pe.editParam)
        return;
    pe.previousEditParam = pe.editParam;
    pe.editParam = (uint8_t)p;
    pe.trackerPattern.selectedParam = (uint8_t)p;
    display.refreshControl(pe.patternControl);
    pe.showActualParamOnBar();
    pe.focusOnPattern();
}

void sendSelection(); // fPatternEdit.cpp: hands the cursor/selection to the sequencer

// Pattern screen, FX column, edit mode: change the effect type (the Tracker's
// "hold FX + turn the jog"), so the main encoder is left for the value.
// Returns false when that doesn't apply (the caller turns the encoder instead).
bool MpcInput::onFxTypeColumn()
{
    cPatternEditor& pe = patternEditor;
    return mtInterface.isPatternScreenActive() && !pe.fillState && pe.selectedPlace < 0 &&
           pe.editMode && !pe.isMicVolView &&
           (pe.editParam == cPatternEditor::EditFx1 || pe.editParam == cPatternEditor::EditFx2);
}

void MpcInput::stepFxType(int steps)
{
    cPatternEditor& pe = patternEditor;
    if (pe.patternViewMode > 0 && !(pe.patternViewMode & (1 << pe.editParam)))
        return; // column hidden in this view: do nothing, as the encoder would
    sendSelection();
    uint8_t fxIndex = pe.editParam == cPatternEditor::EditFx1 ? 1 : 0; // as the core maps it
    const auto& step = sequencer.getPatternToUI()
                         ->track[pe.trackerPattern.actualTrack]
                         .step[pe.trackerPattern.actualStep];
    if (step.fx[fxIndex].type == 0)
    {
        // empty: insert the last used effect first, as the Tracker's FX popup does
        uint8_t name = pe.getPreviousUsedFx();
        sequencer.setSelectionFxType(fxIndex, interfaceGlobals.fxIDs[name > 1 ? name : 2]); // else Volume
    }
    else
    {
        // Stop at the ends instead of wrapping (Mike, step 1d): the list runs
        // Off (name 1) .. last effect. The core steps one name at a time and
        // would go past Off to "none" (empty cell, which the branch above
        // refills) or past the end to "none" as well.
        int name = interfaceGlobals.fxIdToName(step.fx[fxIndex].type);
        int count = interfaceGlobals.fxNameCount(); // 0 = the list has no zero padding
        int last = (count ? count : FX_COUNT) - 1;
        int dir = steps > 0 ? 1 : -1;
        for (int n = abs(steps); n > 0 && name + dir >= 1 && name + dir <= last; n--, name += dir)
            sequencer.changeSelectionFxType(fxIndex, dir);
    }
    pe.showActualParamOnBar();
    pe.showFxInfo();
    pe.refreshPattern();
}

// Q-Link mode 1 (navigate). Q-Links 1-2 move around the sheet, 3-4 work on
// the selected cell:
//   1 = tracks left/right (the Tracker's Left/Right move a whole track)
//   2 = rows up/down (clockwise = down the pattern)
//   3 = Note > Instrument > FX1 > FX2 within the track (Pattern screen);
//       elsewhere left/right like Q-Link 1
//   4 = the selected cell's value (same as the main encoder); on an FX column
//       in edit mode, the effect TYPE (the main encoder changes its value)
//   Song screen (Mike's labels, 2026-11-12): 3 = jump between the song grid,
//       the pattern list and the tempo; 4 = the selected slot's pattern number
//       on the list (edit mode), the value elsewhere
//   Sample Loader / File screen: 2 = jump kListJump rows, 4 = one row
// Song screen, Q-Link 3: one area per step (song grid > pattern list > tempo).
// Left / Right already move between the areas except from the grid, where Right
// walks the tracks first, so that step jumps straight to the list. Outside edit
// mode the grid can't be entered (the Tracker's own rule).
void MpcInput::songJumpArea(int steps)
{
    for (int i = 0; i < abs(steps); i++)
    {
        if (steps > 0 && songEditor.selectedPlace == 0)
        {
            songEditor.selectedPlace = 1;
            songEditor.activateLabelsBorder();
        }
        else
            tapButton(steps > 0 ? interfaceButtonRight : interfaceButtonLeft, 1);
    }
}

void MpcInput::onRelative(uint8_t cc, uint8_t value)
{
    int steps = relSteps(value);
    if (!steps)
        return;
    if (cc == kCcMainEncoder)
    {
        if (!pointScreenZoom(steps))
            turnValue(steps);
        return;
    }
    if (cc < kCcQLink1 || cc >= kCcQLink1 + 4)
        return;
    int knob = cc - kCcQLink1;
    if (fxListOpen())
    {
        // the effect list scrolls on Q-Link 2 and 4 (and the main encoder)
        if (knob == 1 || knob == 3)
        {
            steps = divideQLink(knob, steps, knob == 1 ? 1 : 4);
            if (steps)
                encoder->turn(steps);
        }
        return;
    }
    if (qlinkMode == 1 && pointScreenTurn(knob, steps))
        return;
    if (qlinkScreenMode())
    {
        qlinkScreenTurn(knob, steps);
        return;
    }
    if (knob == 3 && onFxTypeColumn())
    {
        steps = divideQLink(4, steps);
        if (steps)
            stepFxType(steps);
        return;
    }
    steps = divideQLink(knob, steps);
    if (!steps)
        return;
    bool song = mtInterface.isSongScreenActive();
    switch (knob)
    {
        case 0: tapButton(steps > 0 ? interfaceButtonRight : interfaceButtonLeft, abs(steps)); break;
        case 1:
        {
            bool list = mtInterface.isSampleLoaderScreenActive() || mtInterface.isFileScreenActive();
            tapButton(steps > 0 ? interfaceButtonDown : interfaceButtonUp, abs(steps) * (list ? kListJump : 1));
            break;
        }
        case 2:
            if (mtInterface.isPatternScreenActive() && patternEditor.fillState == 0 &&
                patternEditor.selectedPlace < 0)
                stepEditParam(steps);
            else if (song)
                songJumpArea(steps);
            else
                tapButton(steps > 0 ? interfaceButtonRight : interfaceButtonLeft, abs(steps));
            break;
        case 3:
            if (song && songEditor.selectedPlace == 1 && songEditor.editMode)
                songEditor.changeSelectedPlayListPattern(steps);
            else
                turnValue(steps);
            break;
    }
}

// ---------------------------------------------------------------- Q-Link modes
// Q-LINK cycles 1 > 2 > 3 (mode 4, program quick controls, is still to be
// decided). In modes 2 and 3 the four knobs stand for the screen buttons
// under the red box (1-4 or 5-8): touching a knob holds that screen button,
// turning it holds the button and turns the jog, which is how the Tracker+
// changes the parameter above a button. When a finger on the label holds the
// button instead, a popup in the middle of the screen repeats the label while
// its value changes, since the finger covers it.
namespace
{
constexpr uint32_t kQLinkReleaseMs = 600; // untouched knob: let go of its button after this
constexpr uint32_t kValuePopupMs = 1500;
constexpr int kQLinkModes = 3;
} // namespace

void MpcInput::cycleQLinkMode()
{
    setQLinkMode(qlinkMode % kQLinkModes + 1, true);
}

// Modes 2-3 act on the screen buttons, except on screens whose buttons are
// all one-press actions with no value to turn: there the knobs navigate as in
// mode 1 and the red box goes away (Mike, step 2 test: the Pattern screen's
// second page, Render / Invert / Duplicate / Expand / Shrink / Copy / Paste).
bool MpcInput::qlinkScreenMode()
{
    if (qlinkMode == 1)
        return false;
    if (mtInterface.isPatternScreenActive() && patternEditor.actualActionButtonsPage != 0)
        return false;
    return true;
}

void MpcInput::updateQLinkBox()
{
    int group = qlinkScreenMode() ? qlinkMode - 2 : -1;
    if (group == boxGroup || !disp)
        return;
    boxGroup = group;
    disp->overlay().setBoxGroup(group);
    disp->requestRedraw();
}

void MpcInput::setQLinkMode(int mode, bool announce)
{
    releaseQLinkButton();
    memset(qlinkTouched, 0, sizeof qlinkTouched);
    memset(qlinkAcc, 0, sizeof qlinkAcc);
    qlinkMode = mode;
    updateQLinkBox();
    setQLinkLeds();
    if (announce)
    {
        static const char* const kNames[] = { "", "Q-Links: navigate", "Q-Links: buttons 1-4",
                                              "Q-Links: buttons 5-8" };
        mtPopups.show(0, kNames[mode]);
    }
    if (trace)
        printf("input: Q-Link mode %d\n", mode);
}

void MpcInput::holdScreenButton(int trackerButton)
{
    if (qlinkHeld == trackerButton)
        return;
    releaseQLinkButton();
    qlinkHeld = trackerButton;
    buttons->press(trackerButton);
}

void MpcInput::releaseQLinkButton()
{
    if (qlinkHeld < 0)
        return;
    buttons->release(qlinkHeld);
    qlinkHeld = -1;
}

void MpcInput::qlinkTouch(int knob, bool down)
{
    if (qlinkMode == 1 && pointScreenTouch(knob, down))
        return;
    if (!qlinkScreenMode())
    {
        qlinkTouched[knob] = false;
        return; // mode 1: touch does nothing (yet)
    }
    int target = interfaceButton0 + (qlinkMode - 2) * 4 + knob;
    qlinkTouched[knob] = down;
    if (down)
        holdScreenButton(target);
    else if (qlinkHeld == target)
        releaseQLinkButton();
}

void MpcInput::qlinkScreenTurn(int knob, int steps)
{
    steps = divideQLink(knob, steps, 3); // value speed for all four
    if (!steps)
        return;
    holdScreenButton(interfaceButton0 + (qlinkMode - 2) * 4 + knob);
    qlinkTurnMs = platform.getTimer()->millis();
    turnValue(steps);
}

// Turn the jog; when a finger on a label holds its screen button, show that
// label big, since the finger covers it. A Q-Link turn alone shows no popup:
// the label stays in view (Mike, step 2 test).
void MpcInput::turnValue(int steps)
{
    encoder->turn(steps);
    if (touchedButton >= interfaceButton0 && touchedButton <= interfaceButton7)
        showValuePopup(touchedButton - interfaceButton0);
}

void MpcInput::showValuePopup(int label)
{
    popupUntilMs = platform.getTimer()->millis() + kValuePopupMs;
    if (popupShown == label || !disp)
        return;
    popupShown = label;
    disp->overlay().setPopupLabel(label);
    disp->requestRedraw();
}

void MpcInput::pollQLinks()
{
    updateQLinkBox(); // follows screen and page changes
    uint32_t now = platform.getTimer()->millis();
    if (qlinkHeld >= 0 && now - qlinkTurnMs > kQLinkReleaseMs)
    {
        bool touched = false;
        for (bool t : qlinkTouched)
            touched |= t;
        if (!touched)
            releaseQLinkButton();
    }
    // The copy follows the label by itself: a value change redraws the frame.
    if (popupShown >= 0 && disp && (int32_t)(now - popupUntilMs) > 0)
    {
        popupShown = -1;
        disp->overlay().setPopupLabel(-1);
        disp->requestRedraw();
    }
}

// ---------------------------------------------------------------- per-screen maps (step 2b)
// Mike, 2026-11-11: on Sample Playback (1-shot and loop play modes) and the
// Sample Editor's main screen, Q-Link mode 1 is a point map instead of
// navigation. Since 2026-11-12 the order follows Mike's Q-Link labels:
// Q-Link 1 = Start, 2 = End, 3 = Loop start, 4 = Loop end (the editor has no
// loop points: 1 = Start, 2 = End); granular: 1 = Position, 2 = Length;
// wavetable: 1 = Position, 2 = Window size. QL3 / QL4 where they aren't loop
// points: the slow twins of QL1 / QL2 (1-shot, editor: Start / End at the
// finest step; wavetable: Position one window per message, Window size at the
// value speed); granular: 3 = Shape, 4 = Loop. The speed of the turn picks
// the step: messages more than pointFineMs apart move 1/8 of a screen pixel at
// the current zoom (at least one point unit, 1/65536 of the sample, the finest
// the Tracker stores; Mike couldn't see the original one-unit step), quicker
// ones one pixel, very quick ones (under pointFastMs) four pixels. Touching
// a point's knob selects the point and centres the view on it (MPC app style).
// The main encoder zooms (unless a finger holds a label, or the play mode list
// / effect list is the selected item, which the encoder keeps scrolling).
// Modes 2-3 keep working on these screens as before.
namespace
{
// How a point-map knob turns its value:
//   Scaled = by turn speed (fine / coarse / fast, see pointScreenTurn)
//   Fine   = always the finest step, one per message (QL3 / QL4 "slow" twins
//            of QL1 / QL2, for zoomed-in work; Mike 2026-11-12)
//   Plain  = one step per message (short lists: window size)
//   Slow   = a short list at the screen-button value speed (several messages
//            per step: granular Shape and Loop)
//   Slower = half that speed (wavetable window size on QL4; Mike 2026-11-12)
enum class Knob : uint8_t { Scaled, Fine, Plain, Slow, Slower };

struct PointMap
{
    int button[4] = { -1, -1, -1, -1 }; // Tracker screen button per knob, -1 = none
    uint16_t* value[4] = {};            // the point each knob moves
    Knob kind[4] = { Knob::Scaled, Knob::Scaled, Knob::Scaled, Knob::Scaled };
    int zoomButton = -1;                // -1 = the screen has no zoom (encoder turns the value)
    bool* fine = nullptr;               // the screen's mpdjFineStep flag (nullptr: none)
    strZoomParams* zoom = nullptr;
    uint8_t* refreshSpectrum = nullptr;
    uint8_t* refreshPoints = nullptr;
    bool listSelected = false;          // play mode / effect list is the selected item
    uint32_t sampleLength = 0;          // frames, for the zoom steps
};

bool pointMap(PointMap& m)
{
    if (mtInterface.isSamplePlayerScreenActive())
    {
        cSamplePlayback& sp = samplePlayback;
        strInstrument* ins = sp.editorInstrument;
        if (!ins || !ins->isActive)
            return false;
        if (sp.loadedInstrumentType == mtSampleTypeWavetable)
        {
            m.button[0] = interfaceButton1; // Position (window)
            m.button[1] = interfaceButton2; // Window size: a few sizes, one per message
            m.kind[1] = Knob::Plain;
            m.button[2] = interfaceButton1; // slow twins: one window per message
            m.kind[2] = Knob::Fine;
            m.button[3] = interfaceButton2;
            m.kind[3] = Knob::Slower;
            m.listSelected = sp.selectedPlace == 6;
            return true;
        }
        if (sp.loadedInstrumentType != mtSampleTypeWaveFile)
            return false;
        // Mike's Q-Link labels (2026-11-12): QL1 = start, QL2 = end, QL3 = loop / chop
        // start, QL4 = loop / chop end. Granular and wavetable put their window on
        // QL1 (where) and QL2 (how much); their other settings stay on Q-Link mode 2.
        if (ins->playMode == playModeGranular)
        {
            m.button[0] = interfaceButton1; // Position
            m.value[0] = &ins->granular.currentPosition;
            m.button[1] = interfaceButton2; // Length
            m.value[1] = &ins->granular.grainLength;
            m.button[2] = interfaceButton3; // Shape
            m.kind[2] = Knob::Slow;
            m.button[3] = interfaceButton4; // Loop
            m.kind[3] = Knob::Slow;
            m.listSelected = sp.selectedPlace == 6;
            return true; // no zoom on this screen
        }
        bool loop = ins->playMode == playModeLoopForward || ins->playMode == playModeLoopBackward ||
                    ins->playMode == playModePingpong;
        if (!loop && ins->playMode != playModeSingleShot)
            return false; // slice, beat slice: keep navigation (slicing gets its own map later)
        m.button[0] = interfaceButton1;
        m.value[0] = &ins->startPoint;
        m.button[1] = interfaceButton4;
        m.value[1] = &ins->endPoint;
        if (loop)
        {
            m.button[2] = interfaceButton2;
            m.value[2] = &ins->loopPoint1;
            m.button[3] = interfaceButton3;
            m.value[3] = &ins->loopPoint2;
        }
        else
        {
            // 1-shot: QL3 / QL4 = Start / End at the finest step
            m.button[2] = m.button[0];
            m.value[2] = m.value[0];
            m.kind[2] = Knob::Fine;
            m.button[3] = m.button[1];
            m.value[3] = m.value[1];
            m.kind[3] = Knob::Fine;
        }
        m.zoomButton = interfaceButton5;
        m.fine = &sp.mpdjFineStep;
        m.zoom = &sp.zoom;
        m.refreshSpectrum = &sp.refreshSpectrum;
        m.refreshPoints = &sp.refreshPoints;
        m.listSelected = sp.selectedPlace == 6;
        m.sampleLength = ins->sample.length;
        return true;
    }
    if (mtInterface.isSampleEditorScreenActive())
    {
        cSampleEditor& se = sampleEditor;
        // currentEffect is null while start() is showing a popup instead of the screen
        if (se.screenType != cSampleEditor::mainScreen || !se.currentEffect || !se.editorInstrument->isActive)
            return false;
        m.button[0] = interfaceButton0;
        m.value[0] = &se.selection.startPoint;
        m.button[1] = interfaceButton1;
        m.value[1] = &se.selection.endPoint;
        m.button[2] = interfaceButton0; // QL3 / QL4 = Start / End at the finest step
        m.value[2] = m.value[0];
        m.kind[2] = Knob::Fine;
        m.button[3] = interfaceButton1;
        m.value[3] = m.value[1];
        m.kind[3] = Knob::Fine;
        m.zoomButton = interfaceButton2;
        m.fine = &se.mpdjFineStep;
        m.zoom = &se.zoom;
        m.refreshSpectrum = &se.needRefreshSpectrum;
        m.refreshPoints = &se.needRefreshPoints;
        m.listSelected = se.selectedPlace[cSampleEditor::mainScreen] == cSampleEditor::effectButton;
        m.sampleLength = se.editorInstrument->sample.length;
        return true;
    }
    return false;
}
} // namespace

bool MpcInput::pointScreenTurn(int knob, int steps)
{
    PointMap m;
    if (!pointMap(m))
        return false;
    if (m.button[knob] < 0)
        return true; // no point on this knob here (1-shot, editor): do nothing
    uint32_t now = platform.getTimer()->millis();
    uint32_t dt = now - pointMsgMs[knob];
    pointMsgMs[knob] = now;

    Knob kind = m.kind[knob];
    if (kind == Knob::Slow || kind == Knob::Slower)
    {
        // screen-button value speed (kQLinkDiv[3]), or half of it (kQLinkDiv[4] = twice the messages)
        steps = divideQLink(knob, steps, kind == Knob::Slow ? 3 : 4);
        if (!steps)
            return true;
    }
    holdScreenButton(m.button[knob]);
    qlinkTurnMs = now;
    if (kind == Knob::Plain || kind == Knob::Slow || kind == Knob::Slower)
        turnValue(steps); // a short list of values: one per message
    else if (kind == Knob::Fine || dt >= pointFineMs)
    {
        if (m.fine)
            *m.fine = true; // one point unit per message
        turnValue(steps);
        if (m.fine)
            *m.fine = false;
    }
    else if (m.fine)
        turnValue(dt < pointFastMs ? steps * 4 : steps); // screen pixels
    else
        turnValue(dt < pointFastMs ? steps * 8 : steps * 2); // screens counting in their own units
    if (trace)
        printf("input: point knob %d %+d (%s, %u ms) -> %u\n", knob + 1, steps,
               kind == Knob::Plain ? "plain" : kind == Knob::Slow ? "slow" : kind == Knob::Slower ? "slower" : kind == Knob::Fine ? "fine knob"
               : dt >= pointFineMs ? "fine" : dt < pointFastMs ? "fast" : "coarse",
               (unsigned)dt, m.value[knob] ? *m.value[knob] : 0u);
    return true;
}

bool MpcInput::pointScreenTouch(int knob, bool down)
{
    PointMap m;
    if (!pointMap(m))
        return false;
    qlinkTouched[knob] = down && m.button[knob] >= 0;
    if (m.button[knob] < 0)
        return true;
    if (!down)
    {
        if (qlinkHeld == m.button[knob])
            releaseQLinkButton();
        return true;
    }
    holdScreenButton(m.button[knob]); // selects the point, as touching its label does
    if (!m.zoom || !m.value[knob])
        return true; // no waveform view to centre (granular length, wavetable)
    // Centre the view on the point; the spectrum is redrawn around zoomPosition
    // (a no-op at zoom 1, where the whole sample is in view).
    m.zoom->zoomPosition = *m.value[knob];
    m.zoom->lastChangedPoint = 0;
    *m.refreshSpectrum = 1;
    *m.refreshPoints = 1;
    return true;
}

bool MpcInput::pointScreenZoom(int steps)
{
    if (qlinkMode != 1 || touchedButton >= 0)
        return false; // a finger on a label: the encoder turns that label's value
    PointMap m;
    if (!pointMap(m) || m.listSelected || m.zoomButton < 0)
        return false;
    holdScreenButton(m.zoomButton);
    qlinkTurnMs = platform.getTimer()->millis();
    // The Tracker zooms in linear steps of the samples-per-pixel resolution
    // ((max - min) / 100 per click, GP.spectrumChangeZoom), so the first clicks
    // barely change the view (Mike: too slow). Turn each click into a fixed
    // ratio instead: x1.25 per click, full view to the deepest zoom in ~20.
    int value = steps;
    if (m.sampleLength >= 600)
    {
        int32_t maxRes = m.sampleLength / 600, minRes = m.sampleLength / 65535 + 1;
        int32_t step = (maxRes - minRes) / 100;
        if (step < 1)
            step = 1;
        int32_t res = m.zoom->zoomResolution;
        if (res < minRes || res > maxRes || m.zoom->zoomValue <= 1.0f)
            res = m.zoom->zoomResolution = maxRes; // the editor starts with it unset (0)
        int32_t target = (int32_t)lrintf(res * powf(0.8f, (float)steps)); // steps > 0 = zoom in
        value = (int)lrintf((float)(res - target) / step);
        if (value == 0)
            value = steps > 0 ? 1 : -1;
    }
    turnValue(value);
    if (trace)
        printf("input: zoom %+d clicks -> %+d steps, %.2fx\n", steps, value, m.zoom->zoomValue);
    return true;
}

// ---------------------------------------------------------------- tap the pattern grid (step 3)
// Mike, 2026-11-09: tapping a pattern cell moves the cursor there, selects its
// column (Note / Instrument / FX1 / FX2, as Q-Link 3 would) and switches the
// Q-Links to mode 1; the Tracker's own cursor frame marks the cell. In edit mode
// a tap on an FX cell opens the Tracker's effect list (the one holding FX shows
// on the Tracker+): tap a row, or scroll it (main encoder, Q-Link 2 or 4) and
// press the encoder, to put that effect in the cell; ERASE / STOP or a tap
// elsewhere closes it unchanged.
//
// Geometry follows cTracker::update() (libs/NewDisplay/tracker.cpp, ROW_NUM on):
// 27 px of row numbers on each side; 4 tracks of 186 px (view 0) or 8 of 93
// (views showing 1-2 columns); 15 rows of height / 15 from y = 0 (pattern info
// shown) or 28, the cursor on row 7. Vertical arrangement only.
namespace
{
constexpr int kGridSide = 27;
constexpr int kListX = 605, kListTop = 44, kListRow = 27, kListRows = 16; // step popup list
} // namespace

bool MpcInput::fxListOpen()
{
    if (fxPopupByTap && mtPopups.getStepPopupState() != stepPopupFx)
        fxPopupByTap = false; // closed some other way
    return fxPopupByTap;
}

void MpcInput::closeFxList(bool apply)
{
    fxPopupByTap = false;
    if (mtPopups.getStepPopupState() != stepPopupFx)
        return;
    cPatternEditor& pe = patternEditor;
    uint8_t keep = pe.insertOnPopupHideDisabled;
    if (!apply)
        pe.insertOnPopupHideDisabled = 1; // cancelPopups then only closes it
    pe.cancelPopups();
    pe.insertOnPopupHideDisabled = keep;
    if (trace)
        printf("input: effect list closed (%s)\n", apply ? "kept" : "unchanged");
}

void MpcInput::onScreenTap(float x, float y)
{
    if (fxListOpen())
    {
        cList* list = (cList*)display.mpdjControl(mtPopups.mpdjStepPopupList());
        if (list && list->list && x >= kListX && y >= kListTop && y < kListTop + kListRow * kListRows)
        {
            int idx = list->textListPos + (int)((y - kListTop) / kListRow);
            if (idx < list->list->length)
            {
                mtPopups.setStepPopupValue(idx);
                if (trace)
                    printf("input: effect list row %d picked\n", idx);
                closeFxList(true);
                return;
            }
        }
        closeFxList(false);
        return;
    }
    if (mtInterface.isPatternScreenActive())
        tapPatternCell(x, y);
}

void MpcInput::setEditParam(int p)
{
    cPatternEditor& pe = patternEditor;
    if (p == (int)pe.editParam)
        return;
    pe.previousEditParam = pe.editParam;
    pe.editParam = (uint8_t)p;
    pe.trackerPattern.selectedParam = (uint8_t)p;
    display.refreshControl(pe.patternControl);
    pe.showActualParamOnBar();
    pe.focusOnPattern();
}

void MpcInput::tapPatternCell(float x, float y)
{
    cPatternEditor& pe = patternEditor;
    strTrackerPattern& tp = pe.trackerPattern;
    if (pe.fillState || pe.selectedPlace >= 0 || mtPopups.getStepPopupState() != stepPopupNone ||
        mtConfig.general.patternArrangement != 0)
        return;
    const bool info = mtConfig.general.patternInfo != 0;
    const int top = info ? 0 : 28, rowH = (info ? 423 : 395) / 15;
    int row = (int)((y - top) / rowH);
    if (y < top || row < 0 || row > 14 || tp.track[0].row[row].note[0] == 0)
        return; // above the grid, or past the pattern's end
    const int view = pe.patternViewMode;
    const int trackW = (800 - 2 * kGridSide) / (view ? 8 : 4), cols = view ? 8 : 4;
    int col = (int)((x - kGridSide) / trackW);
    if (x < kGridSide || col < 0 || col >= cols)
        return; // row numbers
    int track = tp.firstVisibleTrack + col;
    if (track >= strTrackerPattern::kTrackCount)
        return;
    float rel = x - kGridSide - col * trackW;
    int param;
    if (!view)
        param = rel < 43 ? 0 : rel < 77 ? 1 : rel < 131 ? 2 : 3; // fourParamsSelOffset
    else
    {
        int shown[4], n = 0;
        for (int p = 0; p < 4; p++)
            if (view & (1 << p))
                shown[n++] = p;
        param = n ? shown[std::min(n - 1, (int)(rel * n / trackW))] : 0;
    }

    for (int dt = track - tp.actualTrack; dt; dt += dt > 0 ? -1 : 1)
        dt > 0 ? pe.nextTrack() : pe.previousTrack();
    for (int dr = row - 7; dr; dr += dr > 0 ? -1 : 1)
        dr > 0 ? pe.nextStep() : pe.previousStep(); // does nothing while playing outside edit mode
    setEditParam(param);
    if (qlinkMode != 1)
        setQLinkMode(1, true);
    if (trace)
        printf("input: tap (%.0f, %.0f) -> track %d row %+d param %d\n", x, y, track + 1, row - 7, param);

    if (param >= 2 && pe.editMode && !pe.isMicVolView)
    {
        param == 2 ? pe.showFx1Popup() : pe.showFx2Popup();
        fxPopupByTap = mtPopups.getStepPopupState() == stepPopupFx;
    }
}

// ---------------------------------------------------------------- LEDs
// Controller LEDs are lit with CC messages on channel 1 sent back on the
// Private port (mapped by Mike with MPDJ_LED_PROBE, 2026-11-10). Button LEDs
// use the button's own note number as the CC (PLAY 82, OVER DUB 80, REC 73,
// UNDO 67, TAP TEMPO 53); the four LEDs above Q-LINK are CC 90-93 and follow
// the Q-Link mode. MPDJ_QLINK_LEDS overrides them ("cc:a,b,c,d" or
// "note:a,b,c,d", "off" for none), MPDJ_QLINK_LED_ON the value (default 127).
void MpcInput::midiOut(uint8_t s, uint8_t d1, uint8_t d2)
{
    if (midiFd < 0)
        return;
    uint8_t m[3] = { s, d1, d2 };
    if (write(midiFd, m, 3) != 3 && trace)
        printf("input: LED write failed: %s\n", strerror(errno));
}

void MpcInput::parseQLinkLeds()
{
    const char* v = envStr("MPDJ_QLINK_LEDS", "cc:90,91,92,93");
    ledOn = (uint8_t)atoi(envStr("MPDJ_QLINK_LED_ON", "127"));
    if (!strcmp(v, "off"))
        return;
    if (!strncmp(v, "note:", 5))
        ledStatus = 0x90, v += 5;
    else if (!strncmp(v, "cc:", 3))
        ledStatus = 0xB0, v += 3;
    else
    {
        printf("input: MPDJ_QLINK_LEDS should be cc:a,b,c,d or note:a,b,c,d\n");
        return;
    }
    for (int i = 0; i < 4 && *v; i++)
    {
        char* end;
        long n = strtol(v, &end, 0);
        if (end == v || n < 0 || n > 127)
            break;
        qlinkLed[i] = (int)n;
        ledCount = i + 1;
        v = *end == ',' ? end + 1 : end;
    }
    printf("input: Q-Link mode LEDs: %s %d,%d,%d,%d\n", ledStatus == 0x90 ? "note" : "cc",
           qlinkLed[0], qlinkLed[1], qlinkLed[2], qlinkLed[3]);
}

void MpcInput::setQLinkLeds()
{
    for (int i = 0; i < ledCount; i++)
        if (qlinkLed[i] >= 0)
            midiOut(ledStatus, (uint8_t)qlinkLed[i], i == qlinkMode - 1 ? ledOn : 0);
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
    if (rawMaxX <= rawMinX || rawMaxY <= rawMinY)
    {
        // Not an evdev device (a FIFO of input_event structs for scripted tests):
        // assume the ILI2117's range.
        rawMinX = rawMinY = 0;
        rawMaxX = rawMaxY = 2048;
    }
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
    if (target < 0 && nx >= 0 && nx < 800 && ny >= 0 && ny < kScreenButtonTop)
    {
        onScreenTap(nx, ny);
        return;
    }
    if (target >= 0 && fxListOpen())
        closeFxList(true); // a screen button while the effect list is open: keep the pick
    if (target >= 0)
    {
        touchedButton = target;
        buttons->press(target);
    }
}
