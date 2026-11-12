
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

#ifndef INTERFACE_INTERFACEPOPUPS_H_
#define INTERFACE_INTERFACEPOPUPS_H_

#include "core/modulesBase.h"
#include <elapsedMillis.h>

enum enumStepPopups
{
    stepPopupNone,
    stepPopupNote,
    stepPopupInstr,
    stepPopupFx,
    stepPopupOnlyMidiInstr,
    stepPopupOnlySynthAndMidiInstr
};

struct strPopupStyleConfig
{
    uint8_t time;

    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;

    uint32_t lineColor[4];
    uint32_t lineStyle[4];
};

class InterfacePopups
{
  public:
    void update();

    // gui constrols
    void initPopupsDisplayControls();
    void destroyPopupsDisplayControls();

    void refreshColors();
    //-------------------------------------
    // step popups funct
    void showStepPopup(enumStepPopups stepPopupType, int8_t initVal);
    void hideStepPopups();

    enum noteKeyboardOption
    {
        kDefault,
        kForceShow,
        kShowEditButtons
    };

    enum noteKeyboardFunctionKeys
    {
        kOctDown = 0,
        kNoteDown = 1,
        kNoteUp = 6,
        kOctUp = 7,

    };

    bool toggleNoteKeyboardPopup(noteKeyboardOption option = noteKeyboardOption::kDefault);
    bool isNoteKeyboardPopupActive() { return noteKeyboardPopupActive; }
    void processNotePadInput(uint8_t button, uint8_t state);
    int getNoteKeyboardBaseNote();
    int getNoteKeyboardFirstPadIdx();
    void modifyMiniKeyboardFirstPadIdx(int8_t& firstPad, int delta);

    enum PopupDir
    {
        kNotSet,
        kUp,
        kRight,
        kDown,
        kLeft,
    };
    void changeStepPopupValue(int16_t value, InterfacePopups::PopupDir dir = kNotSet);
    void setStepPopupValue(int16_t value);
    hControl mpdjStepPopupList() const { return popupListControl; } // MPDJ: touch hit-testing

    enumStepPopups getStepPopupState();
    int16_t getStepPopupValue();

    void lightUpPads();

    void toggleStepPopupDescription();

    //-------------------------------------
    // blocking/progress
    void showProgressPopup(const char* text);
    void changePopupProgress(int8_t value);
    void hideProgressPopup();

    void showInfoPopup(const char* text1, const char* text2);
    // MPDJ: one-time note after a Tracker+ project was imported (mpdjPrograms)
    void showProgramsImportNotice();
    void hideInfoPopup();
    //-------------------------------------
    // global popups functs

    void show(uint8_t config_slot, const char* line1, uint8_t time = 0);
    void show(uint8_t config_slot, const char* line1, const char* line2, uint8_t time = 0);
    void show(uint8_t config_slot,
              const char* line1,
              const char* line2,
              const char* line3,
              uint8_t time = 0);
    void show(uint8_t config_slot,
              const char* line1,
              const char* line2,
              const char* line3,
              const char* line4,
              uint8_t time = 0);
    void show(uint8_t config_slot, char** multiLineText, uint8_t lines_count, uint8_t time);
    void hide();

    void config(const strPopupStyleConfig* config);

    int listInstruments(enumStepPopups mode);

    // master volume
    void showMasterVolumePopup();

  private:
    //----------------------------------------------------------
    // step
    void setPopupFunct();
    void setNoteKeyboardPopupFunct();

    void showNotesPopup();
    void showNotesKeyboardPopup(bool showEditButtons = false);
    void hideNoteKeyboardPopup();

    void showInstrumentsPopup(enumStepPopups mode);
    void showFxesPopup();

    void reloadPadsNotes();

    void showActualInstrument();

    void refreshAllList();
    void refreshList(uint8_t n);

    void selectPadOnPopup(int8_t pad);

    void refreshStepPopupDescription();
    void refreshFxDescription();

    int convertInstrumentIdxToPopupListPos(enumStepPopups stepPopupType, int instrIdx);

    hControl popupListControl;
    hControl keyboardControl;
    hControl textBox;

    hControl progressBar;

    hControl bgLabel;
    hControl textLabel1;
    hControl textLabel2;

    hControl notesKeyboard;

    strList popupList;
    strList instrList;
    strPadNames padNamesStruct;
    strLabelData labelDoubleArrow;
    strTextBoxData textBoxData;
    strcNotesKeyboardData notesKeyboardData;

    int8_t selectedActualItem = 0;
    char** ptrActualItemsList = nullptr;

    enumStepPopups stepPopupState = stepPopupNone;
    bool noteKeyboardPopupActive = false;
    int miniKeyboardFirstPadIdx = 12 * 3;
    bool miniKeyboardEditButtonsActive = false;

    //-------------------------------------------------------------
    // global

    hControl textPopup;
    strTextPopupData popupData;

    static const uint8_t textLinesCount = 4;
    strPopupStyleConfig globalConfig = {
        2,
        800 / 2,
        480 / 2,
        400,
        80,
        0xffffff,
        0xffffff,
        0xffffff,
        0xffffff,
        controlStyleCenterX | controlStyleCenterY,
        controlStyleCenterX | controlStyleCenterY,
        controlStyleCenterX | controlStyleCenterY,
        controlStyleCenterX | controlStyleCenterY,
    };

    char* textLines[textLinesCount];
    uint32_t* textStyles[textLinesCount];
    uint32_t* textColors[textLinesCount];

    elapsedMillis popupTimer;
    uint8_t globalPopupDisplayTime = 0;

    //-----------------------------------------------------------
    // blocking / progress popup
    uint32_t progressPopupTextStyle;
    uint32_t progressPopupTextColor;

    char masterVolumeValueText[12];
};

extern InterfacePopups mtPopups;

#endif /* INTERFACE_INTERFACEPOPUPS_H_ */
