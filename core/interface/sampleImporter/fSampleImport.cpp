#include "fileManager.h"
#include "fileManagerDefs.h"
#include "sampleImporter/sampleImporter.h"

#include "core/interfaceMenuPopup.h"
#include "core/interfacePopups.h"

#include "core/graphicProcessing.h"
#include "mtAudioEngine.h"
#include "projectEditor/projectEditor.h"

#include "Platform.h"
#include "mtConfig.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"
#include "util_string.h"

#include <cr_section_macros.h>

__NOINIT(EXTERNAL_RAM) strTrackerSpectrum cSampleImporter::spectrum[2];
__NOINIT(EXTERNAL_RAM) uint8_t cSampleImporter::selectionTab[2][cSampleImporter::maxListLength];

cSampleImporter sampleImporter;
static cSampleImporter* SI = &sampleImporter;

static void functMiniMiddle(uint8_t state);

static void functPads(uint8_t pad, uint8_t state, int16_t velo);

static void functMenu(uint8_t state);

static void functPlayAction();

static void functEnter();
static void functRename();
static void functDeleteUnused();
static void functShift(uint8_t state);

static void functInstrumentAdd();
static void functInstrumentAddNext();
// static  uint8_t functInstrumentWavetableAdd();
static void functInstrumentDelete();

static void functInstrumentImport();

static void functImportAndReplaceInstrumentConfirm();

static void functImportCancel();
static void functImportLoFiApply();
static void functImportApply();
static void functImportStreamApply();
static void functMemoryModeOrToggle();
static void functImportPreview(uint8_t state);
static void functImportStart();
static void functImportEnd();
static void functImportEncoder(int16_t value);

static void functDelete(uint8_t state);

static void functLeft();
static void functRight();
static void functUp();
static void functDown();

static void functCopyPaste();

static void functConfirmKey();

static void functPreviewFile(uint8_t state);

static void functEncoder(int16_t value);

static void functSwitchModule(uint8_t button);

static void functConfirmRename();
static void functCancelRename();
static void functAutoNameRename();

static void functDeleteBackspace(uint8_t state);

static void functStopPatternYes();
static void functStopPatternNo();

static void functDeleteInstrumentConfirm();
static void functDeleteInstrumentDeny();
static void functDeleteUnusedInstrumentsConfirm();

static void functReplaceInstrumentConfirm();
static void functReplaceInstrumentDeny();
static void functSelectPreviewVolume(uint8_t state);

void cSampleImporter::update()
{
    uint8_t managerStatus = newFileManager.getStatus();

    if (managerStatus == fmBrowseSamplesEnd)
    {
        fmCount = newFileManager.getBrowsedFilesList(&fmNames, &currentFolderMemoryFileUsage);
        midiBrowse = false;
        rebuildExplorerView(); // MPDJ: + the virtual /MIDI folder at the root

        showFilesTree();
        handleMemoryBar();

        cancelSelect(listFiles);
        setSelect();

        refreshActionButtons();

        ip->unblockAllInputs();

        newFileManager.clearStatus();
    }
    else if (managerStatus == fmDrawingSampleWaveformEnd)
    {
        refreshSpectrum = 1;
        refreshPoints = 1;

        ip->unblockAllInputs();

        importSampleSamplesCount = newFileManager.getImportingSamplesCount();

        // start ekranu importu
        calcImportFreeMem();
        calcPlayProgressValue();
        showImportScreen();
        setImportFuncts();
        functImportStart();

        newFileManager.clearStatus();
    }
    else if (managerStatus == fmImportSamplesEnd)
    {
        if (addNextFlag)
        {
            addNextFlag = 0;
            selectedSlot = (selectedSlot + addNextOffset >= kSampleInstrumentsCount)
                             ? kSampleInstrumentsCount - 1
                             : selectedSlot + addNextOffset;
        }

        cancelSelect(listInstruments);

        listInstrumentSlots();
        refreshInstrumentsList();

        handleMemoryBar();

        showDefaultScreen();

        refreshActionButtons();

        ip->unblockAllInputs();

        newFileManager.clearStatus();
    }
    else if (managerStatus == fmCopyingInstrumentsEnd)
    {
        listInstrumentSlots();
        refreshInstrumentsList();

        selectionActive[listInstruments] = 0;

        refreshActionButtons();

        handleMemoryBar();
        ip->unblockAllInputs();

        newFileManager.clearStatus();
    }
    else if (managerStatus == fmDeleteInstrumentsEnd)
    {
        newFileManager.clearStatus();

        if (midiAddWaitsForDelete) // MPDJ: the samples are gone, now place the MIDI programs
        {
            midiAddWaitsForDelete = false;
            applyMidiPrograms();
            ip->unblockAllInputs();
            return;
        }

        if (!stepDelete || !stepDeleteUnusedInstruments())
        {
            listInstrumentSlots();
            refreshInstrumentsList();
            handleMemoryBar();

            refreshActionButtons();

            ip->unblockAllInputs();

            stepDelete = false;
        }
    }
    else if (managerStatus >= fmError)
    {
        ip->unblockAllInputs();

        newFileManager.clearStatus();
    }

    if (refreshProgressCalc && refreshPlayProgressValue > 10)
    {
        refreshPlayProgressValue = 0;

        calcPlayProgressValue();
        showImportPreviewValue();
        showImportStartPointValue();
        showImportEndPointValue();

        if (playProgressValue <= importPreviewEndPoint &&
            playProgressValue >= importPreviewStartPoint)
            refreshSpectrumProgress = 1;
    }
    if (refreshSpectrum)
    {
        display.refreshControl(spectrumControl[0]);
        display.refreshControl(spectrumControl[1]);
        refreshSpectrum = 0;
    }
    if (refreshSpectrumProgress)
    {
        display.setControlShow(progressCursor);
        display.setControlValue(progressCursor, playProgressInSpectrum);
        display.refreshControl(progressCursor);

        refreshSpectrumProgress = 0;
    }
    if (refreshPoints)
    {
        processPoints();
        display.refreshControl(SI->pointsControl);
        refreshPoints = 0;
    }

    if (importWithDelay &&
        refreshPlayProgressValue > 100) // opoznienie przetwarzania importu eliminujace laga
    {
        importWithDelay = false;
        importPartOfSample();
    }

    if (delayRefreshGUI && guiRefreshDelay > 300)
    {
        delayRefreshGUI = false;
        handleMemoryBar();
        refreshActionButtons();
        display.synchronizeRefresh();
    }
}

void cSampleImporter::start(uint32_t options)
{
    moduleRefresh = 1;

    if (mtProject.values.lastUsedInstrument >= kSampleInstrumentsCount) // MPDJ: any slot type
    {
        // mtProject.values.lastUsedInstrument = 0;
        selectedSlot = 0;
    }
    else
    {
        selectedSlot = mtProject.values.lastUsedInstrument;
    }

    // focus na wybranym insturmencie
    if (options & 0xff)
    {
        selectedFile = 0; // slabe takie ustawianie tu ale przy imporcie zaznaczenia zabezpiecza
                          // przed zlym zaznaczeniem powodujacym zawiech
        selectedPlace = 1;
        selectedSlot = (options & 0xff) - 1;

        if (selectedSlot >= kSampleInstrumentsCount)
            selectedSlot = 0;

        mtProject.values.lastUsedInstrument = selectedSlot;
    }

    // przeladuj pierwszy raz xxx trzeba to popawic
    if (locationExplorerCount == 0 || interfaceGlobals.refreshFileExplorer)
    {
        interfaceGlobals.refreshFileExplorer = false;

        if (newFileManager.browseSdCard(nullptr))
        {
            SI->ip->blockAllInputs();
        }
    }
    else
    {
        showFilesTree();
        handleMemoryBar();

        cancelSelect(listFiles);
        setSelect();

        // refreshActionButtons();
    }

    listInstrumentSlots();
    showInstrumentsList();

    // ustawienie funkcji
    ip->setButtonObj(interfaceButtonParams, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonPerformance, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonFile, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonConfig, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonMaster, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSamplePlay, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleEdit, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleRec, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSampleLoad, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonSong, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonPattern, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonGame, buttonPress, functSwitchModule);

    ip->setButtonObj(interfaceButtonMiniMenu, functMenu);

    showDefaultScreen();
    setDefaultScreenFunct();

    cancelSelect(listInstruments);

    updateSelection();
    resetInstrSel();
    setSelect();
}

void cSampleImporter::stop()
{

    mtPopups.hideInfoPopup();

    stopPlayingAll();

    moduleRefresh = 0;
}

void cSampleImporter::setDefaultScreenFunct()
{
    // funkcje
    ip->setPotObj(interfacePot0, functEncoder);

    ip->clearButtonsRange(interfaceButton0, interfaceButton7);

    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayAction);

    ip->setButtonObj(interfaceButtonLeft, buttonPress, functLeft);
    ip->setButtonObj(interfaceButtonRight, buttonPress, functRight);
    ip->setButtonObj(interfaceButtonUp, buttonPress, functUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functDown);

    ip->setButtonObj(interfaceButtonDelete, functDelete);

    ip->setButtonObj(interfaceButtonShift, functShift);

    ip->setButtonObj(interfaceButtonInsert, buttonPress, functEnter);

    ip->setButtonObj(interfaceButtonCopy, buttonPress, functCopyPaste);
    //	ip->setButtonObj(interfaceButtonPaste, buttonPress, functPaste);

    // ip->setButtonObj(interfaceButton0, buttonPress, functChangeFolder);
    // ip->setButtonObj(interfaceButton1, buttonPress, functChangeFolder);

    ip->setButtonObj(interfaceButton0, buttonPress, functEnter);
    ip->setButtonObj(interfaceButton1, buttonPress, functInstrumentAddNext); // 3
    commonPreviewButton = interfaceButton2;
    ip->setButtonObj(commonPreviewButton, functPreviewFile);      // 4
    ip->setButtonObj(interfaceButton3, buttonPress, functRename); // 2
    ip->setButtonObj(interfaceButton4, buttonPress, functInstrumentImport);
    ip->setButtonObj(interfaceButton5, buttonPress, functDeleteUnused);
    ip->setButtonObj(interfaceButton6, functSelectPreviewVolume);
    ip->setButtonObj(interfaceButton7, buttonPress, functMemoryModeOrToggle);

    // ip->setButtonObj(interfaceButton4, buttonPress, functChangeInstrument);
    // ip->setButtonObj(interfaceButton5, buttonPress, functInstrumentDelete);

    // ip->setButtonObj(interfaceButton6, buttonPress, functChangeInstrument);
    // ip->setButtonObj(interfaceButton7, buttonPress, functChangeInstrument);

    ip->setButtonObj(interfaceButtonMiniMiddle, functMiniMiddle);

    ip->setPadsGlobal(functPads);
}

//==============================================================================================================

// MPDJ: a sample loaded onto a MIDI program replaces it (after the replace
// confirm): the slot goes back to empty first, so it takes samples again.
static int countMidiPrograms(int first, int last)
{
    int n = 0;
    for (int i = first; i <= last && i < kSampleInstrumentsCount; i++)
        if (isMidiInstrument(i))
            n++;
    return n;
}

static void clearMidiPrograms(int first, int last)
{
    bool changed = false;
    for (int i = first; i <= last && i < kSampleInstrumentsCount; i++)
    {
        if (isMidiInstrument(i))
        {
            mtProject.programs.slot[i] = {};
            changed = true;
        }
    }
    if (changed)
        newFileManager.setProjectStructChanged();
}

// the slots a sample Add / Add next fills: the selected slot + one per selected file
static void sampleAddRange(int& first, int& last)
{
    first = SI->getSelectionStart(listInstruments);
    last = first + (SI->getSelectionEnd(listFiles) - SI->getSelectionStart(listFiles));
}

static void instrumentAdd(bool addNext)
{
    if (SI->explorerNames == nullptr)
    {
        return;
    }
    if (SI->midiBrowse) // MPDJ: in the /MIDI folder, Add makes MIDI programs
    {
        SI->addMidiPrograms(addNext);
        return;
    }
    if (*SI->explorerNames[SI->selectedFile] == '/')
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    uint8_t replaceStart = SI->getSelectionStart(listInstruments);
    uint8_t replaceEnd =
      replaceStart + (SI->getSelectionEnd(listFiles) - SI->getSelectionStart(listFiles));

    if (replaceEnd >= kSampleInstrumentsCount)
    {
        mtPopups.show(0, "Too many samples selected");
        return;
    }
    uint8_t instrToReplaceCount = countMidiPrograms(replaceStart, replaceEnd); // MPDJ
    for (uint8_t i = replaceStart; i <= replaceEnd; i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            instrToReplaceCount++;
        }
    }

    if (instrToReplaceCount > 0)
    {
        SI->replaceSource = 0;
        if (instrToReplaceCount > 1)
            SI->firstLineReplaceInfoPopup =
              SI->firstLineReplacePopupOptions
                [cSampleImporter::moreThanOneInstrumentInfoPupupFirstLineText];
        else
            SI->firstLineReplaceInfoPopup =
              SI->firstLineReplacePopupOptions
                [cSampleImporter::oneInstrumentInfoPupupFirstLineText];
        SI->setReplaceInstrumentHandlerFunctions();
        SI->showInfoPopupReplaceInstrument();
        SI->addNextOperationWantReplaceInstrument = addNext;
    }
    else
    {
        if (SI->explorerNames != nullptr && *SI->explorerNames[SI->selectedFile] != '/')
        {
            SI->sampleType = mtSampleTypeWaveFile;
            SI->addNextFlag = addNext;
            SI->importSamples();
        }
    }
}

static void functInstrumentAdd()
{
    instrumentAdd(false);
}

static void functInstrumentAddNext()
{
    instrumentAdd(true);
}

static void functDelete(uint8_t state)
{
    if (SI->keyboardManager.getState())
    {
        functDeleteBackspace(state);
    }
    else if (SI->selectedPlace == 2 && state == buttonPress)
    {
        SI->setDefaultPreviewVolume();
    }
    else
    {
        if (state == buttonPress)
        {
            functInstrumentDelete();
        }
    }
}

static void functInstrumentDelete()
{
    if (SI->selectedPlace != 1)
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    uint8_t deleteStart = SI->getSelectionStart(listInstruments);
    uint8_t deleteEnd = SI->getSelectionEnd(listInstruments);

    if (deleteStart >= kSampleInstrumentsCount || deleteEnd >= kSampleInstrumentsCount ||
        deleteEnd < deleteStart)
    {
        return;
    }

    uint8_t instrToDeleteCount = countMidiPrograms(deleteStart, deleteEnd); // MPDJ
    for (uint8_t i = deleteStart; i <= deleteEnd; i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            instrToDeleteCount++;
        }
    }

    if (instrToDeleteCount > 0)
    {
        if (instrToDeleteCount > 1)
            SI->firstLineDeleteInfoPopup =
              SI->firstLineDeletePopupOptions
                [cSampleImporter::moreThanOneInstrumentInfoPupupFirstLineText];
        else
            SI->firstLineDeleteInfoPopup =
              SI->firstLineDeletePopupOptions[cSampleImporter::oneInstrumentInfoPupupFirstLineText];
        SI->setDeleteInstrumentHandlerFunctions();
        SI->showInfoPopupDeleteInstrument();
    }
}

//==============================================================================================================
//=======================================           IMPORT
//==========================================
//==============================================================================================================
void cSampleImporter::processPoints()
{
    points.pointsType = 0;

    // if(importStartPoint >= zoom.zoomStart && importStartPoint <= zoom.zoomEnd)
    //{
    points.startPoint = (importStartPoint * 600) / MAX_16BIT;
    //}
    // else points.startPoint = -1;

    // if(importEndPoint >= zoom.zoomStart && importEndPoint <= zoom.zoomEnd)
    //{
    points.endPoint = (importEndPoint * 600) / MAX_16BIT;
    //}
    // else points.endPoint = -1;
}

static void functInstrumentImport()
{
    if (SI->explorerNames == nullptr)
    {
        return;
    }
    if (SI->midiBrowse) // MPDJ: nothing to import from /MIDI; Import = Add there
    {
        SI->addMidiPrograms(false);
        return;
    }
    if (*SI->explorerNames[SI->selectedFile] == '/')
    {
        return;
    }
    if (SI->getSelectionEnd(listFiles) - SI->getSelectionStart(listFiles) > 0)
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    char* fileName = SI->explorerNames[SI->selectedFile];
    if (hasFileExtension(fileName, "pti"))
    {
        return;
    }

    uint8_t instrToReplace = SI->selectedSlot;

    if (instrToReplace < kSampleInstrumentsCount &&
        (mtProject.instrument[instrToReplace].isActive || isMidiInstrument(instrToReplace))) // MPDJ
    {
        SI->replaceSource = 1;
        SI->firstLineReplaceInfoPopup =
          SI->firstLineReplacePopupOptions[cSampleImporter::oneInstrumentInfoPupupFirstLineText];
        SI->setReplaceInstrumentHandlerFunctions();
        SI->showInfoPopupReplaceInstrument();

        return;
    }

    functImportAndReplaceInstrumentConfirm();
}

static void functImportAndReplaceInstrumentConfirm()
{
    SI->clearInfoPopupHandlerFunctions();
    mtPopups.hideInfoPopup();

    clearMidiPrograms(SI->selectedSlot, SI->selectedSlot); // MPDJ: the sample replaces it

    SI->lowerData.clear();
    SI->upperData.clear();

    SI->lowerData.push_back(SI->spectrum[0].lowerData);
    SI->upperData.push_back(SI->spectrum[0].upperData);

#ifdef TRACKER_MINI
    SI->lowerData.push_back(SI->spectrum[1].lowerData);
    SI->upperData.push_back(SI->spectrum[1].upperData);
#endif

    bool result = newFileManager.drawSampleWaveform(
      SI->getSelectionStart(listFiles), SI->lowerData, SI->upperData);

    if (result)
    {
        SI->ip->blockAllInputs();
    }
}

void cSampleImporter::setImportFuncts()
{
    ip->setButtonObj(interfaceButton0, functImportPreview);
    ip->setButtonObj(interfaceButton1, buttonPress, functImportStart);
    ip->setButtonObj(interfaceButton2, buttonPress, functImportEnd);

    ip->setButtonObj(interfaceButton4, buttonPress, functImportStreamApply);
    ip->setButtonObj(interfaceButton5, buttonPress, functImportCancel);
    ip->setButtonObj(interfaceButton6, buttonPress, functImportLoFiApply);
    ip->setButtonObj(interfaceButton7, buttonPress, functImportApply);

    ip->setButtonObj(interfaceButtonMiniMiddle, functImportPreview);

    ip->clearButton(interfaceButton3);

    // ip->clearButton(interfaceButtonPlay);
    ip->clearButton(interfaceButtonRec);
    ip->clearButton(interfaceButtonInsert);
    ip->clearButton(interfaceButtonCopy);
    ip->clearButton(interfaceButtonShift);
    ip->clearButton(interfaceButtonDelete);

    ip->clearButton(interfaceButtonUp);
    ip->clearButton(interfaceButtonLeft);
    ip->clearButton(interfaceButtonDown);
    ip->clearButton(interfaceButtonRight);

    ip->setPotObj(interfacePot0, functImportEncoder);
}

static void functImportCancel()
{
    SI->stopPreviewRefresh();

    SI->showFileList();

    SI->listInstrumentSlots();
    SI->showInstrumentsList();

    SI->handleMemoryBar();

    SI->activateLabelsBorder();

    SI->showDefaultScreen();
    SI->setDefaultScreenFunct();
}

static void functImportLoFiApply()
{
    if (!SI->calcImportIsEnoughMemoryLoFi())
    {
        return;
    }

    functImportCancel();

    SI->ip->blockAllInputs();

    SI->importPartOfSampleWithDelay();

    SI->importIsLoFi = 1;
    SI->importForceStream = 0;
}

static void functImportApply()
{
    if (!SI->calcImportIsEnoughMemoryNormal())
    {
        return;
    }

    functImportCancel();

    SI->ip->blockAllInputs();

    SI->importPartOfSampleWithDelay();

    SI->importIsLoFi = 0;
    SI->importForceStream = 0;
}

static void functImportStreamApply()
{
    if (!SI->calcImportIsEnoughMemoryStream())
    {
        return;
    }

    functImportCancel();

    SI->ip->blockAllInputs();

    SI->importPartOfSampleWithDelay();

    SI->importIsLoFi = 0;
    SI->importForceStream = 1;
}

void cSampleImporter::importPartOfSample()
{
    uint8_t selStart = getSelectionStart(listFiles);

    FileManagerOperation::SampleLoadMode loadMode =
      importForceStream ? FileManagerOperation::SampleLoadMode::kStream
                        : static_cast<FileManagerOperation::SampleLoadMode>(sampleLoadMode);

    bool result = newFileManager.importSamplesToProject(
      selStart, selStart, selectedSlot, importStartPoint, importEndPoint, importIsLoFi == 1, loadMode);

    if (result)
    {
        SI->ip->blockAllInputs();
    }
    else
    {
        SI->ip->unblockAllInputs();
    }
}

// opoznienie przetwarzania importu eliminujace laga
void cSampleImporter::importPartOfSampleWithDelay()
{
    // wykozytanie istniejacego obiektu elapsedMillis
    refreshPlayProgressValue = 0;
    importWithDelay = true;
}

static void functImportPreview(uint8_t state)
{
    static uint8_t lastState = buttonRelease;

    if (state == lastState)
    {
        return;
    }

    lastState = state;
    // Hold is set to button function with Press state . We have to handle it by rejecting
    // repetitive states if we don't want hold.

    if (state == 0)
    {
        SI->stopPreviewRefresh();
        SI->stopPlaying(0, 0, 0);
    }
    else if (state == 1)
    {
        SI->startPreviewRefresh();
        SI->playSelectedSdFile(SI->importStartPoint, SI->importEndPoint);
    }
}

static void functImportStart()
{
    SI->selectedImportPlace = 1;
    SI->points.selected = 1;
    SI->refreshPoints = 1;
}

static void functImportEnd()
{
    SI->selectedImportPlace = 2;
    SI->points.selected = 8;
    SI->refreshPoints = 1;
}

static void functImportEncoder(int16_t value)
{
    switch (SI->selectedImportPlace)
    {
        case 0: break;
        case 1: SI->modImportStartPoint(value); break;
        case 2: SI->modImportEndPoint(value); break;
    }
}

void cSampleImporter::modImportStartPoint(int16_t value)
{
    // obliczenie kroku przesuniecia w zaleznosci od ilosci widzianych probek na wyswietlaczu
    uint16_t move_step = MAX_16BIT / 600;
    value = value * move_step;

    if (SI->importStartPoint + value < SAMPLE_POINT_POS_MIN)
        SI->importStartPoint = 0;
    else if (SI->importStartPoint + value > SAMPLE_POINT_POS_MAX)
        SI->importStartPoint = SAMPLE_POINT_POS_MAX;
    else
        SI->importStartPoint += value;

    if (SI->importStartPoint >= SI->importEndPoint)
    {
        SI->importStartPoint = SI->importEndPoint - 2;
    }

    SI->refreshPoints = 1;
    showImportApplyButtonStyle();
    SI->showImportStartPointValue();
}

void cSampleImporter::modImportEndPoint(int16_t value)
{
    uint16_t move_step = MAX_16BIT / 600;
    value = value * move_step;

    if (SI->importEndPoint + value < SAMPLE_POINT_POS_MIN)
        SI->importEndPoint = 0;
    else if (SI->importEndPoint + value > SAMPLE_POINT_POS_MAX)
        SI->importEndPoint = SAMPLE_POINT_POS_MAX;
    else
        SI->importEndPoint += value;

    if (SI->importEndPoint <= SI->importStartPoint)
    {
        SI->importEndPoint = SI->importStartPoint + 2;
    }

    SI->refreshPoints = 1;
    showImportApplyButtonStyle();
    SI->showImportEndPointValue();
}

void cSampleImporter::calcPlayProgressValue()
{
    float recTimeValue = importSampleSamplesCount / 44100.0;
    float actualPreviewTimeValue = playProgresValueTim / 1000.0;
    float startPointTimeValue = (recTimeValue * importPreviewStartPoint) / MAX_16BIT;

    if (actualPreviewTimeValue > recTimeValue)
        actualPreviewTimeValue = 0;

    playProgressValue = ((startPointTimeValue + actualPreviewTimeValue) / recTimeValue) * MAX_16BIT;

    playProgressInSpectrum = (600 * playProgressValue) / MAX_16BIT;
}

void cSampleImporter::startPreviewRefresh()
{
    importPreviewStartPoint = importStartPoint;
    importPreviewEndPoint = importEndPoint;

    refreshProgressCalc = 1;
    playProgresValueTim = 0;
    display.setClearRefreshFlag(progressCursor);
}

void cSampleImporter::stopPreviewRefresh()
{
    refreshProgressCalc = 0;
    hideImportPreview();
}

void cSampleImporter::calcImportFreeMem()
{
    importMemoryFree = 0;

    if (mtProject.instrument[selectedSlot].isActive)
    {
        importMemoryFree = newFileManager.getSampleMemorySize(selectedSlot, selectedSlot);
    }

    if (mtProject.max_memory > mtProject.used_memory)
    {
        importMemoryFree += mtProject.max_memory - mtProject.used_memory;
    }
}

bool cSampleImporter::calcImportIsEnoughMemoryNormal()
{
    float sampleStartPoint = ((float)importSampleSamplesCount / MAX_16BIT) * importStartPoint;
    float sampleEndPoint = ((float)importSampleSamplesCount / MAX_16BIT) * importEndPoint;

    return ((sampleEndPoint - sampleStartPoint) * 2) <
           (importMemoryFree - 10000); // 10000B mniej dla bezpieczenstwa
}

bool cSampleImporter::calcImportIsEnoughMemoryLoFi()
{
    float sampleStartPoint = ((float)importSampleSamplesCount / MAX_16BIT) * importStartPoint;
    float sampleEndPoint = ((float)importSampleSamplesCount / MAX_16BIT) * importEndPoint;

    return (sampleEndPoint - sampleStartPoint) <
           (importMemoryFree - 10000); // 10000B mniej dla bezpieczenstwa
}

bool cSampleImporter::calcImportIsEnoughMemoryStream()
{
    float sampleStartPoint = ((float)importSampleSamplesCount / MAX_16BIT) * importStartPoint;
    float sampleEndPoint = ((float)importSampleSamplesCount / MAX_16BIT) * importEndPoint;
    float frames = sampleEndPoint - sampleStartPoint;
    if (frames <= (float)kStreamPreCacheFrames)
    {
        return false;
    }
    uint32_t preCacheBytes =
      kStreamPreCacheFrames *
      ((newFileManager.getSampleMode() == FileManagerOperation::kLeftAndRight) ? 4 : 2);
    return (preCacheBytes + 10000) < importMemoryFree;
}
//==============================================================================================================
//==============================================================================================================
//==============================================================================================================

static void functEncoder(int16_t value)
{
    if (SI->keyboardManager.getState())
    {
#ifdef TRACKER_MINI
        SI->keyboardManager.confirmKey();
#endif
        return;
    }

    switch (SI->selectedPlace)
    {
        case 0: SI->changeFileSelection(interfaceGlobals.invertValForList ? -value : value); break;
        case 1:
            SI->changeInstrumentSelection(interfaceGlobals.invertValForList ? -value : value);
            break;
        case 2: SI->changePreviewVolume(value); break;
        default: break;
    }
}

static void functEnter()
{
    switch (SI->selectedPlace)
    {
        case 0: SI->BrowseOrAdd(); break;
        case 1: SI->BrowseOrAdd(); break;
    }
}

static void functRename()
{
    if (SI->selectedPlace != 1)
    {
        return;
    }
    if (mtProject.instrument[SI->selectedSlot].isActive != 1)
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    SI->keyboardManager.fillName(mtProject.instrument[SI->selectedSlot].sample.file_name);
    SI->keyboardManager.activateKeyboard(SAMPLE_NAME_SIZE);

    SI->ip->clearButtonsRange(interfaceButton0, interfaceButton7);

    SI->ip->setButtonObj(interfaceButtonInsert, buttonPress, functConfirmKey);
    SI->ip->setButtonObj(interfaceButton0, buttonPress, functConfirmKey);
    SI->ip->setButtonObj(interfaceButton1, buttonPress, functAutoNameRename);
    SI->ip->setButtonObj(interfaceButton6, buttonPress, functCancelRename);
    SI->ip->setButtonObj(interfaceButton7, buttonPress, functConfirmRename);

    SI->showRenameKeyboard();
}

static void functConfirmRename()
{
    char* const localName = SI->keyboardManager.getName();
    uint8_t length = strlen(localName);
    if (length >= 1)
    {
        uint8_t allow = 0;

        for (uint8_t i = 0; i < length; i++)
        {
            // sprawdza czy zawiera jakikolwiek logiczny znak(nie spacje)
            if (localName[i] != ' ')
            {
                allow = 1;
                break;
            }
        }

        if (allow)
        {
            strncpy(
              mtProject.instrument[SI->selectedSlot].sample.file_name, localName, SAMPLE_NAME_SIZE);

            SI->showFileList();

            SI->listInstrumentSlots();
            SI->showInstrumentsList();

            SI->handleMemoryBar();

            SI->activateLabelsBorder();

            SI->setDefaultScreenFunct();
            SI->showDefaultScreen();

            // SI->displayDelete(SI->selectedPlace);

            //	fileManager.setInstrumentChangeFlag(mtProject.values.lastUsedInstrument);
            newFileManager.setInstrumentStructChanged(mtProject.values.lastUsedInstrument);

            SI->keyboardManager.deactivateKeyboard();
        }
    }
}

static void functCancelRename()
{
    SI->showFileList();

    SI->listInstrumentSlots();
    SI->showInstrumentsList();

    SI->handleMemoryBar();

    SI->activateLabelsBorder();
    SI->keyboardManager.deactivateKeyboard();

    SI->setDefaultScreenFunct();
    SI->showDefaultScreen();

    // SI->displayDelete(SI->selectedPlace);
}

static void functDeleteUnused()
{
    if (SI->selectedPlace != 1)
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    SI->firstLineDeleteInfoPopup = SI->firstLineDeleteUnusedPopupOptions;
    SI->setDeleteUnusedInstrumentsHandlerFunctions();
    SI->showInfoPopupDeleteInstrument();
}

static void functAutoNameRename()
{
    SI->keyboardManager.setRandomName();
}

static void functCopyPaste()
{
    if (SI->keyboardManager.getState())
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    if (SI->buttons->isButtonPressed(interfaceButtonShift))
    {
        SI->pasteInstruments();
    }
    else
    {
        SI->copyInstruments();
    }
}

void cSampleImporter::copyInstruments()
{
    if (SI->selectedPlace == 1)
    {
        SI->instrCopyStart = SI->getSelectionStart(listInstruments);
        SI->copyElementsCount =
          SI->selectionLength[listInstruments] > 0 ? SI->selectionLength[listInstruments] : 1;
    }
}

void cSampleImporter::pasteInstruments()
{
    if (!copyElementsCount || instrCopyStart == selectedSlot)
    {
        return;
    }

    uint8_t willFit = 0;
    uint8_t selectOverMax = 0;

    uint8_t instrActiveInSel = SI->getActiveInstrInSelection();

    SI->copyElement = 0;

    if (!instrActiveInSel)
        return;

    while (SI->selectedSlot + SI->copyElementsCount > kSampleInstrumentsCount)
    {
        SI->copyElementsCount--;
        selectOverMax++;
    }

    for (int i = (SI->copyElementsCount - selectOverMax); i > 0; i--)
    {
        willFit = SI->willCopyInstrFit(i);

        if (willFit)
        {
            break;
        }
        else
        {
            SI->copyElementsCount--;
            instrActiveInSel--;
            // TODO: popout ze kopiowanie zostalo uciete
        }
    }

    if (willFit)
    {
        // MPDJ: the copy carries slot types. A MIDI source makes an empty target a
        // MIDI program; a sample / empty source turns a MIDI target back to empty
        // (a MIDI source over a sample target leaves the sample)
        for (int k = 0; k < copyElementsCount; k++)
        {
            int src = instrCopyStart + k, dst = selectedSlot + k;
            if (src >= kSampleInstrumentsCount || dst >= kSampleInstrumentsCount)
                break;
            if (isMidiInstrument(src))
            {
                if (!mtProject.instrument[dst].isActive)
                    mtProject.programs.slot[dst] = mtProject.programs.slot[src];
            }
            else if (isMidiInstrument(dst))
            {
                mtProject.programs.slot[dst] = {};
            }
        }
        newFileManager.setProjectStructChanged();
        listInstrumentSlots();
        refreshInstrumentsList();

        if (newFileManager.copyInstrumentsInWorkspace(
              instrCopyStart, copyElementsCount, selectedSlot))
        {
            SI->ip->blockAllInputs();
        }
    }
}

static void functShift(uint8_t state)
{
    if (SI->keyboardManager.getState() == 1)
    {
        if (state == 1)
        {
            functConfirmKey();
        }
        return;
    }

    // select
    if (state == buttonPress)
    {
        // cancelAnotherSelect();

        SI->selectionActive[SI->selectedPlace] = 1;

        //		if(SI->currSelectPlace != SI->selectedPlace)
        //		{
        //			if(SI->selectedPlace == 1)
        //			{
        //				SI->cancelSelect(listFiles);
        //			}
        //			else
        //			{
        //				SI->cancelSelect(listInstruments);
        //			}
        //
        //			SI->currSelectPlace = SI->selectedPlace;
        //			SI->selectionLength[SI->selectedPlace] = 1;
        //		}

        //		if(SI->selectedPlace == 0)
        //		{
        //			if(SI->explorerNames != nullptr && *SI->explorerNames[SI->selectedFile] != '/')
        //			{
        //				SI->selectionTab[SI->selectedPlace][SI->selectedFile] = 1;
        //			}
        //		}
        //		else
        //		{
        //			SI->selectionActive[SI->selectedPlace] = 1;
        //			SI->selectionTab[SI->selectedPlace][SI->selectedSlot] = 1;
        //		}

        //		SI->updateSelection();
    }
    else if (state == buttonRelease)
    {
        SI->selectionActive[SI->selectedPlace] = 0;
    }
}

static void functLeft()
{
    if (SI->keyboardManager.getState())
    {
        SI->keyboardManager.makeMove('a');
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    if (SI->selectedPlace == 1)
    {
        SI->selectedPlace = 0;
        SI->handleMemoryBar();
        SI->resetInstrSel();

        // clear instrument list selection
        memset(&SI->selectionTab[listInstruments][0], 0, SI->maxListLength);
        SI->selectionLength[listInstruments] = 0;
    }
    else if (SI->selectedPlace == 0)
    {
        SI->rewindListToBeggining();
        SI->cancelSelect(listFiles);
        SI->handleMemoryBar();
    }
    else if (SI->selectedPlace == 2)
    {
        SI->selectedPlace = 1;
    }

    SI->refreshActionButtons();
    SI->activateLabelsBorder();
}

static void functRight()
{
    if (SI->keyboardManager.getState())
    {
        SI->keyboardManager.makeMove('d');
        return;
    }

    if (SI->selectedPlace == 2)
    {
        return;
    }

    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

    if (SI->selectedPlace == 0)
    {
        SI->selectedPlace = 1;
        //        memset(&SI->selectionTab[listFiles][0], 0, SI->maxListLength);
        //        SI->selectionLength[listFiles] = 0;
    }
    else if (SI->selectedPlace == 1)
    {
        SI->selectedPlace = 2;
        SI->selectedPlaceBeforePreviewVolume = 1;
    }

    SI->refreshActionButtons();
    SI->activateLabelsBorder();
}

static void functUp()
{
    if (SI->keyboardManager.getState())
    {
        SI->keyboardManager.makeMove('w');
        return;
    }

    switch (SI->selectedPlace)
    {
        case 0: SI->changeFileSelection(-1); break;
        case 1: SI->changeInstrumentSelection(-1); break;
        case 2: SI->changePreviewVolume(1); break;
        default: SI->stopPlayingAll();
    }
}

static void functDown()
{
    if (SI->keyboardManager.getState())
    {
        SI->keyboardManager.makeMove('s');
        return;
    }

    switch (SI->selectedPlace)
    {
        case 0: SI->changeFileSelection(1); break;
        case 1: SI->changeInstrumentSelection(1); break;
        case 2: SI->changePreviewVolume(-1); break;
        default: SI->stopPlayingAll();
    }
}

static void functPlayAction()
{
    if (sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP)
    {
        if (SI->buttons->isButtonPressed(interfaceButtonShift))
        {
            sequencer.playSong();
        }
        else
        {
            sequencer.playPattern();
        }
    }
    else
    {
        sequencer.stop();
    }
}

static void functSwitchModule(uint8_t button)
{
    SI->eventFunct(eventSwitchModule, SI, &button, 0);
}

//======================================================================================================================
/*uint8_t cSampleImporter::changeFolderSelection(int16_t value)
{
    if(selectedFolder+value < 0) selectedFolder = 0;
    else if(selectedFolder+value > locationFolderCount-1) selectedFolder = locationFolderCount-1;
    else  selectedFolder += value;

    AddEnterOrRename();

    display.setControlValue(folderListControl, selectedFolder);
    display.refreshControl(folderListControl);

    return 1;
}*/

uint8_t cSampleImporter::changeFileSelection(int16_t value)
{
    value = clamp<int>(value, -5, 5);

    if (selectionActive[selectedPlace])
    {
        if (selectionTab[listFiles][selectedFile] != 1 && checkIfValidSelection(selectedFile))
        {
            selectionTab[listFiles][selectedFile] = 1;
            selectionLength[listFiles]++;
        }
        selectedFile = handleSelecting(selectedFile, locationExplorerCount, value, 1);
        updateSelection();
    }
    else
    {
        selectedFile = clamp(selectedFile + value, 0, locationExplorerCount - 1);
        memset(&selectionTab[listFiles][0], 0, SI->maxListLength);
        SI->selectionLength[listFiles] = 0;
    }

    display.setControlValue(explorerListControl, selectedFile);
    display.refreshControl(explorerListControl);

    refreshGuiAfterDelay();

    if (SI->buttons->isButtonPressed(commonPreviewButton))
    {
        SI->playSelectedSdFile(0, MAX_16BIT);
    }

    return 1;
}

uint8_t cSampleImporter::changeInstrumentSelection(int16_t value)
{
    value = clamp<int>(value, -5, 5);

    if (selectionActive[selectedPlace])
    {
        if (selectionTab[listInstruments][selectedSlot] != 1)
        {
            selectionTab[listInstruments][selectedSlot] = 1;
            selectionLength[listInstruments]++;
        }
        selectedSlot = handleSelecting(selectedSlot, kSampleInstrumentsCount, value, 0);
    }
    else
    {
        selectedSlot = clamp(selectedSlot + value, 0, kSampleInstrumentsCount - 1);
        memset(&selectionTab[listInstruments][0], 0, SI->maxListLength);
        selectionLength[listInstruments] = 0;
    }

    mtProject.values.lastUsedInstrument = selectedSlot;

    display.setControlValue(instrumentListControl, selectedSlot);
    display.refreshControl(instrumentListControl);

    refreshGuiAfterDelay();

    if (SI->buttons->isButtonPressed(commonPreviewButton))
    {
        SI->playSampleFromBank(INTERFACE_BUTTON_PREVIEW, buttonPress, -1);
    }

    return 1;
}

void cSampleImporter::changePreviewVolume(int16_t value)
{
    mtConfig.general.previewVolume = clamp((int)mtConfig.general.previewVolume + value,
                                           (int)PREVIEW_VOLUME_MIN,
                                           (int)PREVIEW_VOLUME_MAX);
    engine.setPreviewVolume(mtConfig.general.previewVolume);
    saveConfig();
    showPreviewVolume();
}

void cSampleImporter::setDefaultPreviewVolume()
{
    mtConfig.general.previewVolume = PREVIEW_VOLUME_DEFAULT;
    engine.setPreviewVolume(mtConfig.general.previewVolume);
    saveConfig();
    showPreviewVolume();
}

void cSampleImporter::refreshGuiAfterDelay()
{
    delayRefreshGUI = true;
    guiRefreshDelay = 0;
}

//======================================================================================================================

void cSampleImporter::refreshActionButtons()
{
    showAddOrEnter();

    renameColorControl();
    addNextColorControl();
    previewColorControl();
    importColorControl();
    deleteUnusedColorControl();
    updateMemoryButtonLabel();
}

void cSampleImporter::BrowseOrAdd()
{
    if (SI->explorerNames == nullptr)
        return;

    // MPDJ: the virtual /MIDI folder
    if (midiBrowse)
    {
        if (selectedFile == 0) // "/.."
        {
            if (selectedPlace == 0)
                leaveMidiFolder();
        }
        else
        {
            addMidiPrograms(false);
        }
        return;
    }
    if (selectedFile == midiFolderIdx)
    {
        if (selectedPlace == 0)
            enterMidiFolder();
        return;
    }

    if (*explorerNames[selectedFile] == '/')
    {
        if (selectedPlace == 0)
        {
            SI->playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

            if (newFileManager.browseSdCard(&selectedFile))
            {
                SI->ip->blockAllInputs();
            }
        }
    }
    else
    {
        functInstrumentAdd();
    }
}

//==============================================================================================================
// MPDJ (step 5a-3): the virtual /MIDI folder of the Program Loader

static char s_midiFolderName[] = "/MIDI";
static char s_midiUpName[] = "/..";
static char s_midiChannelNames[16][20];
static char* s_midiView[17];
static char* s_rootView[cSampleImporter::maxListLength];

void cSampleImporter::rebuildExplorerView()
{
    midiFolderIdx = -1;
    if (midiBrowse)
    {
        s_midiView[0] = s_midiUpName;
        for (int ch = 1; ch <= 16; ch++)
        {
            snprintf(s_midiChannelNames[ch - 1], sizeof(s_midiChannelNames[0]), "MIDI Channel %d", ch);
            s_midiView[ch] = s_midiChannelNames[ch - 1];
        }
        explorerNames = s_midiView;
        locationExplorerCount = 17;
        return;
    }

    // at the card's root, /MIDI goes after the real entries so their indices
    // (which the file manager uses) don't move
    bool atRoot = strcmp(newFileManager.getExplorerPath(), "/") == 0;
    if (fmNames != nullptr && atRoot && fmCount + 1 < maxListLength)
    {
        for (int i = 0; i < fmCount; i++)
            s_rootView[i] = fmNames[i];
        s_rootView[fmCount] = s_midiFolderName;
        midiFolderIdx = fmCount;
        explorerNames = s_rootView;
        locationExplorerCount = fmCount + 1;
        return;
    }
    explorerNames = fmNames;
    locationExplorerCount = fmCount;
}

bool cSampleImporter::isVirtualEntry(int idx) const
{
    return midiBrowse || (midiFolderIdx >= 0 && idx == midiFolderIdx);
}

void cSampleImporter::enterMidiFolder()
{
    playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);
    cancelSelect(listFiles);
    midiBrowse = true;
    rebuildExplorerView();
    selectedFile = 1; // MIDI Channel 1
    display.setControlText(explorerLabelControl, "MIDI");
    display.refreshControl(explorerLabelControl);
    showFilesTree();
    handleMemoryBar();
    refreshActionButtons();
}

void cSampleImporter::leaveMidiFolder()
{
    cancelSelect(listFiles);
    midiBrowse = false;
    rebuildExplorerView();
    selectedFile = midiFolderIdx >= 0 ? midiFolderIdx : 0;
    display.setControlText(explorerLabelControl, "SD Card");
    display.refreshControl(explorerLabelControl);
    showFilesTree();
    handleMemoryBar();
    refreshActionButtons();
}

// Add / Add next in the /MIDI folder: the selected channel(s) become MIDI
// programs in the selected slot and the ones after it
void cSampleImporter::addMidiPrograms(bool addNext)
{
    int chFirst = getSelectionStart(listFiles);
    int chLast = getSelectionEnd(listFiles);
    if (chFirst < 1)
        chFirst = 1; // skip "/.."
    if (chLast < chFirst || chLast > 16)
        return;

    int count = chLast - chFirst + 1;
    int firstSlot = getSelectionStart(listInstruments);
    if (firstSlot + count > kSampleInstrumentsCount)
    {
        mtPopups.show(0, "Too many channels selected");
        return;
    }

    midiAddFirstSlot = firstSlot;
    midiAddFirstChannel = chFirst;
    midiAddCount = count;
    midiAddNext = addNext;

    int occupied = 0;
    for (int i = firstSlot; i < firstSlot + count; i++)
        if (mtProject.instrument[i].isActive || isMidiInstrument(i))
            occupied++;

    if (occupied > 0)
    {
        midiAddPending = true;
        replaceSource = 0;
        firstLineReplaceInfoPopup =
          firstLineReplacePopupOptions[occupied > 1 ? moreThanOneInstrumentInfoPupupFirstLineText
                                                    : oneInstrumentInfoPupupFirstLineText];
        setReplaceInstrumentHandlerFunctions();
        showInfoPopupReplaceInstrument();
        return;
    }
    applyMidiPrograms();
}

void cSampleImporter::applyMidiPrograms()
{
    midiAddPending = false;
    int first = midiAddFirstSlot, last = midiAddFirstSlot + midiAddCount - 1;

    // samples in the way go first (the file manager frees their memory);
    // fmDeleteInstrumentsEnd in update() comes back here
    for (int i = first; i <= last; i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            if (newFileManager.deleteInstruments(first, last))
            {
                midiAddWaitsForDelete = true;
                ip->blockAllInputs();
            }
            else
            {
                mtPopups.show(0, "Busy - try again");
            }
            return;
        }
    }

    for (int k = 0; k < midiAddCount; k++)
    {
        strProgramSlot& s = mtProject.programs.slot[first + k];
        s = {};
        s.type = kProgramMidi;
        s.midi.channel = uint8_t(midiAddFirstChannel + k);
    }
    newFileManager.setProjectStructChanged();
    mtProject.values.lastUsedInstrument = first;

    if (midiAddNext)
    {
        selectedSlot = (last + 1 < kSampleInstrumentsCount) ? last + 1 : kSampleInstrumentsCount - 1;
        int nextFile = midiAddFirstChannel + midiAddCount;
        selectedFile = nextFile <= 16 ? nextFile : 16;
        midiAddNext = false;
    }

    cancelSelect(listFiles);
    cancelSelect(listInstruments);
    listInstrumentSlots();
    showFilesTree();
    showInstrumentsList();
    refreshInstrumentsList();
    handleMemoryBar();
    refreshActionButtons();
}

void cSampleImporter::importSamples()
{
    if (!fullMemoryFlag)
    {
        uint8_t selStart = getSelectionStart(listFiles);
        uint8_t selEnd = getSelectionEnd(listFiles);

        bool result = newFileManager.importSamplesToProject(
          selStart,
          selEnd,
          selectedSlot,
          0,
          MAX_16BIT,
          false,
          static_cast<FileManagerOperation::SampleLoadMode>(sampleLoadMode));
        addNextOffset = (selEnd - selStart) + 1;

        if (result)
        {
            SI->ip->blockAllInputs();
        }
    }
    else
    {
        mtPopups.show(0, "Not enough sample memory");
    }
}

//==============================================================================================

void cSampleImporter::listInstrumentSlots()
{
    for (uint8_t i = 0; i < kSampleInstrumentsCount; i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            if (mtProject.sampleMemory[i].isStreamed)
            {
                snprintf(&interfaceGlobals.intrumentsNames[i][0],
                         sizeof(interfaceGlobals.intrumentsNames[i]),
                         "%d.[S] %s",
                         i + 1,
                         mtProject.instrument[i].sample.file_name);
            }
            else
            {
                snprintf(&interfaceGlobals.intrumentsNames[i][0],
                         sizeof(interfaceGlobals.intrumentsNames[i]),
                         "%d.%s",
                         i + 1,
                         mtProject.instrument[i].sample.file_name);
            }
        }
        else if (isMidiInstrument(i)) // MPDJ: MIDI programs share the 64 slots
        {
            snprintf(&interfaceGlobals.intrumentsNames[i][0],
                     sizeof(interfaceGlobals.intrumentsNames[i]),
                     "%d.[MIDI ch %d]",
                     i + 1,
                     instrumentIdxToMidiChannel(i));
        }
        else
        {
            snprintf(&interfaceGlobals.intrumentsNames[i][0],
                     sizeof(interfaceGlobals.intrumentsNames[i]),
                     "%d.",
                     i + 1);
        }

        interfaceGlobals.ptrIntrumentsNames[i] = &interfaceGlobals.intrumentsNames[i][0];
    }
}

void cSampleImporter::handleMemoryBar()
{

    if (checkIfValidSelection(selectedFile))
    {
        calculateFileMemUsage();
    }
    else
    {
        currentSelectMemorySize = 0;
        fullMemoryFlag = 0;
    }

    calculateMemUsage();
    showMemoryUsage();
}

void cSampleImporter::calculateFileMemUsage()
{
    uint32_t tempSelectMemorySize = 0;
    uint32_t tempInstrumentMemorySize = 0;
    uint8_t tempSelectionLength;

    auto estimateFileRamUsage = [this](uint32_t rawBytes) -> uint32_t {
        uint32_t minStreamableBytes = kStreamPreCacheFrames * sizeof(int16_t);
        uint32_t maxPreCacheBytes =
          kStreamPreCacheFrames *
          ((newFileManager.getSampleMode() == FileManagerOperation::kLeftAndRight) ? 4 : 2);
        if (sampleLoadMode == enSampleLoadMode::kStream && rawBytes > minStreamableBytes)
        {
            return std::min<uint32_t>(rawBytes, maxPreCacheBytes);
        }
        if (sampleLoadMode == enSampleLoadMode::kAuto && rawBytes >= kStreamThresholdBytes &&
            rawBytes > minStreamableBytes)
        {
            return std::min<uint32_t>(rawBytes, maxPreCacheBytes);
        }
        return rawBytes;
    };

    if (selectionLength[listFiles] && selectedPlace == 0)
    {
        uint8_t selectionStart = getSelectionStart(listFiles);

        if (selectedSlot + selectionLength[listFiles] > kSampleInstrumentsCount)
        {
            tempSelectionLength = selectionLength[listFiles] -
                                  (selectedSlot + selectionLength[listFiles] - kSampleInstrumentsCount);
        }
        else
        {
            tempSelectionLength = selectionLength[listFiles];
        }

        // get file selection size
        for (int i = selectionStart; i < (selectionStart + tempSelectionLength); i++)
        {
            if (!isVirtualEntry(i) && i < fmCount) // MPDJ
                tempSelectMemorySize += estimateFileRamUsage(currentFolderMemoryFileUsage[i]);
        }

        // get size when pasted on current isntrument slot
        for (uint32_t i = selectedSlot; i < (selectedSlot + tempSelectionLength); i++)
        {
            if (mtProject.instrument[i].isActive)
            {
                tempInstrumentMemorySize += newFileManager.getSampleMemorySize(i, i);
            }
        }

        currentSelectMemorySize = tempSelectMemorySize - tempInstrumentMemorySize;
    }
    else
    {
        currentSelectMemorySize =
          (isVirtualEntry(selectedFile) || selectedFile >= fmCount) // MPDJ
            ? 0
            : estimateFileRamUsage(currentFolderMemoryFileUsage[selectedFile]);

        if (mtProject.instrument[selectedSlot].isActive)
        {
            currentSelectMemorySize -=
              newFileManager.getSampleMemorySize(selectedSlot, selectedSlot);
        }
    }
}

void cSampleImporter::calculateMemUsage()
{
    int32_t memoryUsageAddCurrentSelect = 0;

    if (mtProject.used_memory > mtProject.max_memory)
    {
        memoryUsage = 100;
    }
    else
    {
        memoryUsage = (mtProject.used_memory * 100.0) / mtProject.max_memory;
    }

    memoryUsageAddCurrentSelect = mtProject.used_memory + currentSelectMemorySize;

    if (memoryUsageAddCurrentSelect < 0)
    {
        memoryUsageAddCurrentSelect = 0;
    }

    if ((uint32_t)memoryUsageAddCurrentSelect > mtProject.max_memory)
    {
        memoryUsageAdd.value = 100;
        fullMemoryFlag = 1;
    }
    else
    {
        memoryUsageAdd.value = (memoryUsageAddCurrentSelect * 100.0) / mtProject.max_memory;
        fullMemoryFlag = 0;
    }
}

/*void cSampleImporter::calculateCurrentSelectMemorySize()
{
//	if(!isWavFile(&locationFileList[selectedFile][0])) return;

    char file_path[255];

    strcpy(file_path, actualPath);
    if(dirLevel > 0)strcat(file_path, "/");
    strcat(file_path, &locationExplorerList[selectedFile][0]);

    currentSelectMemorySize = 2* fileManager.samplesLoader.waveLoader.getInfoAboutWave(file_path);
    if(mtProject.instrument[selectedSlot].isActive) currentSelectMemorySize -=
2*mtProject.instrument[selectedSlot].sample.length;
}*/

void cSampleImporter::calculateLoadProgress()
{
    // loadProgress = fileManager.samplesLoader.getCurrentProgress();//todo
}

void cSampleImporter::calculateCopyingProgress()
{
    // copyingProgress = fileManager.samplesImporter.getProgress();///todo
}

//==============================================================================================
void cSampleImporter::playSelectedSdFile(uint16_t from, uint16_t to)
{

    //	if(sequencer.getSeqState() != Sequencer::SEQ_STATE_STOP)
    //	{
    //		showStopPatternPopup();
    //		setStopPatternFunction();
    //		stopPatternAction = enStopPatternAction::previewSD;
    //		return;
    //	}

    stopPlayingAll();

    if (newFileManager.previevSamplefromSD(selectedFile, from, to))
    {
        //		ip->blockAllInputsExcept(interfaceButton4);
        //		ip->unblockPads();
    }

    playMode = playModeSdFile;
}

void cSampleImporter::playSampleFromBank(uint8_t pad, uint8_t state, int16_t velo)
{
    if (currentCopyStatusFlag || currentLoadStatusFlag)
        return;
    if (!mtProject.instrument[selectedSlot].isActive)
        return;

    //	if(sequencer.getSeqState() != Sequencer::SEQ_STATE_STOP)
    //	{
    //		showStopPatternPopup();
    //		setStopPatternFunction();
    //		stopPatternAction = enStopPatternAction::previewBank;
    //		return;
    //	}
    //
    //	stopPlaying();

    mtProject.values.lastUsedInstrument = SI->selectedSlot;
    playMode = playModeSampleBank;

    uint8_t noteFromPad = mtPadBoard.getNoteFromPad(pad);

    if (SI->buttons->isButtonPressed(commonPreviewButton))
    {

        sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, noteFromPad, -1, pad);
    }

    sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, noteFromPad, -1, pad);
}

void cSampleImporter::stopPlaying(uint8_t pad, uint8_t state, int16_t velo)
{
    if (playMode == playModeSdFile)
    {
        newFileManager.stopPrevievSamplefromSD();
        SI->ip->unblockAllInputs();
    }
    else if (playMode == playModeSampleBank)
    {
        //		mtPadBoard.stopInstrument(pad);
        uint8_t noteFromPad = mtPadBoard.getNoteFromPad(pad);
        sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, noteFromPad, 0, pad);
    }

    //	playMode = playModeStop;
}

void cSampleImporter::stopPlayingAll()
{

    if (playMode == playModeSdFile)
    {
        stopPlaying(0, 0, 0);
    }
    else if (playMode == playModeSampleBank)
    {
        sequencer.stopManualNotes();
    }
}

static void functPreviewFile(uint8_t state)
{
    SI->playFromPads(INTERFACE_BUTTON_PREVIEW, state, -1);
}

void cSampleImporter::playFromPads(uint8_t pad, uint8_t state, int16_t velo)
{
    if (state == 0)
    {
        stopPlaying(pad, state, velo);
    }
    else if (state == 1)
    {
        int placeToSelectPreviewSource =
          selectedPlace == 2 ? selectedPlaceBeforePreviewVolume : selectedPlace;

        switch (placeToSelectPreviewSource)
        {
            case 0:
                if (!isVirtualEntry(selectedFile)) // MPDJ: /MIDI has nothing to preview
                    SI->playSelectedSdFile(0, MAX_16BIT);
                break;
            case 1: SI->playSampleFromBank(pad, state, velo); break;
        }
    }
}

void cSampleImporter::updateSelection()
{
    display.refreshControl(explorerListControl);
    display.refreshControl(instrumentListControl);
}

uint8_t cSampleImporter::handleSelecting(uint8_t selectStart,
                                         uint8_t selectMax,
                                         int16_t value,
                                         uint8_t fileCheck)
{
    if (selectedPlace > 1)
    {
        return selectStart;
    }

    int8_t sign = 1;
    int16_t position;

    if (value < 0)
    {
        sign = -1;
        value *= -1;
    }

    position = selectStart;

    for (int i = 0; i < value; i++)
    {
        if ((position + (1 * sign)) >= 0 && (position + (1 * sign)) < selectMax)
        {
            position += (1 * sign);

            if (!fileCheck || checkIfValidSelection(position))
            {
                if (selectionTab[selectedPlace][position] == 1)
                {
                    uint8_t flag = 1;
                    if (sign < 0)
                    {
                        selectionTab[selectedPlace][position + 1] = 0;
                    }
                    else if (sign > 0)
                    {
                        if (selectionTab[selectedPlace][position - 1] == 1)
                        {
                            selectionTab[selectedPlace][position - 1] = 0;
                        }
                        else
                        {
                            flag = 0;
                        }
                    }

                    if (selectionLength[selectedPlace] && flag)
                    {
                        selectionLength[selectedPlace]--;
                    }
                }
                else
                {
                    if (selectionLength[selectedPlace] < MAX_SELECT)
                    {
                        selectionLength[selectedPlace]++;
                        selectionTab[selectedPlace][position] = 1;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    return position;
}

bool cSampleImporter::checkIfAnyInstrActive()
{
    uint8_t selectionStart = getSelectionStart(listInstruments);
    bool isActive = false;

    for (uint32_t i = selectionStart; i < (selectionStart + selectionLength[listInstruments]); i++)
    {
        if (mtProject.instrument[i].isActive == 1)
        {
            isActive = true;
            break;
        }
    }

    return isActive;
}

void cSampleImporter::cancelSelect(uint8_t placeToCancel)
{
    memset(&SI->selectionTab[placeToCancel][0], 0, maxListLength);
    selectionLength[placeToCancel] = 0;

    updateSelection();
}

int16_t cSampleImporter::getSelectionStart(uint8_t whichSelect)
{
    int16_t selStart = SI->maxListLength;

    for (size_t i = 0; i < SI->maxListLength; i++)
    {
        if (selectionTab[whichSelect][i] == 1)
        {
            selStart = i;
            return selStart;
        }
    }

    if (whichSelect == listFiles)
    {
        return selectedFile;
    }
    else
    {
        return selectedSlot;
    }
}

int16_t cSampleImporter::getSelectionEnd(uint8_t whichSelect)
{
    int16_t selEnd = 0;

    for (int i = SI->maxListLength - 1; i >= 0; i--)
    {
        if (selectionTab[whichSelect][i] == 1)
        {
            selEnd = i;
            return selEnd;
        }
    }

    if (whichSelect == listFiles)
    {
        return selectedFile;
    }
    else
    {
        return selectedSlot;
    }
}

bool cSampleImporter::checkIfValidSelection(uint8_t positionToCheck)
{
    if (explorerNames != nullptr && explorerNames[positionToCheck] != nullptr &&
        !(*explorerNames[positionToCheck] == '/'))
    {
        return true;
    }

    return false;
}

char* cSampleImporter::parseNewName(uint8_t num)
{
    sprintf(instrToCreate, "instr%02d.wav", num);

    return instrToCreate;
}

uint8_t cSampleImporter::getActiveInstrInSelection()
{
    uint8_t activeInstr = 0;

    for (uint8_t instr = instrCopyStart; instr < (instrCopyStart + copyElementsCount); instr++)
    {
        if (mtProject.instrument[instr].isActive == 1)
        {
            activeInstr++;
        }
    }

    return activeInstr;
}

uint8_t cSampleImporter::willCopyInstrFit(uint8_t length)
{
    int32_t memoryUsedBySelection = 0;
    int32_t pasteLocationMemUsage = 0;
    int32_t memoryUsageEstimate = 0;
    uint8_t retValue;

    for (int i = instrCopyStart; i < (length + instrCopyStart); i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            memoryUsedBySelection += newFileManager.getSampleMemorySize(i, i);
        }
    }

    for (int i = selectedSlot; i < (length + selectedSlot); i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            pasteLocationMemUsage += newFileManager.getSampleMemorySize(i, i);
        }
    }

    memoryUsageEstimate = memoryUsedBySelection - pasteLocationMemUsage;

    if ((int32_t)mtProject.used_memory + memoryUsageEstimate < (int32_t)mtProject.max_memory)
    {
        retValue = 1;
    }
    else
    {
        retValue = 0;
    }

    return retValue;
}

void cSampleImporter::resetInstrSel()
{
    instrCopyStart = 0;
    copyElementsCount = 0;
}

void cSampleImporter::setStopPatternFunction()
{
    ip->clearButton(interfaceButton6);
    ip->clearButton(interfaceButton7);

    ip->setButtonObj(interfaceButton6, buttonPress, functStopPatternNo);
    ip->setButtonObj(interfaceButton7, buttonPress, functStopPatternYes);
    ip->blockAllInputsExcept(interfaceButton6, interfaceButton7);
}
void cSampleImporter::clearStopPatternFunction()
{
    ip->clearButton(interfaceButton6);
    ip->clearButton(interfaceButton7);

    ip->setButtonObj(interfaceButton6, functSelectPreviewVolume);
    ip->setButtonObj(interfaceButton7, buttonPress, functMemoryModeOrToggle);

    ip->unblockAllInputs();
}

void cSampleImporter::setDeleteInstrumentHandlerFunctions()
{
    ip->clearButton(interfaceButton6);
    ip->clearButton(interfaceButton7);

    ip->setButtonObj(interfaceButton6, buttonPress, functDeleteInstrumentDeny);
    ip->setButtonObj(interfaceButton7, buttonPress, functDeleteInstrumentConfirm);

    ip->blockAllInputsExcept(interfaceButton6, interfaceButton7);
}

void cSampleImporter::setDeleteUnusedInstrumentsHandlerFunctions()
{
    ip->clearButton(interfaceButton6);
    ip->clearButton(interfaceButton7);

    ip->setButtonObj(interfaceButton6, buttonPress, functDeleteInstrumentDeny);
    ip->setButtonObj(interfaceButton7, buttonPress, functDeleteUnusedInstrumentsConfirm);

    ip->blockAllInputsExcept(interfaceButton6, interfaceButton7);
}

void cSampleImporter::setReplaceInstrumentHandlerFunctions()
{
    ip->clearButton(interfaceButton6);
    ip->clearButton(interfaceButton7);

    ip->setButtonObj(interfaceButton6, buttonPress, functReplaceInstrumentDeny);

    if (replaceSource == 0)
    {
        ip->setButtonObj(interfaceButton7, buttonPress, functReplaceInstrumentConfirm);
    }
    else
    {
        ip->setButtonObj(interfaceButton7, buttonPress, functImportAndReplaceInstrumentConfirm);
    }

    ip->blockAllInputsExcept(interfaceButton6, interfaceButton7);
}
void cSampleImporter::clearInfoPopupHandlerFunctions()
{
    ip->clearButton(interfaceButton6);
    ip->clearButton(interfaceButton7);

    ip->setButtonObj(interfaceButton6, functSelectPreviewVolume);
    ip->setButtonObj(interfaceButton7, buttonPress, functMemoryModeOrToggle);

    ip->unblockAllInputs();
}

static void functMemoryModeOrToggle()
{
    SI->handleMemoryModeOrToggle();
}

void cSampleImporter::handleMemoryModeOrToggle()
{
    if (selectedPlace == 1 && selectedSlot < kSampleInstrumentsCount &&
        mtProject.instrument[selectedSlot].isActive)
    {
        playFromPads(INTERFACE_BUTTON_PREVIEW, buttonRelease, -1);

        uint8_t selStart = getSelectionStart(listInstruments);
        uint8_t selEnd = getSelectionEnd(listInstruments);
        if (selStart >= kSampleInstrumentsCount || selEnd >= kSampleInstrumentsCount ||
            selEnd < selStart)
        {
            selStart = selectedSlot;
            selEnd = selectedSlot;
        }

        bool targetStream = !mtProject.sampleMemory[selectedSlot].isStreamed;
        bool anyChanged = false;

        for (uint8_t idx = selStart; idx <= selEnd; idx++)
        {
            if (!mtProject.instrument[idx].isActive)
            {
                continue;
            }

            if (targetStream && !mtProject.sampleMemory[idx].isStreamed)
            {
                char instrPath[PATCH_SIZE];
                snprintf(instrPath, sizeof(instrPath), cWorkspaceInstrumentFileFormatNew, idx + 1);
                if (newFileManager.demoteRamInstrumentToStream(
                      idx, instrPath, sizeof(strInstrumentFile), true))
                {
                    newFileManager.saveInstrumentStructToWorkspace(idx);
                    anyChanged = true;
                }
            }
            else if (!targetStream && mtProject.sampleMemory[idx].isStreamed)
            {
                if (newFileManager.promoteStreamedInstrumentToRam(idx))
                {
                    newFileManager.saveInstrumentStructToWorkspace(idx);
                    anyChanged = true;
                }
                else
                {
                    mtPopups.show(0, "Not enough sample memory");
                    break;
                }
            }
        }

        if (anyChanged)
        {
            listInstrumentSlots();
            showInstrumentsList();
            handleMemoryBar();
        }
        updateMemoryButtonLabel();
        display.synchronizeRefresh();
    }
    else
    {
        if (sampleLoadMode == enSampleLoadMode::kRam)
        {
            sampleLoadMode = enSampleLoadMode::kAuto;
        }
        else if (sampleLoadMode == enSampleLoadMode::kAuto)
        {
            sampleLoadMode = enSampleLoadMode::kStream;
        }
        else
        {
            sampleLoadMode = enSampleLoadMode::kRam;
        }
        updateMemoryButtonLabel();
        handleMemoryBar();
        display.synchronizeRefresh();
    }
}

static void functConfirmKey()
{
    SI->keyboardManager.confirmKey();
}

static void functDeleteBackspace(uint8_t state)
{
    if ((state == buttonPress) || (state == buttonHold))
    {
        SI->keyboardManager.makeBackspace();
    }
}

//==============================================================================================================
// tracker mini middle button
static void functMiniMiddle(uint8_t state)
{
    if (state != buttonPress)
    {
        return;
    }

    // oblsuga klawiatury
    if (SI->keyboardManager.getState())
    {
        SI->keyboardManager.confirmKey();
        return;
    }

    functEnter();
}

//==============================================================================================================
static void functPads(uint8_t pad, uint8_t state, int16_t velo)
{
    if (SI->keyboardManager.getState())
    {
        SI->keyboardManager.onPadChange(pad, state);
    }
    else
    {
        // xxx playback wylaczony na zlecenie Piotrka - 1.1.17
        //		if((state == 1) || (state == 0))
        //		{
        //			SI->playFromPads(pad,state,velo);
        //			if(state) padsBacklight.setFrontLayer(1,mtConfig.values.padsLightFront, pad);
        //			else padsBacklight.setFrontLayer(0,0,pad);
        //		}
    }
}

static void functStopPatternYes()
{
    sequencer.stop();
    SI->hideStopPatternPopup();
    SI->clearStopPatternFunction();

    switch (SI->stopPatternAction)
    {
        case cSampleImporter::enStopPatternAction::previewBank: break;
        case cSampleImporter::enStopPatternAction::previewSD: break;
        case cSampleImporter::enStopPatternAction::instrAdd: functInstrumentAdd(); break;
        case cSampleImporter::enStopPatternAction::instrAddNext: functInstrumentAddNext(); break;
        case cSampleImporter::enStopPatternAction::instrCopy: functCopyPaste(); break;
        case cSampleImporter::enStopPatternAction::instrDelete: functInstrumentDelete(); break;
        default: break;
    }

    return;
}

static void functStopPatternNo()
{
    SI->hideStopPatternPopup();
    SI->clearStopPatternFunction();
}

static void functDeleteInstrumentConfirm()
{
    SI->clearInfoPopupHandlerFunctions();

    uint8_t deleteStart = SI->getSelectionStart(listInstruments);
    uint8_t deleteEnd = SI->getSelectionEnd(listInstruments);

    if (deleteStart >= kSampleInstrumentsCount || deleteEnd >= kSampleInstrumentsCount ||
        deleteEnd < deleteStart)
    {
        return;
    }

    // MPDJ: ERASE clears MIDI programs right away; samples go through the file manager
    if (countMidiPrograms(deleteStart, deleteEnd) > 0)
    {
        clearMidiPrograms(deleteStart, deleteEnd);
        SI->listInstrumentSlots();
        SI->refreshInstrumentsList();
        SI->refreshActionButtons();
    }

    uint8_t instrToDeleteCount = 0;
    for (uint8_t i = deleteStart; i <= deleteEnd; i++)
    {
        if (mtProject.instrument[i].isActive)
        {
            instrToDeleteCount++;
        }
    }

    if (instrToDeleteCount > 0)
    {
        if (newFileManager.deleteInstruments(SI->getSelectionStart(listInstruments),
                                             SI->getSelectionEnd(listInstruments)))
        {
            SI->ip->blockAllInputs();
        }
    }

    SI->hideStopPatternPopup();
}

void cSampleImporter::initDeleteUnusedInstruments()
{
    memset(instrUseMap, 0, sizeof(instrUseMap));

    for (int patternIdx = PATTERN_INDEX_MIN; patternIdx < PATTERN_INDEX_MAX; patternIdx++)
    {
        const Sequencer::strPattern* pattern =
          newFileManager.loadWorkspacePatternToTempBuffer(patternIdx);

        if (pattern == nullptr)
        {
            continue;
        }

        for (int trackIdx = 0; trackIdx < kSampleTracks; trackIdx++)
        {
            for (int stepIdx = 0; stepIdx < pattern->track[trackIdx].length; stepIdx++)
            {
                const struct Sequencer::strPattern::strTrack::strStep& step =
                  pattern->track[trackIdx].step[stepIdx];

                if (step.note > Sequencer::STEP_NOTE_EMPTY)
                {
                    instrUseMap[step.instrument] = 1;
                }
            }
        }
    }

    stepDelete = true;
}

bool cSampleImporter::stepDeleteUnusedInstruments()
{
    int8_t unusedInstrIdx = -1;
    for (int instrIdx = 0; instrIdx < kSampleInstrumentsCount; instrIdx++)
    {
        if (!instrUseMap[instrIdx] && mtProject.instrument[instrIdx].isActive)
        {
            unusedInstrIdx = instrIdx;
            newFileManager.deleteInstruments(instrIdx, instrIdx);
            break; // only delete one at a time
        }
    }

    return (unusedInstrIdx > -1);
}

static void functDeleteUnusedInstrumentsConfirm()
{
    SI->initDeleteUnusedInstruments();

    if (SI->stepDeleteUnusedInstruments())
    {
        SI->ip->blockAllInputs();
        SI->hideStopPatternPopup();
        return;
    }
    else
    {
        SI->clearInfoPopupHandlerFunctions();
        SI->hideInfoPopup();
        return;
    }
}

static void functDeleteInstrumentDeny()
{
    SI->clearInfoPopupHandlerFunctions();
    SI->hideInfoPopup();
}

static void functReplaceInstrumentConfirm()
{
    if (SI->midiAddPending) // MPDJ: replacing with MIDI programs
    {
        SI->clearInfoPopupHandlerFunctions();
        SI->hideInfoPopup(); // back to the loader's own screen and buttons
        SI->applyMidiPrograms();
        return;
    }
    if (SI->explorerNames != nullptr && *SI->explorerNames[SI->selectedFile] != '/')
    {
        int first, last;
        sampleAddRange(first, last);
        clearMidiPrograms(first, last); // MPDJ: samples replace MIDI programs
        SI->sampleType = mtSampleTypeWaveFile;
        SI->addNextFlag = SI->addNextOperationWantReplaceInstrument ? 1 : 0;
        SI->importSamples();
    }

    SI->clearInfoPopupHandlerFunctions();
    SI->hidePopupButtons();

    SI->addNextOperationWantReplaceInstrument = false;
}

static void functReplaceInstrumentDeny()
{
    SI->midiAddPending = false; // MPDJ
    SI->clearInfoPopupHandlerFunctions();
    SI->hideInfoPopup();

    SI->addNextOperationWantReplaceInstrument = false;
}

static void functMenu(uint8_t state)
{
    if (state == buttonPress)
    {
        mtMenuPopup.showModulePopup(SI);
    }
    else if (state == buttonRelease)
    {
        mtMenuPopup.hideModulePopup(SI);
    }
}

static void functSelectPreviewVolume(uint8_t state)
{
    if (state == buttonPress)
    {
        SI->wasPreviewVolumeHeld = false;

        if (SI->selectedPlace != 2)
        {
            SI->selectedPlaceBeforePreviewVolume = SI->selectedPlace;
            SI->blockReturningToPreviousPlace = false;
        }
        else
        {
            SI->blockReturningToPreviousPlace = true;
        }

        SI->selectedPlace = 2;
        SI->activateLabelsBorder();
    }
    else if (state == buttonHold)
    {
        SI->wasPreviewVolumeHeld = true;
    }
    else if (state == buttonRelease)
    {
        if (SI->selectedPlaceBeforePreviewVolume == 0 && SI->wasPreviewVolumeHeld &&
            !SI->blockReturningToPreviousPlace)
        {
            SI->selectedPlace = SI->selectedPlaceBeforePreviewVolume;
            SI->activateLabelsBorder();
        }
    }
}
