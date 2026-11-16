// MPDJ (2026-11-16): the Turing Machine screen, see turing.h
#include "turing/turing.h"

#include "core/interface.h"
#include "fileManager.h"
#include "mpdjPads.h"
#include "mpdjPrograms.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "patternEditor/patternEditor.h"
#include "scales.h"

#include <stdio.h>
#include <string.h>

cTuring turing;
static cTuring* TU = &turing;

namespace
{
const char* const kLabelNames[cTuring::ParamCount] = { "Track", "Program", "Length", "Change",
                                                       "Density", "Low note", "Range", "Seed" };
const char* const kListNames[cTuring::ParamCount] = { "Track",   "Program",  "Length", "Change",
                                                      "Density", "Low note", "Range",  "Seed (new)" };

// small, seeded PRNG: the same seed and settings always write the same rows
uint32_t nextRand(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void functSwitchModule(uint8_t button) { TU->eventFunct(eventSwitchModule, TU, &button, 0); }
void functEncoder(int16_t value) { TU->changeValue(value); }
void functLeft() { TU->moveParam(-1); }
void functRight() { TU->moveParam(1); }
void functUp() { TU->changeValue(1); }
void functDown() { TU->changeValue(-1); }
void functActionButton(uint8_t button, uint8_t state)
{
    if (state != buttonPress)
        return;
    int b = button - interfaceButton0;
    if (b >= 0 && b < 8)
        TU->selectParam(b);
}
void functPlayAction()
{
    if (sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP)
        sequencer.playPattern();
    else
        sequencer.stop();
}

uint32_t rgb(const uint8_t* c)
{
    return (uint32_t)(c[0] * 2) << 16 | (uint32_t)(c[1] * 2) << 8 | (uint32_t)(c[2] * 2);
}
} // namespace

void cTuring::generate(uint16_t reg, int length, int change, int density, int low, int range,
                       uint32_t seed, int rows, int16_t* out)
{
    // the notes to choose from: the project scale from Low note over Range octaves
    uint8_t notes[64];
    int n = 0;
    int root = mtProject.values.padBoardRootNote % 12;
    uint8_t scale = mtProject.values.padBoardScale;
    for (int note = low; note < low + 12 * range && note <= 127 && n < 64; note++)
        if (scale == 0 || scale > MAX_SCALE || isInScale((uint8_t)note, (uint8_t)root, scale))
            notes[n++] = (uint8_t)note;
    uint32_t rnd = seed ? seed : 1;
    for (int r = 0; r < rows; r++)
    {
        // the register's low 8 bits: the note and the gate (a scrambled copy, so loud
        // notes and hits aren't the same bits)
        uint8_t v = reg & 0xFF;
        uint8_t g = (uint8_t)((v * 167u + 13u) & 0xFF);
        bool hit = g < (density * 256 + 50) / 100;
        out[r] = (hit && n) ? notes[v * n / 256] : -1;
        // one step: the loop's oldest bit comes back in, flipped with the Change chance
        int bit = (reg >> (length - 1)) & 1;
        if ((int)(nextRand(rnd) % 100) < change)
            bit ^= 1;
        reg = (uint16_t)((reg << 1) | bit);
    }
}

void cTuring::newSeed(uint32_t s)
{
    seed = s ? s : 1;
    uint32_t r = seed;
    reg = (uint16_t)nextRand(r);
}

void cTuring::write()
{
    Sequencer::strPattern* pt = sequencer.getPatternToUI();
    int rows = pt->track[0].length + 1;
    int16_t notes[Sequencer::MAXSTEP + 1];
    generate(reg, length, change, density, low, range, seed, rows, notes);
    if (!undoStored)
    {
        newFileManager.storePatternUndoRevision();
        undoStored = true;
    }
    for (int r = 0; r < rows; r++)
    {
        auto& s = pt->track[track].step[r];
        if (notes[r] >= 0)
        {
            s.note = (int8_t)notes[r];
            s.instrument = (uint8_t)program;
        }
        else if (s.note != Sequencer::STEP_NOTE_EMPTY)
        {
            s.note = Sequencer::STEP_NOTE_EMPTY; // the generator owns the track's notes
            s.instrument = 0;
        }
        s.volume = -1;
        s.micromove = -1;
    }
    newFileManager.setActualPatternStructChanged();
}

void cTuring::initDisplayControls()
{
    strControlProperties prop;
    prop.x = kGridX;
    prop.y = kGridY;
    prop.w = kGridW;
    prop.h = kGridH;
    prop.style = controlStyleShow;
    prop.data = &gridData;
    grid = display.createControl<cPadGrid>(&prop);

    for (int i = 0; i < ParamCount; i++)
    {
        paramNamePtrs[i] = (char*)kListNames[i];
        paramValuePtrs[i] = paramValues[i];
        paramValues[i][0] = 0;
    }
    listData.valueEditActive = 0;
    listData.start = param;
    listData.length = ParamCount;
    listData.linesCount = ParamCount;
    listData.params = paramNamePtrs;
    listData.values = paramValuePtrs;
    strControlProperties prop2;
    prop2.style = controlStyleFont1 | controlStyleBackground |
                  (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    prop2.x = 486;
    prop2.y = 29;
    prop2.w = 800 - 486 - 3;
    prop2.h = 394;
    prop2.data = &listData;
    list = display.createControl<cParamValueList>(&prop2);

    for (int i = 0; i < 8; i++)
    {
        strControlProperties p;
        p.text = (char*)kLabelNames[i];
        p.colors = display.getTheme().activeLabelsColors;
        p.style = controlStyleCenterX | controlStyleFont3 | controlStyleCenterY;
        p.x = (800 / 8) * i + (800 / 16);
        p.w = 800 / 8 - 6;
        p.y = 424;
        p.h = 55;
        p.value = 1;
        label[i] = display.createControl<cLabel>(&p);
    }
    strControlProperties pb;
    pb.text = nullptr;
    pb.colors = nullptr;
    pb.value = 255;
    pb.style = controlStyleNoTransparency | controlStyleShow;
    pb.x = 0;
    pb.w = 800;
    pb.y = 425;
    pb.h = 54;
    bgLabel = display.createControl<cBgLabel>(&pb);
}

void cTuring::destroyDisplayControls()
{
    display.destroyControl(grid);
    display.destroyControl(list);
    for (hControl& l : label)
        display.destroyControl(l);
    display.destroyControl(bgLabel);
    grid = list = bgLabel = -1;
    for (hControl& l : label)
        l = -1;
}

void cTuring::start(uint32_t options)
{
    (void)options;
    moduleRefresh = 1;
    undoStored = false;
    // the cursor's track and the title bar's program, each time it opens
    track = clampInt(patternEditor.trackerPattern.actualTrack, 0, kAllTracks - 1);
    program = clampInt(mtProject.values.lastUsedInstrument, 0, kProgramSlots - 1);
    if (!initialised)
    {
        initialised = true;
        newSeed(0x9E3779B9u ^ (uint32_t)sequencer.getSeqState() ^ (uint32_t)mtProject.values.globalTempo);
    }

    for (uint8_t b : { interfaceButtonPerformance, interfaceButtonParams, interfaceButtonFile,
                       interfaceButtonConfig, interfaceButtonMaster, interfaceButtonSamplePlay,
                       interfaceButtonSampleEdit, interfaceButtonSampleRec, interfaceButtonSampleLoad,
                       interfaceButtonSong, interfaceButtonPattern })
        ip->setButtonObj(b, buttonPress, functSwitchModule);
    ip->setButtonObj(interfaceButtonLeft, buttonPress, functLeft);
    ip->setButtonObj(interfaceButtonRight, buttonPress, functRight);
    ip->setButtonObj(interfaceButtonUp, buttonPress, functUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functDown);
    for (int b = 0; b < 8; b++)
        ip->setButtonObj(interfaceButton0 + b, functActionButton);
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayAction);
    ip->setPotObj(interfacePot0, functEncoder);

    titleBar->show();
    for (hControl l : label)
    {
        display.setControlShow(l);
        display.refreshControl(l);
    }
    display.setControlShow(bgLabel);
    display.refreshControl(bgLabel);
    display.setControlShow(list);
    display.setControlShow(grid);
    refresh();
}

void cTuring::stop() { moduleRefresh = 0; }

void cTuring::selectParam(int p)
{
    param = (uint8_t)clampInt(p, 0, ParamCount - 1);
    refresh();
}

void cTuring::moveParam(int steps) { selectParam(param + (steps > 0 ? 1 : -1)); }

void cTuring::changeValue(int steps)
{
    if (!steps)
        return;
    int dir = steps > 0 ? 1 : -1;
    switch (param)
    {
    case ParamTrack: track = clampInt(track + dir, 0, kAllTracks - 1); break;
    case ParamProgram: program = clampInt(program + steps, 0, kProgramSlots - 1); break;
    case ParamLength: length = clampInt(length + dir, 2, 16); break;
    case ParamChange: change = clampInt(change + steps, 0, 100); break;
    case ParamDensity: density = clampInt(density + 5 * dir, 0, 100); break;
    case ParamLow: low = clampInt(low + steps, 0, 115); break;
    case ParamRange: range = clampInt(range + dir, 1, 4); break;
    case ParamSeed:
    {
        uint32_t s = seed * 2654435761u + (uint32_t)(dir > 0 ? 1 : 7);
        newSeed(s);
        break;
    }
    default: return;
    }
    write();
    refresh();
}

void cTuring::toggleBit(int bit)
{
    if (bit < 0 || bit >= 16)
        return;
    reg ^= (uint16_t)(1u << bit);
    write();
    refresh();
}

int cTuring::bitAt(float x, float y) const
{
    if (x < kGridX || x >= kGridX + kGridW || y < kGridY || y >= kGridY + kGridH)
        return -1;
    int col = (int)((x - kGridX) * 4 / kGridW), row = 3 - (int)((y - kGridY) * 4 / kGridH);
    return row * 4 + col;
}

void cTuring::refresh()
{
    static const uint8_t kOn[3] = { 60, 110, 127 };
    for (int b = 0; b < 16; b++)
    {
        strPadGridData::Cell& c = gridData.cells[b];
        bool on = reg & (1u << b), inLoop = b < length;
        snprintf(c.name, sizeof c.name, "%d", b + 1);
        snprintf(c.program, sizeof c.program, "%s", on ? "1" : "0");
        snprintf(c.marks, sizeof c.marks, "%s", inLoop ? (b < 8 ? "loop, note" : "loop") : (b < 8 ? "note" : ""));
        c.color = on ? rgb(kOn) : 0;
        c.empty = !inLoop;
    }
    gridData.selected = -1;

    char prog[24];
    snprintf(paramValues[ParamTrack], sizeof paramValues[0], "Track %d", track + 1);
    if (programType(program) == kProgramEmpty)
        snprintf(prog, sizeof prog, "%d. (empty)", program + 1);
    else if (programType(program) == kProgramMidi)
        snprintf(prog, sizeof prog, "%d. MIDI", program + 1);
    else
        snprintf(prog, sizeof prog, "%d. %s", program + 1, mtProject.instrument[program].sample.file_name);
    snprintf(paramValues[ParamProgram], sizeof paramValues[0], "%s", prog);
    snprintf(paramValues[ParamLength], sizeof paramValues[0], "%d steps", length);
    snprintf(paramValues[ParamChange], sizeof paramValues[0], change ? "%d%%" : "locked", change);
    snprintf(paramValues[ParamDensity], sizeof paramValues[0], "%d%%", density);
    snprintf(paramValues[ParamLow], sizeof paramValues[0], "%s", mtNotes[low]);
    snprintf(paramValues[ParamRange], sizeof paramValues[0], range == 1 ? "%d octave" : "%d octaves", range);
    snprintf(paramValues[ParamSeed], sizeof paramValues[0], "%04X", (unsigned)(seed & 0xFFFF));

    snprintf(labelValues[ParamTrack], sizeof labelValues[0], "%d", track + 1);
    snprintf(labelValues[ParamProgram], sizeof labelValues[0], "%d", program + 1);
    snprintf(labelValues[ParamLength], sizeof labelValues[0], "%d", length);
    snprintf(labelValues[ParamChange], sizeof labelValues[0], change ? "%d%%" : "Lock", change);
    snprintf(labelValues[ParamDensity], sizeof labelValues[0], "%d%%", density);
    snprintf(labelValues[ParamLow], sizeof labelValues[0], "%s", mtNotes[low]);
    snprintf(labelValues[ParamRange], sizeof labelValues[0], "%d oct", range);
    snprintf(labelValues[ParamSeed], sizeof labelValues[0], "%04X", (unsigned)(seed & 0xFFFF));

    display.setControlData(grid, &gridData);
    display.refreshControl(grid);
    listData.start = param;
    display.setControlValue(list, param);
    display.refreshControl(list);
    for (int i = 0; i < 8; i++)
    {
        display.setControlText(label[i], (char*)kLabelNames[i]);
        display.setControlText2(label[i], labelValues[i]);
        display.setControlColors(label[i], i == param ? display.getTheme().highlightedButtonLabelsColors
                                                      : display.getTheme().activeLabelsColors);
        display.refreshControl(label[i]);
    }
    uint8_t sc = mtProject.values.padBoardScale;
    snprintf(title, sizeof title, "Turing Machine");
    titleBar->setTitle(title);
    snprintf(rightLabel, sizeof rightLabel, "-> Track %d, %s", track + 1,
             sc <= MAX_SCALE ? scaleNames[sc] : "Chromatic");
    titleBar->setRightLabel(rightLabel);
    titleBar->showRightLabel(true);
}
