// MPDJ (2026-11-16): the Turing Machine screen, the first generator (milestone 13's
// generator host starts here). A 16-bit shift register, after Music Thing Modular's
// Turing Machine: each row the oldest bit of the loop comes back in (flipped with the
// Change chance), the low 8 bits give the note (a scale over Range octaves from Low
// note) and whether the row plays (Density). It writes the pattern's rows of one
// track whenever a setting changes, so it can be shaped while the pattern plays.
// SHIFT+STEP SEQ opens it on the MPC (again = back). MIT, see NOTICE.md (MPDJ additions).
#pragma once

#include "core/modulesBase.h"
#include "padGridControl.h"
#include "paramValueList.h"

class cTuring : public ModuleBase
{
  public:
    enum Param : uint8_t
    {
        ParamTrack,
        ParamProgram,
        ParamLength,
        ParamChange,
        ParamDensity,
        ParamLow,
        ParamRange,
        ParamSeed,
        ParamMode,     // Notes + hits / Hits only (C5) / Notes only (re-pitch the track's hits)
        ParamVelocity, // Off / On: the register's high byte -> the volume column
        ParamCount
    };

    virtual void update() {}
    virtual void start(uint32_t options);
    virtual void stop();
    virtual void initDisplayControls();
    virtual void destroyDisplayControls();

    void selectParam(int p);
    uint8_t selectedParam() const { return param; }
    void moveParam(int steps);
    void changeValue(int steps);
    void toggleBit(int bit); // a pad (1 = bit 0) or a touch on the grid: flip that bit and write
    int bitAt(float x, float y) const;
    void refresh();

    static constexpr int kGridX = 6, kGridY = 34, kGridW = 470, kGridH = 382;

    // the generator itself (also used by the self-test): the note for each row of
    // `rows`, -1 = no hit
    // vel (optional): a volume 20-80 per row from the register's high byte
    static void generate(uint16_t reg, int length, int change, int density, int low, int range,
                         uint32_t seed, int rows, int16_t* out, int8_t* vel = nullptr);
    enum Mode : uint8_t { ModeBoth, ModeHits, ModeNotes, ModeCount };

  private:
    void write(); // the pattern's rows of the track, one UNDO per visit
    void newSeed(uint32_t s);
    void load(); // from / to the project (mtProject.mpdjTuring)
    void save();

    uint8_t param = ParamDensity;
    int track = 0, program = 0, length = 8, change = 0, density = 50, low = 48, range = 2, mode = 0, velocity = 0;
    uint32_t seed = 1;
    uint16_t reg = 0;
    bool undoStored = false, initialised = false;

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
    char rightLabel[32];
};

extern cTuring turing;
