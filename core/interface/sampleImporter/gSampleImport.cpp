#include "ampLogarythmicValues.h"
#include "core/interfacePopups.h"
#include "math.h"
#include "sampleImporter/sampleImporter.h"
#include "util_string.h"
#include <string.h>

static const uint16_t framesPlaces[3][4] = {
    { 0 + 1, 31, (800 / 8) * 3 - 3, 391 },
    { (800 / 8) * 3 + 1, 31, (800 / 8) * 3 - 3, 391 },
    { (800 / 8) * 6 + 1, 31, (800 / 8) * 1 - 3, 391 },
};

void cSampleImporter::initDisplayControls()
{
    strControlProperties prop;
    frameData.placesCount = 3;
    frameData.startPlace = 0;
    frameData.places[0] = &framesPlaces[0][0];
    frameData.places[1] = &framesPlaces[1][0];
    frameData.places[2] = &framesPlaces[2][0];
    prop.style = 0;
    prop.value = 0;
    prop.data = &frameData;
    frameControl = display.createControl<cFrame>(&prop);

    strControlProperties prop2;
    prop2.value = 1;
    prop2.colors = display.getTheme().activeLabelsColors;
    prop2.style = (controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
    prop2.x = (800 / 8);
    prop2.y = 424;
    prop2.w = 800 / 4 - 6;
    prop2.h = 55;

    for (uint8_t i = 0; i < 8; i++)
    {
        prop2.x = (800 / 8) * i + (800 / 16);
        prop2.w = 800 / 8 - 6;

        label[i] = display.createControl<cLabel>(&prop2);
    }

    prop2.text = nullptr;
    prop2.colors = nullptr;
    prop2.value = 255; // 127
    prop2.style = controlStyleNoTransparency | controlStyleShow;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    bgLabel = display.createControl<cBgLabel>(&prop2);

    prop2.value = 0;
    prop2.colors = display.getTheme().topBarLabelColors;
    prop2.style = (controlStyleShow | controlStyleBackground | controlStyleCenterY |
                   controlStyleCenterX | controlStyleFont4);
    prop2.x = (800 / 16) * 3 - 1;
    prop2.w = (800 / 8) * 3 - 3;
    prop2.y = 32;
    prop2.h = 26;
    explorerLabelControl = display.createControl<cLabel>(&prop2);

    prop2.value = 0;
    prop2.colors = display.getTheme().topBarLabelColors;
    prop2.style = (controlStyleShow | controlStyleBackground | controlStyleCenterY |
                   controlStyleCenterX | controlStyleFont4);
    prop2.x = (800 / 16) * 9 - 1;
    instrumentLabelControl = display.createControl<cLabel>(&prop2);

    explorerList.start = selectedFile;
    explorerList.length = locationExplorerCount;
    explorerList.linesCount = 13;
    explorerList.data = explorerNames;
    explorerList.lineWidthMax = (800 / 8) * 3 - 30;
    explorerList.lineTextLenMax = 32;
    prop.x = 0 + 1;
    prop.y = 29 + 32;
    prop.w = (800 / 8) * 3 - 3;
    prop.h = 394 - 32;
    prop.style = controlStyleBackground |
                 (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop.data = &explorerList;
    explorerListControl = display.createControl<cList>(&prop);

    instrumentList.linesCount = 13;
    instrumentList.start = 0;
    instrumentList.length = 0;
    instrumentList.lineWidthMax = (800 / 8) * 3 - 30;
    instrumentList.lineTextLenMax = 34;
    prop.x = (800 / 8) * 3 + 1;
    prop.w = (800 / 8) * 3 - 3;
    prop.data = &instrumentList;
    instrumentListControl = display.createControl<cList>(&prop);

    prop.x = (800 / 8) * 7 + 1;
    prop.w = 800 / 8 - 3;
    prop.y = 29;
    prop.h = 394;
    prop.style = controlStyleCompareTwoValues | controlStyleBackground |
                 (display.getTheme().barShadow > 0 ? controlStyleGradient : 0) |
                 (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop.data = &memoryUsageAdd;
    memoryBarControl = display.createControl<cBar>(&prop);

    strControlProperties previewVolumeProperties;
    previewVolumeProperties.x = (800 / 8) * 6 + 1;
    previewVolumeProperties.w = 800 / 8 - 3;
    previewVolumeProperties.y = 29;
    previewVolumeProperties.h = 394;
    previewVolumeProperties.style =
      controlStyleValue_0_100 | controlStyleBackground |
      (display.getTheme().barShadow > 0 ? controlStyleGradient : 0) |
      (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    previewVolumeBarControl = display.createControl<cBar>(&previewVolumeProperties);

    points.pointsType = 0;
    points.endPoint = MAX_16BIT;
    points.startPoint = 0;
    spectrum[0].init();
    spectrum[1].init();
    memset(selectionTab, 0, sizeof(selectionTab));

    strControlProperties prop5;
    prop.x = 100;
    prop.y = 75 + ((300 - (MAX_16BIT / 300)) / 2);
    prop.w = 600;
    prop.h = MAX_16BIT / 300;
    progressCursor = display.createControl<cProgressCursor>(&prop);
    prop.data = &points;
    pointsControl = display.createControl<cPoints>(&prop);

    prop.x = 100;
    prop.y = 75 + ((300 - ((MAX_16BIT / 300))) / 2);
    prop.w = 600;
    prop.h = (MAX_16BIT / 300) / 2;
    prop.data = &spectrum[0];
    spectrumControl[0] = display.createControl<cSpectrum>(&prop);

    prop.x = 100;
    prop.y = 75 + ((300 - ((MAX_16BIT / 300))) / 2) + (MAX_16BIT / 300) / 2;
    prop.w = 600;
    prop.h = (MAX_16BIT / 300) / 2;
    prop.data = &spectrum[1];
    spectrumControl[1] = display.createControl<cSpectrum>(&prop);
}

void cSampleImporter::showDefaultScreen()
{
    titleBar->show();
    titleBar->setTitle("Program Loader"); // MPDJ: loads every program type

    display.setControlText(label[0], ""); // Micro SD
    display.setControlText(label[1], "Add next");
    display.setControlText(label[2], "Preview");
    display.setControlText(label[3], "Rename");
    display.setControlText(label[4], "Import"); // Preview
    display.setControlText(label[5], "Delete");
    display.setControlText2(label[5], "unused");
    display.setControlText(label[6], "Preview Vol");
    display.setControlText2(label[6], "");

    display.setControlText(label[7], "Memory");

    display.setControlText2(label[0], "");
    display.setControlText2(label[1], "");
    display.setControlText2(label[2], "");

    display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
    display.setControlColors(label[1], display.getTheme().activeButtonLabelsColors);
    display.setControlColors(label[2], display.getTheme().activeButtonLabelsColors);
    display.setControlColors(label[3], display.getTheme().activeButtonLabelsColors);
    display.setControlColors(label[4], display.getTheme().activeButtonLabelsColors);
    display.setControlColors(label[5], display.getTheme().activeButtonLabelsColors);
    display.setControlColors(label[6], display.getTheme().activeLabelsColors);
    display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 1);
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        display.setControlStyle2(label[i],
                                 controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }
    display.setControlStyle2(label[5],
                             controlStyleCenterX | controlStyleCenterY | controlStyleFont3);

    display.setControlValue(label[0], 0);
    display.setControlValue(label[1], 0);
    display.setControlValue(label[2], 0);
    display.setControlValue(label[3], 0);
    display.setControlValue(label[4], 0);

    updateMemoryButtonLabel();

    display.setControlValue(bgLabel, 255); // 0b11111111
    display.refreshControl(bgLabel);

    display.setControlText(explorerLabelControl, midiBrowse ? "MIDI" : "SD Card");
    display.setControlShow(explorerLabelControl);
    display.refreshControl(explorerLabelControl);
    display.setControlText(instrumentLabelControl, "Programs");
    display.setControlShow(instrumentLabelControl);
    display.refreshControl(instrumentLabelControl);

    keyboardManager.deactivateKeyboard();

    display.setControlShow(previewVolumeBarControl);
    display.refreshControl(previewVolumeBarControl);

    display.setControlHide(pointsControl);
    display.setControlHide(spectrumControl[0]);
    display.setControlHide(spectrumControl[1]);
    display.setControlHide(progressCursor);

    showPreviewVolume();
    // displayDelete(selectedPlace);

    if (locationExplorerCount != 0)
    {

        refreshActionButtons();
    }

    activateLabelsBorder();

    display.synchronizeRefresh();
}

//==============================================================================================================
void cSampleImporter::showFilesTree()
{
    explorerList.start = selectedFile;
    explorerList.length = locationExplorerCount;
    explorerList.linesCount = 13;
    explorerList.data = explorerNames;

    display.setControlData(explorerListControl, &explorerList);
    display.setControlShow(explorerListControl);
    display.refreshControl(explorerListControl);
    display.synchronizeRefresh();
}

void cSampleImporter::showInstrumentsList()
{
    instrumentList.start = selectedSlot;
    instrumentList.length = kSampleInstrumentsCount;
    instrumentList.linesCount = 13;
    instrumentList.data = interfaceGlobals.ptrIntrumentsNames;

    display.setControlData(instrumentListControl, &instrumentList);
    display.setControlShow(instrumentListControl);
    display.refreshControl(instrumentListControl);
}

void cSampleImporter::refreshInstrumentsList()
{
    display.setControlValue(instrumentListControl, selectedSlot);
    display.setControlShow(instrumentListControl);
    display.refreshControl(instrumentListControl);
}

void cSampleImporter::showPreviewVolume()
{
    display.setControlValue(previewVolumeBarControl, mtConfig.general.previewVolume);
    display.refreshControl(previewVolumeBarControl);

    float masterVolumeInDecybels =
      20 * log10f((float)mixAmpLogValues[mtConfig.general.previewVolume]);

    sprintf(previewVolumeText, "%0.2f dB", masterVolumeInDecybels);

    display.setControlText2(label[6], previewVolumeText);
    display.refreshControl(label[6]);
}

void cSampleImporter::showMemoryUsage()
{
    if (!fullMemoryFlag)
    {
        display.setControlColors(memoryBarControl, display.getTheme().sampleLoadMemBarNormalColors);
        display.setControlValue(memoryBarControl, memoryUsage);
        display.setControlData(memoryBarControl, &memoryUsageAdd);
        display.setControlShow(memoryBarControl);
        display.refreshControl(memoryBarControl);

        sprintf(memoryUsageValue, "%d%c", memoryUsage, '%');
        display.setControlText2(label[7], memoryUsageValue);
        display.refreshControl(label[7]);
    }
    else
    {
        display.setControlColors(memoryBarControl, display.getTheme().sampleLoadMemBarFullColors);
        display.setControlValue(memoryBarControl, 100);

        display.setControlData(memoryBarControl, (strBarData*)&memoryUsageAddOnMemoryFull);
        display.setControlShow(memoryBarControl);
        display.refreshControl(memoryBarControl);

        display.setControlText2(label[7], "100%");
        display.refreshControl(label[7]);
    }
}

void cSampleImporter::showStopPatternPopup()
{
    mtPopups.showInfoPopup("This action will stop the pattern.", "Do you want to continue?");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                (controlStyleCenterX | controlStyleCenterY | controlStyleFont3));
        display.setControlStyle2(label[i],
                                 (controlStyleCenterX | controlStyleCenterY | controlStyleFont2));
        display.setControlPosition(label[i], (800 / 8) * i + (800 / 16), 424);
        display.setControlSize(label[i], 800 / 8 - 6, 55);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlValue(label[6], 0);
    display.setControlValue(label[7], 0);

    display.setControlText(label[6], "Cancel");
    display.setControlShow(label[6]);
    display.setControlText(label[7], "Yes");
    display.setControlShow(label[7]);
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255);
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}

void cSampleImporter::hideStopPatternPopup()
{
    mtPopups.hideInfoPopup();

    showDefaultScreen();
    showMemoryUsage();

    display.synchronizeRefresh();
}

void cSampleImporter::hidePopupButtons()
{
    display.setControlHide(label[6]);
    display.setControlHide(label[7]);

    display.setControlValue(bgLabel, 255); // 0
    display.refreshControl(bgLabel);
}

//==============================================================================================================
void cSampleImporter::resizeWaveforms()
{
    assert(lowerData.size() == upperData.size());

    if (lowerData.size() == 2)
    {
        spectrum[0].pointsLimit = strTrackerSpectrum::k200;
        spectrum[1].pointsLimit = strTrackerSpectrum::k200;

        display.setControlSize(spectrumControl[0], 600, (MAX_16BIT / 300) / 2);

        display.setControlShow(spectrumControl[0]);
        display.setControlShow(spectrumControl[1]);

        display.refreshControl(spectrumControl[0]);
        display.refreshControl(spectrumControl[1]);
    }
    else
    {
        assert(lowerData.size() == 1);
        spectrum[0].pointsLimit = strTrackerSpectrum::k300;
        spectrum[1].pointsLimit = strTrackerSpectrum::k300;

        display.setControlSize(spectrumControl[0], 600, (MAX_16BIT / 300));

        display.setControlShow(spectrumControl[0]);
        display.setControlHide(spectrumControl[1]);

        display.refreshControl(spectrumControl[0]);
        display.refreshControl(spectrumControl[1]);
    }
}

void cSampleImporter::activateLabelsBorder()
{
    frameSelectMode(selectedPlace, 1);

    if (selectedPlace > frameData.placesCount - 1)
    {
        return;
    }

    uint32_t* explorerListColors = display.getTheme().sampleLoaderInactiveListColors;
    uint32_t* intrumentListColors = display.getTheme().sampleLoaderInactiveListColors;

    if (selectedPlace == 0)
    {
        explorerListColors = display.getTheme().sampleLoaderActiveListColors;
    }
    else if (selectedPlace == 1)
    {
        intrumentListColors = display.getTheme().sampleLoaderActiveListColors;
    }

    display.setControlColors(explorerListControl, explorerListColors);
    display.setControlColors(instrumentListControl, intrumentListColors);
    display.refreshControl(explorerListControl);
    display.refreshControl(instrumentListControl);

    explorerHeaderColors[0] = explorerListColors[4];
    explorerHeaderColors[1] = explorerListColors[3];
    explorerHeaderColors[2] = explorerListColors[0];
    explorerHeaderColors[3] = explorerListColors[5];

    instrumentHeaderColors[0] = intrumentListColors[4];
    instrumentHeaderColors[1] = intrumentListColors[3];
    instrumentHeaderColors[2] = intrumentListColors[0];
    instrumentHeaderColors[3] = intrumentListColors[5];

    display.setControlColors(explorerLabelControl, explorerHeaderColors);
    display.setControlColors(instrumentLabelControl, instrumentHeaderColors);
    display.refreshControl(explorerLabelControl);
    display.refreshControl(instrumentLabelControl);

    display.setControlValue(frameControl, selectedPlace);
    display.setControlShow(frameControl);
    display.refreshControl(frameControl);
}

//==============================================================================================================
void cSampleImporter::showActualInstrument()
{
    static char actualInstrName[SAMPLE_NAME_SIZE + 4];

    uint8_t i = mtProject.values.lastUsedInstrument;

    if (isMidiInstrument(i)) // MPDJ: slot types, numbered 1-64
    {
        sprintf(actualInstrName, "%d. MIDI Channel %d", i + 1, instrumentIdxToMidiChannel(i));
    }
    else if (i < kSampleInstrumentsCount)
    {
        sprintf(actualInstrName, "%d.", i + 1);
        strncat(actualInstrName, mtProject.instrument[i].sample.file_name, 25);
    }
    else
    {
        actualInstrName[0] = 0;
    }

    display.setControlText(instrumentLabel, actualInstrName);
    display.refreshControl(instrumentLabel);
}

//==============================================================================================================
void cSampleImporter::showRename()
{
    if (selectedPlace == 1)
    {
        display.setControlText(label[3], "Enter");
    }
    else
    {
        display.setControlText(label[3], "Add");
    }
}

void cSampleImporter::showAddOrEnter()
{
    if (explorerNames != nullptr && *explorerNames[selectedFile] == '/')
    {
        display.setControlText(label[0], "Enter");

        if (selectedPlace == 0)
        {
            display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
        }
        else
        {
            display.setControlColors(label[0], display.getTheme().inactiveButtonLabelsColors);
        }
    }
    else
    {
        display.setControlText(label[0], "Add");
        display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
    }

    display.refreshControl(label[0]);
}

void cSampleImporter::addNextColorControl()
{
    if (explorerNames != nullptr && *explorerNames[selectedFile] != '/')
    {
        display.setControlColors(label[1], display.getTheme().activeButtonLabelsColors);
    }
    else
    {
        display.setControlColors(label[1], display.getTheme().inactiveButtonLabelsColors);
    }

    display.refreshControl(label[1]);
}

void cSampleImporter::previewColorControl()
{
    const uint32_t* colors = display.getTheme().activeButtonLabelsColors;

    if (selectedPlace == 0)
    {
        if (explorerNames != nullptr && *explorerNames[selectedFile] == '/')
        {
            colors = display.getTheme().inactiveButtonLabelsColors;
        }
    }
    else
    {
        if (mtProject.instrument[selectedSlot].isActive != 1)
        {
            colors = display.getTheme().inactiveButtonLabelsColors;
        }
    }

    display.setControlColors(label[2], colors);
    display.refreshControl(label[2]);
}

void cSampleImporter::renameColorControl()
{
    const uint32_t* colors = display.getTheme().inactiveButtonLabelsColors;

    if (selectedPlace == 1)
    {
        if (mtProject.instrument[selectedSlot].isActive == 1)
        {
            colors = display.getTheme().activeButtonLabelsColors;
        }
    }

    display.setControlColors(label[3], colors);
    display.refreshControl(label[3]);
}

void cSampleImporter::deleteUnusedColorControl()
{
    const uint32_t* colors = display.getTheme().inactiveButtonLabelsColors;

    if (selectedPlace == 1)
    {
        colors = display.getTheme().activeButtonLabelsColors;
    }

    display.setControlColors(label[5], colors);
    display.refreshControl(label[5]);
}

void cSampleImporter::deleteColorControl()
{
    const uint32_t* colors = display.getTheme().activeButtonLabelsColors;

    if (selectedPlace == 1)
    {
        if ((selectionLength[listInstruments] > 1) && (selectedPlace == 1))
        {
            if (checkIfAnyInstrActive() == 0)
            {
                colors = display.getTheme().inactiveButtonLabelsColors;
            }
        }
        else
        {
            if (mtProject.instrument[selectedSlot].isActive != 1)
            {
                colors = display.getTheme().inactiveButtonLabelsColors;
            }
        }
    }

    display.setControlColors(label[5], colors);
    display.synchronizeRefresh();
}

void cSampleImporter::importColorControl()
{
    const uint32_t* colors = display.getTheme().activeButtonLabelsColors;

    if (explorerNames != nullptr && *explorerNames[selectedFile] == '/')
    {
        colors = display.getTheme().inactiveButtonLabelsColors;
    }
    else if (getSelectionEnd(listFiles) - getSelectionStart(listFiles) > 0)
    {
        colors = display.getTheme().inactiveButtonLabelsColors;
    }
    else
    {
        char* fileName = explorerNames[selectedFile];
        if (hasFileExtension(fileName, "pti"))
        {
            colors = display.getTheme().inactiveButtonLabelsColors;
        }
    }

    display.setControlColors(label[4], colors);
    display.refreshControl(label[4]);
}

void cSampleImporter::showFileList()
{
    display.setControlShow(explorerListControl);
    display.refreshControl(explorerListControl);
}

void cSampleImporter::rewindListToBeggining()
{
    selectedFile = 0;
    display.setControlValue(explorerListControl, selectedFile);
    display.refreshControl(explorerListControl);
}

void cSampleImporter::moveInstrListToEnd()
{
    selectedSlot = kSampleInstrumentsCount - 1;
    display.setControlValue(instrumentListControl, selectedSlot);
    display.refreshControl(instrumentListControl);
}

void cSampleImporter::setSelect()
{
    explorerList.selectTab = &selectionTab[listFiles][0];
    instrumentList.selectTab = &selectionTab[listInstruments][0];
}

void cSampleImporter::frameSelectMode(uint8_t place, uint8_t mode)
{
    if (place == 0)
    {
        explorerList.selectionActive = mode;
        display.refreshControl(explorerListControl);

        instrumentList.selectionActive = 0;
        display.refreshControl(instrumentListControl);
    }
    else if (place == 1)
    {
        instrumentList.selectionActive = mode;
        display.refreshControl(instrumentListControl);

        explorerList.selectionActive = 0;
        display.refreshControl(explorerListControl);
    }
}

void cSampleImporter::showRenameKeyboard()
{
    display.setControlPosition(label[7], (800 / 8) * 7 + (800 / 16), -1);
    display.setControlSize(label[7], 800 / 4 - 6, -1);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
        display.setRemoveControlStyle(label[i], controlStyleShowBitmap);
        display.setControlColors(label[i], display.getTheme().activeButtonLabelsColors);
    }

    display.setControlText(label[0], "Enter");
    display.setControlText(label[1], "Auto name");
    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Rename");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }

    display.setControlHide(explorerLabelControl);
    display.setControlHide(instrumentLabelControl);

    display.setControlHide(explorerListControl);
    display.refreshControl(explorerListControl);
    display.setControlHide(instrumentListControl);
    display.refreshControl(instrumentListControl);
    display.setControlHide(memoryBarControl);
    display.refreshControl(memoryBarControl);
    display.setControlHide(previewVolumeBarControl);
    display.refreshControl(previewVolumeBarControl);
    display.setControlHide(frameControl);
    display.refreshControl(frameControl);

    display.setControlValue(bgLabel, 255); // 226
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}

void cSampleImporter::showInfoPopupDeleteInstrument()
{
    mtPopups.showInfoPopup(firstLineDeleteInfoPopup, "Do you want to continue?");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                (controlStyleCenterX | controlStyleCenterY | controlStyleFont3));
        display.setControlStyle2(label[i],
                                 (controlStyleCenterX | controlStyleCenterY | controlStyleFont2));
        display.setControlPosition(label[i], (800 / 8) * i + (800 / 16), 424);
        display.setControlSize(label[i], 800 / 8 - 6, 55);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlValue(label[6], 0);
    display.setControlValue(label[7], 0);
    display.setControlText(label[6], "Cancel");
    display.setControlShow(label[6]);
    display.setControlText(label[7], "Yes");
    display.setControlShow(label[7]);
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255);
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}

void cSampleImporter::showInfoPopupReplaceInstrument()
{
    mtPopups.showInfoPopup(firstLineReplaceInfoPopup, "Do you want to continue?");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                (controlStyleCenterX | controlStyleCenterY | controlStyleFont3));
        display.setControlStyle2(label[i],
                                 (controlStyleCenterX | controlStyleCenterY | controlStyleFont2));
        display.setControlPosition(label[i], (800 / 8) * i + (800 / 16), 424);
        display.setControlSize(label[i], 800 / 8 - 6, 55);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlValue(label[6], 0);
    display.setControlValue(label[7], 0);
    display.setControlText(label[6], "Cancel");
    display.setControlShow(label[6]);
    display.setControlText(label[7], "Yes");
    display.setControlShow(label[7]);
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 0b11000000
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}

void cSampleImporter::hideInfoPopup()
{
    mtPopups.hideInfoPopup();
    showDefaultScreen();
    showMemoryUsage();

    display.synchronizeRefresh();
}

//==============================================================================================================

void cSampleImporter::showImportScreen()
{

    display.setControlHide(explorerListControl);
    display.setControlHide(explorerListControl);
    display.setControlHide(frameControl);
    display.setControlPosition(label[7], (800 / 8) * 7 + (800 / 16), -1);
    display.setControlSize(label[7], 800 / 4 - 6, -1);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 1);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
        display.setControlStyle(label[i],
                                controlStyleShow | controlStyleCenterX | controlStyleCenterY |
                                  controlStyleFont3);
        display.setControlStyle2(label[i],
                                 controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        display.setControlColors(label[i], display.getTheme().activeButtonLabelsColors);
        display.refreshControl(label[i]);
    }

    display.setControlStyle2(label[0],
                             controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
    display.setControlStyle2(label[1],
                             controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
    display.setControlStyle2(label[2],
                             controlStyleCenterX | controlStyleCenterY | controlStyleFont2);

    display.setControlValue(label[0], 0);
    display.setControlValue(label[5], 0);

    display.setControlText(label[0], "Preview");
    display.setControlText(label[1], "Start");
    display.setControlText(label[2], "End");
    display.setControlText(label[4], "Import");
    display.setControlText2(label[4], "Stream");
    display.setControlText(label[5], "Cancel");
    display.setControlText(label[6], "Import");
    display.setControlText2(label[6], "Low quality");
    display.setControlText(label[7], "Import");
    display.setControlText2(label[7], "Normal");

    display.setControlHide(explorerLabelControl);
    display.setControlHide(instrumentLabelControl);
    display.setControlHide(explorerListControl);
    display.setControlHide(instrumentListControl);
    display.setControlHide(memoryBarControl);
    display.setControlHide(previewVolumeBarControl);
    display.setControlHide(frameControl);

    display.setControlValue(bgLabel, 255); // 0b11101111
    display.refreshControl(bgLabel);

    display.setControlShow(pointsControl);
    resizeWaveforms();
    // display.setControlShow(progressCursor);

    display.refreshControl(previewVolumeBarControl);
    display.refreshControl(pointsControl);
    display.refreshControl(spectrumControl[0]);
    display.refreshControl(spectrumControl[1]);
    // display.refreshControl(progressCursor);

    importApplyNormalButtonLastStyle = 255;
    importApplyLoFiButtonLastStyle = 255;
    importApplyStreamButtonLastStyle = 255;
    showImportApplyButtonStyle();

    hideImportPreview();
    showImportStartPointValue();
    showImportEndPointValue();
    // showImportPreviewValue();
    mtPopups.hideInfoPopup();

    display.synchronizeRefresh();
}

void cSampleImporter::hideImportScreen()
{

    showDefaultScreen();
}

void cSampleImporter::showImportStartPointValue()
{
    float recTimeValue = importSampleSamplesCount / 44100.0;
    float localStartPoint = (recTimeValue * importStartPoint) / MAX_16BIT;

    sprintf(startPointValueText, "%.3fs", localStartPoint);

    display.setControlText2(label[1], startPointValueText);
    display.setControlShow(label[1]);
    display.refreshControl(label[1]);
    // display.setClearRefreshFlag(label[1]);
}

void cSampleImporter::showImportEndPointValue()
{
    float recTimeValue = importSampleSamplesCount / 44100.0;
    float localEndPoint = (recTimeValue * importEndPoint) / MAX_16BIT;

    sprintf(endPointValueText, "%.3fs", localEndPoint);

    display.setControlText2(label[2], endPointValueText);
    display.setControlShow(label[2]);
    display.refreshControl(label[2]);
    // display.setClearRefreshFlag(label[2]);
}

void cSampleImporter::showImportPreviewValue()
{
    float playTimeValue =
      ((playProgressValue * ((float)importSampleSamplesCount / MAX_16BIT)) / 44100.0);
    float localEndPoint =
      ((importSampleSamplesCount * ((float)importPreviewEndPoint / MAX_16BIT)) / 44100.0);
    if (playTimeValue >= (localEndPoint - 0.01))
        playTimeValue = localEndPoint;

    sprintf(playTimeValueText, "%.3fs", playTimeValue);

    display.setControlText2(label[0], playTimeValueText);
    display.setControlShow(label[0]);
    display.refreshControl(label[0]);
    // display.setClearRefreshFlag(label[0]);
}

void cSampleImporter::hideImportPreview()
{
    display.setControlText2(label[0], "");
    display.refreshControl(label[0]);

    display.setControlHide(progressCursor);
    display.refreshControl(progressCursor);
}

void cSampleImporter::showImportApplyButtonStyle()
{
    if (calcImportIsEnoughMemoryNormal())
    {
        if (importApplyNormalButtonLastStyle != 1)
        {
            display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
            display.refreshControl(label[7]);
            importApplyNormalButtonLastStyle = 1;
        }
    }
    else
    {
        if (importApplyNormalButtonLastStyle != 0)
        {
            display.setControlColors(label[7], display.getTheme().inactiveButtonLabelsColors);
            display.refreshControl(label[7]);
            importApplyNormalButtonLastStyle = 0;
        }
    }

    if (calcImportIsEnoughMemoryLoFi())
    {
        if (importApplyLoFiButtonLastStyle != 1)
        {
            display.setControlColors(label[6], display.getTheme().activeButtonLabelsColors);
            display.refreshControl(label[6]);
            importApplyLoFiButtonLastStyle = 1;
        }
    }
    else
    {
        if (importApplyLoFiButtonLastStyle != 0)
        {
            display.setControlColors(label[6], display.getTheme().inactiveButtonLabelsColors);
            display.refreshControl(label[6]);
            importApplyLoFiButtonLastStyle = 0;
        }
    }

    if (calcImportIsEnoughMemoryStream())
    {
        if (importApplyStreamButtonLastStyle != 1)
        {
            display.setControlColors(label[4], display.getTheme().activeButtonLabelsColors);
            display.refreshControl(label[4]);
            importApplyStreamButtonLastStyle = 1;
        }
    }
    else
    {
        if (importApplyStreamButtonLastStyle != 0)
        {
            display.setControlColors(label[4], display.getTheme().inactiveButtonLabelsColors);
            display.refreshControl(label[4]);
            importApplyStreamButtonLastStyle = 0;
        }
    }
}

void cSampleImporter::updateMemoryButtonLabel()
{
    if (selectedPlace == 1 && selectedSlot < kSampleInstrumentsCount &&
        mtProject.instrument[selectedSlot].isActive)
    {
        bool streamed = mtProject.sampleMemory[selectedSlot].isStreamed;
        bool canStream = (mtProject.instrument[selectedSlot].sample.length > kStreamPreCacheFrames) &&
                         isPlayModeStreamable(mtProject.instrument[selectedSlot].playMode);
        display.setControlText(label[7], streamed ? "Mode: [S]" : "Mode: RAM");
        display.setControlColors(
          label[7],
          (streamed || canStream) ? display.getTheme().activeButtonLabelsColors
                                  : display.getTheme().inactiveButtonLabelsColors);
    }
    else
    {
        const char* modeText = "Mem: RAM";
        if (sampleLoadMode == enSampleLoadMode::kAuto)
        {
            modeText = "Mem: Auto";
        }
        else if (sampleLoadMode == enSampleLoadMode::kStream)
        {
            modeText = "Mem: [S]";
        }
        display.setControlText(label[7], modeText);
        display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
    }
    display.refreshControl(label[7]);
}
