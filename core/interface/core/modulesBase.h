#ifndef INTERFACE_MODULESBASE_H_
#define INTERFACE_MODULESBASE_H_

#include "core/interfaceDefs.h"
#include "display.h"
#include "mtKeyboardManager.h"
#include "titleBar.h"

#include <stdint.h>

class InputProcessor;
class Platform;
class Buttons;
class TitleBar;
class Synth;

//#############################################################################################################
//#############################################################################################################

class ModuleBase
{
    typedef void (*EventFunct)(uint8_t, void*, void*, void*);
    friend class Interface;

  public:
    ModuleBase() {}
    ~ModuleBase() {}

    virtual void update() = 0;
    virtual void start(uint32_t options) = 0;
    virtual void stop() = 0;

    virtual void initDisplayControls() = 0;
    virtual void destroyDisplayControls() = 0;

    virtual uint8_t getCurrentModeIdx() { return 0; }
    virtual void hideControlsUnderMiniMenu() {}
    virtual bool wasLastSwitchingSuccessfull() { return true; }

    static void setEventFunct(void (*func)(uint8_t, void*, void*, void*))
    {
        ModuleBase::eventFunct = func;
    };
    static void setInputProcessor(InputProcessor* ip) { ModuleBase::ip = ip; };
    static void setPlatform(Platform* platform) { ModuleBase::platform = platform; };
    static void setButtons(Buttons* buttons) { ModuleBase::buttons = buttons; };
    static void setTitleBar(TitleBar* titleBar) { ModuleBase::titleBar = titleBar; };
    static void setSynth(Synth* synth) { ModuleBase::synth = synth; };

    static InputProcessor* ip;
    static EventFunct eventFunct;
    static Buttons* buttons;
    static TitleBar* titleBar;
    static mtKeyboardManager keyboardManager;

    uint8_t moduleRefresh = 0;

  protected:
    static Platform* platform;
    static Synth* synth;
};

class Buttons
{
  public:
    void init(SwitchGroup* switchGroup) { this->switchGroup = switchGroup; }
    inline void setHoldTime(int timeMs) { switchGroup->setHoldTime(timeMs); }
    inline void setActionCallback(SwitchGroup::ActionType action, void (*callback)(uint8_t))
    {
        switchGroup->setActionCallback(action, callback);
    }
    bool isButtonPressed(int buttonIdx);

  private:
    SwitchGroup* switchGroup;
};

//#############################################################################################################
//#############################################################################################################
class InputProcessor
{

  public:
    InputProcessor(uint8_t _potsCount, uint8_t _buttonsCount, uint8_t _padsCount)
      : potsCount(_potsCount)
      , buttonsCount(_buttonsCount)
      , padsCount(_padsCount)
      , blockPots(new uint8_t[_potsCount])
      , blockButtons(new uint8_t[_buttonsCount])
      , blockPads(new uint8_t[_padsCount])
      , pots(new strPotObject[_potsCount])
      , buttons(new strButtonObject[_buttonsCount])
    {
        potsCleared = 0;
        buttonsCleared = 0;
        padsCleared = 0;
        clearAll();
        unblockAllInputs();
    }

    ~InputProcessor() {}

    //----------------------
    void clearAll();
    void clearAllPots();
    void clearGlobalPads();
    void clearAllButtons();
    void clearButton(uint8_t button);
    void clearButtonsRange(uint8_t from, uint8_t to);

    //----------------------
    void blockAllInputs();
    void blockAllInputsExcept(uint8_t btn);
    void blockAllInputsExcept(uint8_t btn1, uint8_t btn2);
    void blockAllInputsExceptScreenButtons();
    void unblockAllInputs();
    uint8_t isPowerButtonBlocked() { return blockPowerButton; }

    //----------------------
    void setPotObj(uint8_t objectID, void (*funct)(int16_t), bool shiftBoost = true);
    void setPotObj(uint8_t objectID, void (*funct)(int16_t, uint8_t), bool shiftBoost = true);

    void setButtonObj(uint8_t objectID, uint8_t state, void (*funct)(void));
    void setButtonObj(uint8_t objectID, uint8_t state, void (*funct)(uint8_t));
    void setButtonObj(uint8_t objectID, void (*funct)(uint8_t));
    void setButtonObj(uint8_t objectID, void (*funct)(uint8_t, uint8_t));

    void setPowerButtonObj(void (*funct)(uint8_t));

    // MPDJ: lets the MPC input layer see which buttons the current screen binds
    // (e.g. to answer a confirm popup that cleared everything but screen buttons 6-7)
    bool hasButtonObj(uint8_t objectID) const
    {
        return objectID < buttonsCount && buttons[objectID].mode != 0;
    }

    void setPadsGlobal(void (*funct)(uint8_t, uint8_t, int16_t));

    //----------------------
    void processButtonsInput(uint8_t button, uint8_t state);
    void processPotsInput(uint8_t pot, int16_t value, uint8_t state = 255);
    void processPadsInput(uint8_t pad, uint8_t state, int16_t velo);
    bool processPowerButton(uint8_t state);

  private:
    struct strButtonObject
    {
        uint8_t mode = 0;
        void (*funct1)(void) = nullptr;
        void (*funct2)(uint8_t) = nullptr;
        void (*funct3)(uint8_t, uint8_t) = nullptr;
        uint8_t state = 0;
    };

    struct strPotObject
    {
        uint8_t mode = 0;
        void (*funct1)(int16_t) = nullptr;
        void (*funct2)(int16_t, uint8_t) = nullptr;
        bool boost = false;
    };

    void (*padsGlobalFunct)(uint8_t, uint8_t, int16_t) = nullptr;

    const uint8_t potsCount;
    const uint8_t buttonsCount;
    const uint8_t padsCount;

    uint8_t* blockPots;
    uint8_t* blockButtons;
    uint8_t* blockPads;
    uint8_t blockPowerButton;

    strPotObject* pots;
    strButtonObject* buttons;

    void (*powrButtonFunct)(uint8_t) = nullptr;

    uint8_t potsCleared = 1;
    uint8_t buttonsCleared = 1;
    uint8_t padsCleared = 1;
};

#endif /* INTERFACE_MODULESBASE_H_ */
