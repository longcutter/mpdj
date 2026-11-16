#ifndef INTERFACE_PROJECTEDITOR_H_
#define INTERFACE_PROJECTEDITOR_H_

#include "FileBrowser.h"
#include "core/modulesBase.h"
#include "mtKeyboardManager.h"

class FileBrowser;

class cProjectEditor : public ModuleBase
{

  public:
    // MPDJ (touch, 2026-11-16): a tap on row `row` of list `which` selects it
    void mpdjTapList(int which, int row);
    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}

    cProjectEditor()
    {
        currentBrowserMode = kNone;
        fileBrowserData.mode = kNotInitBrowser;
        fileBrowserData.browserList = nullptr;
        fileBrowserData.selectionList = nullptr;
    }
    virtual ~cProjectEditor() {}

    //-------------------------------------------------------------
    void showDefaultScreen();
    void setDefaultScreenFunct();

    //-------------------------------------------------------------
    void showExportWindow();
    void setExportWindowFuncts();
    void showLabelDuringExport();

    //-------------------------------------------------------------
    void showModBrowser(bool reloadList = true);
    void refreshModBrowser();
    void refreshModBrowserLabels();
    void setModBrowserFunct();

    //-------------------------------------------------------------
    void showProjectBrowser(bool reloadList = true);
    void refreshProjectBrowser();
    void refreshProjectBrowserLabels();
    void setProjectBrowserFunct();
    bool browserEnter();

    void showRecentList();
    void processRecentList();

    void cleanupListsData();

    //-------------------------------------------------------------
    void showClockWarning();
    void setClockWarningScreenFunct();
    //-------------------------------------------------------------

    void lightUpPadBoard();

    void createNewProject();

    void saveProject();

    uint8_t loadProjectValues();

    // FileBrowserMode lastBrowserMode;

    enum BrowserMode
    {
        kNone,
        kOpenProject,
        kOpenMod,
        kSaveAsProject,
    };

    BrowserMode currentBrowserMode;

    void openBrowser(BrowserMode mode);

    //-------------------------------------------------------------
    strLabelData labelArrow;
    strList projectList;
    strList recentList;
    strFrameData frameData;

    hControl frameControl;
    hControl label[8] = { -1 };
    hControl blankSpaceCover = -1;
    hControl fileListControl = -1;
    hControl recentListControl = -1;
    hControl coverImg = -1;
    hControl bgLabel = -1;
    hControl browserLabelControl = -1;
    hControl recentLabelControl = -1;

    uint8_t selectedMod;
    char** modsList;
    uint8_t modsListLength;

    //-------------------------------------------------------------
    void functShowSaveLastWindow();
    void functShowSaveLastWindowBeforeOpen();
    void functShowSaveLastWindowBeforeExportToMod();
    void functShowOverwriteWindow();
    void showSaveLastWindow();

    void showDeleteProjectLastWindow(bool deletingProject);
    void showDeleteModLastWindow();

    void showKeyboard(const char* confirmButtonText);
    void showOverwriteWindow();

    void showStopPatternWindow();
    void setStopPatternFunct();

    void activateLabelsBorder();
    int selectedPlace = 0;

    //-------------------------------------------------------------
    enum AfterSaveOperation
    {
        noOperationAfterSave = 0,
        newProjectAfterSave,
        openProjectAfterSave,
        openRecentAfterSave,
        importModAfterSave,
        exportModAfterSave,
    };

    AfterSaveOperation afterSaveOperation = noOperationAfterSave;
    char* afterSavePath;
    char* afterSaveName;
    void cleanupAfterSaveData();

    //-------------------------------------------------------------

    char currentInfo[100];

    enum stopAction
    {
        stopActionOpenProject = 1,
        stopActionNewProject,
        stopActionExport,
        stopActionImportMod,
        stopActionExportIt
    };
    uint8_t stopAction = 0;

    // project cover
    uint8_t refreshCover = 0;
    elapsedMillis coverDelay;
    void hideProjectCover();
    void refreshProjectCover(uint16_t delay_ms);

    // export
    void startExport(uint8_t source);

    // clock for export setup
    bool checkClockConfig(uint8_t source);
    uint8_t saveClockConfig;
    uint8_t saveExportSource;

    FileBrowserData fileBrowserData;
    FileBrowser fileBrowser;

    char* recentProjectNameList[kRecentProjectListLength];
    int recentProjectListPos = 0;
    void changeRecentListPos(int value);
};

extern cProjectEditor projectEditor;

#endif /* INTERFACE_PROJECTEDITOR_H_ */
