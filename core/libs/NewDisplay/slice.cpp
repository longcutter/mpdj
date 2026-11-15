#include "FT812.h"

#include "display.h"
#include "displayControls.h"
#include "sliceControl.h"

cSlicePoints::cSlicePoints(strControlProperties* properties)
  : cDisplayControl(controlIdSlicePoints, 1)
{
    colorsCount = 3;
    colors = display.getTheme().sliceColors;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        points = nullptr;
        value = 0;
        width = 0;
        height = 0;
        style = 0;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    points = (strSlicePoints*)properties->data;

    posX = properties->x;
    posY = properties->y;

    text = properties->text;
    value = properties->value;

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

cSlicePoints::~cSlicePoints()
{
}

//--------------------------------------------------------------------------------
void cSlicePoints::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    textFont = fonts[textFont].handle;
}

void cSlicePoints::setText(const char* text)
{
    this->text = text;
}

void cSlicePoints::setValue(int value)
{
}

void cSlicePoints::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cSlicePoints::setData(void* data)
{
    points = (strSlicePoints*)data;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cSlicePoints::update()
{
    if (points == nullptr)
    {
        return;
    }

    API_LINE_WIDTH(16);

    // MPDJ (5g): a bar along the top over the selected slice, and the End mark
    if (points->selFrom >= 0 && points->selTo > points->selFrom)
    {
        API_COLOR(colors[1]);
        API_LINE_WIDTH(32);
        API_BEGIN(LINES);
        API_VERTEX2F(points->selFrom + posX, posY + 3);
        API_VERTEX2F(points->selTo + posX, posY + 3);
        API_END();
        API_LINE_WIDTH(16);
    }
    if (points->endMark >= 0)
    {
        int16_t x = points->endMark + posX;
        API_COLOR(points->selected + 1 == points->sliceCount ? colors[1] : colors[0]);
        API_BEGIN(LINES);
        API_VERTEX2F(x, posY - 1);
        API_VERTEX2F(x, posY + height);
        API_END();
        API_BEGIN(LINE_STRIP); // a flag pointing left: this is where the last slice stops
        API_VERTEX2F(x, posY - 2);
        API_VERTEX2F(x - 8, posY - 6);
        API_VERTEX2F(x, posY - 10);
        API_END();
    }

    uint16_t start = 0;
    constexpr int selectedDiff = 9;

    for (uint8_t i = 0; i < points->sliceCount; i++)
    {
        if (points->slice[i] >= 0)
        {
            start = points->slice[i] + posX;
            bool selected = false;

            if (points->selected == i)
            {
                selected = true;
                API_COLOR(points->isSliceZerocrossed[i] ? colors[2] : colors[1]);
            }
            else
            {
                API_COLOR(colors[0]);
            }

            // point
            API_BEGIN(LINES);
            API_VERTEX2F(start, posY - (1 + (selected ? selectedDiff : 0)));
            API_VERTEX2F(start, posY + height);
            API_END();

            // znaczek
            API_BEGIN(LINE_STRIP);
            API_VERTEX2F(start - 4, posY - (7 + (selected ? selectedDiff : 0)));
            API_VERTEX2F(start + 4, posY - (7 + (selected ? selectedDiff : 0)));
            API_VERTEX2F(start, posY - (2 + (selected ? selectedDiff : 0)));
            API_VERTEX2F(start - 4, posY - (7 + (selected ? selectedDiff : 0)));
            API_END();
        }
    }
}
