#ifndef LIBS_NEWDISPLAY_SONGPLAYER_H_
#define LIBS_NEWDISPLAY_SONGPLAYER_H_

#include "displayControls.h"

typedef struct
{
    uint8_t useSpecialIcons = 0;
    int8_t iconPositionInList;
    uint8_t iconNum;

} text_icon_t;

typedef struct
{
    int16_t start;
    uint16_t length;
    uint16_t linesCount;

    uint8_t* data;
    char** names;

    text_icon_t icon;

} localList_t;

typedef struct
{
    uint8_t isActive;
    uint8_t startSongPosition;
    uint8_t patternSelectionLength;
    uint8_t startTrack;
    uint8_t trackSelectionLength;

} song_selection_t;

typedef struct
{
    uint8_t isPlaying;
    uint8_t trackPreview;
    uint8_t currentSongPosition;
    uint8_t patternLength;
    uint8_t positionInPattern;

} progress_control_t;

typedef struct
{
    uint8_t songLength;
    uint8_t* songData;
    uint8_t* isEditMode;
    uint8_t isListActive;

    song_selection_t selection;
    progress_control_t progress;
    localList_t* list;

    uint8_t normalTracksCount;
    uint8_t midiTracksCount;

    uint8_t* normalPatternsBitmask;
    uint8_t* midiPatternsBitmask;

    uint8_t* normalTracksMute;
    uint8_t* midiTracksMute;

    char* normalTracksNames;
    char* midiTracksNames;

    uint8_t normalTracksNamesBuffLen;
    uint8_t midiTracksNamesBuffLen;

} player_data_t;

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cSongPlayer : public cDisplayControl
{
  public:
    cSongPlayer(strControlProperties* properties = nullptr);
    virtual ~cSongPlayer();

    virtual void update();
    virtual void update2();
    virtual void update3();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);

    uint16_t textStyle;
    int16_t textFont;
    uint8_t keyboardShift = 0;
    uint16_t textListPos = 0;
    // MPDJ (touch, 2026-11-16): the pattern-list row under (x, y), or -1
    int mpdjRowAt(int x, int y) const;
    // the grid cell (song slot, track) under (x, y); false if none
    bool mpdjCellAt(int x, int y, int* slot, int* track) const;

  private:
    void showList();
    void drawTrackNames();
    void drawIcon(int pos, int row, int x, int y, int h);
    void fillBlocks();
    void drawProgressLine();
    void calculateSelection(uint8_t pattern,
                            uint8_t track,
                            uint16_t x,
                            uint16_t y,
                            uint16_t x_end,
                            uint16_t y_end);
    void drawSelection();

    player_data_t* controlData;

    uint16_t ramPartSize[5] = { 0 };
    uint8_t refreshStep;

    uint8_t selectionCalcInit = 0;
    uint16_t xSelectionStart;
    uint16_t ySelectionStart;
    uint16_t xSelectionEnd;
    uint16_t ySelectionEnd;

    const strFont* font;
    localList_t* list;
    int16_t listPosition;

    uint16_t barPos;
    uint8_t disableBar;

    int8_t listAnimationStep = 0;
};

#endif
