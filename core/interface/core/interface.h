
#ifndef INTERFACE_INTERFACE_H_
#define INTERFACE_INTERFACE_H_

#include "core/interfaceDefs.h"
#include "core/modulesBase.h"
#include "elapsedMillis.h"
#include <stdint.h>

#include "mtHardware.h"

#include "core/interfacePopups.h"
#include "interfaceMenuPopup.h"

typedef ModuleBase* hModule;
class InterfacePopups;
class InterfaceMenuPopup;
class Synth;

// Higher number = higher priority
enum action_priorities
{
    noSdCardPriority = 0x01U,
    powerButtonActionPriority = 0x02U,
};

// enumenActualBlockingScreen
//{
//	noneBlockingScreen = 0,
//	noSdScreen,
//	fileTransferScreen,
//};
class Platform;
class Battery;

class Interface
{
  public:
    bool isRunning() const; // MPDJ: past the start screen
    Interface() {}
    ~Interface() {}

    // interface.cpp -----------------------------
    Platform* getPlatform();
    void begin(Platform* platform, Synth* synth);
    void update();
    void setOperatingMode(uint8_t mode);

    void activateModule(hModule module, uint32_t options);
    void deactivateModule(hModule module);
    void switchModuleByButton(hModule module, uint8_t button, uint32_t options);
    void switchModuleToPrevious(hModule module, uint8_t options);
    int8_t getButtonIndex(uint8_t button);
    void toggleActiveModule(uint8_t state);
    bool isSongScreenActive();
    bool isPatternScreenActive();
    // MPDJ (5d-1): Pad Assign (SHIFT+PROGRAM EDIT): open it, or back to the
    // screen it was opened from
    bool isPadAssignScreenActive();
    void mpdjTogglePadAssign();
    bool isPerformanceModeScreenActive();
    bool isMasterScreenActive();
    bool isInstrumentParamsScreenActive();
    bool isSamplePlayerScreenActive();
    // MPDJ (MPC controls)
    bool isSampleEditorScreenActive();
    bool isSampleLoaderScreenActive();
    bool isFileScreenActive();
    bool isConfigScreenActive();
    bool isSampleRecorderScreenActive();

    void handleCardSlotAction(bool state);
    void handlePowerButtonAction(uint8_t state);
    void handleFileTransferMode(uint8_t state);
    // Enters or leaves USB storage mode without the UI; returns the resulting state.
    bool setFileTransferMode(bool enable);

    void refreshInterfaceControlsColors();

    friend void interfaceEnvents(uint8_t event, void* param1, void* param2, void* param3);

    // interfaceMisc.cpp -----------------------------
    void openStartupProject();
    uint8_t detectProjectLoadState();
    void openNewProject();

    void initStartScreen();
    void refreshStartScreen();
    void hideStartScreen();
    void destroyStartScreen();

    void activateInterface();
    void activateNoSdScreen();
    void activateFileTransferModeScreen();
    void activateShutdownMode();

    void showDisplayNoSdCard();
    void hideDisplayNoSdCard();

    void showDisplayFileTransferMode();
    void hideDisplayFileTransfer();

    void showDisplayShutdown();
    void refreshDisplayShutdown();
    void hideDisplayShutdown();

    void commonThingsUpdate();
    void processFileManagerPopupProgress(uint8_t status);

    // interfaceInputs.cpp -----------------------------
    void potChange(uint8_t n, int16_t value);
    void buttonChange(uint8_t n, uint8_t value);
    bool powerButtonChange(uint8_t state);
    void padPressed(uint8_t n, int8_t x, int8_t y, uint8_t velo);
    void padReleased(uint8_t n);
    void padHold(uint8_t n);
    void SDCardChange(uint8_t state);
    void initSpecialMiniTrackerButtons();

    void showLowBatteryPopup();
    void showChargingBatteryPopup();
    void showEmptyBatteryPopup();

    void changeMasterVolume(int value);

    Theme* theme();

    InputProcessor* ip;

  private:
    friend InterfacePopups;
    friend InterfaceMenuPopup;

    enum SpecialScreensMode
    {
        specialScreenModeDisabled = 0,
        noSdCardScreenMode,
        usbStorageScreenMode,
    };

    void processOperatingMode();
    void doStartTasks();
    void processStartScreen();
    void processPowerOffSequence();
    bool isOnScreenModule(ModuleBase* module);

    static const uint8_t modulesCount;
    static const hModule modules[];

    static const uint8_t modulesButtonsCount;
    static const uint32_t modulesButtons[][3];

    uint8_t operatingMode = 0;

    hModule onScreenModule = nullptr;
    uint32_t lastOptions = 0;

    uint8_t toggledState =
      0; // blokuje toggl(1) kiedy nie bylo toggle(0) a modul tez nie jest aktywny

    hModule previousModule = nullptr;
    uint32_t previousModuleOptions = 0;

    SpecialScreensMode specialScreenMode = specialScreenModeDisabled;

    // interfaceMisc.cpp -----------------------------
    uint8_t minStartTimeCounter = 0;
    uint8_t startProjectLoadingProgress = 0;
    hControl startScreenControl = -1;
    const char* projectLoadText = "Opening last project...";

    hControl turnOffProgressBar = -1;
    char turnOffText[20];

    hControl noSdTextControl = -1;
    hControl descriptionTextBox = -1;
    hControl fileTransferLabelControl = -1;

    // styl popupu interfejsu
    strPopupStyleConfig popupConfig;

    uint8_t shutdownSaveFlag = 0;

    // globalne popupy fileManagera
    uint8_t fileManagerPopupEnabled = 0;
    uint8_t fileManagerPopupState = 0;
    int8_t fileManagerLastProgress;
    uint8_t noProgressChangeCounter = 0;
    Platform* platform;
    FileSystem* fileSystem;

    Buttons buttons;

    Synth* synth;
};

extern Interface mtInterface;

extern void interfaceEnvents(uint8_t event, void* param1, void* param2, void* param3);

#endif /* INTERFACE_INTERFACE_H_ */
