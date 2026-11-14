// MPDJ (step 5d-1): the Pad Assign screen, see padAssign.h
#include "padAssign/padAssign.h"

#include "core/interface.h"
#include "fileManager.h"
#include "mpdjPads.h"
#include "mpdjPrograms.h"
#include "mtStructs.h"
#include "mtSequencer.h"
#include "mtAudioEngine.h"
#include "core/interfacePopups.h"

#include <stdio.h>
#include <string.h>

cPadAssign padAssign;
static cPadAssign* PA = &padAssign;

namespace
{
const char* const kParamNames[cPadAssign::ParamCount] = { "Program", "Track", "Tune", "Level",
                                                          "Mode",    "Choke", "Cut",  "Color" };
const char* const kListNames[cPadAssign::ParamCount] = { "Program", "Track",       "Tune",           "Level",
                                                         "Mode",    "Choke group", "Cut on release", "Color" };

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

void functEncoder(int16_t value) { PA->changeValue(value); }

// Transport works here as on every Tracker screen (Mike 2026-11-14: tune pads
// against the running pattern): Play toggles, Shift+Play plays the song
void functPlayAction()
{
    if (sequencer.getSeqState() == Sequencer::SEQ_STATE_STOP)
    {
        if (PA->buttons->isButtonPressed(interfaceButtonShift))
            sequencer.playSong();
        else
            sequencer.playPattern();
    }
    else
        sequencer.stop();
}

int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// 5d-3 (Mike 2026-11-14, option A): Tune and Level belong to the program, not
// the pad: the program's instrument Tune (semitones) and Volume (0-100), the
// same values as the Instrument Editor, so the pattern follows too. Per-step
// tune effects still apply on top. nullptr for no program / MIDI / empty.
strInstrument* sampleProgram(const strPadSlot* p)
{
    if (!p || p->program == kPadNoProgram || p->program >= kSampleInstrumentsCount)
        return nullptr;
    if (programType(p->program) != kProgramSample)
        return nullptr;
    return &mtProject.instrument[p->program];
}

void refreshVoices(uint32_t mask)
{
    for (uint8_t i = 0; i < audioEngine::kVoices; i++)
        instrumentPlayer[i].setRefreshBit(mask);
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
    prop.x = kGridX;
    prop.y = kGridY;
    prop.w = kGridW;
    prop.h = kGridH;
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
    ip->setButtonObj(interfaceButtonPlay, buttonPress, functPlayAction);
    ip->setPotObj(interfacePot0, functEncoder); // QL4 / main encoder: the value (5d-2)

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

// A touch on the grid (Tracker 800x480 coordinates): the pad under it, -1 if none
int cPadAssign::padAt(float x, float y) const
{
    if (x < kGridX || x >= kGridX + kGridW || y < kGridY || y >= kGridY + kGridH)
        return -1;
    int col = (int)((x - kGridX) * 4 / kGridW), row = 3 - (int)((y - kGridY) * 4 / kGridH);
    return (selPad / kPadsPerBank) * kPadsPerBank + row * 4 + col;
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

// 5d-2: one step of the selected setting per encoder message; edits go into
// the project's pad map at once (saved with the project)
void cPadAssign::changeValue(int steps)
{
    if (selPad < 0 || selPad >= kPads || steps == 0)
        return;
    strPadSlot& p = mtProject.pads.pad[selPad];
    int dir = steps > 0 ? 1 : -1;
    switch (param)
    {
    case ParamProgram:
    {
        // None, then slots 1-64 (index -1 = None)
        int v = p.program == kPadNoProgram ? -1 : p.program;
        v = clampInt(v + steps, -1, kProgramSlots - 1);
        p.program = v < 0 ? kPadNoProgram : (uint8_t)v;
        break;
    }
    case ParamTrack: p.track = (uint8_t)clampInt(p.track + steps, 0, kAllTracks - 1); break;
    case ParamTune:
    case ParamLevel:
    {
        strInstrument* in = sampleProgram(&p);
        if (!in)
            return;
        if (param == ParamTune)
        {
            in->tune = (int8_t)clampInt(in->tune + steps, MIN_INSTRUMENT_TUNE, MAX_INSTRUMENT_TUNE);
            refreshVoices(TUNE_MASK);
        }
        else
        {
            in->volume = (uint8_t)clampInt(in->volume + steps, 0, MAX_INSTRUMENT_VOLUME);
            refreshVoices(VOLUME_MASK);
        }
        p.note = kPadNoteDefault; // the 5d-2 build's per-pad tune: back to the program's pitch
        newFileManager.setInstrumentStructChanged(p.program);
        break;
    }
    case ParamMode:
    {
        int m = clampInt(padModeOf(p.mode) + dir, 0, kPadModeCount - 1);
        p.mode = padModeByte((PadMode)m, padGroupOf(p.mode), padCutOf(p.mode));
        break;
    }
    case ParamChoke: setPadGroup(selPad, (uint8_t)clampInt(padGroupOf(p.mode) + steps, 0, kPadGroups)); break;
    case ParamCut: setPadCutOnRelease(selPad, dir > 0); break;
    case ParamColor: setPadColor(selPad, (uint8_t)clampInt(padColor(selPad) + dir, 0, kPadColors)); break;
    default: return;
    }
    newFileManager.setProjectStructChanged();
    refresh();
}

// ---- 5d-4: copy -----------------------------------------------------------

namespace
{
struct PendingCopy
{
    bool active = false;
    int srcPad = -1, dstPad = -1;
    uint8_t program = 0;
} pending;

char copyMsg[48];

// The target pad gets the source's settings, keeping its own track
void applyPadCopy(int srcPad, int dstPad, uint8_t program)
{
    strPadSlot& d = mtProject.pads.pad[dstPad];
    const strPadSlot& s = mtProject.pads.pad[srcPad];
    uint8_t track = d.track;
    d = s;
    d.track = track;
    d.program = program;
    setPadColor(dstPad, padColor(srcPad));
    newFileManager.setProjectStructChanged();
}

bool slotFree(int slot)
{
    return programType(slot) == kProgramEmpty && !mtProject.instrument[slot].isActive &&
           mtProject.programs.slot[slot].type != kProgramMidi;
}

// A free slot for a copy of a sample program: the first one after the last
// sample, so no other sample moves in memory (safe while the pattern plays).
// Otherwise any free slot, and *moves = true (playback has to stop).
int freeSampleSlot(bool* moves)
{
    int last = -1;
    for (int i = 0; i < kSampleInstrumentsCount; i++)
        if (mtProject.instrument[i].isActive)
            last = i;
    for (int i = last + 1; i < kSampleInstrumentsCount; i++)
        if (slotFree(i))
        {
            *moves = false;
            return i;
        }
    for (int i = 0; i < kSampleInstrumentsCount; i++)
        if (slotFree(i))
        {
            *moves = true;
            return i;
        }
    return -1;
}

int freeSlot()
{
    for (int i = 0; i < kProgramSlots; i++)
        if (slotFree(i))
            return i;
    return -1;
}
} // namespace

const char* cPadAssign::copyPad(int dst, bool shareProgram)
{
    int src = selPad;
    if (dst < 0 || dst >= kPads || src < 0 || src >= kPads || dst == src)
        return nullptr;
    if (pending.active)
        return "Still copying";
    char sName[4], dName[4];
    padName(src, sName);
    padName(dst, dName);
    const strPadSlot& s = mtProject.pads.pad[src];

    if (shareProgram || s.program == kPadNoProgram || programType(s.program) == kProgramEmpty)
    {
        applyPadCopy(src, dst, s.program);
        selPad = dst;
        refresh();
        snprintf(copyMsg, sizeof copyMsg, "%s copied to %s", sName, dName);
        return copyMsg;
    }

    if (programType(s.program) == kProgramMidi)
    {
        int slot = freeSlot();
        if (slot < 0)
            return "No free program slot";
        mtProject.programs.slot[slot] = mtProject.programs.slot[s.program];
        applyPadCopy(src, dst, (uint8_t)slot);
        selPad = dst;
        refresh();
        snprintf(copyMsg, sizeof copyMsg, "%s copied to %s, program %d", sName, dName, slot + 1);
        return copyMsg;
    }

    bool moves = false;
    int slot = freeSampleSlot(&moves);
    if (slot < 0)
        return "No free program slot";
    if ((uint64_t)mtProject.used_memory + newFileManager.getSampleMemorySize(s.program, s.program) >=
        mtProject.max_memory)
        return "Not enough sample memory";
    if (moves && sequencer.getSeqState() != Sequencer::SEQ_STATE_STOP)
        sequencer.stop(); // samples move in memory: nothing may play them meanwhile
    if (!newFileManager.copyInstrumentsInWorkspace(s.program, 1, (uint8_t)slot))
        return "Busy - try again";
    pending.active = true;
    pending.srcPad = src;
    pending.dstPad = dst;
    pending.program = (uint8_t)slot;
    return "Copying...";
}

void cPadAssign::update()
{
    if (!pending.active)
        return;
    uint8_t st = newFileManager.getStatus();
    if (st == fmCopyingInstrumentsEnd)
    {
        newFileManager.clearStatus();
        pending.active = false;
        applyPadCopy(pending.srcPad, pending.dstPad, pending.program);
        char sName[4], dName[4];
        padName(pending.srcPad, sName);
        padName(pending.dstPad, dName);
        snprintf(copyMsg, sizeof copyMsg, "%s copied to %s, program %d", sName, dName, pending.program + 1);
        mtPopups.show(0, copyMsg);
        if (mtInterface.isPadAssignScreenActive())
        {
            selPad = pending.dstPad;
            refresh();
        }
    }
    else if (st >= fmError)
    {
        newFileManager.clearStatus();
        pending.active = false;
        mtPopups.show(0, "Copy failed");
    }
}

void cPadAssign::fillValues()
{
    const strPadSlot* p = padSlot(selPad);
    if (!p)
        return;
    programName(p->program, paramValues[ParamProgram], sizeof paramValues[0]);
    snprintf(paramValues[ParamTrack], sizeof paramValues[0], "Track %d", p->track + 1);
    const strInstrument* in = sampleProgram(p);
    if (in)
    {
        snprintf(paramValues[ParamTune], sizeof paramValues[0], in->tune ? "%+d st" : "0 st", in->tune);
        snprintf(paramValues[ParamLevel], sizeof paramValues[0], "%u", in->volume);
    }
    else
    {
        snprintf(paramValues[ParamTune], sizeof paramValues[0], "-");
        snprintf(paramValues[ParamLevel], sizeof paramValues[0], "-");
    }
    snprintf(paramValues[ParamMode], sizeof paramValues[0], "%s%s", padModeName(p->mode),
             padModeOf(p->mode) == kPadModeAuto ? (padIsGate(selPad) ? " (gate)" : " (one shot)") : "");
    uint8_t g = padGroupOf(p->mode);
    if (g)
        snprintf(paramValues[ParamChoke], sizeof paramValues[0], "%u", g);
    else
        snprintf(paramValues[ParamChoke], sizeof paramValues[0], "Off");
    snprintf(paramValues[ParamCut], sizeof paramValues[0], "%s", padCutOf(p->mode) ? "On" : "Off");
    uint8_t c = padColor(selPad);
    if (c)
        snprintf(paramValues[ParamColor], sizeof paramValues[0], "%s", kPadColorNames[c]);
    else
        snprintf(paramValues[ParamColor], sizeof paramValues[0], "Auto (%s)",
                 kPadColorNames[p->track % kPadColors + 1]);

    // the labels' short values
    if (p->program == kPadNoProgram)
        snprintf(labelValues[ParamProgram], sizeof labelValues[0], "--");
    else
        snprintf(labelValues[ParamProgram], sizeof labelValues[0], "%02d", p->program + 1);
    snprintf(labelValues[ParamTrack], sizeof labelValues[0], "%d", p->track + 1);
    if (in)
    {
        snprintf(labelValues[ParamTune], sizeof labelValues[0], in->tune ? "%+d" : "0", in->tune);
        snprintf(labelValues[ParamLevel], sizeof labelValues[0], "%u", in->volume);
    }
    else
    {
        snprintf(labelValues[ParamTune], sizeof labelValues[0], "-");
        snprintf(labelValues[ParamLevel], sizeof labelValues[0], "-");
    }
    snprintf(labelValues[ParamMode], sizeof labelValues[0], "%s", padModeName(p->mode));
    snprintf(labelValues[ParamChoke], sizeof labelValues[0], "%s", g ? paramValues[ParamChoke] : "Off");
    snprintf(labelValues[ParamCut], sizeof labelValues[0], "%s", paramValues[ParamCut]);
    snprintf(labelValues[ParamColor], sizeof labelValues[0], "%s", c ? kPadColorNames[c] : "Auto");
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
            c.color = empty ? 0 : rgb(padColorRgb(pad, p->track));
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
