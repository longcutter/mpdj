// MPDJ (2026-11-16): the Drum Grid screen, the second generator. In the spirit of
// Mutable Instruments' Grids (its maps are GPL, so these are MPDJ's own): four
// one-bar drum patterns with a strength (0-255) per step for kick, snare and hat sit
// at the corners of an X / Y map; a point in between blends them, and each part plays
// the steps whose strength beats its density. Chaos varies each bar (seeded). It
// writes kick, snare and hat onto the tracks of pads 1-3 of a bank (their programs),
// louder for stronger steps, whenever a setting changes. MIT, see NOTICE.md.
#pragma once

#include "core/modulesBase.h"
#include "padGridControl.h"
#include "paramValueList.h"

class cDrumGrid : public ModuleBase
{
  public:
    enum Param : uint8_t
    {
        ParamX,
        ParamY,
        ParamKick,
        ParamSnare,
        ParamHat,
        ParamChaos,
        ParamSeed,
        ParamBank,
        ParamCount
    };

    virtual void update() {}
    virtual void start(uint32_t options);
    virtual void stop();
    virtual void initDisplayControls();
    virtual void destroyDisplayControls();

    void selectParam(int p);
    void moveParam(int steps);
    void changeValue(int steps);
    void refresh();

    // per part (0 kick, 1 snare, 2 hat) and row: volume 0-100 of a hit, -1 = none
    static void generate(int x, int y, const int density[3], int chaos, uint32_t seed, int rows,
                         int8_t out[3][128]);

  private:
    void write(); // one UNDO per visit

    uint8_t param = ParamX;
    int x = 0, y = 0, density[3] = { 50, 50, 50 }, chaos = 0, bank = 0;
    uint32_t seed = 1;
    bool undoStored = false;
    int8_t preview[3][128];

    hControl grid = -1;
    hControl list = -1;
    hControl label[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    hControl bgLabel = -1;
    strPadGridData gridData;
    strParamValueList listData;
    char paramValues[ParamCount][24];
    char* paramNamePtrs[ParamCount];
    char* paramValuePtrs[ParamCount];
    char labelValues[ParamCount][12];
    char title[32];
    char rightLabel[40];
};

extern cDrumGrid drumGrid;
