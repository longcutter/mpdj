// MPDJ (step 5e-1): the Track Mute screen, see trackMute.h
#include "trackMute/trackMute.h"

#include "core/interface.h"
#include "fileManager.h"
#include "mpdjPads.h"
#include "mpdjTracks.h"
#include "mtAudioEngine.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "mtConfig.h"
#include "patternEditor/patternEditor.h"

#include <stdio.h>
#include <string.h>

cTrackMute trackMute;
static cTrackMute* TM = &trackMute;

void mpdjSetTrackMute(int track, bool mute)
{
    if (track < 0 || track >= kAllTracks)
        return;
    trackMuteRef(track) = mute;
    engine.muteTrack(track, mute);
    patternEditor.trackerPattern.inactive[track] = mute;
    newFileManager.setProjectStructChanged();
}

namespace
{
uint32_t rgb(const uint8_t* c)
{
    return (uint32_t)(c[0] * 2) << 16 | (uint32_t)(c[1] * 2) << 8 | (uint32_t)(c[2] * 2);
}

void functSwitchModule(uint8_t button) { TM->eventFunct(eventSwitchModule, TM, &button, 0); }

void functPlayAction()
{
    if (sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP)
    {
        if (TM->buttons->isButtonPressed(interfaceButtonShift))
            sequencer.playSong();
        else
            sequencer.playPattern();
    }
    else
        sequencer.stop();
}

void functUnmuteAll(uint8_t button, uint8_t state)
{
    (void)button;
    if (state == buttonPress)
        TM->unmuteAll();
}

void functMode(uint8_t button, uint8_t state)
{
    (void)button;
    if (state == buttonPress)
        TM->toggleMode();
}

void functTiming(uint8_t button, uint8_t state)
{
    (void)button;
    if (state == buttonPress)
        TM->cycleTiming();
}

const char* const kLabels[8] = { "Unmute all", "", "", "", "", "", "Timing", "Pads" };
const char* const kTimingNames[3] = { "Now", "Next bar", "Pattern" };
const char* const kTimingShort[3] = { "", "next bar", "@ pattern" };
} // namespace

void cTrackMute::initDisplayControls()
{
    strControlProperties prop;
    prop.x = kGridX;
    prop.y = kGridY;
    prop.w = kGridW;
    prop.h = kGridH;
    prop.style = controlStyleShow;
    prop.data = &gridData;
    grid = display.createControl<cPadGrid>(&prop);

    for (int i = 0; i < 8; i++)
    {
        strControlProperties p;
        p.text = (char*)kLabels[i];
        p.colors = display.getTheme().activeLabelsColors;
        p.style = controlStyleCenterX | controlStyleFont3 | controlStyleCenterY;
        p.x = (800 / 8) * i + (800 / 16);
        p.w = 800 / 8 - 6;
        p.y = 424;
        p.h = 55;
        p.value = i >= 6 ? 1 : 0; // the Timing and Pads labels have two lines
        label[i] = display.createControl<cLabel>(&p);
    }
    strControlProperties pb;
    pb.text = nullptr;
    pb.colors = nullptr;
    pb.value = 255;
    pb.style = controlStyleNoTransparency | controlStyleShow;
    pb.x = 0;
    pb.w = 800;
    pb.y = 425;
    pb.h = 54;
    bgLabel = display.createControl<cBgLabel>(&pb);
}

void cTrackMute::destroyDisplayControls()
{
    display.destroyControl(grid);
    for (hControl& l : label)
    {
        display.destroyControl(l);
        l = -1;
    }
    display.destroyControl(bgLabel);
    grid = bgLabel = -1;
}

void cTrackMute::start(uint32_t options)
{
    (void)options;
    moduleRefresh = 1;
    for (uint8_t b : { interfaceButtonPerformance, interfaceButtonParams, interfaceButtonFile,
                       interfaceButtonConfig, interfaceButtonMaster, interfaceButtonSamplePlay,
                       interfaceButtonSampleEdit, interfaceButtonSampleRec, interfaceButtonSampleLoad,
                       interfaceButtonSong, interfaceButtonPattern })
        ip->setButtonObj(b, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButton0, functUnmuteAll);
    ip->setButtonObj(interfaceButton6, functTiming);
    ip->setButtonObj(interfaceButton7, functMode);
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayAction);

    titleBar->show();
    for (hControl l : label)
    {
        display.setControlShow(l);
        display.refreshControl(l);
    }
    display.setControlShow(bgLabel);
    display.refreshControl(bgLabel);
    display.setControlShow(grid);
    refresh();
}

void cTrackMute::stop() { moduleRefresh = 0; }

void cTrackMute::flip(int track, bool queueable)
{
    if (track < 0 || track >= kAllTracks)
        return;
    if (queueable && mtConfig.general.trackMuteTiming != 0 && sequencer.isPlay())
        mpdjMuteQueue(track); // again before it happens = cancel
    else
        mpdjSetTrackMute(track, !trackMuteRef(track));
    if (mtInterface.isTrackMuteScreenActive())
        refresh();
}

void cTrackMute::toggle(int track) { flip(track, true); }

void cTrackMute::cycleTiming()
{
    mtConfig.general.trackMuteTiming = (mtConfig.general.trackMuteTiming + 1) % 3;
    saveConfig();
    refresh();
}

void cTrackMute::syncQueued()
{
    bool onScreen = mtInterface.isTrackMuteScreenActive();
    if (uint16_t a = mpdjMuteTakeApplied())
    {
        for (int t = 0; t < kAllTracks; t++)
            if (a & (1u << t))
                patternEditor.trackerPattern.inactive[t] = trackMuteRef(t);
        newFileManager.setProjectStructChanged();
        if (mtInterface.isPatternScreenActive())
            display.refreshControl(patternEditor.patternControl);
        if (onScreen)
            refresh();
    }
    if (!sequencer.isPlay() && mpdjMutePending())
    {
        // stopped before the bar came: apply now
        uint16_t m = mpdjMuteTakePending();
        for (int t = 0; t < kAllTracks; t++)
            if (m & (1u << t))
                mpdjSetTrackMute(t, !trackMuteRef(t));
        if (onScreen)
            refresh();
    }
}

void cTrackMute::padDown(int track)
{
    if (track < 0 || track >= kAllTracks)
        return;
    bool momentary = mtConfig.general.trackMuteMode == 1;
    flip(track, !momentary); // Momentary is always immediate
    held[track] = momentary;
}

void cTrackMute::padUp(int track)
{
    if (track < 0 || track >= kAllTracks || !held[track])
        return;
    held[track] = false;
    flip(track, false); // momentary: back as it was, right away
}

void cTrackMute::toggleMode()
{
    mtConfig.general.trackMuteMode = !mtConfig.general.trackMuteMode;
    saveConfig();
    refresh();
}

void cTrackMute::unmuteAll()
{
    bool queue = mtConfig.general.trackMuteTiming != 0 && sequencer.isPlay();
    uint16_t pending = mpdjMutePending();
    for (int t = 0; t < kAllTracks; t++)
    {
        bool p = pending & (1u << t);
        bool m = trackMuteRef(t);
        if (!queue)
        {
            if (m)
                mpdjSetTrackMute(t, false);
        }
        else if (m != p) // muted and not yet queued, or unmuted with a mute queued
            mpdjMuteQueue(t);
    }
    refresh();
}

int cTrackMute::trackAt(float x, float y) const
{
    if (x < kGridX || x >= kGridX + kGridW || y < kGridY || y >= kGridY + kGridH)
        return -1;
    int col = (int)((x - kGridX) * 4 / kGridW), row = 3 - (int)((y - kGridY) * 4 / kGridH);
    return row * 4 + col;
}

void cTrackMute::refresh()
{
    int muted = 0;
    for (int t = 0; t < kAllTracks; t++)
    {
        strPadGridData::Cell& c = gridData.cells[t];
        bool m = trackMuteRef(t);
        muted += m;
        snprintf(c.name, sizeof c.name, "T%d", t + 1);
        snprintf(c.program, sizeof c.program, "%s", trackNameRef(t));
        if (mpdjMutePending() & (1u << t))
            snprintf(c.marks, sizeof c.marks, "%s %s", m ? "PLAY" : "MUTE", kTimingShort[mtConfig.general.trackMuteTiming]);
        else
            snprintf(c.marks, sizeof c.marks, "%s", m ? "MUTED" : "");
        c.color = m ? 0 : rgb(kPadPalette[t % kPadColors]);
        c.empty = m;
    }
    gridData.selected = -1;
    snprintf(modeText, sizeof modeText, "%s", mtConfig.general.trackMuteMode ? "Momentary" : "Latch");
    display.setControlText2(label[7], modeText);
    display.refreshControl(label[7]);
    snprintf(timingText, sizeof timingText, "%s", kTimingNames[mtConfig.general.trackMuteTiming]);
    display.setControlText2(label[6], timingText);
    display.refreshControl(label[6]);
    display.setControlData(grid, &gridData);
    display.refreshControl(grid);

    snprintf(title, sizeof title, "Track Mute");
    titleBar->setTitle(title);
    snprintf(rightLabel, sizeof rightLabel, muted ? "%d muted" : "none muted", muted);
    titleBar->setRightLabel(rightLabel);
    titleBar->showRightLabel(true);
}
