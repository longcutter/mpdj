#include "core/interface.h"

#include "projectEditor/projectEditor.h"

#include "fileManager.h"
#include "songTimer.h"

#include "Platform.h"
#include "mtSleep.h"
#include <display.h>
#include <mtAudioExport.h>

elapsedMillis screenRefreshTimer;

// extern cProjectEditor* PE;
static void functEndFileTransferMode();
extern void changeVolume(int16_t value);

static const char* noSdText = "Please insert SD card to continue";
static const char* usbStorageModeText = "USB Storage Mode";
static const char* usbStorageModeDisclaimer = {
    "Do not unplug anything while using this mode as it might damage file system on the SD "
    "card. Before exiting this mode, safely eject the Disk in your computer OS."
};

//==================================================================================================
//============================= LADOWANIE PROJEKTU PRZY STARCE =====================================
//==================================================================================================
void Interface::openStartupProject()
{
    if (!newFileManager.openProjectFromWorkspace())
    {
        openNewProject();
    }
}

uint8_t Interface::detectProjectLoadState()
{

    uint8_t startStatus = newFileManager.getStatus();

    if (startStatus != fmLoadEnd)
    {
        if (startStatus >= fmError)
        {
            newFileManager.clearStatus();

            openNewProject();
        }

        return 0;
    }

    if (startProjectLoadingProgress < 100)
        return 0;

    newFileManager.clearStatus();

    // taski na koniec otwierania projektu startowego
    fileManagerPopupEnabled = 1;

    return 1;
}

void Interface::openNewProject()
{
    newFileManager.createNewProjectInWorkspace();
    newFileManager.openProjectFromWorkspace();
}
//=====================================================================================
//============================== START SCREEN =========================================
//=====================================================================================
void Interface::initStartScreen()
{
    strControlProperties prop;
    prop.x = 190;
    prop.y = 280;
    prop.style = 0; // controlStyleShow;
    prop.h = 100;
    prop.w = 420;

    prop.data = interfaceGlobals.currentFirmwareVersion;
    startScreenControl = display.createControl<cStartScreen>(&prop);

    minStartTimeCounter = 0;
    startProjectLoadingProgress = 0;

    display.setControlShow(startScreenControl);
    display.setControlValue(startScreenControl, startProjectLoadingProgress);
    display.setControlText(startScreenControl, projectLoadText);
    display.refreshControl(startScreenControl);

    // uiFM.setButtonObj(interfaceButton7, buttonPress, functHide);
}

void Interface::refreshStartScreen()
{
    if (screenRefreshTimer < 40)
        return;
    screenRefreshTimer = 0;

    startProjectLoadingProgress = newFileManager.getProgress();

#ifdef PLATFORM_MPC
    // MPDJ: keep the logo up for at least ~5 s (Mike): the bar may reach 100% no
    // sooner than 125 ticks of 40 ms. The counter stops at 255 so a slow load
    // can't wrap it back to a low cap
    if (startProjectLoadingProgress > minStartTimeCounter * 4 / 5)
        startProjectLoadingProgress = minStartTimeCounter * 4 / 5;
    if (minStartTimeCounter < 255)
        minStartTimeCounter++;
#else
    // spowolnienie jesli start szybciej niz ~ 2 sek
    if (startProjectLoadingProgress > minStartTimeCounter * 2)
        startProjectLoadingProgress = minStartTimeCounter * 2;
    minStartTimeCounter++;
#endif

    if (fileManagerLastProgress == startProjectLoadingProgress && fileManagerLastProgress < 100)
    {
        noProgressChangeCounter++;

        if (noProgressChangeCounter > 5)
        {
            startProjectLoadingProgress = fileManagerLastProgress + 1;
        }
    }

    if (startProjectLoadingProgress >= 0 && fileManagerLastProgress < startProjectLoadingProgress)
    {
        noProgressChangeCounter = 0;
        fileManagerLastProgress = startProjectLoadingProgress;

        display.setControlShow(startScreenControl);
        display.setControlValue(startScreenControl, startProjectLoadingProgress);
        display.setControlText(startScreenControl, projectLoadText);
        display.refreshControl(startScreenControl);
    }
}

void Interface::hideStartScreen()
{
    display.setControlHide(startScreenControl);
    display.refreshControl(startScreenControl);
}

void Interface::destroyStartScreen()
{
    display.destroyControl(startScreenControl);
    startScreenControl = -1;
}

//=====================================================================================
//========================== AKTYWACJA INTERFEJSU =====================================
//=====================================================================================
void Interface::activateInterface()
{
    //	hideDisplayNoSdCard();
    //	hideDisplayFileTransfer();
    //	hideDisplayShutdown();
    //
    turnOffProgressBar = -1;
    noSdTextControl = -1;
    fileTransferLabelControl = -1;
    display.destroyModulesControls();

    interfaceGlobals.refreshFileExplorer = true;
    uint8_t state = 1;
    interfaceEnvents(eventToggleActiveModule, 0, 0, &state);

    specialScreenMode = specialScreenModeDisabled;
}

//=====================================================================================
//========================= DEAKTYWACJA INTERFEJSU ==================================
//=====================================================================================

void Interface::activateNoSdScreen()
{
    uint8_t state = 0;
    interfaceEnvents(eventToggleActiveModule, 0, 0, &state);

    hideDisplayFileTransfer();
    showDisplayNoSdCard();

    specialScreenMode = noSdCardScreenMode;
}

void Interface::activateFileTransferModeScreen()
{
    uint8_t state = 0;
    interfaceEnvents(eventToggleActiveModule, 0, 0, &state);

    ip->blockAllInputsExcept(interfaceButton7);
    ip->setButtonObj(interfaceButton7, buttonPress, functEndFileTransferMode);

    showDisplayFileTransferMode();

    specialScreenMode = usbStorageScreenMode;
}

void Interface::activateShutdownMode()
{
    uint8_t state = 0;
    interfaceEnvents(eventToggleActiveModule, 0, 0, &state);

    showDisplayShutdown();
}

static void functEndFileTransferMode()
{
    mtInterface.ip->unblockAllInputs();
    mtInterface.handleFileTransferMode(0);
}

//=====================================================================================
//========================= EKRAN BRAKU KARTY SD ======================================
//=====================================================================================
void Interface::showDisplayNoSdCard()
{
    strControlProperties prop;
    prop.x = 400;
    prop.y = 240;
    prop.w = 240;
    prop.h = 25;
    prop.colors = display.getTheme().noSdInfoTextColors;
    prop.style = (controlStyleCenterY | controlStyleCenterX);
    noSdTextControl = display.createControl<cSimpleText>(&prop);

    display.setControlText(noSdTextControl, noSdText);
    display.setControlShow(noSdTextControl);
    display.refreshControl(noSdTextControl);
}

void Interface::showDisplayFileTransferMode()
{
    strControlProperties prop;
    prop.x = 400;
    prop.y = 240;
    prop.w = 240;
    prop.h = 25;
    prop.colors = display.getTheme().noSdInfoTextColors;
    prop.style = (controlStyleCenterY | controlStyleCenterX | controlStyleShow);
    prop.text = usbStorageModeText;
    noSdTextControl = display.createControl<cSimpleText>(&prop);

    display.refreshControl(noSdTextControl);

    prop.style = controlStyleFont1 | controlStyleShow;
    prop.colors = display.getTheme().configItemEditorTextLabelColors;
    prop.value = 3;
    prop.x = 100;
    prop.y = 300;
    prop.w = 600;
    prop.h = 100;
    prop.text = usbStorageModeDisclaimer;
    descriptionTextBox = display.createControl<cTextBox>(&prop);

    display.refreshControl(descriptionTextBox);

    prop.x = (800 / 8) * 7 + 800 / 16;
    prop.y = 424;
    prop.w = 800 / 8;
    prop.h = 30;
    prop.colors = display.getTheme().activeButtonLabelsColors;
    prop.style = controlStyleCenterX | controlStyleFont4 | controlStyleShow;
    prop.text = "Exit";
    fileTransferLabelControl = display.createControl<cLabel>(&prop);

    display.refreshControl(fileTransferLabelControl);
}

void Interface::hideDisplayNoSdCard()
{
    display.destroyControl(noSdTextControl);
    noSdTextControl = -1;
}

void Interface::hideDisplayFileTransfer()
{
    display.destroyControl(noSdTextControl);
    noSdTextControl = -1;

    display.destroyControl(descriptionTextBox);
    descriptionTextBox = -1;

    display.destroyControl(fileTransferLabelControl);
    fileTransferLabelControl = -1;
}

//=====================================================================================
//============================ EKRAN WYLACZANIA  ======================================
//=====================================================================================

void Interface::showDisplayShutdown()
{
    strControlProperties prop;

    prop.x = 190;
    prop.y = 170;
    prop.style = controlStyleValue_0_100 | controlStyleBackground;
    prop.h = 100;
    prop.w = 420;
    turnOffProgressBar = display.createControl<cHorizontalBar>(&prop);

    refreshDisplayShutdown();
}

void Interface::refreshDisplayShutdown()
{
    if (screenRefreshTimer < 30)
        return;
    screenRefreshTimer = 0;

    //	sprintf(turnOffText, "Shutdown in %.1fs", (lowPower.getTimeLeft()/1000.0));
    sprintf(turnOffText, "Shutting down");

    display.setControlValue(turnOffProgressBar, lowPower.getShutdownProgress());
    display.setControlText(turnOffProgressBar, turnOffText);
    display.setControlShow(turnOffProgressBar);
    display.refreshControl(turnOffProgressBar);
}

void Interface::hideDisplayShutdown()
{
    display.destroyControl(turnOffProgressBar);
    turnOffProgressBar = -1;
}

// takie różne globalne rzeczy
void Interface::commonThingsUpdate()
{
    if (songTimer.enabled)
    {
        songTimer.refresh();
    }

    titleBar.refresh();

    if (fileManagerPopupEnabled)
    {
        uint8_t managerStatus = newFileManager.getStatus();

        if (managerStatus > fmIdle && managerStatus < fmLoadEnd)
        {
            int8_t text_index = -1;

            switch (managerStatus)
            {
                case fmBrowsingSamples: text_index = 1; break;
                case fmBrowsingFirmwares: text_index = 3; break;
                case fmImportingSamplesToWorkspace: text_index = 4; break;
                case fmCopyingInstrumentsInWorkspace: text_index = 5; break;
                case fmDeleteingInstruments: text_index = 6; break;
                case fmPreviewSampleFromSd: break;
                case fmLoadingProjectfromWorkspace: text_index = 8; break;
                case fmLoadingProjectFromProjects: text_index = 9; break;
                case fmSavingProjectToWorkspace: break;
                case fmSavingProjectToProjects: text_index = 11; break;
                case fmLoadingPatternFromWorkspace: break;
                case fmExportingSoundSong: text_index = 13; break;
                case fmExportingSoundSongStems: text_index = 14; break;
                case fmExportingSoundPattern: text_index = 15; break;
                case fmExportingSoundPatternStems: text_index = 16; break;
                case fmExportingSoundRenderSelection: text_index = 17; break;
                case fmExportingProjectSamples: text_index = 24; break;
                case fmSavingRecordedSound: text_index = 18; break;
                case fmSavingImportingRecordedSound: text_index = 19; break;
                case fmImportingSampleFromSampleEditor: text_index = 20; break;
                case fmImportingMod: text_index = 21; break;
                case fmExportingMod: text_index = 22; break;
                case fmReloadingSamples: text_index = 23; break;
                case fmBrowsingGames: text_index = 24; break;
                default: text_index = 0; break;
            }

            if (fileManagerPopupState == 0 ||
                (text_index >= 0 && fileManagerPopupState != text_index)) // pokaz popup
            {
                if (text_index >= 0)
                {
                    fileManagerPopupState = text_index;
                    fileManagerLastProgress = 0;
                    noProgressChangeCounter = 0;
                    mtPopups.showProgressPopup(&fileManagerPopupText[text_index][0]);
                    mtPopups.changePopupProgress(0);

                    display.processDisplayUpdateImmediate();
                }
            }
            else // przetwarzaj popup - progress
            {
                if (screenRefreshTimer > 50)
                {
                    screenRefreshTimer = 0;

                    processFileManagerPopupProgress(managerStatus);
                }
            }
        }
        else // zniknij popup
        {
            if (fileManagerPopupState > 0)
            {
                mtPopups.hideProgressPopup();
                fileManagerPopupState = 0;
                fileManagerLastProgress = 0;
                noProgressChangeCounter = 0;
            }
        }
    }
}

void Interface::processFileManagerPopupProgress(uint8_t status)
{

    int8_t progress = -1;

    switch (status)
    {
        case fmBrowsingSamples: break;
        case fmBrowsingFirmwares: break;
        case fmImportingSamplesToWorkspace: progress = newFileManager.getProgress(); break;
        case fmCopyingInstrumentsInWorkspace: break;
        case fmDeleteingInstruments: break;
        case fmPreviewSampleFromSd: break;
        case fmLoadingProjectfromWorkspace: break;
        case fmLoadingProjectFromProjects: progress = newFileManager.getProgress(); break;
        case fmSavingProjectToWorkspace: break;
        case fmSavingProjectToProjects: progress = newFileManager.getProgress(); break;
        case fmLoadingPatternFromWorkspace: break;
        case fmExportingSoundSong:
        case fmExportingSoundSongStems:
        case fmExportingSoundPattern:
        case fmExportingSoundPatternStems:
        case fmExportingSoundRenderSelection:
            progress = clamp<int>(exporter.getProgress(), 0, 100);
            break;
        case fmExportingProjectSamples: progress = newFileManager.getProgress(); break;
        case fmSavingRecordedSound: progress = newFileManager.getProgress(); break;
        case fmSavingImportingRecordedSound: progress = newFileManager.getProgress(); break;
        case fmImportingSampleFromSampleEditor: break;
        case fmImportingMod: progress = newFileManager.importMod_getProgress(); break;
        case fmExportingMod: progress = newFileManager.exportItFile_getProgress(); break;
        default: break;
    }

    // pchanie progressu kiedy nie zmienia sie przez dluzszy czas
    if (fileManagerLastProgress == progress && fileManagerLastProgress < 100)
    {
        noProgressChangeCounter++;

        if (noProgressChangeCounter > 5)
        {
            progress = fileManagerLastProgress + 1;
        }
    }

    if (progress >= 0 && fileManagerLastProgress < progress)
    {
        noProgressChangeCounter = 0;
        fileManagerLastProgress = progress;
        mtPopups.changePopupProgress(progress);
    }
}

void Interface::showLowBatteryPopup()
{
    // debugLog.addLine("Battery is low");
    mtPopups.show(4, "Battery is low, please connect device to the power supply");
}

void Interface::showChargingBatteryPopup()
{
    // debugLog.addLine("Battery is charging");
    mtPopups.show(4, "Battery is charging");
}

void Interface::showEmptyBatteryPopup()
{
    // debugLog.addLine("Battery is empty");
    // mtPopups.show(4, "battery is dead");
}

void Interface::changeMasterVolume(int value)
{
    changeVolume(value);
    mtPopups.showMasterVolumePopup();
}
