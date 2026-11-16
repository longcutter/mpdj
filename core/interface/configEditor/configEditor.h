#ifndef INTERFACE_CONFIGEDITOR_H_
#define INTERFACE_CONFIGEDITOR_H_

#include "core/modulesBase.h"

#include "mtStructs.h"

#include "configEditor/configMenuDefs.h"
#include "mtGridEditor.h"

#undef MAX_SELECT_NODES
#define MAX_SELECT_NODES 7

//#define GRID_EDITOR_ENABLED

enum enScreenType
{
    screenTypeGridScreen,
    screenTypePadScreen
};

class cConfigEditor : public ModuleBase
{

  public:
    // MPDJ (touch, 2026-11-16): a tap on row `row` of list `which` selects it
    void mpdjTapList(int which, int row);
    virtual void update();
    virtual void start(uint32_t options);
    virtual void stop();

    virtual void initDisplayControls();
    virtual void destroyDisplayControls() {}

    cConfigEditor()
    {
        label[8] = { -1 };
        editorInstrument = nullptr;
        frameControl = -1;
        popoutWindowLabel = -1;
    }
    virtual ~cConfigEditor() {}

    //-----------------------------------------

    void showDefaultConfigScreen();
    void showMasterScreen();

    // void turnOffPerformanceMode();
    // void turnOffRadio();

    // menu
    void createConfigMenu();
    void hideConfigMenu();

    void createMenuBaseList();
    void reloadSubmenu();
    void loadConfigTextList(strItemTypeListText* itemSetup);
    void loadConfigValuesList(strItemTypeListValues* itemSetup);

    bool menuGoOut();
    void menuGoIn();
    void changeMenuListPosition(uint8_t list, int16_t value, uint8_t source = 0);
    void chanegeItemValue(void* selectedMenuItem, int16_t value);

    void configListConfirm(uint8_t list_pos);

    void itemEditorOpen(menu_item_t type,
                        const char* title,
                        const char* description,
                        const void* setup);
    void itemEditorClose();
    void itemEditorApply();
    void itemEditorChangeValue(int16_t value);

    void refreshConfigMenu();
    void setLabelByMenuItemType(uint8_t label, menu_item_t type, uint8_t variant = 0);
    void getSelectedItemInfo(void (**actionFunct)(void), void** menu_item, uint8_t* item_level);

    void setGridScreenFunction();
    void setPadScreenFunction();
    void reloadPadScreenDisplayedValue(uint8_t value);

    void showGridScreen();
    void refreshGridScreen();
    void hideGridScreen();
    void showPadScreen();
    void hidePadScreen();

    void refreshPadValue(uint8_t value, enScreenType screen);
    void refreshPadScreenFrame();

    void refreshGridScreenFrame();

    //

    //-----------------------------------------

    void setConfigScreenFunct();

    void activateLabelsBorder();
    void refreshListsColors();

    //
    uint8_t flashingState = 0;
    uint8_t firmwareFoundNum;
    hControl popoutWindowLabel;

    uint8_t configPopupActive = 0;

    void showConfigList4(uint8_t list_width, uint8_t start, uint8_t length, char** listText);
    void showConfigList4Val(uint8_t* value,
                            uint8_t min,
                            uint8_t max,
                            uint8_t interval,
                            uint8_t dev);
    void hideConfigList();

    void changeConfigListPosition(int16_t value);

    void showConfigItemLabel(const char* text);
    void hideConfigItemLabel();

    void showConfigItemDesctription(const char* text);
    void hideConfigItemDesctription();

    void reloadConfigLabels();

    void showSubmenu();
    void hideSubmenu();

    char** ptrfirmwareNamesList;

    void showFirmwareUpdatePopout();
    void updateFirmware(bool yes);

    void hidePopout();

    void showResetConfigPopout();
    void resetTrackerConfig(bool yes);

    void resizeLabelConfigDefault();
    void resizeLabelConfigMaster();
    void resizeFirmwareLabel(uint8_t control); // 0 - revert to normal, 1 - rescale to double label;

    uint8_t processUpdate = 0;

    //----------------------------------

    strFrameData frameData;
    strLabelData labelArrow[2];

    hControl label[8];
    hControl blankSpaceCover[2] = { -1 };
    hControl batteryLabelControl = -1;
    hControl audioPerformanceLabelControl = -1;
    hControl memoryUsageLabelControl = -1;

    hControl configBasemenuListControl;
    hControl configSubmenuListControl;
    hControl configListControl;
    hControl configLabel;
    hControl creditsTextBox;
    hControl descriptionTextBox;

    hControl gridNameLabel = -1;
    hControl instrumentLabel = -1;

    hControl bgLabel;
    hControl frameControl;
    hControl gridPadsControl = -1;

    hControl padBar[2] = { -1, -1 };
    hControl padList = -1;

    uint8_t selectedPlace = 0;
    uint8_t selectedPlacePadScreen = 0;

    strInstrument* editorInstrument;

    strTextBoxData textBoxData;

    strPadNamesGridEditor padNamesStruct;

    strList padListData;

    int16_t padScreenDisplayedValue[3];
    char microtuneValue[6];
    char padScreenTitleLabelName[20];
    uint8_t selectedPlaceGridScreen;

    const char* padListNames[2] = { (char*)"On", (char*)"Off" };

    elapsedMillis infoDataRefresh;
#ifdef TRACKER_MINI
    char batteryText[30];
    char batteryText2[30];
#endif
    char audioPerformanceText[30];
    char audioPerformanceText2[30];
    char heapUsageText[30];
    char stackUsageText[30];
    //----------------------------------
    // listy

    strList basemenuList;
    strParamValueList submenuList;

    strList configList;

    uint8_t submenuShown = 0;
    uint8_t itemEditorShown = 0;

    char** ptrTempValuesList = nullptr;
    uint16_t lastTempListLength;

    uint8_t configListShown = 0;
    uint8_t selectedConfigListPosition = 0;

    uint8_t updatePopupShown = 0;
    uint8_t resetConfigPopupShown = 0;

    uint8_t exitOnButtonRelease = 0;

    void changeLabelText(uint8_t labelIdx, const char* text);

    //---------- credits ------------

    void openCredits();
    void closeCredits();
    void showCreditsControls();
    void hideCreditsControls();

    ///-------------------------------

    void saveConfigToEeprom();

    void showBatteryInfo();
    void showDiagnosticInfo();
};

extern cConfigEditor configEditor;

//===========================================================================
//===========================================================================
//===========================================================================

void firmwareUpgradeActivate();
void firmwareUpgradeDeactivate();

#endif /* INTERFACE_CONFIGEDITOR_H_ */
