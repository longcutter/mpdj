// MPDJ (step 5e): the Track Mute screen, opened with TRACK MUTE on the MPC.
// The 16 tracks as a 4x4 grid (pad N = track N, whatever the bank); a pad hit
// or a touch on a cell mutes / unmutes the track. MIT, see NOTICE.md (MPDJ
// additions).
#pragma once

#include "core/modulesBase.h"
#include "padGridControl.h"

class cTrackMute : public ModuleBase
{
  public:
    virtual void update() {}
    virtual void start(uint32_t options);
    virtual void stop();
    virtual void initDisplayControls();
    virtual void destroyDisplayControls();

    void toggle(int track);
    // 5e-2: pads follow Latch / Momentary; a release ends a momentary flip
    // (on any screen, so a pad held while leaving still lets go)
    void padDown(int track);
    void padUp(int track);
    void toggleMode();
    void cycleTiming(); // 5e-3: Now / Next bar / Next pattern
    // Every poll, any screen: finish changes the audio thread applied (grey
    // the Pattern screen, save flag); apply queued ones at once when stopped
    void syncQueued();
    void unmuteAll();
    // 2026-11-16: per-track swing: screen button 2 flips the pads / touches between
    // muting and setting which tracks the project swing moves
    void toggleSwingEdit();
    bool swingEdit = false;
    int trackAt(float x, float y) const; // touch -> track 0-15, -1 off the grid
    void refresh();

    static constexpr int kGridX = 6, kGridY = 34, kGridW = 788, kGridH = 382;

  private:
    hControl grid = -1;
    hControl label[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    hControl bgLabel = -1;
    strPadGridData gridData;
    char title[32];
    char rightLabel[32];
    bool held[16] = {};
    char modeText[12];
    char timingText[12];
    char swingText[12];
    char gridText[8];
    char humanText[12];
    uint32_t lastBlinkMs = 0;
    void flip(int track, bool queueable); // queue (Latch + timing + playing) or now
};

extern cTrackMute trackMute;

// Mute / unmute a track the way the Pattern screen's mute buttons do (the
// voice, the pattern's greyed column, the project's save flag)
void mpdjSetTrackMute(int track, bool mute);
