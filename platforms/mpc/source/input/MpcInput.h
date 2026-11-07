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
//   MPDJ_TOUCH_SWAP_XY=1, MPDJ_TOUCH_INVERT_X=1, MPDJ_TOUCH_INVERT_Y=1
//                               fix-ups if taps land in the wrong place
#pragma once

#include <stdint.h>

class FbOutput;
class SwitchGroupImpl;
class EncoderImpl;

class MpcInput
{
  public:
    void open(SwitchGroupImpl* buttons, SwitchGroupImpl* pads, EncoderImpl* encoder,
              const FbOutput* fb);
    void poll();
    void close();

  private:
    // ---- controller MIDI
    void openMidi();
    void pollMidi();
    void onMidi(uint8_t status, uint8_t d1, uint8_t d2);
    void onButton(uint8_t note, bool down);
    void onPad(uint8_t note, bool down);
    void onRelative(uint8_t cc, uint8_t value);
    void tapButton(int trackerButton, int times);
    void tapWithShift(int trackerButton);
    bool coreShift();
    void releaseCoreShift();
    void tapTempo();
    bool mpcShift = false; // MPC SHIFT physically held
    static constexpr int kTaps = 5;
    uint32_t tapTimes[kTaps] = {};
    int tapCount = 0;
    int divideQLink(int knob, int steps);
    void stepEditParam(int steps);
    bool stepFxType(int steps);
    int qlinkAcc[4] = {};
    int qlinkDivider = 4; // MPDJ_QLINK_DIV

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
    int padBank = 0;            // 0..2 = Tracker pad columns 0-3, 4-7, 8-11
    int8_t padHeld[128];        // MPC pad note -> Tracker pad index sent on press
    int8_t buttonHeld[128];     // MPC button note -> Tracker button sent on press
};
