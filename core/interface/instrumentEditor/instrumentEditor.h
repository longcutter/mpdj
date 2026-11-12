#ifndef INTERFACE_INSTRUMENTEDITOR_H_
#define INTERFACE_INSTRUMENTEDITOR_H_

#include "core/modulesBase.h"

#include "mtStructs.h"

#include "mtSequencer.h"

#if GUI_HANDLE_SYNTH
#include "Synth.h"
#endif
#include <map>

enum mtInstrumentEditorMode
{
    mtInstEditModeParams,
    mtInstEditModeEnv,
    mtInstEditModeInstrSynth,
    mtInstEditModeMidi,
#if GUI_HANDLE_SYNTH
    mtInstEditModeSynth,
    mtInstEditModeSynthMacros,
    mtInstEditModeSynthPatchEdit,
#endif
    mtInstEditModeCount,
};

const uint8_t filterModeCount = 4;
const uint8_t ccNumbersCount = 131;

#undef MAX_SELECT_NODES
#define MAX_SELECT_NODES 8

class cInstrumentEditor : public ModuleBase
{
  public:
    enum SampleInstrumentParamsCC
    {
        kTune = 5,
        kFinetune = 3,
        kVolume = 7,
        kPanning = 10,
        kFiltertype = 9,
        kCutoff = 1,
        kResonance = 11,
        kOverdrive = 12,
        kBitDepth = 13,
        kReverbSend = 14,
        kDelaySend = 15,
        kVolumeAutomationType = 20,
        kVolumeAttack_ShapeLFO = 21,
        kVolumeDecay_SpeedLFO = 22,
        kVolumeSustain = 23,
        kVolumeRelease = 24,
        kVolumeAmount = 25,
        kPanningAutomationType = 26,
        kPanningAttack_ShapeLFO = 27,
        kPanningDecay_SpeedLFO = 28,
        kPanningSustain = 29,
        kPanningRelease = 30,
        kPanningAmount = 31,
        kCutoffAutomationType = 83,
        kCutoffAttack_ShapeLFO = 84,
        kCutoffDecay_SpeedLFO = 85,
        kCutoffSustain = 86,
        kCutoffRelease = 87,
        kCutoffAmount = 88,
        kWavetablePositionAutomationType = 102,
        kWavetablePositionAttack_ShapeLFO = 103,
        kWavetablePositionDecay_SpeedLFO = 104,
        kWavetablePositionSustain = 105,
        kWavetablePositionRelease = 106,
        kWavetablePositionAmount = 107,
        kGranularPositionAutomationType = 108,
        kGranularPositionAttack_ShapeLFO = 109,
        kGranularPositionDecay_SpeedLFO = 110,
        kGranularPositionSustain = 111,
        kGranularPositionRelease = 112,
        kGranularPositionAmount = 113,
        kFinetuneAutomationType = 114,
        kFinetuneAttack_ShapeLFO = 115,
        kFinetuneDecay_SpeedLFO = 116,
        kFinetuneSustain = 117,
        kFinetuneRelease = 118,
        kFinetuneAmount = 119
    };

    enum SynthInstrumentParamsCC
    {
        kMacro1 = 110,
        kMacro2 = 111,
        kMacro3 = 112,
        kMacro4 = 113,
        kMacro5 = 114,
        kMacro6 = 115,
        kSynthDelaySend = 116,
        kSynthReverbSend = 117,
        kSynthPanning = 118,
        kSynthGain = 119
    };

    std::map<SynthInstrumentParamsCC, int> macroCcIdToIdx = {
        { SynthInstrumentParamsCC::kMacro1, 0 }, { SynthInstrumentParamsCC::kMacro2, 1 },
        { SynthInstrumentParamsCC::kMacro3, 2 }, { SynthInstrumentParamsCC::kMacro4, 3 },
        { SynthInstrumentParamsCC::kMacro5, 4 }, { SynthInstrumentParamsCC::kMacro6, 5 }
    };

    enum ValueChangeSource
    {
        kArrowButton = 0,
        kEncoder,
    };

    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}
    virtual void hideControlsUnderMiniMenu();
    virtual uint8_t getCurrentModeIdx();

    cInstrumentEditor() {}
    ~cInstrumentEditor() {}
    Synth* getSynth() { return synth; };

    void showDefaultScreen();
    void showActualInstrument();
    void showTitleBar();

    void refreshFramePosition();
    void activateLabelsBorder();

    void setBaseScreenFunct();
    void setDefaultScreenFunct();
    void clearDefaultScreenFunct();

    void lightUpPadBoard();

    void setDefaultValue(uint8_t place);

    void changeParamValue(int value, ValueChangeSource source = kArrowButton);

    void cancelPopups();
    void hideControlUnderMenu();

    //----------------------------------
    strLabelData labelArrow;
    strLabelData labelImage;
    strLabelData labelDataMore;

    hControl label[8];
    hControl doubleLabelAutomation;
    hControl barControl[8];
    hControl blankSpaceCover[2];
    hControl noteLabel;

    hControl list1Control;
    hControl list2Control;
    hControl list3Control;
    hControl list4Control;

    strList list1Data;
    strList list2Data;
    strList list3Data;
    strList list4Data;

    hControl modelListControl;
    strNewListData modelListData;

    hControl imgGridControl;
    strImgGridData imgGridData;

    hControl paramsListControl;
    strNewListData paramsListData;

    hControl bgLabel;
    hControl frameControl;
    strFrameData frameData;

    uint8_t selectedPlace[mtInstEditModeCount] = { 0, 0, 1 };
    uint32_t localActiveBarColors[5] = { 0 };
    uint32_t localInactiveBarColors[5] = { 0 };

    mtInstrumentEditorMode mode = mtInstEditModeParams;

    strInstrument* editorInstrument;

    bool rollListOver;

    char labelText[8][12];

    //----------------------------------
    // MIDI CC processing
    void onControlChangeReceive(int param, int value);
    int16_t getControlChangeDelta(SampleInstrumentParamsCC param, int cc_value);
    void changeControlChangeParam(SampleInstrumentParamsCC param, int16_t delta);
    int getEnvelopeIdx(SampleInstrumentParamsCC param);
#ifdef FEATURE_SYNTH
    void handleSynthParamControlChange(int synthPartIdx,
                                       SynthInstrumentParamsCC paramCcId,
                                       int value);
    float getSynthCCParamValueMax(int synthPartIdx, SynthInstrumentParamsCC paramCcId);
    float getSynthCCParamValueMin(int synthPartIdx, SynthInstrumentParamsCC paramCcId);
    bool isParamShown(int synthPartIdx, SynthInstrumentParamsCC paramCcId);
    std::optional<mtInstrumentEditorMode> paramToMode(SynthInstrumentParamsCC paramCcId);
    bool isSynthPercSectionBegin(int note);
    int convertPadForEditPatchScreen(int pad);
#endif

    //----------------------------------
    // multi value edit
    select_node_t selectNodes[MAX_SELECT_NODES];

    void addNode(editFunct_t funct, uint8_t nodeNum, uint8_t reverseInput = 0);
    void removeNode(uint8_t nodeNum);
    void stepThroughNodes(int16_t value, bool invert);
    void clearAllNodes();
    void cancelMultiFrame();
    bool isAnyNodeActive();

    //----------------------------------
    // instrument params
    hControl filterModeListControl;
    strList filterModeList;
    uint8_t filterModeListPos;

    void showInstrumentParams();

    void showParamsVolume();
    void showParamsPanning();
    void showParamsTune();
    void showParamsFineTune();
    void showFilterType();
    void showFilterCutOff();
    void showFilterResonance();
    void showParamsDelaySend();
    void showParamsReverbSend();
    void showParamsOverdrive();
    void showParamsBitDepth();

    static void changeParamsVolume(int16_t value);
    static void changeParamsPanning(int16_t value);
    static void changeParamsTune(int16_t value);
    static void changeParamsFineTune(int16_t value);
    static void changeFilterType(int16_t value);
    static void changeFilterCutoff(int16_t value);
    static void changeFilterResonance(int16_t value);
    static void changeParamsDelaySend(int16_t value);
    static void changeParamsReverbSend(int16_t value);
    static void changeParamsOverdrive(int16_t value);
    static void changeParamsBitDepth(int16_t value);

    void setDefaultVolume();
    void setDefaultPanning();
    void setDefaultTune();
    void setDefaultFinetune();
    void setDefaultFilterType();
    void setDefaultCutoff();
    void setDefaultResonance();
    void setDefaultDelaySend();
    void setDefaultReverbSend();
    void setDefaultOverdrive();
    void setDefaultBitDepth();

    //----------------------------------
    // envelopes lists
    uint8_t selectedEnvelope = 0;

    void showInstrumentEnv();

    void showEnvList();
    void showEnvState();
    void showEnvAttack();
    void showEnvDecay();
    void showEnvSustain();
    void showEnvRelease();
    void showEnvAmount();
    void showLfoShape();
    void showLfoSpeed();
    void showLfoAmount();
    void showWtEnvWave();
    void showWtWarpXY();
    void showWtWarpX();
    void showWtWarpY();
    void showWtSustain();

    static void changeSelectedEnv(int16_t value);
    static void changeSelectedEnvState(int16_t value);
    static void changeSelectedEnvAttack(int16_t value);
    static void changeSelectedEnvDecay(int16_t value);
    static void changeSelectedEnvSustain(int16_t value);
    static void changeSelectedEnvRelease(int16_t value);
    static void changeSelectedEnvAmount(int16_t value);
    static void changeSelectedLfoShape(int16_t value);
    static void changeSelectedLfoSpeed(int16_t value);
    static void changeSelectedLfoAmount(int16_t value);
    static void changeSelectedWtWave(int16_t value);
    static void changeSelectedWtWarpXY(int16_t value);
    static void changeSelectedWtWarpX(int16_t value);
    static void changeSelectedWtWarpY(int16_t value);
    static void changeSelectedWtSustain(int16_t value);

    void changeEnvState(int idx, int16_t value);
    void changeEnvAttack(int idx, int16_t value);
    void changeEnvDecay(int idx, int16_t value);
    void changeEnvSustain(int idx, int16_t value);
    void changeEnvRelease(int idx, int16_t value);
    void changeEnvAmount(int idx, int16_t value);
    void changeLfoShape(int idx, int16_t value);
    void changeLfoSpeed(int idx, int16_t value);
    void changeLfoAmount(int idx, int16_t value);
    void changeWtWave(int idx, int16_t value);
    void changeWtWarpXY(int idx, int16_t value);
    void changeWtWarpX(int idx, int16_t value);
    void changeWtWarpY(int idx, int16_t value);
    void changeWtSustain(int idx, int16_t value);

    void setDefaultEnvAttack();
    void setDefaultEnvDecay();
    void setDefaultEnvSustain();
    void setDefaultEnvRelease();
    void setDefaultEnvAmount();
    void setDefaultLfoShape();
    void setDefaultLfoSpeed();
    void setDefaultLfoAmount();
    void setDefaultWtWave();
    void setDefaultWtWarp();
    void setDefaultWtSustain();

    //----------------------------------
    // instrument synthesizer
    hControl instrumentSynthCCListControl;
    hControl instrumentListControl;

    strList instSynthCCData;
    strList instData;

    strLabelData synthCCArrows;
    strLabelData instArrows;

    uint8_t instSynthCCListPos = 0;
    uint8_t instListPos = 0;
    bool openLoaderPending = false; // MPDJ: PROGRAM EDIT on an empty slot opens the Program Loader

    void showSynthCCList();
    void showInstList();
    void changeSynthCCList(int16_t value);
    void changeInstList(int16_t value);

    void showInstrumentSynth();
    void setInstrumentSynthScreenFunct();

    //----------------------------------
    // sends screen
    void switchToEffectScreen();
    void setEffectScreenFunct();
    void showEffectScreen();
    void showSendScreenFrame();

    bool isEffectsWindow;
    uint8_t selectedPlaceSends;

    float calcInstrumentVolumeDecybelValue(uint8_t val);
    float calcInstrumentSendsDecybelValue(uint8_t val);

    //----------------------------------
    // instr MIDI
    hControl ccNameListControl[6];
    strList ccNameListData[6];

    uint8_t getActualMidiInstrumentCCValue(int ccIndex);

    void showInstrumentMidiParams();

    void showParamsVelocity();
    void showParamsOffset();
    void showParamsCC(uint8_t index);

    void setDefaultMidiVelocity();
    void setDefaultMidiOffset();
    void setDefaultMidiCC(uint8_t index);
    void setPlayButtonAction();
#if GUI_HANDLE_SYNTH
    //----------------------------------
    // synth
    static constexpr int kSynthPreviewNote1 = 60 - 2 * 12;
    static constexpr int kSynthPreviewNote2 = 60;

    char synthPatchNameText[45];
    char* patchfileNamesBuffer;
    char** patchListDataName;
    int16_t patchListPosition;
    std::string previousPatchName = {};

    void changeSynthParamValue(int value, ValueChangeSource source = kArrowButton);

    void changeSynthModel(int16_t value);
    void changeSynthPolyphony(int16_t value);
    void changeSynthGain(int16_t value);
    void changeSynthPanning(int16_t value);
    void changeSynthReverbSend(int16_t value);
    void changeSynthDelaySend(int16_t value);
    void changeSynthPatchListPos(int16_t value);

    void setDefaultSynthModel();
    void setDefaultSynthPolyphony();
    void setDefaultSynthGain();
    void setDefaultSynthPanning();
    void setDefaultSynthReverb();
    void setDefaultSynthDelay();

    void setSynthPanning(int synthPartIdx, float value, bool show = true);
    void setSynthGain(int synthPartIdx, float value, bool show = true);

    bool browsePatchList();
    int loadPatchList();
    void revertPatch();
    void initCurrentPatch();

    void showSynthParamsScreen();
    void setSynthParamsScreenFunct();
    void setSynthPatchInitPopupFunct();

    void showSynthModel(uint8_t synthPart);
    void showSynthPolyphony(uint8_t synthPart);
    void showSynthGain(uint8_t synthPart);
    void showSynthPanning(uint8_t synthPart);
    void showSynthPatchName(uint8_t synthPart);

    void showSynthPatchBrowser();
    void showSynthPatchList();
    void showSynthPatchInitPopup();

    void exitSynthEditor();

    void playSynthNoteByPad(uint8_t pad, uint8_t state, uint8_t customLightPad = 255);
    void synthCommonPreviewButtonFunct(uint8_t state);

    // macros

    char* macrosNameBufor = nullptr;

    void showSynthMacrosScreen();
    void setSynthMacrosScreenFunct();

    void showSynthMacrosNames();
    void cleanMacrosResources();

    void changeSynthMacroParamValue(int value);
    void changeSynthMacro(int macroIdx, int16_t value);

    void setSynthMacro(int synthPartIdx, int macroIdx, int value, bool show = true);
    int getMacroMax(int synthPartIdx, int macroIdx);
    bool isMacroBipolar(int synthPartIdx, int macroIdx);

    void setDefaultSynthMacroValue(int macroIdx);

    void showSynthReverbSend(uint8_t syntPartIdx);
    void showSynthDelaySend(uint8_t syntPartIdx);
    void showSynthMacroValue(uint8_t syntPartIdx, uint8_t macroIdx);

    //----------------------------------
    // patch editor

    static constexpr int kGridRowsCount = 4;
    static constexpr int kGridColumnsCount = 4;

    static constexpr int kMaxSynthSectionStringsCount = kGridRowsCount * kGridColumnsCount;
    static constexpr int kMaxSynthParamStringsCount = 30;

    static constexpr int kMaxSynthListStringLen = 24;
    static constexpr int kMaxSynthGridStringLen = 8;
    static constexpr int kMaxMacroParamMods = 5;

    enum ParamValueMode
    {
        kNone = 0,
        kValue,
        kList,
    };

    int8_t sectionImgList[kGridRowsCount * kGridColumnsCount] = { -1 };
    int8_t sectionListPos = 0;
    uint8_t sectionListLen;
    char* sectionNamesGridBuff = nullptr;
    char* sectionNamesBufor = nullptr;
    char** sectionNamesList = nullptr;
    int* sectionIds = nullptr;

    uint8_t firstMacroIdx = 0;
    int paramListPos = 0;
    char* paramListTextBuff = nullptr;
    char** paramListNames = nullptr;
    char** paramListValues = nullptr;
    int* sectionParamsIds = nullptr;

    ParamValueMode paramValueMode;
    int paramValueListPos = 0;
    char* paramValueListTextBuff = nullptr;
    char** paramValueListNames = nullptr;

    void cleanPatchEditorResources();
    bool isMacroSelected() { return sectionListPos >= firstMacroIdx; }

    void setSynthPatchEditorScreenFunct();
    void setSynthPatchSaveAsKeyboardFunctions();
    void setSynthMacroNameKeyboardFunctions();

    void showSynthPatchEditorScreen();
    void showSaveAsKeyboard();

    void changeSynthPatchEditorValue(int value, ValueChangeSource source = kArrowButton);

    void changeSynthPatchSection(int16_t value);
    void changeSynthPatchParam(int16_t value);
    void changeSynthPatchParamValue(int16_t value);
    void changeSynthMacroName();

    void setSynthReverbSend(int synthPartIdx, float value, bool show = true);
    void setSynthDelaySend(int synthPartIdx, float value, bool show = true);
    void setSynthPatchParamDefault();

    void showSynthPatchSection(bool refreshList = false);
    void showSynthPatchSectionParam(bool refreshList = false);
    void showSynthPatchMacroParam(bool refreshList = false);
    void showSynthPatchSectionParamValue(bool refreshList = false);
    void showSynthPatchMacroParamValue(bool refreshList = false);

    void gridChangeSelection(int direction);
    void setSection(int sectionIdx);

#endif
};

extern cInstrumentEditor instrumentEditor;

#endif /* INTERFACE_INSTRUMENTEDITOR_H_ */
