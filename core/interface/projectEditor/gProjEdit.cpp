
#include "projectEditor/projectEditor.h"
#include <mtAudioExport.h>

#include "core/interfacePopups.h"

#include "display.h"
#include "fileManager.h"

constexpr uint32_t coverRamAddres = 670000;

static const uint16_t framesPlaces[2][4] = {
    { 0 + 1, 31, (800 / 8) * 3 - 3, 391 },
    { (800 / 8) * 3 + 1, 31, (800 / 8) * 3 - 3, 391 },
};

void cProjectEditor::initDisplayControls()
{
    strControlProperties prop;
    frameData.placesCount = 2;
    frameData.startPlace = 0;
    frameData.places[0] = &framesPlaces[0][0];
    frameData.places[1] = &framesPlaces[1][0];
    prop.style = 0;
    prop.value = 0;
    prop.data = &frameData;
    frameControl = display.createControl<cFrame>(&prop);

    strControlProperties prop2;
    for (uint8_t i = 0; i < 8; i++)
    {
        prop2.value = 1;
        prop2.colors = display.getTheme().activeButtonLabelsColors;
        prop2.style = controlStyleCenterX | controlStyleCenterY | controlStyleFont3;
        prop2.x = (800 / 8) * i + (800 / 16);
        prop2.w = 800 / 8 - 6;
        prop2.y = 424;
        prop2.h = 55;
        label[i] = display.createControl<cLabel>(&prop2);
    }

    prop2.text = nullptr;
    prop2.colors = nullptr;
    prop2.value = 255;
    prop2.style = controlStyleNoTransparency | controlStyleShow;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    bgLabel = display.createControl<cBgLabel>(&prop2);

    prop2.value = 0;
    prop2.colors = display.getTheme().topBarLabelColors;
    prop2.style =
      controlStyleBackground | controlStyleCenterY | controlStyleCenterX | controlStyleFont4;
    prop2.x = (800 / 16) * 3;
    prop2.w = (800 / 8) * 3 - 6;
    prop2.y = 32;
    prop2.h = 26;
    browserLabelControl = display.createControl<cLabel>(&prop2);

    prop2.value = 0;
    prop2.colors = display.getTheme().topBarLabelColors;
    prop2.style =
      controlStyleBackground | controlStyleCenterY | controlStyleCenterX | controlStyleFont4;
    prop2.x = (800 / 16) * 9;
    recentLabelControl = display.createControl<cLabel>(&prop2);

    projectList.linesCount = 13;
    projectList.start = 0;
    projectList.length = 0;
    prop.x = 1;
    prop.y = 29 + 32;
    prop.w = (800 / 8 * 3) - 3;
    prop.h = 394 - 32;
    prop.value = 0;
    prop.style = controlStyleBackground |
                 (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop.data = &projectList;
    prop.colors = display.getTheme().projectActiveListColors;
    fileListControl = display.createControl<cList>(&prop);

    recentList.linesCount = 10;
    recentList.start = 0;
    recentList.length = 0;
    prop.x = (800 / 8 * 3) + 1;
    prop.y = 29 + 32;
    prop.w = (800 / 8 * 3) - 3;
    prop.h = 394 - 32;
    prop.style = controlStyleBackground |
                 (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop.data = &recentList;
    prop.colors = display.getTheme().projectActiveListColors;
    recentListControl = display.createControl<cList>(&prop);

    strControlProperties prop8;
    prop8.x = 400;
    prop8.y = 220;
    prop8.value = coverRamAddres;
    prop8.style = (controlStyleCenterX | controlStyleCenterY /*| controlStyleTransformImg*/);
    coverImg = display.createControl<cImg>(&prop8);

    prop8.value = 0;
    prop8.data = nullptr;
    prop8.text = nullptr;
    prop8.colors = display.getTheme().blankSpaceCoverLabelColors;
    prop8.style = controlStyleBackground |
                  (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop8.x = (800 / 8) * 7 + 1;
    prop8.w = 800 / 8 - 2;
    prop8.y = 29;
    prop8.h = 394;
    blankSpaceCover = display.createControl<cLabel>(&prop8);
}

void cProjectEditor::showDefaultScreen()
{
    titleBar->show(true);
    titleBar->setTitle("File");

    static char projNameTitleLabel[28];
    strncpy(projNameTitleLabel, newFileManager.getCurrentProjectName(), 26);
    projNameTitleLabel[27] = 0;
    titleBar->showRightLabel(true);
    titleBar->setRightLabel(projNameTitleLabel);

    // lista
    display.setControlHide(fileListControl);
    display.setControlHide(recentListControl);

    display.setControlValue(label[0], 1);
    display.setControlValue(label[1], 1);
    display.setControlValue(label[2], 1);
    display.setControlValue(label[3], 1);
    display.setControlValue(label[4], 1);
    display.setControlValue(label[5], 1);
    display.setControlValue(label[6], 1);
    display.setControlValue(label[7], 1);

    display.setControlText(label[0], "New");
    display.setControlText(label[1], "Open");
    display.setControlText(label[2], "Import Mod");
    display.setControlText(label[3], "Save");
    display.setControlText(label[4], "Save As");
    display.setControlText(label[5], "Export");
#ifdef PLATFORM_MPC
    display.setControlText(label[6], "Save Kit"); // MPDJ Kits
    display.setControlText(label[7], "Restore"); // MPDJ recovery (2026-11-16)
#else
    display.setControlText(label[6], "");
    display.setControlText(label[7], "");
#endif

    display.setControlText2(label[0], "Project");
    display.setControlText2(label[1], "");
    display.setControlText2(label[2], "");
    display.setControlText2(label[3], "");
    display.setControlText2(label[4], "");
    display.setControlText2(label[5], "");
    display.setControlText2(label[6], "");
    display.setControlText2(label[7], "");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlColors(label[i], display.getTheme().activeButtonLabelsColors);
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3);
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlValue(label[0], 1);

    display.setControlHide(browserLabelControl);
    display.setControlHide(recentLabelControl);

    display.setControlValue(bgLabel, 255);
    display.refreshControl(bgLabel);

    keyboardManager.deactivateKeyboard();

    mtPopups.hideInfoPopup();

    display.setControlSize(blankSpaceCover, 800, -1);
    display.setControlPosition(blankSpaceCover, 0 + 1, -1);
    display.setControlShow(blankSpaceCover);
    display.refreshControl(blankSpaceCover);

    display.setControlHide(frameControl);

    refreshProjectCover(500);

    display.synchronizeRefresh();
}

//==============================================================================================================
void cProjectEditor::showProjectBrowser(bool reloadList)
{
    if (currentBrowserMode == kOpenProject)
    {
        titleBar->setTitle("File - Open Project");
    }
    else
    {
        titleBar->setTitle("File - Save Project");
    }

    projectList.start = fileBrowserData.browserListPosition;
    projectList.length = fileBrowserData.browserListLength;
    projectList.linesCount = 13;
    projectList.selectionActive = 1;
    projectList.selectTab = (uint8_t*)fileBrowserData.selectionList;
    projectList.data = fileBrowserData.browserList;

    if (reloadList)
    {
        display.setControlData(fileListControl, &projectList);
    }
    display.setControlShow(fileListControl);
    display.refreshControl(fileListControl);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    if (currentBrowserMode == kSaveAsProject)
    {
        display.setControlText(label[7], "Save");
    }
    else
    {
        display.setControlText(label[7], "Open");
    }

    display.setControlText(label[0], "Enter");
    display.setControlText(label[2], "Delete");
    display.setControlText(label[3], "New Folder");
    display.setControlText(label[6], "Cancel");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                                  controlStyleShow);
        display.refreshControl(label[i]);
    }

    if (currentBrowserMode == kOpenProject)
    {
        display.setControlText(recentLabelControl, "Recent");
        display.setControlShow(recentLabelControl);
        display.refreshControl(recentLabelControl);

        display.setControlSize(blankSpaceCover, 200, -1);
        display.setControlPosition(blankSpaceCover, 600 + 1, -1);

        showRecentList();
    }
    else
    {
        display.setControlSize(blankSpaceCover, 500, -1);
        display.setControlPosition(blankSpaceCover, 300 + 1, -1);
    }

    display.setControlShow(blankSpaceCover);
    display.refreshControl(blankSpaceCover);

    display.setControlText(browserLabelControl, "SD Card");
    display.setControlShow(browserLabelControl);
    display.refreshControl(browserLabelControl);

    display.setControlValue(bgLabel, 255); // 0b10011111
    display.refreshControl(bgLabel);

    selectedPlace = 0;
    activateLabelsBorder();
    refreshProjectBrowserLabels();

    keyboardManager.deactivateKeyboard();
    hideProjectCover();

    display.synchronizeRefresh();
}

void cProjectEditor::showRecentList()
{
    display.setControlData(recentListControl, &recentList);
    display.setControlShow(recentListControl);
    display.refreshControl(recentListControl);
}

void cProjectEditor::refreshProjectBrowserLabels()
{
    if (selectedPlace == 1)
    {
        if (recentList.length > 0)
        {
            display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
        }
        else
        {
            display.setControlColors(label[7], display.getTheme().inactiveButtonLabelsColors);
        }

        // display.setControlText(label[0], "Open");
        display.setControlColors(recentListControl, display.getTheme().projectActiveListColors);
        display.setControlColors(fileListControl, display.getTheme().projectInactiveListColors);
        display.refreshControl(recentListControl);
        display.setControlColors(label[3], display.getTheme().inactiveButtonLabelsColors);
        display.setControlColors(label[2], display.getTheme().inactiveButtonLabelsColors);
        display.setControlColors(label[0], display.getTheme().inactiveButtonLabelsColors);
    }
    else
    {
        if (fileBrowser.isPositionOnSelectionList())
        {
            if (currentBrowserMode == kOpenProject)
            {
                display.setControlColors(label[0], display.getTheme().inactiveButtonLabelsColors);
                display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
            }
            else // save
            {
                display.setControlColors(label[0], display.getTheme().inactiveButtonLabelsColors);
                display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
            }
        }
        else
        {
            if (currentBrowserMode == kOpenProject)
            {
                display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
                display.setControlColors(label[7], display.getTheme().inactiveButtonLabelsColors);
            }
            else // save
            {
                display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
                display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
            }
        }

        if (currentBrowserMode == kOpenProject)
        {
            display.setControlColors(recentListControl, display.getTheme().projectInactiveListColors);
            display.refreshControl(recentListControl);
        }

        if (fileBrowserData.browserListPosition == 0 && !fileBrowser.isRoot())
        {
            display.setControlColors(label[2], display.getTheme().inactiveButtonLabelsColors);
        }
        else
        {
            display.setControlColors(label[2], display.getTheme().activeButtonLabelsColors);
        }

        display.setControlColors(label[3], display.getTheme().activeButtonLabelsColors);
        display.setControlColors(fileListControl, display.getTheme().projectActiveListColors);
    }

    display.refreshControl(fileListControl);
    display.refreshControl(label[0]);
    display.refreshControl(label[2]);
    display.refreshControl(label[3]);
    display.refreshControl(label[7]);
}

void cProjectEditor::refreshProjectBrowser()
{
    refreshProjectBrowserLabels();

    display.setControlValue(fileListControl, fileBrowserData.browserListPosition);
    display.refreshControl(fileListControl);
}

void cProjectEditor::showModBrowser(bool reloadList)
{
    titleBar->setTitle("File - Import Mod");

    projectList.start = fileBrowserData.browserListPosition;
    projectList.length = fileBrowserData.browserListLength;
    projectList.linesCount = 13;
    projectList.selectionActive = 1;
    projectList.selectTab = (uint8_t*)fileBrowserData.selectionList;
    projectList.data = fileBrowserData.browserList;

    if (reloadList)
    {
        display.setControlData(fileListControl, &projectList);
    }
    display.setControlColors(fileListControl, display.getTheme().projectActiveListColors);
    display.setControlShow(fileListControl);
    display.refreshControl(fileListControl);

    display.setControlHide(recentListControl);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    // bottom labels
    refreshModBrowserLabels();

    display.setControlText(label[0], "Enter");
    display.setControlText(label[2], "Delete");
    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Import");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                                  controlStyleShow);
        display.refreshControl(label[i]);
    }

    display.setControlText(browserLabelControl, "SD Card");
    display.setControlShow(browserLabelControl);
    display.setControlHide(recentLabelControl);
    display.refreshControl(browserLabelControl);

    display.setControlValue(bgLabel, 255); // 0b10001111
    display.refreshControl(bgLabel);

    display.setControlSize(blankSpaceCover, 500, -1);
    display.setControlPosition(blankSpaceCover, 300 + 1, -1);
    display.setControlShow(blankSpaceCover);
    display.refreshControl(blankSpaceCover);

    selectedPlace = 0;
    display.setControlHide(frameControl);
    hideProjectCover();

    display.synchronizeRefresh();
}

void cProjectEditor::refreshModBrowserLabels()
{
    if (fileBrowser.isPositionOnSelectionList())
    {
        display.setControlColors(label[0], display.getTheme().inactiveButtonLabelsColors);
        display.setControlColors(label[2], display.getTheme().activeButtonLabelsColors);
        display.setControlColors(label[7], display.getTheme().activeButtonLabelsColors);
    }
    else
    {
        display.setControlColors(label[0], display.getTheme().activeButtonLabelsColors);
        display.setControlColors(label[2], display.getTheme().inactiveButtonLabelsColors);
        display.setControlColors(label[7], display.getTheme().inactiveButtonLabelsColors);
    }

    display.refreshControl(label[0]);
    display.refreshControl(label[2]);
    display.refreshControl(label[7]);
}

void cProjectEditor::refreshModBrowser()
{
    refreshModBrowserLabels();

    display.setControlValue(fileListControl, fileBrowserData.browserListPosition);
    display.refreshControl(fileListControl);
}

void cProjectEditor::showKeyboard(const char* confirmButtonText)
{
    mtPopups.hideInfoPopup();

    display.setControlHide(fileListControl);
    display.setControlHide(recentListControl);
    display.setControlHide(browserLabelControl);
    display.setControlHide(recentLabelControl);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlText(label[0], "Enter");
    display.setControlText(label[1], "Auto Name");
    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], confirmButtonText);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                                  controlStyleShow);
        display.setControlColors(label[i], display.getTheme().activeButtonLabelsColors);
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 226
    display.refreshControl(bgLabel);

    hideProjectCover();

    display.setControlHide(blankSpaceCover);
    display.setControlHide(frameControl);

    display.synchronizeRefresh();
}

void cProjectEditor::showSaveLastWindow()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlText(label[5], "Cancel");
    display.setControlText(label[6], "Don't Save");
    display.setControlText(label[7], "Save");
    display.setControlColors(label[5], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[5], controlStyleBackground);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    sprintf(currentInfo, "\"%s\"?", newFileManager.getCurrentProjectName());

    mtPopups.showInfoPopup("Do you want to save the changes to ", currentInfo);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 0b11100000
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}

void cProjectEditor::showDeleteProjectLastWindow(bool deletingProject)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Delete");
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 0b11000000
    display.refreshControl(bgLabel);

    sprintf(currentInfo, "\"%s\"?", fileBrowser.getSelectedFolderName());

    if (deletingProject)
    {
        mtPopups.showInfoPopup("Do you want to delete project:", currentInfo);
    }
    else
    {
        mtPopups.showInfoPopup("Do you want to delete folder:", currentInfo);
    }

    display.synchronizeRefresh();
}
void cProjectEditor::showDeleteModLastWindow()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Delete");
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 0b11000000
    display.refreshControl(bgLabel);

    sprintf(currentInfo, "\"%s\"?", fileBrowser.getSelectedItemName());
    mtPopups.showInfoPopup("Do you want to delete file:", currentInfo);

    display.synchronizeRefresh();
}

void cProjectEditor::showOverwriteWindow()
{
    for (uint8_t i = 0; i < 7; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Overwrite");
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 0b11000000
    display.refreshControl(bgLabel);

    sprintf(currentInfo, "\"%s\"?", keyboardManager.getName());
    mtPopups.showInfoPopup("Do you want overwrite ", currentInfo);

    display.synchronizeRefresh();
}

void cProjectEditor::showStopPatternWindow()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");
    }

    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Yes");
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);
    }

    display.setControlValue(bgLabel, 255); // 0b11000000
    display.refreshControl(bgLabel);

    mtPopups.showInfoPopup("This action will stop the pattern.", "Do you want to continue?");

    display.synchronizeRefresh();
}

void cProjectEditor::showExportWindow()
{
    titleBar->setTitle("File - Export");

    display.setControlText(label[0], "Export");
    display.setControlText2(label[0], "Song");
    display.setControlText(label[1], "Export");
    display.setControlText2(label[1], "Song Stems");
    display.setControlText(label[2], "Export");
    display.setControlText2(label[2], "Pattern");
    display.setControlText(label[3], "Export");
    display.setControlText2(label[3], "Patt Stems");
    display.setControlText(label[4], "Export");
    display.setControlText2(label[4], "As .it");
    display.setControlText(label[5], "Export");
    display.setControlText2(label[5], "Samples");
    display.setControlText(label[6], "");
    display.setControlText(label[7], "Back");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                                  controlStyleShow);
        display.setControlValue(label[i], 1);
        display.refreshControl(label[i]);
    }

    display.setControlValue(label[6], 0);
    display.setControlValue(label[7], 0);

    display.setControlValue(bgLabel, 255);
    display.refreshControl(bgLabel);

    display.setControlHide(frameControl);

    hideProjectCover();

    mtPopups.hideInfoPopup();
    display.synchronizeRefresh();
}

void cProjectEditor::showLabelDuringExport()
{
    display.setControlText(label[0], "");
    display.setControlText2(label[0], "");
    display.setControlText(label[1], "");
    display.setControlText2(label[1], "");
    display.setControlText(label[2], "");
    display.setControlText2(label[2], "");
    display.setControlText(label[3], "");
    display.setControlText2(label[3], "");
    display.setControlText(label[4], "");
    display.setControlText2(label[4], "");
    display.setControlText(label[5], "");
    display.setControlText2(label[5], "");
    display.setControlText(label[6], "");
    display.setControlText(label[7], "Cancel");

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlStyle(label[i],
                                controlStyleCenterX | controlStyleCenterY | controlStyleFont3 |
                                  controlStyleShow);
        display.setControlValue(label[i], 1);
        display.refreshControl(label[i]);
    }

    display.setControlValue(label[7], 0);

    display.setControlValue(bgLabel, 255); // 0b10000000
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();

    display.processDisplayUpdateImmediate();
}

void cProjectEditor::hideProjectCover()
{
    refreshCover = 0;
    display.setControlHide(coverImg);
}

void cProjectEditor::activateLabelsBorder()
{
    if (selectedPlace > frameData.placesCount - 1)
    {
        return;
    }

    display.setControlValue(frameControl, selectedPlace);
    display.setControlShow(frameControl);
    display.refreshControl(frameControl);
}

void cProjectEditor::showClockWarning()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 0);
        display.setControlText(label[i], "");
        display.setControlText2(label[i], "");

        display.refreshControl(label[i]);
        display.setControlShow(label[i]);
    }

    display.setControlText(label[6], "Cancel");
    display.setControlText(label[7], "Yes");
    display.setControlColors(label[6], display.getTheme().highlightedButtonLabelsColors);
    display.setControlColors(label[7], display.getTheme().highlightedButtonLabelsColors);
    display.setAddControlStyle(label[6], controlStyleBackground);
    display.setAddControlStyle(label[7], controlStyleBackground);

    display.refreshControl(label[6]);
    display.refreshControl(label[6]);

    sprintf(currentInfo,
            "Start rendering using internal clock source (%.0f BPM)?",
            mtProject.values.globalTempo);
    mtPopups.showInfoPopup("External clock source is set.", currentInfo);

    display.setControlValue(bgLabel, 255); // 192
    display.setControlShow(bgLabel);
    display.refreshControl(bgLabel);

    display.synchronizeRefresh();
}
