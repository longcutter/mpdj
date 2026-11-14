// MPC One+ controls -> Tracker inputs. The agreed control map is the project
// doc claude/mpdj-control-map.tsv (build steps 1 and 1b are implemented here).
//
// The controller (pads, buttons, Q-Links, main encoder) speaks MIDI on the
// Private raw MIDI port hw:0,0,1; the touchscreen is /dev/input/event0.
// Everything is read non-blocking from the main loop, so callbacks into the
// tracker core run on the same thread as the UI.
//
// Environment:
//   MPDJ_INPUT_TRACE=1          print every MIDI message and touch
//   MPDJ_MIDI_DEV=/dev/snd/midiC0D0, MPDJ_MIDI_SUBDEV=1
//   MPDJ_TOUCH_DEV=/dev/input/event0
//   MPDJ_QLINK_DIV=4            Q-Link speed scale (4 = the built-in speeds,
//                               higher = slower)
//   MPDJ_QLINK_LEDS=cc:90,91,92,93 (default; note:..., off) Q-Link mode LEDs, MPDJ_QLINK_LED_ON=127
//   MPDJ_TOUCH_SWAP_XY=1, MPDJ_TOUCH_INVERT_X=1, MPDJ_TOUCH_INVERT_Y=1
//                               fix-ups if taps land in the wrong place
#pragma once

#include <stdint.h>

class DisplayDriverImpl;
class FbOutput;
class SwitchGroupImpl;
class EncoderImpl;

class MpcInput
{
  public:
    void open(SwitchGroupImpl* buttons, SwitchGroupImpl* pads, EncoderImpl* encoder,
              DisplayDriverImpl* display);
    void poll();
    void close();

    // Sideloads (sideload/Sideload.h): let go of everything held in the core,
    // switch the LEDs off and close the touchscreen; the controller MIDI port
    // stays open so MPDJ can relay it to the sideload and watch for the way back.
    void suspend();
    void resume();
    int controllerFd() const { return midiFd; }

  private:
    // ---- controller MIDI
    void openMidi();
    void pollMidi();
    void onMidi(uint8_t status, uint8_t d1, uint8_t d2);
    void onButton(uint8_t note, bool down);
    void onPad(uint8_t note, bool down, uint8_t velocity);
    // MPDJ pads (step 5b-3): the pad engine plays the pad map on the Pattern
    // screen; elsewhere, in popups, while recording / editing, and in grid
    // mode (16 LEVELS) pads drive the Tracker's 4x12 grid as before
    bool padEngineHere() const;
    void onRelative(uint8_t cc, uint8_t value);
    void tapButton(int trackerButton, int times);
    void tapWithShift(int trackerButton);
    bool coreShift();
    void releaseCoreShift();
    void tapTempo();
    void saveProject();
    void pollSave();
    bool savePending = false;
    bool onProgramScreen();
    int confirmPopupButton(uint8_t note, int act);
    void closeProgramPicker();
    bool programPicker = false; // SHIFT holding the Tracker's Instr button
    bool mpcShift = false; // MPC SHIFT physically held
    uint32_t sideloadComboMs = 0; // SHIFT+MAIN held since (MPDJ_SIDELOAD set)
    static constexpr int kTaps = 5;
    uint32_t tapTimes[kTaps] = {};
    int tapCount = 0;
    static constexpr uint32_t kTapHoldMs = 1000;
    uint32_t tapHoldMs = 0; // TAP TEMPO press time while it may become a hold
    void pollTapHold();
    int divideQLink(int knob, int steps, int divIdx = -1);
    void songJumpArea(int steps);
    void stepEditParam(int steps);
    void setEditColumn(int param, bool micVol);
    void flipMicVolView();
    void padStepEntry(int pad, uint8_t velocity);
    bool onFxTypeColumn();
    void stepFxType(int steps);
    int qlinkAcc[5] = {}; // [4] = Q-Link 4 on an FX column (FX type)
    int qlinkDivider = 4; // MPDJ_QLINK_DIV

    // ---- Q-Link modes (build step 2): 1 = navigate, 2 = screen buttons 1-4,
    // 3 = screen buttons 5-8 (turn = change, touch the knob = press)
    void cycleQLinkMode();
    void setQLinkMode(int mode, bool announce);
    void qlinkScreenTurn(int knob, int steps);
    void qlinkTouch(int knob, bool down);
    void holdScreenButton(int trackerButton);
    void releaseQLinkButton();
    void turnValue(int steps); // main encoder / value knob: + value popup for a held label
    void showValuePopup(int label);
    void pollQLinks();
    bool qlinkScreenMode(); // modes 2-3 and the screen has value buttons
    void updateQLinkBox();
    int boxGroup = -1;      // red box shown: 0 = labels 1-4, 1 = 5-8, -1 none

    // ---- per-screen Q-Link maps (build step 2b): Sample Playback and the Sample
    // Editor's main screen take over mode 1: one knob per point, turned slowly =
    // fine (one point unit per message), fast = coarse (screen pixels); touching
    // a point's knob selects it and centres the view on it; the main encoder zooms
    bool pointScreenTurn(int knob, int steps);
    bool pointScreenTouch(int knob, bool down);
    bool pointScreenZoom(int steps);
    uint32_t pointMsgMs[4] = {};

    // ---- tap the pattern grid (build step 3): a tap moves the cursor to the
    // cell, selects its column and switches the Q-Links to mode 1; in edit mode
    // a tap on an FX cell opens the Tracker's effect list, and a tap on a list
    // row picks that effect
    void onScreenTap(float x, float y);
    void tapPatternCell(float x, float y);
    bool fxListOpen();
    void closeFxList(bool apply);
    void setEditParam(int param);
    bool fxPopupByTap = false;
    uint32_t pointFineMs = 30, pointFastMs = 8; // MPDJ_POINT_FINE_MS / _FAST_MS
    int qlinkMode = 1;
    int qlinkHeld = -1;          // Tracker screen button held for the Q-Links
    bool qlinkTouched[4] = {};   // Q-Link touch notes 84-87
    uint32_t qlinkTurnMs = 0, popupUntilMs = 0;
    int popupShown = -1;
    DisplayDriverImpl* disp = nullptr;

    // ---- LEDs (Q-Link mode LEDs; MPDJ_QLINK_LEDS until the probe maps them)
    void midiOut(uint8_t status, uint8_t d1, uint8_t d2);
    void parseQLinkLeds();
    void setQLinkLeds();
    // Button LEDs for state (pollButtonLeds)
    bool buttonLedsEnabled = true;
    bool noteDown[128] = {};     // MPC button notes held right now
    uint8_t ledSent[128];        // last value sent per LED CC, 0xFF = unknown
    // Pad lights (5c-1): SysEx on the controller's Public port
    int padLedFd = -1;
    uint8_t padLedSent[16][3];   // last RGB sent per pad, 0xFF = unknown
    uint32_t padLedPollMs = 0;
    uint32_t trackHitCount[16] = {};
    uint32_t trackHitMs[16] = {};
    uint8_t trackHitProgram[16] = {};
    uint8_t trackHitLevel[16] = {};
    uint8_t trackHitNote[16] = {};
    void openPadLeds();
    void pollPadLights();
    void setPadLed(int pad, uint8_t r, uint8_t g, uint8_t b);
    void padLedsOff();
    int lastEngineNote = -1; // the last pad (note) the pad engine took
    bool turnHeldPadColour(int steps);
    uint32_t beatLedMs = 0;      // when TAP TEMPO last flashed
    uint32_t beatRefMs = 0;      // free-running beat clock while stopped
    int lastBeatPos = -1;        // sequencer row seen last while playing
    void pollButtonLeds();
    void buttonLedsOff();
    void setButtonLed(uint8_t cc, uint8_t value);
    uint8_t qlinkLedValue();     // Q-Link mode LED level, from the Button lights setting
    bool qlinkLedOnEnv = false;  // MPDJ_QLINK_LED_ON given
    int lightsModeSent = -1;
    int qlinkLed[4] = { -1, -1, -1, -1 };
    int ledCount = 0;
    uint8_t ledStatus = 0xB0, ledOn = 127;

    int midiFd = -1;
    uint8_t runningStatus = 0, data[2] = {}, dataCount = 0;
    bool inSysex = false;

    // ---- touchscreen
    void openTouch();
    void pollTouch();
    void onTouchFrame();

    int touchFd = -1;
    int rawMinX = 0, rawMaxX = 0, rawMinY = 0, rawMaxY = 0;
    bool swapXY = false, invertX = false, invertY = false;
    int slot = 0, rawX = 0, rawY = 0;
    bool touching = false, wasTouching = false;
    int touchedButton = -1;

    // ---- state
    SwitchGroupImpl* buttons = nullptr;
    SwitchGroupImpl* pads = nullptr;
    EncoderImpl* encoder = nullptr;
    const FbOutput* fb = nullptr;
    bool trace = false;
    int padBank = 0;            // 0..3 = banks A-D (grid: Tracker pad columns 0-3, 4-7, 8-11; D = C)
    int bankBeforeNotes = -1;   // the bank to return to when Notes mode ends (5b-8 follow-up)
    bool padGrid = false;       // 16 LEVELS: the Pattern screen's pads drive the 4x12 grid
    bool repeatHeld = false;    // NOTE REPEAT held (5b-7a)
    bool repeatLatched = false; // SHIFT+NOTE REPEAT latch (5b-7a)
    bool repeatUnlatchOnRelease = false; // a press while latched (5b-7b: a turn keeps the latch)
    int repeatDivision = 4;              // index into kRepeatDivisions (5b-7b), 4 = 1/16
    void turnRepeatDivision(int steps);
    void updateNoteRepeat();
    void showNotesPopup();
    bool levelsHeld = false;   // 16 LEVELS held (5b-8b)
    bool levelsTurned = false; // the encoder turned while it was held
    void toggleLevels();
    void pollLevels();
    void turnNotesScale(int steps);
    int8_t padHeld[128];        // MPC pad note -> Tracker pad index sent on press
    int8_t enginePadHeld[128];  // MPC pad note -> MPDJ pad (0-63) pressed in the pad engine
    int8_t buttonHeld[128];     // MPC button note -> Tracker button sent on press
};
