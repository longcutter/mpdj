// MPDJ (2026-11-16): the Drum Grid screen, the second generator. In the spirit of
// Mutable Instruments' Grids (its maps are GPL, so these are MPDJ's own): four
// one-bar drum patterns with a strength (0-255) per step for kick, snare, closed hat
// and open hat sit at the corners of an X / Y map; a point in between blends them, and
// each part plays the steps whose strength beats its density (an open hat takes the
// closed hat's place on its step). Chaos varies each bar (seeded). It writes the parts
// onto the tracks of pads 1-4 of a bank (their programs) whenever a setting changes;
// stronger steps are louder, up to the program's own volume (2026-11-22: was up to
// 90, too loud). MIT, see NOTICE.md.
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
        ParamOpenHat, // 2026-11-22 (Mike): pad 4
        ParamChaos,
        ParamBank,
        ParamSeed, // in the list only (the labels hold eight)
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

    static constexpr int kParts = 4;
    // per part (0 kick, 1 snare, 2 closed hat, 3 open hat) and row: a hit's level 50-100
    // (% of the program's volume), -1 = none
    static void generate(int x, int y, const int density[kParts], int chaos, uint32_t seed, int rows,
                         int8_t out[kParts][128]);

  private:
    void write(); // one UNDO per visit

    uint8_t param = ParamX;
    int x = 0, y = 0, density[kParts] = { 50, 50, 50, 30 }, chaos = 0, bank = 0;
    uint32_t seed = 1;
    bool undoStored = false;
    int8_t preview[kParts][128];

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
