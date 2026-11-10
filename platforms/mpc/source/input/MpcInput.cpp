#include "MpcInput.h"

#include "PlatformImpl.h"
#include "DisplayDriverImpl.h"
#include "eve/FbOutput.h"
#include "core/interfaceDefs.h"
#include "core/interface.h"
#include "display.h"
#include "core/interfacePopups.h"
#include "fileManager.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"
#include "projectEditor/projectEditor.h"
#include "songEditor/songEditor.h"

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
void MpcInput::open(SwitchGroupImpl* b, SwitchGroupImpl* p, EncoderImpl* e, DisplayDriverImpl* d)
{
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
    pollSave();
    pollQLinks();
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
    if (down && note >= kNoteBankA && note <= kNoteBankC)
    {
        padBank = note - kNoteBankA;
        if (trace)
            printf("input: pad bank %c\n", 'A' + padBank);
        return;
    }

    if (!down)
    {
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

    switch (m->act)
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
//   Song screen: 3 = pattern number of the selected slot (edit mode)
void MpcInput::onRelative(uint8_t cc, uint8_t value)
{
    int steps = relSteps(value);
    if (!steps)
        return;
    if (cc == kCcMainEncoder)
    {
        turnValue(steps);
        return;
    }
    if (cc < kCcQLink1 || cc >= kCcQLink1 + 4)
        return;
    int knob = cc - kCcQLink1;
    if (qlinkMode != 1)
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
    switch (knob)
    {
        case 0: tapButton(steps > 0 ? interfaceButtonRight : interfaceButtonLeft, abs(steps)); break;
        case 1: tapButton(steps > 0 ? interfaceButtonDown : interfaceButtonUp, abs(steps)); break;
        case 2:
            if (mtInterface.isPatternScreenActive() && patternEditor.fillState == 0 &&
                patternEditor.selectedPlace < 0)
                stepEditParam(steps);
            else if (mtInterface.isSongScreenActive())
                songEditor.changeSelectedPlayListPattern(steps); // in edit mode (REC)
            else
                tapButton(steps > 0 ? interfaceButtonRight : interfaceButtonLeft, abs(steps));
            break;
        case 3: turnValue(steps); break;
    }
}

// ---------------------------------------------------------------- Q-Link modes
// Q-LINK cycles 1 > 2 > 3 (mode 4, program quick controls, is still to be
// decided). In modes 2 and 3 the four knobs stand for the screen buttons
// under the red box (1-4 or 5-8): touching a knob holds that screen button,
// turning it holds the button and turns the jog, which is how the Tracker+
// changes the parameter above a button. A popup in the middle of the screen
// repeats the label while its value changes, since a finger may cover it.
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

void MpcInput::setQLinkMode(int mode, bool announce)
{
    releaseQLinkButton();
    memset(qlinkTouched, 0, sizeof qlinkTouched);
    memset(qlinkAcc, 0, sizeof qlinkAcc);
    qlinkMode = mode;
    if (disp)
    {
        disp->overlay().setBoxGroup(mode == 2 ? 0 : mode == 3 ? 1 : -1);
        disp->requestRedraw();
    }
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
    if (qlinkMode == 1)
        return; // mode 1: touch does nothing (yet)
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

// Turn the jog; when it changes a screen button's parameter (the button is
// held by a Q-Link or a finger on its label), show that label big.
void MpcInput::turnValue(int steps)
{
    encoder->turn(steps);
    int held = qlinkHeld >= 0 ? qlinkHeld : touchedButton;
    if (held >= interfaceButton0 && held <= interfaceButton7)
        showValuePopup(held - interfaceButton0);
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
    if (target >= 0)
    {
        touchedButton = target;
        buttons->press(target);
    }
}
