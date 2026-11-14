// MPDJ (step 5d-1): the Pad Assign screen, see padAssign.h
#include "padAssign/padAssign.h"

#include "core/interface.h"
#include "mpdjPads.h"
#include "mpdjPrograms.h"
#include "mtStructs.h"

#include <stdio.h>
#include <string.h>

cPadAssign padAssign;
static cPadAssign* PA = &padAssign;

namespace
{
const char* const kParamNames[cPadAssign::ParamCount] = { "Program", "Track", "Tune", "Level",
                                                          "Mode",    "Choke", "Cut",  "Colour" };
const char* const kListNames[cPadAssign::ParamCount] = { "Program", "Track",       "Tune",           "Level",
                                                         "Mode",    "Choke group", "Cut on release", "Colour" };

uint32_t rgb(const uint8_t* c)
{
    // palette entries are 0-127 per channel (pad LED range): to 0-255
    return (uint32_t)(c[0] * 2) << 16 | (uint32_t)(c[1] * 2) << 8 | (uint32_t)(c[2] * 2);
}

void programName(uint8_t program, char* out, size_t size)
{
    if (program == kPadNoProgram)
        snprintf(out, size, "None");
    else if (programType(program) == kProgramMidi)
        snprintf(out, size, "%d. MIDI ch %d", program + 1, instrumentIdxToMidiChannel(program));
    else if (programType(program) == kProgramEmpty)
        snprintf(out, size, "%d. (empty)", program + 1);
    else
        snprintf(out, size, "%d. %s", program + 1, mtProject.instrument[program].sample.file_name); // as the title bar
}

void functSwitchModule(uint8_t button)
{
    PA->eventFunct(eventSwitchModule, PA, &button, 0);
}

void functLeft() { PA->moveSelection(-1, 0); }
void functRight() { PA->moveSelection(1, 0); }
void functUp() { PA->moveSelection(0, 1); }
void functDown() { PA->moveSelection(0, -1); }

void functActionButton(uint8_t button, uint8_t state)
{
    if (state != buttonPress)
        return;
    int b = button - interfaceButton0;
    if (b >= 0 && b < cPadAssign::ParamCount)
        PA->selectParam(b);
}
} // namespace

void cPadAssign::initDisplayControls()
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
        paramNamePtrs[i] = paramNames[i];
        paramValuePtrs[i] = paramValues[i];
        snprintf(paramNames[i], sizeof paramNames[i], "%s", kListNames[i]);
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
        p.text = (char*)kParamNames[i];
        p.colors = display.getTheme().activeLabelsColors;
        p.style = controlStyleCenterX | controlStyleFont3 | controlStyleCenterY;
        p.x = (800 / 8) * i + (800 / 16);
        p.w = 800 / 8 - 6;
        p.y = 424;
        p.h = 55;
        p.value = 1; // two lines: name, value
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

void cPadAssign::destroyDisplayControls()
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

void cPadAssign::start(uint32_t options)
{
    (void)options;
    moduleRefresh = 1;

    // mode buttons leave for their screens, as on every Tracker screen
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

void cPadAssign::stop()
{
    moduleRefresh = 0;
}

void cPadAssign::selectPad(int pad)
{
    if (pad < 0 || pad >= kPads)
        return;
    selPad = pad;
    refresh();
}

void cPadAssign::showBank(int bank)
{
    if (bank < 0 || bank >= kPadBanks)
        return;
    selPad = bank * kPadsPerBank + selPad % kPadsPerBank;
    refresh();
}

void cPadAssign::moveSelection(int dx, int dy)
{
    int bank = selPad / kPadsPerBank, n = selPad % kPadsPerBank;
    int col = n % 4 + dx, row = n / 4 + dy; // row 0 = pads 1-4 (bottom)
    col = col < 0 ? 0 : (col > 3 ? 3 : col);
    row = row < 0 ? 0 : (row > 3 ? 3 : row);
    selPad = bank * kPadsPerBank + row * 4 + col;
    refresh();
}

void cPadAssign::moveParam(int steps)
{
    selectParam(param + steps);
}

void cPadAssign::selectParam(int p)
{
    p = p < 0 ? 0 : (p >= ParamCount ? ParamCount - 1 : p);
    param = (uint8_t)p;
    refresh();
}

void cPadAssign::fillValues()
{
    const strPadSlot* p = padSlot(selPad);
    if (!p)
        return;
    programName(p->program, paramValues[ParamProgram], sizeof paramValues[0]);
    snprintf(paramValues[ParamTrack], sizeof paramValues[0], "Track %d", p->track + 1);
    if (p->note < 0)
        snprintf(paramValues[ParamTune], sizeof paramValues[0], "0 (own pitch)");
    else
    {
        int st = p->note - 60; // C5 = the sample's own pitch
        snprintf(paramValues[ParamTune], sizeof paramValues[0], "%+d st", st);
    }
    snprintf(paramValues[ParamLevel], sizeof paramValues[0], "100%%"); // per-pad level: 5d-3
    snprintf(paramValues[ParamMode], sizeof paramValues[0], "%s%s", padModeName(p->mode),
             padModeOf(p->mode) == kPadModeAuto ? (padIsGate(selPad) ? " (gate)" : " (one shot)") : "");
    uint8_t g = padGroupOf(p->mode);
    if (g)
        snprintf(paramValues[ParamChoke], sizeof paramValues[0], "%u", g);
    else
        snprintf(paramValues[ParamChoke], sizeof paramValues[0], "Off");
    snprintf(paramValues[ParamCut], sizeof paramValues[0], "%s", padCutOf(p->mode) ? "On" : "Off");
    uint8_t c = padColour(selPad);
    if (c)
        snprintf(paramValues[ParamColour], sizeof paramValues[0], "%s", kPadColourNames[c]);
    else
        snprintf(paramValues[ParamColour], sizeof paramValues[0], "Auto (%s)",
                 kPadColourNames[p->track % kPadColours + 1]);

    // the labels' short values
    if (p->program == kPadNoProgram)
        snprintf(labelValues[ParamProgram], sizeof labelValues[0], "--");
    else
        snprintf(labelValues[ParamProgram], sizeof labelValues[0], "%02d", p->program + 1);
    snprintf(labelValues[ParamTrack], sizeof labelValues[0], "%d", p->track + 1);
    snprintf(labelValues[ParamTune], sizeof labelValues[0], "%+d", p->note < 0 ? 0 : p->note - 60);
    snprintf(labelValues[ParamLevel], sizeof labelValues[0], "100%%");
    snprintf(labelValues[ParamMode], sizeof labelValues[0], "%s", padModeName(p->mode));
    snprintf(labelValues[ParamChoke], sizeof labelValues[0], "%s", g ? paramValues[ParamChoke] : "Off");
    snprintf(labelValues[ParamCut], sizeof labelValues[0], "%s", paramValues[ParamCut]);
    snprintf(labelValues[ParamColour], sizeof labelValues[0], "%s", c ? kPadColourNames[c] : "Auto");
}

void cPadAssign::refresh()
{
    int bank = selPad / kPadsPerBank;
    for (int n = 0; n < kPadsPerBank; n++)
    {
        int pad = bank * kPadsPerBank + n;
        const strPadSlot* p = padSlot(pad);
        strPadGridData::Cell& c = gridData.cells[n];
        padName(pad, c.name);
        bool none = !p || p->program == kPadNoProgram;
        bool empty = none || programType(p->program) == kProgramEmpty;
        if (none)
            c.program[0] = 0;
        else
            programName(p->program, c.program, sizeof c.program);
        if (p)
        {
            uint8_t g = padGroupOf(p->mode);
            char gs[6] = "";
            if (g)
                snprintf(gs, sizeof gs, " G%u", g);
            snprintf(c.marks, sizeof c.marks, "T%d%s%s", p->track + 1, gs, padCutOf(p->mode) ? " Cut" : "");
            c.colour = empty ? 0 : rgb(padColourRgb(pad, p->track));
        }
        c.empty = empty;
    }
    gridData.selected = (int8_t)(selPad % kPadsPerBank);
    display.setControlData(grid, &gridData);
    display.refreshControl(grid);

    fillValues();
    listData.start = param;
    display.setControlData(list, &listData);
    display.setControlValue(list, param);
    display.refreshControl(list);

    for (int i = 0; i < 8; i++)
    {
        display.setControlText2(label[i], labelValues[i]);
        display.setControlColors(label[i], i == param ? display.getTheme().highlightedButtonLabelsColors
                                                      : display.getTheme().activeLabelsColors);
        display.refreshControl(label[i]);
    }

    char name[4];
    padName(selPad, name);
    snprintf(title, sizeof title, "Pad Assign  Bank %c", 'A' + bank);
    titleBar->setTitle(title);
    titleBar->setRightLabel(paramValues[ParamProgram]);
    titleBar->showRightLabel(true);
}
