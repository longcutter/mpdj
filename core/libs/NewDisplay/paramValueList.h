#ifndef LIBS_PARAMVALUE_LIST_H_
#define LIBS_PARAMVALUE_LIST_H_

#include "displayControls.h"

#define LIST_LENGTH_MAX    50
#define BITMAP_ICON_OFFSET 5

struct strParamValueList
{
    uint8_t valueEditActive;
    int16_t start;
    uint16_t length;
    uint16_t linesCount;

    char** params;
    char** values;
};

//--------------------------------------------------------------------
//--------------------------------------------------------------------
//--------------------------------------------------------------------
class cParamValueList : public cDisplayControl
{
  public:
    cParamValueList(strControlProperties* properties = nullptr);
    virtual ~cParamValueList();

    virtual void update();

    virtual void setStyle(uint32_t style);
    virtual void setText(const char* text);
    virtual void setValue(int value);
    virtual void setColors(const uint32_t* colors);
    virtual void setData(void* data);
    // MPDJ (touch, 2026-11-16): the row under (x, y), or -1
    int mpdjRowAt(int x, int y) const;
    // touch scrolling (mpdjListView.h): the scrolled view's first row, -1 = follow the selection
    int16_t mpdjViewTop = -1, mpdjViewSel = -1;
    void mpdjScroll(int rows);

    uint16_t textStyle;
    const strFont* font;

    strParamValueList* list;

    int16_t listPosition;

    uint16_t barPos;
    uint16_t textListPos;

    uint8_t disableBar;

    int8_t listAnimationStep = 0;
};

#endif /*LIBS_PARAMVALUE_LIST_H_*/
