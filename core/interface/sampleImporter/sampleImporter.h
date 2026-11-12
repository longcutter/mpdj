#ifndef INTERFACE_SAMPLEIMPORTER_H_
#define INTERFACE_SAMPLEIMPORTER_H_

#include "core/modulesBase.h"
#include "mtKeyboardManager.h"
#include "mtStructs.h"

//#define PREVIOUS_POSITION_LIFO	25
#define MAX_SELECT kSampleInstrumentsCount // MPDJ: 64 slots

enum
{
    listFiles,
    listInstruments,
};

class Encoder;

class cSampleImporter : public ModuleBase
{

  public:
    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}

    cSampleImporter() {}
    virtual ~cSampleImporter() {}

    void showDefaultScreen();
    void showActualInstrument();
    void showFilesTree();
    void showInstrumentsList();
    void showPreviewVolume();
    void refreshInstrumentsList();
    void showMemoryUsage();
    void activateLabelsBorder();
    void resizeWaveforms();

    void showStopPatternPopup();
    void hideStopPatternPopup();
    void setStopPatternFunction();
    void clearStopPatternFunction();
    void hidePopupButtons();

    void setDeleteInstrumentHandlerFunctions();
    void setDeleteUnusedInstrumentsHandlerFunctions();
    void setReplaceInstrumentHandlerFunctions();
    void clearInfoPopupHandlerFunctions();

    void showInfoPopupDeleteInstrument();
    void showInfoPopupReplaceInstrument();
    void hideInfoPopup();

    enum struct enStopPatternAction
    {
        previewSD,
        previewBank,
        instrCopy,
        instrAdd,
        instrAddNext,
        instrDelete
    } stopPatternAction;

    strList explorerList;
    strList instrumentList;

    strFrameData frameData;

    strLabelData labelArrow[2];

    static strTrackerSpectrum spectrum[2];
    strTrackerPoints points;
    std::vector<int8_t*> lowerData;
    std::vector<int8_t*> upperData;

    hControl label[8];

    hControl explorerListControl;
    hControl instrumentListControl;

    hControl memoryBarControl;
    hControl previewVolumeBarControl;
    hControl frameControl;

    hControl bgLabel;

    hControl explorerLabelControl;
    hControl instrumentLabelControl;

    hControl instrumentLabel;

    hControl spectrumControl[2];
    hControl progressCursor;
    hControl pointsControl;

    uint8_t selectedPlace = 0;
    uint8_t selectedPlaceBeforePreviewVolume = 0;
    bool blockReturningToPreviousPlace = false;
    bool wasPreviewVolumeHeld = false;

    void setDefaultScreenFunct();

    uint8_t changeFileSelection(int16_t value);
    uint8_t changeInstrumentSelection(int16_t value);
    void changePreviewVolume(int16_t value);
    void setDefaultPreviewVolume();

    // void setInstrumentsListPos(uint8_t pos);

    void showAddOrEnter();
    void showRename();

    void addNextColorControl();

    // copy paste
    void copyInstruments();
    void pasteInstruments();

    uint8_t instrCopyStart;

    uint8_t commonPreviewButton; // = interfaceButton2; // ustawiane w setDefaultScreenFunct()

    //--------------------------------------------------------------
    void refreshActionButtons();

    void BrowseOrAdd();
    void importSamples();

    // pliki wybranym folderze
    uint16_t locationExplorerCount = 0;
    char** explorerNames;
    uint8_t selectedFile = 0;
    char copyingInfo[20];
    uint32_t* currentFolderMemoryFileUsage;

    //--------------------------------------------------------------

    // instrumenty
    void listInstrumentSlots();
    uint8_t selectedSlot = 0;

    //--------------------------------------------------------------
    // MPDJ (step 5a-3): the Program Loader. The file list shows the file
    // manager's list (fmNames / fmCount) plus a virtual "/MIDI" folder at the
    // end of the card's root; inside it, "/.." and MIDI channels 1-16. Every
    // action on those entries is handled here (the file manager never sees them).
    char** fmNames = nullptr;
    uint8_t fmCount = 0;
    bool midiBrowse = false;
    int midiFolderIdx = -1; // index of "/MIDI" in the root list, or -1
    void rebuildExplorerView();
    bool isVirtualEntry(int idx) const; // "/MIDI" or anything inside it
    void enterMidiFolder();
    void leaveMidiFolder();
    void addMidiPrograms(bool addNext);
    void applyMidiPrograms();
    // a MIDI add waiting for the replace confirm / for samples to be deleted
    bool midiAddPending = false;
    bool midiAddWaitsForDelete = false;
    uint8_t midiAddFirstSlot = 0;
    uint8_t midiAddFirstChannel = 1;
    uint8_t midiAddCount = 0;
    bool midiAddNext = false;

    uint8_t instrUseMap[kSampleInstrumentsCount + 1];
    bool stepDelete;
    void initDeleteUnusedInstruments();
    bool stepDeleteUnusedInstruments();

    //--------------------------------------------------------------
    // memory

    void calculateLoadProgress();
    void calculateCopyingProgress();
    uint8_t memoryUsage;
    strBarData memoryUsageAdd;
    const strBarData memoryUsageAddOnMemoryFull = { 100 };
    int32_t currentSelectMemorySize;
    uint8_t fullMemoryFlag = 0;
    uint8_t loadProgress = 0;
    uint8_t copyingProgress = 0;
    uint8_t lastCopyStatusFlag = 0;
    uint8_t currentCopyStatusFlag = 0;
    uint8_t copyElement = 0;
    uint8_t sampleType = 0;
    uint8_t copyElementsCount;
    uint8_t firstMemBarLoadFlag = 0;

    uint8_t lastLoadStatusFlag = 0;
    uint8_t currentLoadStatusFlag = 0;

    //--------------------------------------------------------------
    // odsluch
    void playSelectedSdFile(uint16_t from, uint16_t to);
    void playSampleFromBank(uint8_t pad, uint8_t state, int16_t velo);
    void stopPlaying(uint8_t pad, uint8_t state, int16_t velo);
    void stopPlayingAll();
    void playFromPads(uint8_t pad, uint8_t state, int16_t velo);

    uint8_t playMode;

    enum
    {
        playModeStop,
        playModeSdFile,
        playModeSampleBank,

    };

    //----------------------------------
    void showFileList();
    void rewindListToBeggining();
    void moveInstrListToEnd();

    static const uint8_t maxListLength = 100;
    uint8_t selectionActive[2] = { 0, 0 };
    uint32_t explorerHeaderColors[4];
    uint32_t instrumentHeaderColors[4];
    uint8_t shiftPressed;
    static uint8_t selectionTab[2][maxListLength];
    uint8_t selectionLength[2];
    //	uint8_t currSelectPlace=0;

    //----------------------------------
    // impport
    uint8_t refreshSpectrum = 0;
    uint8_t refreshSpectrumProgress = 0;
    uint8_t refreshPoints = 0;
    uint8_t refreshProgressCalc = 0;
    bool importWithDelay = 0;

    uint16_t importStartPoint;
    uint16_t importEndPoint = MAX_16BIT;

    uint16_t importPreviewStartPoint;
    uint16_t importPreviewEndPoint = MAX_16BIT;

    uint32_t importSampleSamplesCount = 0;
    uint32_t importMemoryFree = 0;
    uint8_t importIsLoFi = 0;
    uint8_t importForceStream = 0;

    enum class enSampleLoadMode : uint8_t
    {
        kRam = 0,
        kAuto = 1,
        kStream = 2,
    };
    enSampleLoadMode sampleLoadMode = enSampleLoadMode::kRam;

    void updateMemoryButtonLabel();
    void handleMemoryModeOrToggle();
    bool calcImportIsEnoughMemoryStream();

    uint16_t playProgressValue = 0;
    uint16_t playProgressInSpectrum = 0; // 0 - 600

    elapsedMillis playProgresValueTim;
    elapsedMillis refreshPlayProgressValue;

    void refreshGuiAfterDelay();
    elapsedMillis guiRefreshDelay;
    bool delayRefreshGUI = false;

    uint8_t playInProgressFlag = 0;
    uint8_t importApplyNormalButtonLastStyle = 255;
    uint8_t importApplyLoFiButtonLastStyle = 255;
    uint8_t importApplyStreamButtonLastStyle = 255;

    strZoomParams zoom;
    strSpectrumParams params;

    uint8_t selectedImportPlace;

    char startPointValueText[8];
    char endPointValueText[8];
    char playTimeValueText[8];
    char previewVolumeText[12];

    void calcPlayProgressValue();
    void startPreviewRefresh();
    void stopPreviewRefresh();

    void processPoints();

    void showImportScreen();
    void hideImportScreen();

    void setImportFuncts();

    void showImportStartPointValue();
    void showImportEndPointValue();
    void showImportPreviewValue();
    void hideImportPreview();
    void showImportApplyButtonStyle();

    void modImportStartPoint(int16_t value);
    void modImportEndPoint(int16_t value);

    void importPartOfSample();
    void importPartOfSampleWithDelay();
    void calcImportFreeMem();
    bool calcImportIsEnoughMemoryNormal();
    bool calcImportIsEnoughMemoryLoFi();

    //----------------------------------
    void updateSelection();

    uint8_t handleSelecting(uint8_t selectStart,
                            uint8_t selectMax,
                            int16_t value,
                            uint8_t fileCheck);

    void cancelSelect(uint8_t placeToCancel);
    int16_t getSelectionStart(uint8_t whichSelect);
    int16_t getSelectionEnd(uint8_t whichSelect);

    bool checkIfValidSelection(uint8_t positionToCheck);

    void handleMemoryBar();
    void calculateFileMemUsage();
    void calculateMemUsage();
    char memoryUsageValue[8];

    void setSelect();
    void frameSelectMode(uint8_t place, uint8_t mode);

    void displayDelete(uint8_t onOff);

    char instrToCreate[15];
    char* parseNewName(uint8_t num);
    uint8_t willCopyInstrFit(uint8_t length);
    uint8_t getActiveInstrInSelection();
    void resetInstrSel();

    void handleSequenceCopyingLoading();

    void showRenameKeyboard();

    uint8_t folderIsChanged = 1;

    void previewColorControl();
    void renameColorControl();
    void deleteColorControl();
    void importColorControl();
    void deleteUnusedColorControl();
    bool checkIfAnyInstrActive();

    uint8_t addNextFlag = 0;
    uint8_t addNextOffset = 0;

    enum infoPupupFirstLineText
    {
        oneInstrumentInfoPupupFirstLineText,
        moreThanOneInstrumentInfoPupupFirstLineText,
        infoPupupFirstLineTextCount
    };

    const char* firstLineDeletePopupOptions[infoPupupFirstLineTextCount] = {
        (char*)"This action will delete the instrument.",
        (char*)"This action will delete the instruments."
    };

    const char* firstLineReplacePopupOptions[infoPupupFirstLineTextCount] = {
        (char*)"This action will replace the instrument.",
        (char*)"This action will replace the instruments."
    };

    const char* firstLineDeleteUnusedPopupOptions =
      "This action will delete all unused instruments.";

    const char* firstLineDeleteInfoPopup =
      firstLineDeletePopupOptions[oneInstrumentInfoPupupFirstLineText];
    const char* firstLineReplaceInfoPopup =
      firstLineDeletePopupOptions[oneInstrumentInfoPupupFirstLineText];

    uint8_t replaceSource = 0;

    bool addNextOperationWantReplaceInstrument;
};

extern cSampleImporter sampleImporter;

#endif /* INTERFACE_SAMPLEIMPORTER_H_ */
