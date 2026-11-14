// MPDJ (step 5d): the Pad Assign screen, opened with SHIFT+PROGRAM EDIT on
// the MPC. A 4x4 grid of the current bank's pads, the selected pad's settings
// as a list, and the same settings on the eight screen-button labels. Hitting
// a pad selects and plays it; the bank buttons switch banks. MIT, see
// NOTICE.md (MPDJ additions).
#pragma once

#include "core/modulesBase.h"
#include "padGridControl.h"
#include "paramValueList.h"

class cPadAssign : public ModuleBase
{
  public:
    enum Param : uint8_t
    {
        ParamProgram,
        ParamTrack,
        ParamTune,
        ParamLevel,
        ParamMode,
        ParamChoke,
        ParamCut,
        ParamColour,
        ParamCount
    };

    virtual void update() {}
    virtual void start(uint32_t options);
    virtual void stop();
    virtual void initDisplayControls();
    virtual void destroyDisplayControls();

    // From the platform: a pad hit on this screen selects it (0-63); a bank
    // button shows that bank (the same pad number there)
    void selectPad(int pad);
    void showBank(int bank);
    int selectedPad() const { return selPad; }
    // Q-Link 3: the setting
    void moveParam(int steps);
    void selectParam(int p); // a screen button / label touch
    // QL1 / QL2 (Left / Right / Up / Down) move around the grid
    void moveSelection(int dx, int dy);

    void refresh();

  private:
    int selPad = 0;
    uint8_t param = 0;

    hControl grid = -1;
    hControl list = -1;
    hControl label[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    hControl bgLabel = -1;

    strPadGridData gridData;
    strParamValueList listData;
    char paramNames[ParamCount][16];
    char paramValues[ParamCount][24];
    char* paramNamePtrs[ParamCount];
    char* paramValuePtrs[ParamCount];
    char labelValues[ParamCount][12];
    char title[32];

    void fillValues();
};

extern cPadAssign padAssign;
