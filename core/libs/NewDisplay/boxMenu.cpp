
#include "FT812.h"

#include "boxMenuControl.h"
#include "display.h"

#include <string.h>

cBoxMenu::cBoxMenu(strControlProperties* properties)
  : cDisplayControl(controlIdBoxMenu, 1)
{
    colorsCount = 7;
    colors = display.getTheme().miniBoxMenuColors;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        data = nullptr;
        width = 0;
        height = 0;
        style = 0;
        value = 0;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    posX = properties->x;
    posY = properties->y;

    text = properties->text;

    value = properties->value;

    data = (strBoxMenuData*)(properties->data);

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

cBoxMenu::~cBoxMenu()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cBoxMenu::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    int8_t textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    font = &fonts[textFont];
}

void cBoxMenu::setText(const char* text)
{
    this->text = text;
}

void cBoxMenu::setValue(int value)
{
    this->value = value;
}

void cBoxMenu::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cBoxMenu::setData(void* data)
{
    this->data = (strBoxMenuData*)(data);
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cBoxMenu::update()
{
    if (data == nullptr)
    {
        return;
    }

    int colorIdx = -1;
    int boxSpacing = 3;
    int boxW = (width - ((data->columnsCount + 1) * boxSpacing)) / data->columnsCount;
    int boxH = (height - ((data->rowsCount + 1) * boxSpacing)) / data->rowsCount;
    int frameX = posX;
    int frameY = posY;

    API_SAVE_CONTEXT();

    // bg

    API_COLOR(colors[3]);
    API_BEGIN(RECTS);
    API_VERTEX2F(posX, posY);
    API_VERTEX2F(posX + boxSpacing + (boxSpacing + boxW) * data->columnsCount,
                 posY + boxSpacing + (boxSpacing + boxH) * data->rowsCount);

    // squares

    for (int column = 0; column < data->columnsCount; column++)
    {
        for (int row = 0; row < data->rowsCount; row++)
        {
            int currentBoxIdx = row * data->columnsCount + column;
            int curentColorIdx = currentBoxIdx == value ? 2 : 1;

            if (colorIdx != curentColorIdx)
            {
                colorIdx = curentColorIdx;
                API_COLOR(colors[colorIdx]);
            }

            int boxX = posX + boxSpacing + (boxSpacing + boxW) * column;
            int boxY = posY + boxSpacing + (boxSpacing + boxH) * row;

            API_VERTEX2F(boxX, boxY);
            API_VERTEX2F(boxX + boxW, boxY + boxH);

            if (currentBoxIdx == value)
            {
                frameX = boxX;
                frameY = boxY;
            }
        }
    }

    API_END();

    // text

    colorIdx = 0;
    API_COLOR(colors[colorIdx]);

    for (int column = 0; column < data->columnsCount; column++)
    {
        for (int row = 0; row < data->rowsCount; row++)
        {
            int currentBoxIdx = row * data->columnsCount + column;

            if (data->textList[currentBoxIdx] == nullptr)
            {
                continue;
            }

            int textX = posX + boxSpacing + (boxSpacing + boxW) * column + boxW / 2;
            int textY = posY + boxSpacing + (boxSpacing + boxH) * row + boxH / 2;

            char* text = data->textList[currentBoxIdx];
            if (!text)
                continue; // MPDJ: a box without text (address 0 is readable on the MCU)
            int textLen = strlen(text);
            char* spacePtr = nullptr;

            if (getTextWidth(font, data->textList[currentBoxIdx], textLen) + 10 > boxW)
            {
                spacePtr = strchr(text, ' ');
                if (spacePtr != nullptr)
                {
                    textLen = spacePtr - text;
                    textY -= font->height / 2;
                }
            }

            API_CMD_TEXT(
              textX, textY, font->handle, textStyle, data->textList[currentBoxIdx], textLen);

            if (spacePtr != nullptr)
            {
                textY += font->height;
                API_CMD_TEXT(textX, textY, font->handle, textStyle, spacePtr + 1);
            }
        }
    }

    // dots

    if (data->modes != nullptr)
    {
        constexpr int dotSize = 3;
        constexpr int dotSpacing = 13;
        constexpr int dotBottomOffset = 12;

        API_POINT_SIZE(16 * dotSize);
        API_BEGIN(FTPOINTS);

        for (int column = 0; column < data->columnsCount; column++)
        {
            for (int row = 0; row < data->rowsCount; row++)
            {
                int currentBoxIdx = row * data->columnsCount + column;
                if (data->modes[currentBoxIdx] != nullptr && data->modes[currentBoxIdx]->count > 0)
                {
                    int boxX = posX + boxSpacing + (boxSpacing + boxW) * column;
                    int boxY = posY + boxSpacing + (boxSpacing + boxH) * row;

                    int dotsX = (boxX + (boxW / 2)) -
                                (((data->modes[currentBoxIdx]->count - 1) * dotSpacing) / 2);
                    int dotsY = boxY + boxH - dotBottomOffset;

                    for (int dot = 0; dot < data->modes[currentBoxIdx]->count; dot++)
                    {
                        int curentColorIdx = data->modes[currentBoxIdx]->current == dot ? 6 : 5;

                        if (colorIdx != curentColorIdx)
                        {
                            colorIdx = curentColorIdx;
                            API_COLOR(colors[colorIdx]);
                        }

                        API_VERTEX2F(dotsX + dot * dotSpacing, dotsY);
                    }
                }
            }
        }

        API_END();
    }

    // frame

    API_COLOR(colors[4]);
    API_LINE_WIDTH(kRedFrameWidth);
    API_BEGIN(LINE_STRIP);

    API_VERTEX2F(frameX, frameY);
    API_VERTEX2F(frameX + boxW, frameY);
    API_VERTEX2F(frameX + boxW, frameY + boxH);
    API_VERTEX2F(frameX, frameY + boxH);
    API_VERTEX2F(frameX, frameY);

    API_END();

    API_RESTORE_CONTEXT();
}
