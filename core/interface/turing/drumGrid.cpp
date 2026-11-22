// MPDJ (2026-11-16): the Drum Grid screen, see drumGrid.h
#include "turing/drumGrid.h"

#include "core/interface.h"
#include "fileManager.h"
#include "mpdjPads.h"
#include "mpdjPrograms.h"
#include "mtSequencer.h"
#include "mtStructs.h"

#include <stdio.h>
#include <string.h>

cDrumGrid drumGrid;
static cDrumGrid* DG = &drumGrid;

namespace
{
// MPDJ's own corner patterns: strength 0-255 per 16th, [corner][part][step];
// corners: 0 = x 0 / y 0 four on the floor, 1 = x max / y 0 breakbeat,
// 2 = x 0 / y max half-time, 3 = x max / y max broken
// clang-format off
const uint8_t kCorner[4][cDrumGrid::kParts][16] = {
    { // four on the floor
        { 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 40, 0 },
        { 0, 0, 0, 0, 230, 0, 0, 0, 0, 0, 0, 0, 230, 0, 0, 30 },
        { 30, 80, 240, 80, 30, 80, 240, 80, 30, 80, 240, 80, 30, 80, 240, 90 },
        { 0, 0, 0, 0, 0, 0, 110, 0, 0, 0, 0, 0, 0, 0, 210, 0 }, // open hat: the last off-beat
    },
    { // breakbeat
        { 255, 0, 0, 0, 0, 0, 140, 0, 0, 0, 220, 90, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 255, 0, 0, 70, 0, 60, 0, 0, 255, 0, 0, 80 },
        { 200, 70, 200, 70, 200, 70, 200, 70, 200, 70, 200, 70, 200, 70, 200, 70 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 190, 0, 0, 0, 0, 110 },
    },
    { // half-time
        { 255, 0, 0, 0, 0, 0, 0, 110, 0, 0, 150, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0, 0, 0, 0, 60 },
        { 220, 150, 150, 150, 220, 150, 150, 150, 220, 150, 150, 150, 220, 120, 120, 120 },
        { 0, 0, 0, 0, 0, 0, 160, 0, 0, 0, 0, 0, 0, 0, 200, 0 },
    },
    { // broken
        { 255, 0, 0, 150, 0, 0, 120, 0, 0, 0, 200, 0, 0, 90, 0, 0 },
        { 0, 0, 0, 0, 200, 0, 0, 60, 0, 0, 0, 220, 0, 0, 100, 0 },
        { 40, 150, 220, 40, 40, 130, 200, 40, 40, 140, 210, 40, 40, 120, 230, 40 },
        { 0, 0, 0, 140, 0, 0, 0, 0, 0, 0, 200, 0, 0, 0, 0, 0 },
    },
};
// clang-format on
const char* const kPartNames[cDrumGrid::kParts] = { "Kick", "Snare", "Hat", "OHat" };
const char* const kLabelNames[cDrumGrid::ParamCount] = { "X", "Y", "Kick", "Snare", "Hat", "Open hat", "Chaos", "Pads", "Seed" };
const char* const kListNames[cDrumGrid::ParamCount] = { "X (four / break)", "Y (half-time)", "Kick density",
                                                        "Snare density",    "Hat density",   "Open hat density",
                                                        "Chaos",            "Pads 1-4 of bank", "Seed (new)" };

uint32_t nextRand(uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void functSwitchModule(uint8_t button) { DG->eventFunct(eventSwitchModule, DG, &button, 0); }
void functEncoder(int16_t value) { DG->changeValue(value); }
void functLeft() { DG->moveParam(-1); }
void functRight() { DG->moveParam(1); }
void functUp() { DG->changeValue(1); }
void functDown() { DG->changeValue(-1); }
void functActionButton(uint8_t button, uint8_t state)
{
    if (state != buttonPress)
        return;
    int b = button - interfaceButton0;
    if (b >= 0 && b < 8)
        DG->selectParam(b);
}
void functPlayAction()
{
    if (sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP)
        sequencer.playPattern();
    else
        sequencer.stop();
}
} // namespace

void cDrumGrid::generate(int x, int y, const int density[kParts], int chaos, uint32_t seed, int rows,
                         int8_t out[kParts][128])
{
    uint32_t rnd = seed ? seed : 1;
    for (int r = 0; r < rows && r < 128; r++)
    {
        int step = r % 16;
        for (int p = 0; p < kParts; p++)
        {
            // bilinear blend of the four corners
            int top = kCorner[0][p][step] * (255 - x) + kCorner[1][p][step] * x;
            int bottom = kCorner[2][p][step] * (255 - x) + kCorner[3][p][step] * x;
            int s = (top * (255 - y) + bottom * y) / (255 * 255);
            // chaos: each bar (and the first) a seeded nudge up or down
            int j = (int)(nextRand(rnd) % 201) - 100; // -100 .. 100
            s = clampInt(s + j * chaos * 128 / 10000, 0, 255);
            int threshold = 255 - density[p] * 255 / 100;
            // level: 50-100 % of the program's volume by strength (2026-11-22; was a
            // volume of 40-90, louder than the programs' standard 50)
            out[p][r] = (s > 0 && s >= threshold && density[p] > 0) ? (int8_t)(50 + s * 50 / 255) : (int8_t)-1;
        }
        if (out[3][r] >= 0)
            out[2][r] = -1; // an open hat takes the closed hat's place
    }
}

void cDrumGrid::write()
{
    Sequencer::strPattern* pt = sequencer.getPatternToUI();
    int rows = pt->track[0].length + 1;
    generate(x, y, density, chaos, seed, rows, preview);
    if (!undoStored)
    {
        newFileManager.storePatternUndoRevision();
        undoStored = true;
    }
    // review: parts may share a track (pads 1-3 on one track): per track, a row takes the
    // first part (kick, snare, hat) that hits there, and is cleared only when none does
    const strPadSlot* slots[kParts];
    for (int p = 0; p < kParts; p++)
    {
        slots[p] = padSlot(bank * kPadsPerBank + p);
        if (slots[p] && (slots[p]->program == kPadNoProgram || programType(slots[p]->program) == kProgramEmpty))
            slots[p] = nullptr; // no sound on that pad: its track is left alone
    }
    for (int p = 0; p < kParts; p++)
    {
        if (!slots[p])
            continue;
        int t = slots[p]->track;
        bool first = true; // the first part on this track writes it
        for (int q = 0; q < p; q++)
            if (slots[q] && slots[q]->track == t)
                first = false;
        if (!first)
            continue;
        for (int r = 0; r < rows; r++)
        {
            auto& s = pt->track[t].step[r];
            int who = -1;
            for (int q = p; q < kParts && who < 0; q++)
                if (slots[q] && slots[q]->track == t && preview[q][r] >= 0)
                    who = q;
            if (who >= 0)
            {
                s.note = Sequencer::STEP_NOTE_DEFAULT;
                s.instrument = slots[who]->program;
                int pv = sequencer.getInstrumentVelo(slots[who]->program); // the program's volume
                s.volume = (int8_t)((pv * preview[who][r] + 50) / 100);
            }
            else
            {
                if (s.note != Sequencer::STEP_NOTE_EMPTY)
                {
                    s.note = Sequencer::STEP_NOTE_EMPTY; // the generator owns these tracks' notes
                    s.instrument = 0;
                }
                s.volume = -1;
            }
            s.micromove = -1;
        }
    }
    newFileManager.setActualPatternStructChanged();
}

void cDrumGrid::initDisplayControls()
{
    strControlProperties prop;
    prop.x = 6;
    prop.y = 34;
    prop.w = 470;
    prop.h = 382;
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

void cDrumGrid::destroyDisplayControls()
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

void cDrumGrid::start(uint32_t options)
{
    (void)options;
    moduleRefresh = 1;
    undoStored = false;
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

    generate(x, y, density, chaos, seed, 16, preview);
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

void cDrumGrid::stop() { moduleRefresh = 0; }

void cDrumGrid::selectParam(int p)
{
    param = (uint8_t)clampInt(p, 0, ParamCount - 1);
    refresh();
}

void cDrumGrid::moveParam(int steps) { selectParam(param + (steps > 0 ? 1 : -1)); }

void cDrumGrid::changeValue(int steps)
{
    if (!steps)
        return;
    int dir = steps > 0 ? 1 : -1;
    switch (param)
    {
    case ParamX: x = clampInt(x + 8 * steps, 0, 255); break;
    case ParamY: y = clampInt(y + 8 * steps, 0, 255); break;
    case ParamKick:
    case ParamSnare:
    case ParamHat:
    case ParamOpenHat:
        density[param - ParamKick] = clampInt(density[param - ParamKick] + 5 * dir, 0, 100);
        break;
    case ParamChaos: chaos = clampInt(chaos + 5 * dir, 0, 100); break;
    case ParamSeed: seed = seed * 2654435761u + (uint32_t)(dir > 0 ? 1 : 7); break;
    case ParamBank: bank = clampInt(bank + dir, 0, kPads / kPadsPerBank - 1); break;
    default: return;
    }
    write();
    refresh();
}

void cDrumGrid::refresh()
{
    // the grid: the first bar's 16 steps (step 1 bottom left, as the pads)
    for (int st = 0; st < 16; st++)
    {
        strPadGridData::Cell& c = gridData.cells[st];
        snprintf(c.name, sizeof c.name, "%d", st + 1);
        char hits[24] = "";
        for (int p = 0; p < kParts; p++)
            if (preview[p][st] >= 0)
            {
                strncat(hits, hits[0] ? " " : "", sizeof hits - strlen(hits) - 1);
                strncat(hits, kPartNames[p], sizeof hits - strlen(hits) - 1);
            }
        snprintf(c.program, sizeof c.program, "%s", hits);
        c.marks[0] = 0;
        static const uint8_t kKick[3] = { 120, 30, 20 }, kSnare[3] = { 120, 100, 20 }, kHat[3] = { 40, 110, 120 },
                             kOpen[3] = { 90, 60, 120 };
        const uint8_t* col = preview[0][st] >= 0 ? kKick : preview[1][st] >= 0 ? kSnare : preview[3][st] >= 0 ? kOpen : kHat;
        c.color = hits[0] ? ((uint32_t)(col[0] * 2) << 16 | (uint32_t)(col[1] * 2) << 8 | (uint32_t)(col[2] * 2)) : 0;
        c.empty = !hits[0];
    }
    gridData.selected = -1;

    snprintf(paramValues[ParamX], sizeof paramValues[0], "%d%%", x * 100 / 255);
    snprintf(paramValues[ParamY], sizeof paramValues[0], "%d%%", y * 100 / 255);
    for (int p = 0; p < kParts; p++)
        snprintf(paramValues[ParamKick + p], sizeof paramValues[0], "%d%%", density[p]);
    snprintf(paramValues[ParamChaos], sizeof paramValues[0], chaos ? "%d%%" : "Off", chaos);
    snprintf(paramValues[ParamSeed], sizeof paramValues[0], "%04X", (unsigned)(seed & 0xFFFF));
    char where[48] = "";
    for (int p = 0; p < kParts; p++)
    {
        const strPadSlot* s = padSlot(bank * kPadsPerBank + p);
        char one[16];
        if (s && s->program != kPadNoProgram)
            snprintf(one, sizeof one, "%sT%d", p ? " " : "", s->track + 1);
        else
            snprintf(one, sizeof one, "%s-", p ? " " : "");
        strncat(where, one, sizeof where - strlen(where) - 1);
    }
    snprintf(paramValues[ParamBank], sizeof paramValues[0], "%c: %s", 'A' + bank, where);
    for (int i = 0; i < ParamCount; i++)
        snprintf(labelValues[i], sizeof labelValues[0], "%s", paramValues[i]);
    snprintf(labelValues[ParamBank], sizeof labelValues[0], "%c1-4", 'A' + bank);

    display.setControlData(grid, &gridData);
    display.refreshControl(grid);
    listData.start = param;
    display.setControlData(list, &listData);
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
    snprintf(title, sizeof title, "Drum Grid");
    titleBar->setTitle(title);
    snprintf(rightLabel, sizeof rightLabel, "Pads 1-4 -> %s", where);
    titleBar->setRightLabel(rightLabel);
    titleBar->showRightLabel(true);
}
