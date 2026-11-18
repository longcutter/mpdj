#ifndef LIBS_NEWDISPLAY_LIST_H_
#define LIBS_NEWDISPLAY_LIST_H_

#include "displayControls.h"

#define LIST_LENGTH_MAX    50
#define BITMAP_ICON_OFFSET 5

typedef struct
{
    uint8_t useSpecialIcons = 0;
    int8_t iconPositionInList;
    uint8_t iconNum;

} special_icon_t;

struct strList
{
    int16_t start;
    uint16_t length;
    uint16_t linesCount;

    uint8_t* selectTab;
    uint8_t selectionActive;

    special_icon_t icon;

    uint16_t lineWidthMax = 0;
    uint16_t lineTextLenMax = 0;

    char** data;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cList : public cDisplayControl
{
  public:
    cList(strControlProperties* properties = nullptr);
    virtual ~cList();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);
    // MPDJ (touch, 2026-11-16): the list row under (x, y) in the 800 x 480 UI, or -1
    int mpdjRowAt(int x, int y) const;
    // touch scrolling (mpdjListView.h): the scrolled view's first row, -1 = follow the selection
    int16_t mpdjViewTop = -1, mpdjViewSel = -1;
    void mpdjScroll(int rows);

    uint16_t textStyle;

    strList* list;

    int16_t listPosition;

    uint16_t barPos;
    uint16_t textListPos;

    uint8_t disableBar;

    int8_t listAnimationStep = 0;

    uint32_t textScrollTime;
    uint32_t textScrollTimeStart;
    uint8_t textScrollItem;
};

#endif
