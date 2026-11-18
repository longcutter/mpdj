#include "mpdjListView.h"
#include "FT812.h"

#include "display.h"
#include "displayControls.h"
#include "paramValueList.h"

cParamValueList::cParamValueList(strControlProperties* properties)
  : cDisplayControl(controlIdParamValueList, 1)
{
    colorsCount = 7;
    colors = display.getTheme().activeListColors;

    listPosition = 0;

    listAnimationStep = 0;
    disableBar = 0;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        value = 0;
        width = 0;
        height = 0;
        style = 0;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    posX = properties->x;
    posY = properties->y;

    // text = properties->text;
    value = properties->value;

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);

    setData(properties->data);
}

cParamValueList::~cParamValueList()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cParamValueList::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0)
                // | (style & controlStyleCenterY ? OPT_CENTERY : 0)
                | (style & controlStyleRightX ? OPT_RIGHTX : 0);

    int8_t textFont = FONT_INDEX_FROM_STYLE;
    textFont = (textFont >= 0) ? textFont : 0;
    font = &fonts[textFont];

    // textFont = FONT_INDEX_FROM_STYLE;
    // textFont = (textFont>=0) ? textFont : 0;
    // textFont =  fonts[textFont].handle;
}

void cParamValueList::setText(const char* text)
{
    this->text = text;
}

void cParamValueList::setValue(int value)
{
    if (value >= list->length)
    {
        if (list->length > 0)
            listPosition = list->length - 1;
        else
            listPosition = 0;

        disableBar = 0;
    }
    else if (value < 0)
    {
        listPosition = 0;
        disableBar = 1;
    }
    else
    {
        listPosition = value;
        disableBar = 0;
    }
}

void cParamValueList::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

/*
void cParamValueList::setDefaultColors(uint32_t* colors)
{
    memcpy(defaultColors, colors, colorsCount*4);
}
*/

void cParamValueList::setData(void* data)
{
    if (data == nullptr)
        return;
    list = (strParamValueList*)data;

    if (list->start < 0)
    {
        list->start = 0;
        listPosition = 0;
        disableBar = 1;
    }
    else
    {
        listPosition = list->start;
        disableBar = 0;
    }

    if (listPosition == 0) // pierwsza pozycja listy*
    {
        barPos = 0;
        textListPos = 0;
    }
    else if (listPosition == 1) // druga pozycja listy
    {
        barPos = 1;
        textListPos = 0;
    }
    else if (listPosition >= list->length - 1) // ostatnia pozycja listy
    {
        barPos = (list->length > list->linesCount ? list->linesCount - 1 : list->length - 1);
        textListPos = (list->length > list->linesCount ? list->length - (list->linesCount) : 0);
    }
    else if (listPosition >= list->length - 2) // przed ostatnia pozycja listy
    {
        barPos = (list->length > list->linesCount ? list->linesCount - 2 : list->length - 2);
        textListPos = (list->length > list->linesCount ? list->length - (list->linesCount) : 0);
    }
    else
    {
        barPos = (list->length > list->linesCount
                    ? (listPosition > list->linesCount - 2 ? list->linesCount - 2 : listPosition)
                    : listPosition);
        textListPos =
          (list->length > list->linesCount
             ? (listPosition > list->linesCount - 2 ? listPosition - (list->linesCount - 2) : 0)
             : 0);
    }
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cParamValueList::update()
{
    if (list == nullptr)
        return;

    // int16_t posX = this->posX;
    int16_t posY = this->posY;

    if (style & controlStyleCenterY)
    {
        posY = this->posY - (height / 2);
    }

    int16_t x_pos = posX + 3, y_pos, h_row = 27; // font->height+8;
    int16_t x_pos_end = x_pos + width - 6;
    uint8_t lines;

    if (list->length > list->linesCount)
    {
        x_pos_end -= 7;
    }

    // tlo listy
    if (style & controlStyleBackground)
    {
        API_COLOR(colors[3]);
        API_LINE_WIDTH(16);
        API_BEGIN(RECTS);
        API_VERTEX2F(posX, posY);
        API_VERTEX2F(posX + width, posY + height);
        API_END();

        API_BLEND_FUNC(SRC_ALPHA, ZERO);

        if (style & controlStyleBottomShadow)
        {
            API_SAVE_CONTEXT();
            uint16_t grad_y = posY + height - 10;
            uint16_t grad_h = 12;
            API_SCISSOR_XY(posX, grad_y);
            API_SCISSOR_SIZE(width + 1, grad_h);
            API_CMD_GRADIENT(0, grad_y, colors[3], 0, grad_y + grad_h, 0x0);
            API_RESTORE_CONTEXT();
        }

        //		API_COLOR(colors[1]);
        //		API_LINE_WIDTH(1);
        //		API_BEGIN(LINES);
        //		API_VERTEX2F(posX-1, posY);
        //		API_VERTEX2F(posX-1, posY+height);
        //		API_VERTEX2F(posX+width+1, posY);
        //		API_VERTEX2F(posX+width+1, posY+height);
        //		API_END();

        API_BLEND_FUNC(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    }

    bool mpdjHideBar = mpdjListView(this); // touch scrolling (2026-11-17)
    posY += 4;

    if (list->start == listPosition)
    {
        // geneorwanie poczatkowego menu
        listAnimationStep = 0;
        highPrioRefresh = false;

        if (!disableBar && !mpdjHideBar)
        {
            // x_pos = posX+3;
            y_pos = posY + (barPos * h_row);

            // ramka
            if (list->valueEditActive)
            {
                x_pos = x_pos_end - ((x_pos_end - x_pos) / 3);
                // x_pos_end = w_bar/3;
                API_COLOR(display.getTheme().frameColors[0]);
            }
            else
            {
                API_COLOR(colors[0]);
            }

            API_LINE_WIDTH(kRedFrameWidth);
            API_BEGIN(LINE_STRIP);
            API_VERTEX2F(x_pos, y_pos);
            API_VERTEX2F(x_pos_end, y_pos);
            API_VERTEX2F(x_pos_end, y_pos + h_row);
            API_VERTEX2F(x_pos, y_pos + h_row);
            API_VERTEX2F(x_pos, y_pos);
            API_END();
        }

        // lista tekstow
        x_pos = posX + 13;
        y_pos = posY + h_row / 2;

        lines = (list->length >= list->linesCount) ? list->linesCount : list->length;

        API_SAVE_CONTEXT();

        // uint8_t txt_offset;
        API_COLOR(colors[4]);

        API_SCISSOR_XY(posX, posY);
        API_SCISSOR_SIZE(x_pos_end - posX, h_row * list->linesCount);

        x_pos_end -= 7;

        for (uint8_t i = 0; i < lines; i++)
        {
            int16_t rowIdx = i + textListPos;
            API_COLOR((!disableBar && rowIdx == list->start) ? colors[5] : colors[4]);

            char* param = *(list->params + rowIdx);
            char* value = *(list->values + rowIdx);

            API_CMD_TEXT(x_pos, y_pos + (i * h_row), font->handle, OPT_CENTERY, param);

            API_CMD_TEXT(
              x_pos_end, y_pos + (i * h_row), font->handle, OPT_RIGHTX | OPT_CENTERY, value);
        }

        API_RESTORE_CONTEXT();
    }
    else
    {
        int8_t dir, mode;   // kieurnek przewijania ; tryb/rodzaj przewijania
        uint16_t diffrence; // skok belki/listy w pixelach przy jedym odsiwezeniu listy

        // Geneorwanie zmieny menu.

        // okreslenie kierunku i skoku listy
        if (list->start > listPosition)
        {
            dir = -1;
            diffrence = list->start - listPosition;
        }
        else
        {
            dir = 1;
            diffrence = listPosition - list->start;
        }

        if (diffrence > 8)
            diffrence = 75;
        else if (diffrence > 4)
            diffrence = 50;
        else if (diffrence > 1)
            diffrence = 25;
        else
            diffrence = 8; // 2;

        mode = 0;
        if (barPos == 0 || barPos == list->linesCount - 1)
            mode = 0;
        else if (barPos == 1)
            mode = ((dir == 1 || list->start == 1) ? 0 : 1);
        else if (barPos == list->linesCount - 2)
            mode = ((dir == -1 || list->start == list->length - 2) ? 0 : 1);
        else
            mode = 0;

        if (mode == 0) // ruch belka(mode = 0)
        {
            listAnimationStep = listAnimationStep + (dir * diffrence);
            if (listAnimationStep >= h_row)
            {
                listAnimationStep = 0;
                barPos = barPos + 1;
                list->start = list->start + 1;
            }
            else if (listAnimationStep <= -1 * h_row)
            {
                listAnimationStep = 0;
                barPos = barPos - 1;
                list->start = list->start - 1;
            }
        }
        else // ruch tekstem(mode = 1)
        {
            listAnimationStep = listAnimationStep + (dir * diffrence);
            if (listAnimationStep >= h_row)
            {
                listAnimationStep = 0;
                list->start = list->start + 1;
                textListPos = textListPos + 1;
            }
            else if (listAnimationStep <= -1 * h_row)
            {
                listAnimationStep = 0;
                list->start = list->start - 1;
                textListPos = textListPos - 1;
            }
        }

        highPrioRefresh = true;

        if (!disableBar)
        {
            y_pos = posY + (barPos * h_row) + (mode ? 0 : listAnimationStep);
            // if(list->length > list->linesCount) w_bar = width-13;

            // ramka
            if (list->valueEditActive)
            {
                x_pos = x_pos_end - ((x_pos_end - x_pos) / 3);
                API_COLOR(display.getTheme().frameColors[0]);
            }
            else
            {
                API_COLOR(colors[0]);
            }

            API_LINE_WIDTH(kRedFrameWidth);
            API_BEGIN(LINE_STRIP);
            API_VERTEX2F(x_pos, y_pos);
            API_VERTEX2F(x_pos_end, y_pos);
            API_VERTEX2F(x_pos_end, y_pos + h_row);
            API_VERTEX2F(x_pos, y_pos + h_row);
            API_VERTEX2F(x_pos, y_pos);
            API_END();
        }

        x_pos = posX + 13;
        y_pos = posY + h_row / 2 + (mode ? ((-1 * h_row) - listAnimationStep) : 0);

        API_SAVE_CONTEXT();

        API_COLOR(colors[4]);

        API_SCISSOR_XY(posX, posY);
        API_SCISSOR_SIZE(x_pos_end - posX, h_row * list->linesCount);

        x_pos_end -= 7;

        lines = (list->length >= list->linesCount) ? list->linesCount : list->length;

        if (mode == 1)
        {
            if (dir == 1) // tekst porusza sie w gore (wartosci w dol)
            {
                y_pos += h_row;

                for (int8_t i = 0; i < lines; i++)
                {
                    int16_t rowIdx = i + textListPos;
                    API_COLOR((!disableBar && rowIdx == list->start) ? colors[5] : colors[4]);

                    API_CMD_TEXT(x_pos,
                                 y_pos + (i * h_row),
                                 font->handle,
                                 OPT_CENTERY,
                                 *(list->params + rowIdx));

                    API_CMD_TEXT(x_pos_end,
                                 y_pos + (i * h_row),
                                 font->handle,
                                 OPT_RIGHTX | OPT_CENTERY,
                                 *(list->values + rowIdx));
                }
            }
            else if (dir == -1) // tekst porusza sie w dol (wartosci w gore)
            {
                for (int8_t i = 0; i < (lines + 1); i++)
                {

                    int16_t table_offset = textListPos - 1;
                    if (table_offset < 0)
                        table_offset = 0;

                    int16_t rowIdx = i + table_offset;
                    API_COLOR((!disableBar && rowIdx == list->start) ? colors[5] : colors[4]);

                    API_CMD_TEXT(x_pos,
                                 y_pos + (i * h_row),
                                 font->handle,
                                 OPT_CENTERY,
                                 *(list->params + rowIdx));

                    API_CMD_TEXT(x_pos_end,
                                 y_pos + (i * h_row),
                                 font->handle,
                                 OPT_RIGHTX | OPT_CENTERY,
                                 *(list->values + rowIdx));
                }
            }
        }
        else
        {
            for (uint8_t i = 0; i < lines; i++)
            {
                int16_t rowIdx = i + textListPos;
                API_COLOR((!disableBar && rowIdx == list->start) ? colors[5] : colors[4]);

                API_CMD_TEXT(x_pos,
                             y_pos + (i * h_row),
                             font->handle,
                             OPT_CENTERY,
                             *(list->params + rowIdx));

                API_CMD_TEXT(x_pos_end,
                             y_pos + (i * h_row),
                             font->handle,
                             OPT_RIGHTX | OPT_CENTERY,
                             *(list->values + rowIdx));
            }
        }

        API_RESTORE_CONTEXT();
    }

    // pasek przewijania
    if (list->length > list->linesCount)
    {
        int16_t y_length = ((height - 14) * list->linesCount) / list->length;
        y_pos = posY + 6 + (list->start * ((height - 14) - y_length)) / list->length;

        // API_BLEND_FUNC(SRC_ALPHA , ZERO);

        API_COLOR(colors[6]);
        API_LINE_WIDTH(16);
        API_BEGIN(RECTS);
        API_VERTEX2F(posX + (width - 6), posY + 4);
        API_VERTEX2F(posX + (width - 3), (posY + height) - 10);
        API_END();

        API_COLOR(colors[2]);
        API_LINE_WIDTH(8);
        API_BEGIN(LINES);
        // API_LINE_WIDTH(24);
        // API_BEGIN(RECTS);
        API_VERTEX2F(posX + (width - 5), y_pos);
        API_VERTEX2F(posX + (width - 5), y_pos + y_length);

        API_VERTEX2F(posX + (width - 4), y_pos);
        API_VERTEX2F(posX + (width - 4), y_pos + y_length);

        API_END();
        API_LINE_WIDTH(16);

        // API_BLEND_FUNC(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
    }
}

// MPDJ (touch, 2026-11-16): same geometry as cList (27-high rows, 4 top margin)
int cParamValueList::mpdjRowAt(int x, int y) const
{
    if (list == nullptr || !(style & controlStyleShow))
        return -1;
    int top = (style & controlStyleCenterY) ? posY - height / 2 : posY;
    int left = (style & controlStyleCenterX) ? posX - width / 2 : posX;
    if (x < left || x >= left + width || y < top + 4 || y >= top + height)
        return -1;
    int row = (y - top - 4) / 27;
    int lines = list->length < list->linesCount ? list->length : list->linesCount;
    if (row >= lines)
        return -1;
    return textListPos + row;
}

void cParamValueList::mpdjScroll(int rows) { mpdjListScroll(this, rows); }
