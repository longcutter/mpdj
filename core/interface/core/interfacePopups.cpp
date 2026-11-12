#include "core/interfacePopups.h"
#include "core/interface.h"

#include "core/songTimer.h"
#include "mtPadBoard.h"
#include "mtPadsBacklight.h"

#include "mtSequencer.h"
#include "padToBeatNoteConverter.h"

#ifdef GUI_HANDLE_SYNTH
#include "Synth.h"
#endif

#include "config.h"
#include "patternEditor/patternEditor.h"
#include "string.h"
#include "util_math.h"

#include <map>

static void functLeftPopup();
static void functRightPopup();
static void functUpPopup();
static void functDownPopup();

static void functEncoderPopup(int16_t value);

static void functPadsPopup(uint8_t pad, uint8_t state, int16_t velo);

static void functDescriptionButton();

static void functNoteActionButton(uint8_t button, uint8_t state);

extern uint8_t functInvert();
extern cPatternEditor patternEditor;

InterfacePopups mtPopups;
static InterfacePopups* POP = &mtPopups;

// MPDJ: the Midi-track picker lists the MIDI programs only (programListSlots)
static int midiListLength()
{
    uint8_t slots[kProgramSlots];
    int n = programListSlots(true, slots);
    return n > 0 ? n : 1;
}

static uint8_t midiListSlot(int pos)
{
    uint8_t slots[kProgramSlots];
    int n = programListSlots(true, slots);
    if (n == 0)
        return mtProject.values.lastUsedInstrument; // no MIDI programs yet
    return slots[pos < 0 ? 0 : (pos < n ? pos : n - 1)];
}

//=====================================================================================================
//
//=====================================================================================================

std::map<int, int> idxToNoteOrder = {
    { 0, 36 },  { 1, 37 },  { 2, 38 },  { 3, 39 },  { 4, 40 },  { 5, 41 },  { 6, 42 },  { 7, 43 },
    { 8, 44 },  { 9, 45 },  { 10, 46 }, { 11, 47 }, { 12, 24 }, { 13, 25 }, { 14, 26 }, { 15, 27 },
    { 16, 28 }, { 17, 29 }, { 18, 30 }, { 19, 31 }, { 20, 32 }, { 21, 33 }, { 22, 34 }, { 23, 35 },
    { 24, 12 }, { 25, 13 }, { 26, 14 }, { 27, 15 }, { 28, 16 }, { 29, 17 }, { 30, 18 }, { 31, 19 },
    { 32, 20 }, { 33, 21 }, { 34, 22 }, { 35, 23 }, { 36, 0 },  { 37, 1 },  { 38, 2 },  { 39, 3 },
    { 40, 4 },  { 41, 5 },  { 42, 6 },  { 43, 7 },  { 44, 8 },  { 45, 9 },  { 46, 10 }, { 47, 11 },
};

int noteToPadIdx(int noteIdx)
{
    for (const auto& pair : idxToNoteOrder)
    {
        if (pair.second == noteIdx)
        {
            return pair.first;
        }
    }
    assert(false);
    return (0);
}

int padToNoteIdx(int pad)
{
    return idxToNoteOrder[pad];
}

void InterfacePopups::initPopupsDisplayControls()
{

    strControlProperties prop;
    popupList.start = 0;
    popupList.linesCount = 16;
    popupList.length = 16;
    popupList.data = interfaceGlobals.ptrIntrumentsNames;
    popupList.lineWidthMax = 800 / 4 - 30;
    popupList.lineTextLenMax = 22;
    prop.x = (800 / 4) * (3) + 5;
    prop.y = 40;
    prop.w = 800 / 4;
    prop.h = 430;
    prop.data = &popupList;
    prop.colors = display.getTheme().stepPopupListColor;
    popupListControl = display.createControl<cList>(&prop);

    padNamesStruct.length = 5;
    padNamesStruct.name = interfaceGlobals.padNamesPointer;
    prop.style = 0;
    prop.x = 13;
    prop.y = 130;
    prop.w = 780;
    prop.h = 260;
    prop.colors = nullptr;
    prop.value = -1;
    prop.data = &padNamesStruct;
    keyboardControl = display.createControl<cNotePopout>(&prop);

    strControlProperties prop1;
    prop1.x = 190;
    prop1.y = 170;
    prop1.style = controlStyleValue_0_100 | controlStyleCenterX | controlStyleNoTransparency;
    prop1.h = 100;
    prop1.colors = display.getTheme().popupHorizontalBarColors;
    progressBar = display.createControl<cHorizontalBar>(&prop1);

    strControlProperties prop3;
    prop3.style = (controlStyleBackground |
                   (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0));
    prop3.x = 800 / 2 - 150;
    prop3.y = 480 / 2 - 50;
    prop3.w = 300;
    prop3.h = 70;
    prop3.data = nullptr;
    prop3.colors = display.getTheme().popupTextColors;
    textPopup = display.createControl<cTextPopup>(&prop3);

    // textBoxData
    prop3.style = 0;
    prop3.x = 0;
    prop3.y = 0;
    prop3.w = 50;
    prop3.h = 50;
    prop3.data = nullptr; //&textBoxData;
    prop3.colors = display.getTheme().stepPopupDescriptionTextBoxColors;
    textBox = display.createControl<cTextBox>(&prop3);

    strControlProperties prop2;
    prop2.style = (controlStyleCenterY | controlStyleBackground);
    prop2.x = 30;
    prop2.y = 0;
    prop2.colors = display.getTheme().stepPopupTitleLabelColor;
    textLabel1 = display.createControl<cLabel>(&prop2);
    prop2.x = 769;
    prop2.value = 0;
    prop2.colors = display.getTheme().stepPopupTitleLabelColor;
    prop2.style = (controlStyleCenterX | controlStyleFont3);
    textLabel2 = display.createControl<cLabel>(&prop2);

    prop2.style = (controlStyleBackground);
    prop2.x = 0;
    prop2.y = 0;
    prop2.w = 795;
    prop2.h = 25;
    prop2.data = nullptr;
    prop2.colors = display.getTheme().stepPopupBgLabelColor;
    bgLabel = display.createControl<cLabel>(&prop2);

    notesKeyboardData.labels = interfaceGlobals.padNamesPointer;
    prop2.text = nullptr;
    prop2.colors = display.getTheme().noteKeyboardColors;
    prop2.data = &notesKeyboardData;
    prop2.value = 0;
    prop2.style = controlStyleFont3;
    prop2.x = 0;
    prop2.w = 800;
    prop2.y = 425;
    prop2.h = 54;
    notesKeyboard = display.createControl<cNotesKeyboard>(&prop2);
}

void InterfacePopups::destroyPopupsDisplayControls()
{
    /*

    for(uint8_t i = 0; i<4; i++)
    {
        display.destroyControl(listControl[i]);
        listControl[i] = nullptr;
    }

    display.destroyControl(keyboardControl);
    keyboardControl = nullptr;

     */
}

void InterfacePopups::refreshColors()
{
    display.setControlColors(popupListControl, display.getTheme().stepPopupListColor);
    display.setControlColors(keyboardControl, display.getTheme().notePopupColors);
    display.setControlColors(progressBar, display.getTheme().popupHorizontalBarColors);
    display.setControlColors(textPopup, display.getTheme().popupTextColors);
    display.setControlColors(textBox, display.getTheme().stepPopupDescriptionTextBoxColors);
    display.setControlColors(textLabel1, display.getTheme().stepPopupTitleLabelColor);
    display.setControlColors(textLabel2, display.getTheme().stepPopupTitleLabelColor);
    display.setControlColors(bgLabel, display.getTheme().stepPopupBgLabelColor);
    display.setControlColors(notesKeyboard, display.getTheme().noteKeyboardColors);
}

void InterfacePopups::update()
{
    if (globalPopupDisplayTime > 0)
    {
        if (popupTimer > 1000)
        {
            popupTimer = 0;

            if (globalPopupDisplayTime == 1)
            {
                hide();
            }
            else
            {
                globalPopupDisplayTime--;
            }
        }
    }
}

//=====================================================================================================
// show
//=====================================================================================================

enumStepPopups InterfacePopups::getStepPopupState()
{
    return stepPopupState;
}

void InterfacePopups::reloadPadsNotes()
{
    for (uint8_t i = 0; i < 48; i++)
    {
        interfaceGlobals.padNamesPointer[i] = (char*)mtNotes[mtPadBoard.getNoteFromPad(i)];
    }
}

void InterfacePopups::showNotesPopup()
{
    display.hideAllControls();

    display.setControlPosition(bgLabel, 2, 0);
    display.setControlSize(bgLabel, 795, 26);
    display.setControlColors(bgLabel, display.getTheme().stepPopupTitleLabelColor);
    display.setControlStyle(bgLabel, controlStyleShow | controlStyleBackground);
    display.refreshControl(bgLabel);

    display.setControlText(textLabel1, " Notes");
    display.setControlPosition(textLabel1, 2, 0);
    display.setControlColors(textLabel1, display.getTheme().stepPopupTitleLabelColor);
    display.setControlSize(textLabel1, 800, 26);
    display.setControlValue(textLabel1, 0);
    display.setControlStyle(textLabel1,
                            controlStyleShow | controlStyleCenterY | controlStyleBackground |
                              controlStyleFont4);
    display.refreshControl(textLabel1);

    reloadPadsNotes();

    showActualInstrument();

    display.setControlValue(keyboardControl, selectedActualItem);
    display.setControlShow(keyboardControl);
    display.refreshControl(keyboardControl);

    display.synchronizeRefresh();
}

char* noteKeyboardNames[8];

void InterfacePopups::showNotesKeyboardPopup(bool showEditButtons)
{
    miniKeyboardEditButtonsActive = showEditButtons;

    setNoteKeyboardPopupFunct();

    reloadPadsNotes();

    int firstNoteIdx = idxToNoteOrder[miniKeyboardFirstPadIdx];
    for (int idx = 0; idx < 8; idx++)
    {
        int padIdx = noteToPadIdx(firstNoteIdx + idx);
        noteKeyboardNames[idx] = interfaceGlobals.padNamesPointer[padIdx];
    }

    if (showEditButtons)
    {
        noteKeyboardNames[noteKeyboardFunctionKeys::kOctDown] = (char*)"Oct-";
        noteKeyboardNames[noteKeyboardFunctionKeys::kNoteDown] = (char*)"Note-";

        noteKeyboardNames[noteKeyboardFunctionKeys::kNoteUp] = (char*)"Note+";
        noteKeyboardNames[noteKeyboardFunctionKeys::kOctUp] = (char*)"Oct+";
    }

    notesKeyboardData.labels = noteKeyboardNames;
    notesKeyboardData.editButtonsVisible = showEditButtons;

    display.setControlShow(notesKeyboard);
    display.refreshControl(notesKeyboard);

    display.synchronizeRefresh();
}

void InterfacePopups::hideNoteKeyboardPopup()
{
    miniKeyboardEditButtonsActive = false;
    display.setControlHide(notesKeyboard);
}

void InterfacePopups::showInstrumentsPopup(enumStepPopups mode)
{
    display.setControlPosition(bgLabel, 600 + 2, 29);
    display.setControlSize(bgLabel, 200, 450); // 419
    display.setControlColors(bgLabel, display.getTheme().stepPopupBgLabelColor);

    display.setControlStyle(bgLabel, controlStyleShow | controlStyleBackground);
    display.refreshControl(bgLabel);

    display.setControlText(textLabel1, "  Instruments");
    display.setControlColors(textLabel1, display.getTheme().stepPopupTitleLabelColor);
    display.setControlPosition(textLabel1, 600 + 1, 0);
    display.setControlSize(textLabel1, 200, 26);
    display.setControlValue(textLabel1, 0);
    display.setControlStyle(textLabel1,
                            controlStyleShow | controlStyleCenterY | controlStyleBackground |
                              controlStyleFont4);
    display.refreshControl(textLabel1);

    instrList.length = listInstruments(mode);

    ptrActualItemsList = (char**)(interfaceGlobals.ptrIntrumentsNames);

    instrList.start = selectedActualItem;
    instrList.linesCount = 14;
    instrList.data = interfaceGlobals.ptrIntrumentsNames;
    instrList.lineWidthMax = 800 / 4 - 30;
    instrList.lineTextLenMax = 21;

    display.setControlSize(popupListControl, -1, 390);
    display.setControlPosition(popupListControl, 600, 30);
    display.setControlData(popupListControl, &instrList);
    display.setControlShow(popupListControl);
    display.refreshControl(popupListControl);

    display.synchronizeRefresh();
}

void InterfacePopups::showFxesPopup()
{
    labelDoubleArrow.bitmaps[0].xValue = 600 + 15;
    labelDoubleArrow.bitmaps[0].yValue = 451;
    labelDoubleArrow.bitmaps[1].bitmapIndex = 0;

    refreshStepPopupDescription();
    display.setControlColors(bgLabel, display.getTheme().stepPopupBgLabelColor);
    display.setControlStyle(bgLabel, controlStyleShow | controlStyleBackground);
    display.refreshControl(bgLabel);

    display.setControlText(textLabel1, "  Fx");
    display.setControlValue(textLabel1, 0);
    display.setControlColors(textLabel1, display.getTheme().stepPopupTitleLabelColor);
    display.setControlStyle(textLabel1,
                            controlStyleShow | controlStyleCenterY | controlStyleBackground |
                              controlStyleFont4);
    display.refreshControl(textLabel1);

    // showActualInstrument();
    ptrActualItemsList = (char**)(interfaceGlobals.ptrFxNames);

    instrList.start = selectedActualItem;
    instrList.linesCount = 15;
    instrList.length = FX_COUNT - FX_COUNT_HIDDEN_FXes;
    instrList.data = (char**)interfaceGlobals.ptrFxNames;

    display.setControlSize(popupListControl, -1, 420);
    display.setControlPosition(popupListControl, 600, 30);
    display.setControlData(popupListControl, &instrList);
    display.setControlShow(popupListControl);
    display.refreshControl(popupListControl);

    // refreshAllList();

    display.synchronizeRefresh();
}

//=====================================================================================================
// show / hide
//=====================================================================================================
void InterfacePopups::showStepPopup(enumStepPopups stepPopupType, int8_t initVal)
{
    if (stepPopupType == stepPopupNone)
        return;

    if (stepPopupState != stepPopupNone)
    {
        hideStepPopups();
    }

    if (stepPopupType == enumStepPopups::stepPopupNote)
    {
        sequencer.stopManualNotes();
    }

    stepPopupState = stepPopupType;

    if (stepPopupType == stepPopupInstr || stepPopupType == stepPopupOnlyMidiInstr ||
        stepPopupType == stepPopupOnlySynthAndMidiInstr)
    {
        initVal = convertInstrumentIdxToPopupListPos(stepPopupType, initVal);
    }

    setStepPopupValue(initVal);

    setPopupFunct();

    switch (stepPopupState)
    {
        case stepPopupNone: break;
        case stepPopupNote: showNotesPopup(); break;
        case stepPopupFx: showFxesPopup(); break;
        case stepPopupInstr:
        case stepPopupOnlyMidiInstr:
        case stepPopupOnlySynthAndMidiInstr: showInstrumentsPopup(stepPopupState); break;
        default: break;
    }

    lightUpPads();
}

int InterfacePopups::convertInstrumentIdxToPopupListPos(enumStepPopups stepPopupType, int instrIdx)
{
    int listPos = 0;

    if (stepPopupType == stepPopupInstr)
    {
#if GUI_HANDLE_SYNTH
        listPos = instrIdx < SYNTH_INSTRUMENTS_START_IDX ? instrIdx + kSynthInstrumentsCount
                                                         : instrIdx - SYNTH_INSTRUMENTS_START_IDX;
#else
        listPos = instrIdx;
#endif
    }
#if GUI_HANDLE_SYNTH
    else if (stepPopupType == stepPopupOnlyMidiInstr)
#else
    else if (stepPopupType == stepPopupOnlyMidiInstr ||
             stepPopupType == stepPopupOnlySynthAndMidiInstr)
#endif
    {
        // MPDJ: the list holds the MIDI programs only (programListSlots)
        uint8_t slots[kProgramSlots];
        int n = programListSlots(true, slots);
        listPos = 0;
        for (int i = 0; i < n; i++)
            if (slots[i] == instrIdx)
                listPos = i;
    }
#if GUI_HANDLE_SYNTH
    else if (stepPopupType == stepPopupOnlySynthAndMidiInstr)
    {
        if (instrIdx < MIDI_INSTRUMENTS_START_IDX)
        {
            instrIdx = SYNTH_INSTRUMENTS_START_IDX;
        }
        listPos = instrIdx >= SYNTH_INSTRUMENTS_START_IDX
                    ? instrIdx - SYNTH_INSTRUMENTS_START_IDX
                    : instrIdx - MIDI_INSTRUMENTS_START_IDX + kSynthInstrumentsCount;
    }
#endif
    return listPos;
}

bool InterfacePopups::toggleNoteKeyboardPopup(noteKeyboardOption option)
{

    bool showEditButtons = option == noteKeyboardOption::kShowEditButtons;

    sequencer.stopManualNotes();

    if (option != noteKeyboardOption::kDefault || !noteKeyboardPopupActive)
    {
        noteKeyboardPopupActive = true;
        showNotesKeyboardPopup(showEditButtons);
    }
    else
    {
        noteKeyboardPopupActive = false;
        hideNoteKeyboardPopup();
    }

    return noteKeyboardPopupActive;
}

void InterfacePopups::processNotePadInput(uint8_t button, uint8_t state)
{

    if (miniKeyboardEditButtonsActive && state == buttonPress)
    {
        int8_t tempIdx = miniKeyboardFirstPadIdx;
        if (button == noteKeyboardFunctionKeys::kOctDown)
        {

            modifyMiniKeyboardFirstPadIdx(tempIdx, 12);
            toggleNoteKeyboardPopup(noteKeyboardOption::kShowEditButtons);
            return;
        }
        else if (button == noteKeyboardFunctionKeys::kNoteDown)
        {
            modifyMiniKeyboardFirstPadIdx(tempIdx, 1);
            toggleNoteKeyboardPopup(noteKeyboardOption::kShowEditButtons);
            return;
        }
        else if (button == noteKeyboardFunctionKeys::kNoteUp)
        {

            modifyMiniKeyboardFirstPadIdx(tempIdx, -1);
            toggleNoteKeyboardPopup(noteKeyboardOption::kShowEditButtons);
            return;
        }
        else if (button == noteKeyboardFunctionKeys::kOctUp)
        {
            modifyMiniKeyboardFirstPadIdx(tempIdx, -12);
            toggleNoteKeyboardPopup(noteKeyboardOption::kShowEditButtons);
            return;
        }
    }

    int firstNoteIdx = padToNoteIdx(miniKeyboardFirstPadIdx);
    int pad = noteToPadIdx(firstNoteIdx + button);

    mtInterface.ip->processPadsInput(pad, state, state == 0 ? 0 : 127);
}

int InterfacePopups::getNoteKeyboardBaseNote()
{
    return mtPadBoard.getNoteFromPad(miniKeyboardFirstPadIdx);
}

int InterfacePopups::getNoteKeyboardFirstPadIdx()
{
    return miniKeyboardFirstPadIdx;
}

void InterfacePopups::modifyMiniKeyboardFirstPadIdx(int8_t& idx, int delta)
{
    delta = -delta;

    assert(idx >= 0 && idx < interfacePadsCount);
    if (delta > 1)
    {
        delta = 12;
    }
    else if (delta < -1)
    {
        delta = -12;
    }
    int noteIdx = idxToNoteOrder[idx];

    noteIdx = clamp<int>(noteIdx + delta, 0, interfacePadsCount - 8);

    for (const auto& pair : idxToNoteOrder)
    {
        if (pair.second == noteIdx)
        {
            miniKeyboardFirstPadIdx = pair.first;
            idx = pair.first;
        }
    }

    assert(false); // nothing found in idxToNoteOrder
}

void InterfacePopups::hideStepPopups()
{
    if (stepPopupState)
    {
        stepPopupState = stepPopupNone;

        display.setControlHide(popupListControl);
        display.setControlHide(popupListControl);
        display.setControlHide(popupListControl);
        display.setControlHide(popupListControl);

        display.setControlHide(keyboardControl);

        display.setControlHide(bgLabel);
        display.setControlHide(textLabel1);
        display.setControlHide(textLabel2);

        display.setControlHide(textBox);

        mtInterface.ip->clearButton(interfaceButtonRec);
        //		mtInterface.ip->clearButton(interfaceButtonShift);
        //		mtInterface.ip->clearButton(interfaceButtonEnter);

        mtInterface.ip->clearButton(interfaceButtonLeft);
        mtInterface.ip->clearButton(interfaceButtonRight);
        mtInterface.ip->clearButton(interfaceButtonUp);
        mtInterface.ip->clearButton(interfaceButtonDown);

        mtInterface.ip->clearAllPots();
        mtInterface.ip->clearGlobalPads();
    }

    padsBacklight.clearGlobalPads(0, 1, 1);

    if (noteKeyboardPopupActive == true)
    {
        showNotesKeyboardPopup();
    }
}

void InterfacePopups::setPopupFunct()
{
    mtInterface.ip->clearButtonsRange(interfaceButton0, interfaceButton7);

    switch (stepPopupState)
    {
        case stepPopupNote:
            mtInterface.ip->clearButton(interfaceButtonInstr);
            mtInterface.ip->clearButton(interfaceButtonFx1);
            mtInterface.ip->clearButton(interfaceButtonFx2);
            break;
        case stepPopupInstr:
        case stepPopupOnlyMidiInstr:
        case stepPopupOnlySynthAndMidiInstr:
            mtInterface.ip->clearButton(interfaceButtonNote);
            mtInterface.ip->clearButton(interfaceButtonFx1);
            mtInterface.ip->clearButton(interfaceButtonFx2);
            break;
        case stepPopupFx:
            mtInterface.ip->clearButton(interfaceButtonNote);
            mtInterface.ip->clearButton(interfaceButtonInstr);
            mtInterface.ip->setButtonObj(interfaceButton6, buttonPress, functDescriptionButton);
            mtInterface.ip->setButtonObj(interfaceButton7, buttonPress, functDescriptionButton);
            break;
        default: break;
    }

    mtInterface.ip->setPadsGlobal(functPadsPopup);
    mtInterface.ip->clearButton(interfaceButtonRec);

    mtInterface.ip->setButtonObj(interfaceButtonLeft, buttonPress, functLeftPopup);
    mtInterface.ip->setButtonObj(interfaceButtonRight, buttonPress, functRightPopup);
    mtInterface.ip->setButtonObj(interfaceButtonUp, buttonPress, functUpPopup);
    mtInterface.ip->setButtonObj(interfaceButtonDown, buttonPress, functDownPopup);

    mtInterface.ip->clearButton(interfaceButtonShift);

    mtInterface.ip->clearButton(interfaceButtonCopy);
    mtInterface.ip->clearButton(interfaceButtonDelete);

    mtInterface.ip->setPotObj(interfacePot0, functEncoderPopup);
}

void InterfacePopups::setNoteKeyboardPopupFunct()
{
    mtInterface.ip->clearButtonsRange(interfaceButton0, interfaceButton7);

    mtInterface.ip->setButtonObj(interfaceButton0, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton1, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton2, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton3, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton4, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton5, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton6, functNoteActionButton);
    mtInterface.ip->setButtonObj(interfaceButton7, functNoteActionButton);
}

//=====================================================================================================
//
//=====================================================================================================
// value = desired list position
void InterfacePopups::setStepPopupValue(int16_t value)
{
    if (stepPopupState == stepPopupNone)
    {
        return;
    }

    switch (stepPopupState)
    {
        case stepPopupNote:
        {
            selectedActualItem = clamp<int8_t>(value, 0, 47);
            selectPadOnPopup(selectedActualItem);
            mtProject.values.lastUsedNote = mtPadBoard.convertPadToNote(selectedActualItem);
            break;
        }
        case stepPopupInstr:
        {
            value = clamp<int8_t>(value, 0, kTotalInstrumentsCount - 1);
            selectedActualItem = value;
#if GUI_HANDLE_SYNTH
            if (value < kSynthInstrumentsCount)
            {
                mtProject.values.lastUsedInstrument = SYNTH_INSTRUMENTS_START_IDX + value;
            }
            else
            {
                mtProject.values.lastUsedInstrument = value - kSynthInstrumentsCount;
            }
#else
            mtProject.values.lastUsedInstrument = value;
#endif

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);
            break;
        }
        case stepPopupOnlySynthAndMidiInstr:
        {
            value = clamp<int8_t>(value,
                                  0,
                                  midiListLength() // MPDJ: MIDI programs
#if GUI_HANDLE_SYNTH
                                    + kSynthInstrumentsCount
#endif
                                    - 1);

            selectedActualItem = value;
#if GUI_HANDLE_SYNTH
            if (selectedActualItem < kSynthInstrumentsCount)
            {
                mtProject.values.lastUsedInstrument =
                  SYNTH_INSTRUMENTS_START_IDX + selectedActualItem;
            }
            else
            {
                mtProject.values.lastUsedInstrument =
                  MIDI_INSTRUMENTS_START_IDX + (selectedActualItem - kSynthInstrumentsCount);
            }
#else
            mtProject.values.lastUsedInstrument = midiListSlot(selectedActualItem);
#endif

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);
            break;
        }
        case stepPopupOnlyMidiInstr:
        {
            selectedActualItem = clamp<int8_t>(value, 0, midiListLength() - 1);
            mtProject.values.lastUsedInstrument = midiListSlot(selectedActualItem);

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);
            break;
        }
        case stepPopupFx:
        {
            selectedActualItem = clamp<int8_t>(value, 0, FX_COUNT - 1 - FX_COUNT_HIDDEN_FXes);
            mtProject.values.lastUsedFx = selectedActualItem;

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);

            if (mtConfig.interface.fxPopupDescription)
            {
                mtPopups.refreshFxDescription();
            }

            break;
        }
        default: break;
    }

    lightUpPads();
}

void InterfacePopups::changeStepPopupValue(int16_t value, InterfacePopups::PopupDir dir)
{
    if (stepPopupState == stepPopupNone)
    {
        return;
    }

    if (stepPopupState == stepPopupNote)
    {
#ifdef TRACKER_MINI
        switch (dir)
        {
            case InterfacePopups::PopupDir::kUp: value = -12; break;
            case InterfacePopups::PopupDir::kRight: value = -1; break;
            case InterfacePopups::PopupDir::kDown: value = 12; break;
            case InterfacePopups::PopupDir::kLeft: value = 1; break;
            case InterfacePopups::PopupDir::kNotSet: break;
        }
#else
        switch (dir)
        {
            case InterfacePopups::PopupDir::kUp: value = -12; break;
            case InterfacePopups::PopupDir::kRight: value = 1; break;
            case InterfacePopups::PopupDir::kDown: value = 12; break;
            case InterfacePopups::PopupDir::kLeft: value = -1; break;
            case InterfacePopups::PopupDir::kNotSet: break;
        }
#endif
    }
    else if (dir > 0)
    {
        value = (dir == 1 ? -1 : (dir == 2 ? 12 : (dir == 3 ? 1 : (-12))));
    }

    switch (stepPopupState)
    {
        case stepPopupNote:
        {
#ifdef TRACKER_MINI
            mtPopups.modifyMiniKeyboardFirstPadIdx(selectedActualItem, value);
#else
            selectedActualItem = clamp<int8_t>(selectedActualItem + value, 0, 47);
#endif
            selectPadOnPopup(selectedActualItem);
            mtProject.values.lastUsedNote = mtPadBoard.convertPadToNote(selectedActualItem);
            break;
        }
        case stepPopupInstr:
        {
            selectedActualItem =
              clamp<int8_t>(selectedActualItem + value, 0, kTotalInstrumentsCount - 1);

#if GUI_HANDLE_SYNTH
            if (selectedActualItem < kSynthInstrumentsCount)
            {
                mtProject.values.lastUsedInstrument =
                  SYNTH_INSTRUMENTS_START_IDX + selectedActualItem;
            }
            else
            {
                mtProject.values.lastUsedInstrument = selectedActualItem - kSynthInstrumentsCount;
            }
#else
            mtProject.values.lastUsedInstrument = selectedActualItem;
#endif

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);

            break;
        }
        case stepPopupOnlySynthAndMidiInstr:
        {
            selectedActualItem = clamp<int8_t>(selectedActualItem + value,
                                               0,
                                               midiListLength() // MPDJ: MIDI programs
#if GUI_HANDLE_SYNTH
                                                 + kSynthInstrumentsCount
#endif
                                                 - 1);

#if GUI_HANDLE_SYNTH
            if (selectedActualItem < kSynthInstrumentsCount)
            {
                mtProject.values.lastUsedInstrument =
                  SYNTH_INSTRUMENTS_START_IDX + selectedActualItem;
            }
            else
            {
                mtProject.values.lastUsedInstrument =
                  MIDI_INSTRUMENTS_START_IDX + (selectedActualItem - kSynthInstrumentsCount);
            }
#else
            mtProject.values.lastUsedInstrument = midiListSlot(selectedActualItem);
#endif

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);
            break;
        }
        case stepPopupOnlyMidiInstr:
        {
            selectedActualItem =
              clamp<int8_t>(selectedActualItem + value, 0, midiListLength() - 1);

            mtProject.values.lastUsedInstrument = midiListSlot(selectedActualItem);

            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);
            break;
        }
        case stepPopupFx:
        {
            padsBacklight.setBackLayer(0, 0, selectedActualItem);

            selectedActualItem =
              clamp<int8_t>(selectedActualItem + value, 0, FX_COUNT - 1 - FX_COUNT_HIDDEN_FXes);

            mtProject.values.lastUsedFx = selectedActualItem;
            display.setControlValue(popupListControl, selectedActualItem);
            display.refreshControl(popupListControl);

            if (mtConfig.interface.fxPopupDescription)
            {
                mtPopups.refreshFxDescription();
            }
            break;
        }

        default: break;
    }

    lightUpPads();
}

void InterfacePopups::refreshFxDescription()
{
    display.setControlValue(textBox, 10);
    display.setControlText(textBox, interfaceGlobals.fxDescPtr(selectedActualItem));
    // display.setControlShow(textBox);
    display.refreshControl(textBox);
}

int16_t InterfacePopups::getStepPopupValue()
{
    return selectedActualItem;
}

void InterfacePopups::lightUpPads()
{
    if (stepPopupState == stepPopupNone)
    {
        return;
    }

    padsBacklight.clearGlobalPads(0, 1, 1);

    switch (stepPopupState)
    {
        case stepPopupInstr:
        case stepPopupOnlyMidiInstr:
        case stepPopupOnlySynthAndMidiInstr:
        {
            if (selectedActualItem >= 0 && selectedActualItem < 48)
            {
                padsBacklight.setBackLayer(1, mtConfig.values.padsLightBack, selectedActualItem);
            }
            break;
        }
        case stepPopupFx:
        {
            uint8_t show_fx = mtProject.values.lastUsedFx;

            if (show_fx < 48)
            {
                padsBacklight.setBackLayer(1, mtConfig.values.padsLightBack, show_fx);
            }
            break;
        }

        default: break;
    }
}

//=====================================================================================================
//
//=====================================================================================================

void InterfacePopups::showActualInstrument()
{
    static char actualInstrName[SAMPLE_NAME_SIZE + 4];

    uint8_t i = mtProject.values.lastUsedInstrument;

    sprintf(actualInstrName, "%d.", i + 1);

    strncat(actualInstrName, mtProject.instrument[i].sample.file_name, 25);

    display.setControlText(textLabel2, actualInstrName);
    display.setControlColors(textLabel2, display.getTheme().stepPopupTitleLabelColor);
    display.setControlStyle(textLabel2, controlStyleShow | controlStyleCenterY);
    display.setControlPosition(textLabel2, 585, 0);
    display.refreshControl(textLabel2);
}

int InterfacePopups::listInstruments(enumStepPopups mode)
{
    int instrIdx = 0;

#if GUI_HANDLE_SYNTH
    if (mode != stepPopupOnlyMidiInstr)
    {
        for (uint8_t i = 0; i < kSynthInstrumentsCount; i++)
        {
            strncpy(&interfaceGlobals.intrumentsNames[instrIdx][0],
                    mtInterface.synth->getSynthName(i).c_str(),
                    SAMPLE_NAME_SIZE);
            interfaceGlobals.intrumentsNames[instrIdx][SAMPLE_NAME_SIZE] = 0;
            interfaceGlobals.ptrIntrumentsNames[instrIdx] =
              &interfaceGlobals.intrumentsNames[instrIdx][0];
            instrIdx++;
        }
    }
#endif

    // MPDJ: one list of the 64 program slots by number, or only the MIDI
    // programs (Midi tracks). List position -> slot: midiListSlot()
    bool midiOnly = mode == stepPopupOnlyMidiInstr || mode == stepPopupOnlySynthAndMidiInstr;
    uint8_t slots[kProgramSlots];
    int n = programListSlots(midiOnly, slots);
    for (int k = 0; k < n; k++)
    {
        int i = slots[k];
        char* name = &interfaceGlobals.intrumentsNames[instrIdx][0];
        if (isMidiInstrument(i))
        {
            sprintf(name, "%d. MIDI Channel %d", i + 1, instrumentIdxToMidiChannel(i));
        }
        else
        {
            sprintf(name, "%d. ", i + 1);
            if (mtProject.instrument[i].isActive)
            {
                strncat(name, mtProject.instrument[i].sample.file_name, SAMPLE_NAME_SIZE);
                name[SAMPLE_NAME_SIZE + 5] = 0;
            }
        }
        interfaceGlobals.ptrIntrumentsNames[instrIdx] = name;
        instrIdx++;
    }
    if (n == 0) // a list control needs a line
    {
        strcpy(&interfaceGlobals.intrumentsNames[instrIdx][0], "No MIDI programs");
        interfaceGlobals.ptrIntrumentsNames[instrIdx] = &interfaceGlobals.intrumentsNames[instrIdx][0];
        instrIdx++;
    }

    return instrIdx;
}

// void InterfacePopups::refreshAllList()
//{
//	for(uint8_t i = 0; i<4; i++)
//	{
//		if(selectedActualItem >= i*12 && selectedActualItem < (i+1)*12)
//		{
//			popupList.start = selectedActualItem%12;
//		}
//		else
//		{
//			popupList.start = -1;
//		}
//
//		popupList.length = 12;
//		popupList.linesCount = 12;
//		popupList.data = ptrActualItemsList+(i*12);
//
//
//		display.setControlData(listControl, &popupList);
//		display.setControlShow(listControl);
//
//		refreshList(i);
//	}
//}
//
//
// void InterfacePopups::refreshList(uint8_t n)
//{
//	int8_t position = -1;
//
//	if(selectedActualItem >= n*12 && selectedActualItem < (n+1)*12)
//	{
//		position = selectedActualItem%12;
//	}
//
//	display.setControlValue(listControl, position);
//	display.refreshControl(listControl);
//}

void InterfacePopups::selectPadOnPopup(int8_t pad)
{
    display.setControlValue(keyboardControl, pad);
    display.refreshControl(keyboardControl);
}

void InterfacePopups::toggleStepPopupDescription()
{
    mtConfig.interface.fxPopupDescription = !mtConfig.interface.fxPopupDescription;
    refreshStepPopupDescription();
}

void InterfacePopups::refreshStepPopupDescription()
{
    labelDoubleArrow.bitmaps[0].xValue = 600 + 15;
    labelDoubleArrow.bitmaps[0].yValue = 451;
    labelDoubleArrow.bitmaps[1].bitmapIndex = 0;

    display.setControlData(textLabel2, &labelDoubleArrow);
    display.setControlText(textLabel2, "Description");
    display.setControlStyle(textLabel2,
                            controlStyleShow | controlStyleCenterY | controlStyleFont1 |
                              controlStyleShowBitmap);
    display.setControlColors(textLabel2, display.getTheme().activeButtonLabelsColors);
    display.setControlPosition(textLabel2, 600 + 40, 445);
    display.setControlSize(textLabel2, 200, 26);

    if (mtConfig.interface.fxPopupDescription)
    {
        display.setControlPosition(textLabel1, 360 + 1, 0);
        display.setControlSize(textLabel1, 440, 26);
        display.setControlValue(textLabel1, 0);
        display.refreshControl(textLabel1);

        display.setControlSize(bgLabel, 440, 450);
        display.setControlPosition(bgLabel, 360, 29);
        display.refreshControl(bgLabel);

        labelDoubleArrow.bitmaps[0].bitmapIndex = displayDoubleArrowR;
        display.refreshControl(textLabel2);

        display.setControlSize(textBox, 240, 479);
        display.setControlPosition(textBox, 360, 29);
        display.setControlShow(textBox);

        mtPopups.refreshFxDescription();

        if (!(patternEditor.trackerPattern.popupMode & 4))
        {
            patternEditor.trackerPattern.popupMode |= 4;
            display.refreshControl(patternEditor.patternControl);
        }
    }
    else
    {
        display.setControlPosition(textLabel1, 600 + 1, 0);
        display.setControlSize(textLabel1, 200, 26);
        display.setControlValue(textLabel1, 0);
        display.refreshControl(textLabel1);

        display.setControlPosition(bgLabel, 600 + 2, 29);
        display.setControlSize(bgLabel, 200, 450); // 419
        display.refreshControl(bgLabel);

        labelDoubleArrow.bitmaps[0].bitmapIndex = displayDoubleArrowL;
        display.refreshControl(textLabel2);

        display.setControlHide(textBox);

        if (patternEditor.trackerPattern.popupMode & 4)
        {
            patternEditor.trackerPattern.popupMode &= ~(4);
            display.refreshControl(patternEditor.patternControl);
        }
    }
}

//=========================================================================================================
static void functLeftPopup()
{
    mtPopups.changeStepPopupValue(0, InterfacePopups::PopupDir::kLeft);
}

static void functRightPopup()
{
    mtPopups.changeStepPopupValue(0, InterfacePopups::PopupDir::kRight);
}

static void functUpPopup()
{
    mtPopups.changeStepPopupValue(0, InterfacePopups::PopupDir::kUp);
}

static void functDownPopup()
{
    mtPopups.changeStepPopupValue(0, InterfacePopups::PopupDir::kDown);
}

//=========================================================================================================
static void functEncoderPopup(int16_t value)
{
    mtPopups.changeStepPopupValue(interfaceGlobals.invertValForList ? -value : value);
}

//=========================================================================================================
static void functDescriptionButton()
{
    mtPopups.toggleStepPopupDescription();
}

//=========================================================================================================
static void functNoteActionButton(uint8_t button, uint8_t state)
{
    mtPopups.processNotePadInput(button, state);
}

//=========================================================================================================
static void functPadsPopup(uint8_t pad, uint8_t state, int16_t)
{
    if (state == buttonPress)
    {
        padsBacklight.setFrontLayer(1, mtConfig.values.padsLightBack, pad);
        mtPopups.setStepPopupValue(pad);

        switch (POP->getStepPopupState())
        {
            case stepPopupNote:
            {
                uint8_t noteFromPad = mtPadBoard.getNoteFromPad(pad);
                sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, noteFromPad, -1, pad);
                break;
            }
            case stepPopupInstr:
            {
                int8_t noteToPlay = Sequencer::STEP_NOTE_DEFAULT;
                if (mtInterface.isPatternScreenActive())
                {
                    sendSelection();
                    noteToPlay = sequencer.getSelectionFirstNote();
                }
                sequencer.handleNoteOn(Sequencer::GRID_OUTSIDE_PATTERN, noteToPlay, -1, pad);
                break;
            }
            default: break;
        }
    }
    else if (state == buttonRelease)
    {
        padsBacklight.setFrontLayer(0, 0, pad);

        enumStepPopups popupState = POP->getStepPopupState();
        switch (popupState)
        {
            case stepPopupNote:
            {
                uint8_t noteFromPad = mtPadBoard.getNoteFromPad(pad);
                sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, noteFromPad, 0, pad);
                break;
            }
            case stepPopupInstr:
            {
                int8_t noteToPlay = Sequencer::STEP_NOTE_DEFAULT;
                if (mtInterface.isPatternScreenActive())
                {
                    sendSelection();
                    noteToPlay = sequencer.getSelectionFirstNote();
                }
                sequencer.handleNoteOff(Sequencer::GRID_OUTSIDE_PATTERN, noteToPlay, 0, pad);
                break;
            }
            default: break;
        }
    }
}

//=========================================================================================================
//=========================================================================================================
//=========================================================================================================

/*
//	wywolywanie popupu	- max 4 linie tekstu
void show(config_slot, linia_1_textu);
void show(config_slot, linia_1_textu,linia_2_textu);
void show(config_slot, linia_1_textu,linia_1_textu...);

//	 ukrywanie jesli potrzba szybciej niz ustawiony czas
void hide();

//	configuracja wygladu popupu
void config(slot, struktura_konfigutracji);
*/
void InterfacePopups::show(uint8_t config_slot, const char* line1, uint8_t time)
{
    textLines[0] = (char*)line1;

    show(config_slot, textLines, 1, time);
}

void InterfacePopups::show(uint8_t config_slot, const char* line1, const char* line2, uint8_t time)
{
    textLines[0] = (char*)line1;
    textLines[1] = (char*)line2;

    show(config_slot, textLines, 2, time);
}

void InterfacePopups::show(uint8_t config_slot,
                           const char* line1,
                           const char* line2,
                           const char* line3,
                           uint8_t time)
{
    textLines[0] = (char*)line1;
    textLines[1] = (char*)line2;
    textLines[2] = (char*)line3;

    show(config_slot, textLines, 3, time);
}

void InterfacePopups::show(uint8_t config_slot,
                           const char* line1,
                           const char* line2,
                           const char* line3,
                           const char* line4,
                           uint8_t time)
{
    textLines[0] = (char*)line1;
    textLines[1] = (char*)line2;
    textLines[2] = (char*)line3;
    textLines[3] = (char*)line4;

    show(config_slot, textLines, 4, time);
}

void InterfacePopups::show(uint8_t, char** multiLineText, uint8_t lines_count, uint8_t time)
{
    // ustawienie czasu
    popupTimer = 0;

    globalPopupDisplayTime = time != 0 ? time : globalConfig.time;

    // kopiowanie configu
    popupData.textLinesCount = lines_count;
    popupData.multiLineText = multiLineText;

    for (uint8_t i = 0; i < textLinesCount; i++)
    {
        textStyles[i] = &globalConfig.lineStyle[i];
        textColors[i] = &globalConfig.lineColor[i];
    }

    popupData.multiLineStyle = textStyles;
    popupData.multiLineColors = textColors;

    // odsiwezenie kontrolki
    display.setControlData(textPopup, &popupData);
    display.setControlPosition(textPopup, globalConfig.x, globalConfig.y);
    display.setControlSize(textPopup, globalConfig.w, globalConfig.h);
    display.setControlStyle(
      textPopup,
      controlStyleShow | controlStyleCenterY | controlStyleCenterX | controlStyleBackground |
        (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0) |
        controlStyleNoTransparency);
    display.setControlShow(textPopup);
    display.refreshControl(textPopup);
    display.setControlHide(progressBar);

    display.synchronizeRefresh();
}

void InterfacePopups::showProgramsImportNotice()
{
    if (programsTakeImportNotice())
    {
        show(0,
             "Polyend Tracker project imported:",
             "MIDI instruments M01-M16 are now",
             "programs 49-64 (MIDI channels 1-16).",
             8);
    }
}

void InterfacePopups::hide()
{
    if (globalPopupDisplayTime)
    {
        globalPopupDisplayTime = 0;

        display.setControlHide(progressBar);
        display.setControlHide(textPopup);
        display.refreshControl(textPopup);
    }
}

void InterfacePopups::config(const strPopupStyleConfig* config)
{
    memcpy(&globalConfig, config, sizeof(globalConfig));
}

// ##############################################################################################################
// ##############################################################################################################
// ##############################################################################################################
void InterfacePopups::showProgressPopup(const char* text)
{
    globalPopupDisplayTime = 0;

    textLines[0] = (char*)text;
    textLines[1] = nullptr;

    progressPopupTextStyle = controlStyleCenterX | controlStyleCenterY;
    progressPopupTextColor = 0xffffffff;

    popupData.textLinesCount = 2;
    popupData.multiLineText = textLines;

    for (uint8_t i = 0; i < textLinesCount; i++)
    {
        textStyles[i] = &progressPopupTextStyle;
        textColors[i] = &progressPopupTextColor;
    }

    popupData.multiLineStyle = textStyles;
    popupData.multiLineColors = textColors;

    // odsiwezenie kontrolki
    display.setControlData(textPopup, &popupData);
    display.setControlPosition(textPopup, 800 / 2, 480 / 2 - 15);
    display.setControlSize(textPopup, 300, 70);
    display.setControlStyle(
      textPopup,
      controlStyleShow | controlStyleBackground | controlStyleCenterX | controlStyleCenterY |
        (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0) |
        controlStyleNoTransparency);
    display.setControlShow(textPopup);
    display.refreshControl(textPopup);

    display.setControlValue(progressBar, 0);
    display.setControlPosition(progressBar, 250, 195);
    display.setControlSize(progressBar, 300, 70);
    display.setControlValue(progressBar, 0);

    display.synchronizeRefresh();
}

void InterfacePopups::changePopupProgress(int8_t value)
{
    display.setControlShow(progressBar);
    display.setControlValue(progressBar, value);
    display.refreshControl(progressBar);

    display.synchronizeRefresh();
}

void InterfacePopups::hideProgressPopup()
{
    display.setControlValue(progressBar, 0);
    display.setControlHide(textPopup);
    display.setControlHide(progressBar);
    display.refreshControl(progressBar);
}

void InterfacePopups::showInfoPopup(const char* text1, const char* text2)
{
    globalPopupDisplayTime = 0;

    textLines[0] = (char*)text1;
    if (text2 == nullptr)
    {
        textLines[1] = nullptr;
        popupData.textLinesCount = 1;
    }
    else
    {
        textLines[1] = (char*)text2;
        popupData.textLinesCount = 2;
    }

    progressPopupTextStyle = controlStyleCenterX | controlStyleCenterY;
    progressPopupTextColor = 0xffffffff;

    popupData.multiLineText = textLines;

    for (uint8_t i = 0; i < textLinesCount; i++)
    {
        textStyles[i] = &progressPopupTextStyle;
        textColors[i] = &progressPopupTextColor;
    }

    popupData.multiLineStyle = textStyles;
    popupData.multiLineColors = textColors;

    // odsiwezenie kontrolki
    display.setControlData(textPopup, &popupData);
    display.setControlPosition(textPopup, 800 / 2, 480 / 2);
    display.setControlSize(textPopup, 450, 80);
    display.setControlStyle(
      textPopup,
      controlStyleShow | controlStyleCenterY | controlStyleCenterX | controlStyleBackground |
        (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0) |
        controlStyleNoTransparency);
    display.setControlShow(textPopup);
    display.refreshControl(textPopup);
    display.setControlHide(progressBar);

    display.synchronizeRefresh();
}

void InterfacePopups::hideInfoPopup()
{
    display.setControlHide(progressBar);
    display.setControlHide(textPopup);
    display.refreshControl(textPopup);
}

void InterfacePopups::showMasterVolumePopup()
{
    popupTimer = 0;
    globalPopupDisplayTime = 2;

    float masterVolumeInDecybels = map((float)(mtConfig.audioCodecConfig.volume / 100.0f),
                                       0.0f,
                                       1.0f,
                                       MIN_ANALOG_VOLUME_IN_DB,
                                       MAX_ANALOG_VOLUME_IN_DB);

    if (mtConfig.audioCodecConfig.volume == MASTER_VOLUME_DEFAULT)
    {
        masterVolumeInDecybels = 0;
    }

    snprintf(masterVolumeValueText, 12, "%0.2f dB", masterVolumeInDecybels);

    textLines[0] = (char*)"Master Volume";
    textLines[1] = nullptr;
    textLines[2] = masterVolumeValueText;

    progressPopupTextStyle = controlStyleCenterX | controlStyleCenterY;
    progressPopupTextColor = 0xffffffff;

    popupData.textLinesCount = 3;
    popupData.multiLineText = textLines;

    for (uint8_t i = 0; i < textLinesCount; i++)
    {
        textStyles[i] = &progressPopupTextStyle;
        textColors[i] = &progressPopupTextColor;
    }

    popupData.multiLineStyle = textStyles;
    popupData.multiLineColors = textColors;

    display.setControlData(textPopup, &popupData);
    display.setControlPosition(textPopup, 800 / 2, 480 / 2 - 15);
    display.setControlSize(textPopup, 300, 70);
    display.setControlStyle(
      textPopup,
      controlStyleShow | controlStyleCenterY | controlStyleCenterX | controlStyleBackground |
        (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0) |
        controlStyleNoTransparency);
    display.setControlShow(textPopup);
    display.refreshControl(textPopup);

    display.setControlValue(progressBar, mtConfig.audioCodecConfig.volume);
    display.setControlPosition(progressBar, 250, 177);
    display.setControlSize(progressBar, 300, 80);
    display.setControlShow(progressBar);
    display.refreshControl(progressBar);

    display.synchronizeRefresh();
}
