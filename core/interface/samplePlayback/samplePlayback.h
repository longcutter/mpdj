#ifndef INTERFACE_SAMPLEPLAYBACK_H_
#define INTERFACE_SAMPLEPLAYBACK_H_

#include "core/modulesBase.h"

#include "mtSequencer.h"

#include "mtStructs.h"

#undef MAX_SELECT_NODES
#define MAX_SELECT_NODES 4

const char playModeFunctLabels[playModeCount][15] = {
    "1-Shot", "Forward Loop", "Backward Loop", "Pingpong Loop",
    "Slice",  "Beat Slice",   "Wavetable",     "Granular",

};

constexpr uint8_t MAX_WAVETABLE_WINDOWS_COUNTER = 11;
constexpr uint8_t MIN_WAVETABLE_WINDOWS_COUNTER = 5;

class cSamplePlayback : public ModuleBase
{

  public:
    enum ParamsCC
    {
        kWavetablePosition_GranularPosition = 16,
        kWavetableWindow_GranularPositionLength = 17,
        kGranularShape = 18,
        kGranularLoop = 19
    };

    static constexpr int kNoTrack = -1;

    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}

    cSamplePlayback()
    {
        playModeListControl = -1;
        frameControl = -1;
        wtPositionCursor = -1;
        bgLabel = -1;

        editorInstrument = nullptr;

        progressCursor = -1;
        granularCursor = -1;
        spectrumControl[0] = -1;
        spectrumControl[1] = -1;
        pointsControl = -1;
        slicePointsControl = -1;
        label[8] = { -1 };
        // popupLabel = -1;
        lastSampleLength = 0;
    }
    virtual ~cSamplePlayback() {}

    void showDefaultScreen();
    void showZoomValue();
    void showPlayModeList();
    void showTitleBar();
    void hideUselessControls();
    void resizeWaveforms();

    void setDefaultScreenFunct();
    void cancelPopups();

    void processPoints();
    void processSlicePoints();
    void showPreviewValue();
    void hidePreviewValue();

    char startPointValueText[9];
    char endPointValueText[9];
    char loopPoint1ValueText[9];
    char loopPoint2ValueText[9];
    char playTimeValueText[9];
    char equalSliceNumberText[8];
    void showStartPointValue();
    void showEndPointValue();
    void showLoopPoint1Value();
    void showLoopPoint2Value();
    void showSlicesSelectValue();
    void showSlicesAdjustValue();
    void showSlicesEqualNumberValue();
    void showSlicesPlayThruValue();

    void onEndTrackDisplay();

    void noteOnHandle(uint8_t channel, uint8_t note, uint8_t velocity, int16_t source);
    void noteOffHandle(uint8_t channel, uint8_t note, uint8_t velocity, int16_t source);

    void lightUpPadBoard();

    void hideLoopPoints();
    void showLoopPoints();

    void showWavetablePositionCursor();
    void hideWavetablePositionCursor();

    void selectCorrectPoints();

    void refreshLiveParams();
    void refreshGranularAutomation();
    void refreshWavetableAutomation();
    void refreshPreviewTimeLabel();

    void calcPlayProgressValue();
    void activateLabelsBorder();

    void showAutoSlicePopup();
    void setAutoSlicePopupFunct();

    void modEqualSliceNumber(int16_t val);
    void modSlicePlayThru(int16_t val);
    bool slice2ndScreenActive();

    void setDefault(uint8_t pos);
    void setDefaultStartPoint();
    void setDefaultLoopPoint1();
    void setDefaultLoopPoint2();
    void setDefaultEndPoint();
    void setDefaultZoom();
    void setDefaultSelectedSlice();
    void setDefaultEqualSliceNumber();
    void setDefaultWavetablePosition();
    void setDefaultWavetableWindow();
    void setDefaultGranularPosition();
    void setDefaultGranularLength();
    void setDefaultGranularShape();
    void setDefaultGranularLoop();

    void onControlChangeReceive(int param, int value);
    int getControlChangeDelta(ParamsCC param, int cc_value);
    void changeControlChangeParam(ParamsCC param, int delta);

    strFrameData frameData;

    strLabelData labelArrow;
    strLabelData labelDataMore;

    hControl label[8];
    hControl playModeListControl;
    hControl spectrumControl[2];
    hControl pointsControl;
    hControl slicePointsControl;
    hControl frameControl;
    hControl progressCursor;
    hControl granularCursor;
    hControl wtPositionCursor;
    hControl bgLabel;

    uint8_t selectedPlace = 6;
    // MPDJ: while set, start / end / loop point moves step one point unit (1/65536
    // of the sample) instead of one screen pixel (MPC Q-Links, slow turn = fine)
    bool mpdjFineStep = false;
#ifdef PLATFORM_MPC
    // MPDJ (5g): MPC-style slicing on Slice / Beat Slice programs (code at the end of
    // fSamplePlay.cpp). One page of labels: Slice, Start, End, Method, Amount, Apply,
    // Play Thru, Play Mode; selectedPlace 0-5 = labels 1-6, 6 = the play mode list,
    // 7 = Play Thru. Slices are linked (mpdjSlices.h)
    enum SliceMethod : uint8_t { kSliceManual, kSliceAuto, kSliceEvenly, kSliceBeats, kSliceChop, kSliceMethods };
    // Chop (2026-11-16): each slice written out as its own WAV (Samples/Chops/<name>/) and
    // loaded into the next empty program slot, one after another (mpdjChopPoll)
    struct ChopRun
    {
        bool active = false;
        bool reloading = false; // all saved: the samples are being reloaded into memory
        int8_t source = -1;
        uint8_t count = 0, done = 0;
        uint8_t slots[MAX_SLICE_NUMBER] = {};
        char dir[96] = {}, base[28] = {};
    } mpdjChop;
    int mpdjChopSlots(uint8_t* out, int want) const; // the empty slots the chops would fill
    uint8_t mpdjChopFrom = 0;          // they're looked for from this slot up (To slots; session only)
    void mpdjChopMoveFrom(int dir);    // To slots turned: start at the next / previous empty slot
    void mpdjChopStart();
    bool mpdjChopNext();
    void mpdjChopPoll();
    uint8_t mpdjSliceMethod = kSliceManual; // screen state, not saved
    uint8_t mpdjAutoSens = 1;               // Auto: 0 Low, 1 Mid (the Tracker's), 2 High
    uint8_t mpdjBeatDiv = 2;                // Beats: index into 1 bar, 1/2, 1/4, 1/8, 1/16
    bool mpdjApplyPending = false;          // Evenly / Beats / Clear waiting for the overwrite popup
    bool mpdjPendingClear = false;          // the popup is for Clear (Manual's Apply, SHIFT+ERASE)
    bool mpdjPopupOpen = false;             // the overwrite popup is up: knobs, pads and touch wait
    int8_t mpdjPadVoice = -1;               // track / voice the pad audition plays on
    void mpdjSliceRemove(int slice);        // ERASE + pad
    void mpdjAskClear();                    // SHIFT+ERASE
    int mpdjPadLight(int slice) const;      // 0 dark, 1 slice, 2 selected, 3 playing, 4 next (Manual)
    // Undo (5g-5): UNDO / SHIFT+UNDO on the slice page, 16 steps, session only. A step
    // is the program's slices, End and Play Thru before a burst of changes (changes
    // closer than 0.7 s together are one step), found by comparing with a shadow copy
    struct SliceState
    {
        int8_t program = -1;
        uint8_t count = 0, selected = 0, playThru = 0;
        uint16_t end = 0;
        uint16_t slices[MAX_SLICE_NUMBER] = {};
        bool same(const SliceState& o) const;
    };
    static constexpr int kSliceUndo = 16;
    SliceState mpdjShadow, mpdjUndo[kSliceUndo], mpdjRedo[kSliceUndo];
    int mpdjUndoCount = 0, mpdjRedoCount = 0;
    uint32_t mpdjLastChangeMs = 0;
    void mpdjSliceTake(SliceState& s) const;
    void mpdjSlicePut(const SliceState& s);
    void mpdjSlicePoll();
    const char* mpdjSliceUndo(bool redo); // popup text
    // Loop Sync (5g-6, mpdjLoopSync.h): label 6 in the loop play modes (the main
    // encoder zooms on the MPC, so the Zoom label isn't needed)
    bool mpdjLoopUi() const;
    void mpdjLoopSyncTurn(int16_t value);
    void mpdjLoopSyncLabel();
    char mpdjPopupText[32] = {};
    void mpdjApply();
    void mpdjDoApply();
    char mpdjSliceText[8][16] = {};
    int mpdjPadSlice = -1;                  // slice the held pad plays (release ends it)
    bool mpdjSliceUi() const;
    void mpdjShowSliceLabels();
    void mpdjSliceValues();
    void mpdjSliceTurn(int knob, int32_t eighths); // Q-Link point knob, 1/8 screen pixels
    void mpdjSliceTouchKnob(int knob);
    void mpdjSliceZoom(int16_t value);
    void mpdjSlicePad(int slice, bool down, uint8_t velocity = 127);
    void mpdjSliceTapWave(int x);
    void mpdjSliceChanged(bool all);
    void mpdjSliceSelect(int slice);
    bool mpdjSliceButton(uint8_t n, uint8_t state);
    bool mpdjSliceEncoder(int16_t value);
    uint16_t mpdjSliceKnobPoint(int knob) const;
#endif

    //----------------------------------
    // spectrum + points

    enum enOverwriteSliceSource
    {
        overvwriteAutoSlice,
        overwriteEqualSlice
    };

    uint8_t refreshSpectrum = 0;
    uint8_t refreshPoints = 0;
    uint8_t refreshSlicePoints = 0;
    uint8_t currentAutoSlice = 0;
    uint8_t lastAutoSlice = 0;
    uint8_t overwriteSliceSource;

    uint8_t refreshSpectrumProgress = 0;
    uint32_t playProgressValue = 0;      // 0 - MAX_LEN_RECORD
    uint16_t playProgressInSpectrum = 0; // 0 - 600
    uint8_t loopDirection = 0;
    elapsedMicros refreshPlayProgressValue = 0;
    float playPitch = 1.0;
    bool sliceScreenMode2;
    int8_t displayedTrack = -1;

    strInstrument* editorInstrument = nullptr;
    uint32_t lastSampleLength = 0xFFFFFFFF;

    strZoomParams zoom;

    char zoomTextValue[7];

    char sliceSelectTextValue[11];
    char sliceAdjustTextValue[9];

    static strTrackerSpectrum spectrum[2];
    strTrackerPoints points;
    strSlicePoints slicePoints;

    wt_position_data_t wtPosition;

    strLineIndicatorSelection lineIndicatorSelection;

    //----------------------------------
    // lista play mode
    strList playModeList;

    void listPlayMode();

    char* playModeNames[playModeCount];

    //----------------------------------
    // odtwarzanie
    uint8_t currentSeqState = 0;
    uint8_t lastSeqState = 0;
    uint8_t isPlayingSample = 0;
    uint8_t isPlayingWavetable = 0;
    uint32_t currentEnvelopeWtPos;
    uint32_t lastEnvelopeWtPos;
    uint32_t currentEnvelopeGranPos;
    uint32_t lastEnvelopeGranPos;
    uint32_t currentEnvelopeGranLen = 0;
    uint32_t lastEnvelopeGranLen = 0;
    int8_t playNote = 24;
    uint8_t glidePreviewDif = 0;
    uint8_t seqReleaseFlag = 0;
    uint8_t startNoteStoppedSeq = 0;

    //----------------------------------
    // multisel
    select_node_t selectNodes[MAX_SELECT_NODES];

    void addNode(editFunct_t funct, uint8_t nodeNum);
    void removeNode(uint8_t nodeNum);
    void stepThroughNodes(int16_t value);
    void clearAllNodes();
    void cancelMultiFrame();
    //--------------------------------------
    uint8_t loadedInstrumentType = 0;
    char wavetablePositionText[7];
    char wavetableWindowSizeText[7];
    void showWavetablePosition();
    void showWavetableWindowSize();
    uint16_t convertWavetableWindowsCounterToSize(uint8_t cnt);
    uint8_t convertWavetableWindowsSizeToCounter(uint16_t size);
    uint8_t wavetableWindowsCounter = 11;
    uint8_t refreshWavetablePosition;

    void processWavetableCursor(uint32_t position);

    char granularPositionTextValue[15];
    char grainLengthTextValue[15];
    char shapeText[15];
    char loopTypeText[15];

    uint16_t granularPositionInSpectrum;
    float grainLengthMs;

    void showGranularPositionValue();
    void showGrainLengthValue();
    void showShapeText();
    void showLoopTypeText();

    float samplePosToTime(uint32_t pos);

    void formatTimeSeconds(char* destBuffer, size_t destBufferSize, float value);
};

extern cSamplePlayback samplePlayback;

#endif /* INTERFACE_SAMPLEPLAYBACK_H_ */
