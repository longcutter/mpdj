#include "FT812.h"
#include "display.h"
#include "displayControls.h"

#include "newListControl.h"

#include "Tracker.h"
#include "util_math.h"
#include <string.h>

#define SCROLL_BAR_WIDTH  3
#define SCROLL_BAR_MARGIN 3
#define BAR_TYPE          0
#define BAR_ROUND         6

#define SCROLL_MARGIN         30
#define TEXT_SCROLL_SPEED_DIV 30
#define TEXT_SCROLL_SPACER    30
// strFont::width has to be corrected to be used for stride calculation
#define TEXT_SCROLL_FONT_WIDTH_CORRECTION (-2)

NewList::NewList(strControlProperties* properties)
  : cDisplayControl(controlIdImgList, 1)
{
    colorsCount = 7;
    colors = display.getTheme().activeListColors;

    listPosition = 0;

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

    value = properties->value;

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);

    setData(properties->data);
}

NewList::~NewList()
{
}

//--------------------------------------------------------------------------------
void NewList::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    int8_t fontIndex = FONT_INDEX_FROM_STYLE;
    fontIndex = (fontIndex >= 0) ? fontIndex : 0;
    font = &fonts[fontIndex];
}

//--------------------------------------------------------------------------------
void NewList::setText(const char* text)
{
    this->text = text;
}

//--------------------------------------------------------------------------------
void NewList::setValue(int value)
{
    if (data == nullptr)
    {
        return;
    }

    if (value >= data->length)
    {
        if (data->length > 0)
        {
            listPosition = data->length - 1;
        }
        else
        {
            listPosition = 0;
        }
        disableBar = false;
    }
    else if (value < 0)
    {
        listPosition = 0;
        disableBar = true;
    }
    else
    {
        listPosition = value;
        disableBar = false;
    }
}
//--------------------------------------------------------------------------------

void NewList::setColors(const uint32_t* colors)
{
    this->colors = colors;
}
//--------------------------------------------------------------------------------

void NewList::setData(void* data)
{
    if (data == nullptr)
    {
        return;
    }

    this->data = (strNewListData*)data;

    assert(this->data->length > 0);
    assert(this->data->linesCount > 0);

    if (this->data->start < 0)
    {
        this->data->start = 0;
        listPosition = 0;
        disableBar = true;
    }
    else
    {
        listPosition = this->data->start;
        disableBar = false;
    }

    if (listPosition == 0)
    {
        barPos = 0;
        listPos = 0;
    }
    else if (listPosition == 1)
    {
        barPos = 1;
        listPos = 0;
    }
    else if (listPosition >= this->data->length - 1)
    {
        barPos = (this->data->length > this->data->linesCount ? this->data->linesCount - 1
                                                              : this->data->length - 1);
        listPos = (this->data->length > this->data->linesCount
                     ? this->data->length - (this->data->linesCount)
                     : 0);
    }
    else if (listPosition >= this->data->length - 2)
    {
        barPos = (this->data->length > this->data->linesCount ? this->data->linesCount - 2
                                                              : this->data->length - 2);
        listPos = (this->data->length > this->data->linesCount
                     ? this->data->length - (this->data->linesCount)
                     : 0);
    }
    else
    {
        barPos = (this->data->length > this->data->linesCount
                    ? (listPosition > this->data->linesCount - 2 ? this->data->linesCount - 2
                                                                 : listPosition)
                    : listPosition);
        listPos = (this->data->length > this->data->linesCount
                     ? (listPosition > this->data->linesCount - 2
                          ? listPosition - (this->data->linesCount - 2)
                          : 0)
                     : 0);
    }
}

//--------------------------------------------------------------------------------
void NewList::update()
{
    if (data == nullptr)
    {
        return;
    }

    bool scrollBarVisible = data->length > data->linesCount;
    int posY = this->posY;

    if (style & controlStyleCenterY)
    {
        posY = this->posY - (height / 2);
    }

    int posX = this->posX;
    int rowH = data->rowHeight > 0 ? data->rowHeight : 27;
    int endPosX =
      posX + width - (scrollBarVisible ? (SCROLL_BAR_MARGIN * 2 + SCROLL_BAR_WIDTH) : 3);

    API_SAVE_CONTEXT();

    if (style & controlStyleBackground)
    {
        API_COLOR(colors[3]);
        API_LINE_WIDTH(16);
        API_BEGIN(RECTS);
        API_VERTEX2F(posX, posY);
        API_VERTEX2F(posX + width, posY + height);
        API_END();

        if (style & controlStyleBottomShadow)
        {
            API_SAVE_CONTEXT();
            API_BLEND_FUNC(SRC_ALPHA, ZERO);
            uint16_t grad_y = posY + height - 10;
            uint16_t grad_h = 12;
            API_SCISSOR_XY(posX, grad_y);
            API_SCISSOR_SIZE(width + 1, grad_h);
            API_CMD_GRADIENT(0, grad_y, colors[3], 0, grad_y + grad_h, 0x0);
            API_BLEND_FUNC(SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
            API_RESTORE_CONTEXT();
        }
    }

    int moveValue = listPosition - data->start;

    int barTargetPos = clamp(barPos + moveValue, 0, data->linesCount - 1);

    int barMovedValue = barTargetPos - barPos;

    if (barTargetPos == data->linesCount - 1 && listPosition < data->length - 1)
    {
        barTargetPos--;
    }
    else if (barTargetPos == 0 && listPosition > 0)
    {
        barTargetPos++;
    }

    barMovedValue = barTargetPos - barPos;
    int remainListMove = moveValue - barMovedValue;

    listPos += remainListMove;
    barPos = barTargetPos;
    data->start = listPosition;

    posY += 4;

    if (!disableBar)
    {
        int barPosY = posY + (barPos * rowH);
        drawBar(posX + 3, barPosY, endPosX, barPosY + rowH);
    }

    posX += 13;
    endPosX -= 7;

    API_SAVE_CONTEXT();
    API_SCISSOR_XY(posX, posY);
    API_SCISSOR_SIZE(endPosX - posX, rowH * data->linesCount);

    posY = posY + rowH / 2;

    if (data->imageIds != nullptr)
    {
        drawImagesList(posX, endPosX, posY, rowH);
    }
    else if (data->params != nullptr)
    {
        drawTextList(posX, endPosX, posY, rowH, font, font);
    }

    API_RESTORE_CONTEXT();

    if (scrollBarVisible)
    {
        drawScrollBar();
    }

    API_RESTORE_CONTEXT();
}

void NewList::drawBar(int x1, int y1, int x2, int y2)
{
    if (data->selectionActive)
    {
        API_COLOR(colors[1]);
    }
    else
    {
        API_COLOR(colors[0]);
    }

#if BAR_TYPE == 0
    API_LINE_WIDTH(8);
    API_BEGIN(LINE_STRIP);
    API_VERTEX2F(x1, y1);
    API_VERTEX2F(x2, y1);
    API_VERTEX2F(x2, y2);
    API_VERTEX2F(x1, y2);
    API_VERTEX2F(x1, y1);
    API_END();
#else

    API_BEGIN(RECTS);
    if (style & controlStyleRoundedBorder)
    {
        API_LINE_WIDTH(16 * BAR_ROUND);
        API_VERTEX2F(x1 + BAR_ROUND, y1 + BAR_ROUND);
        API_VERTEX2F(x2 - BAR_ROUND, y2 - BAR_ROUND);
    }
    else
    {
        API_LINE_WIDTH(16);
        API_VERTEX2F(x1, y1);
        API_VERTEX2F(x2, y2);
    }
    API_END();
#endif
}

void NewList::drawScrollBar()
{
    constexpr int kScrollBarW = SCROLL_BAR_WIDTH;

    int scrollBarX = posX + width - kScrollBarW - SCROLL_BAR_MARGIN;
    int scrollBarY = posY + 8;
    int scrollBarH = height - 14;

    int scrollH = (scrollBarH * data->linesCount) / data->length;
    int scrollY = scrollBarY + 2 + (scrollBarH - scrollH) * (data->start / (float)(data->length));

    if (style & controlStyleRoundedBorder)
    {
        constexpr int kRoundSize = kScrollBarW / 2;
        scrollBarX += kRoundSize;
        scrollBarY += kRoundSize;
        scrollY += kRoundSize;

        API_COLOR(colors[6]);
        API_LINE_WIDTH(16 * kRoundSize);
        API_BEGIN(LINES);
        API_VERTEX2F(scrollBarX, scrollBarY);
        API_VERTEX2F(scrollBarX, scrollBarY + scrollBarH);
        API_END();

        API_COLOR(colors[2]);
        API_BEGIN(LINES);
        API_VERTEX2F(scrollBarX, scrollY);
        API_VERTEX2F(scrollBarX, scrollY + scrollH);
        API_END();
    }
    else
    {
        API_COLOR(colors[6]);
        API_LINE_WIDTH(16);
        API_BEGIN(RECTS);
        API_VERTEX2F(scrollBarX, scrollBarY);
        API_VERTEX2F(scrollBarX + kScrollBarW, scrollBarY + scrollBarH);
        API_END();

        API_COLOR(colors[2]);
        API_LINE_WIDTH(8);
        API_BEGIN(LINES);
        API_VERTEX2F(scrollBarX + 1, scrollY);
        API_VERTEX2F(scrollBarX + 1, scrollY + scrollH);
        API_VERTEX2F(scrollBarX + 2, scrollY);
        API_VERTEX2F(scrollBarX + 2, scrollY + scrollH);
        API_END();
    }
}

void NewList::drawTextList(int x1,
                           int x2,
                           int y,
                           int rowH,
                           const strFont* font1,
                           const strFont* font2)
{
    assert(data->params != nullptr);

    if (textScrollItem != data->start)
    {
        textScrollStage = 0;
        textScrollItem = data->start;
    }

    bool showHeadLineStyle = listHasHeadlineStyle();

    uint32_t lastColor = colors[4];
    API_COLOR(lastColor);

    int rowsCount = (data->length >= data->linesCount) ? data->linesCount : data->length;

    for (int rowIdx = 0; rowIdx < rowsCount; rowIdx++)
    {
        int dataIdx = rowIdx + listPos;
        dataIdx = clamp(dataIdx, 0, data->length > 0 ? data->length - 1 : 0);

        setRowTextColor(&lastColor, dataIdx);

        int paramX = x1 + (showHeadLineStyle ? 15 : 0);
        int paramY = y + rowH * rowIdx;
        const strFont* paramFont = font1;

        getRowParamTextStyle(dataIdx, &paramX, &paramY, paramFont);

        char* paramText = data->params[dataIdx];

        if (paramText != nullptr)
        {
            if (data->scrollText && textScrollItem == dataIdx)
            {
                getRowParamTextScroll(x2 - x1, &paramX, paramFont, paramText);
            }

            API_CMD_TEXT(paramX, paramY, paramFont->handle, OPT_CENTERY, paramText);
        }

        if (data->values != nullptr)
        {
            API_CMD_TEXT(
              x2, paramY, font2->handle, OPT_RIGHTX | OPT_CENTERY, data->values[dataIdx]);
        }
    }
}

bool NewList::listHasHeadlineStyle()
{
    bool headlineStyle = false;

    if (data->styles != nullptr)
    {
        int rowsCount = (data->length >= data->linesCount) ? data->linesCount : data->length;

        for (int idx = 0; idx < rowsCount; idx++)
        {
            if (data->styles[idx] == strNewListData::Style::kHeadline)
            {
                headlineStyle = true;
            }
        }
    }

    return headlineStyle;
}

void NewList::setRowTextColor(uint32_t* lastColorBuff, int row)
{
    uint32_t color = colors[4];

    if (data->selectionActive && data->selectTab != nullptr && data->selectTab[row])
    {
        color = (colors[5] != colors[4]) ? colors[5] : display.getTheme().red;
    }
    else if (!disableBar && row == data->start)
    {
        color = colors[5];
    }

    if (color != *lastColorBuff)
    {
        API_COLOR(color);
        *lastColorBuff = color;
    }
}

void NewList::getRowParamTextStyle(int dataIdx,
                                   [[maybe_unused]] int* paramX,
                                   [[maybe_unused]] int* paramY,
                                   [[maybe_unused]] const strFont* paramFont)
{
    strNewListData::Style rowStyle =
      data->styles != nullptr ? data->styles[dataIdx] : strNewListData::Style::kNoStyle;

    if (rowStyle == strNewListData::Style::kHeadline)
    {
        *paramX -= 10;
        *paramY -= 2;
        paramFont = &fonts[1];
    }
}

void NewList::getRowParamTextScroll(int textSpace,
                                    int* paramX,
                                    const strFont* font,
                                    const char* text)
{
    int xOffset = 0;
    if (text == nullptr) // MPDJ: see list.cpp
        text = "";
    int textLen = strlen(text);
    int textWidth = 0;
    constexpr int kScrollStartDelay = 600;
    constexpr int kScrollEndDelay = 1000;

    textWidth = getTextWidth(font, text, textLen);

    int scrollLength = textWidth - textSpace;

    if (scrollLength > 0)
    {
        autoRefresh = true;

        Timer* timer = Tracker::getInstance()->getPlatform()->getTimer();
        bool switchStage = false;

        if (textScrollStage == 1)
        {
            xOffset = 0;
            if (timer->millis() - textScrollTimeStart > kScrollStartDelay)
            {
                switchStage = true;
            }
        }
        else if (textScrollStage == 3)
        {
            xOffset = scrollLength;
            if (timer->millis() - textScrollTimeStart > kScrollEndDelay)
            {
                switchStage = true;
            }
        }
        else if (textScrollStage == 2)
        {
            int deltaTime = (timer->millis() - textScrollTimeStart) / TEXT_SCROLL_SPEED_DIV;
            deltaTime = deltaTime < 11 ? (deltaTime * deltaTime) / 30
                                       : deltaTime - 7; // smooth out start with quadratic function
            // text += (xOffset > font->data[(uint8_t)text[0]]) ? 1 : 0;

            if (deltaTime >= scrollLength)
            {
                xOffset = scrollLength;
                switchStage = true;
            }
            else
            {
                xOffset = deltaTime % scrollLength;
            }
        }
        else
        {
            switchStage = true;
        }

        *paramX -= xOffset;

        if (switchStage)
        {
            textScrollTimeStart = timer->millis();
            textScrollStage++;
            if (textScrollStage >= 4)
            {
                textScrollStage = 0;
            }
        }
    }
}

void NewList::drawImagesList(int x1, int x2, int y, int rowH)
{
    assert(data->imageIds != nullptr);
    assert(x1 <= x2);

    int rowsCount = (data->length >= data->linesCount) ? data->linesCount : data->length;

    API_COLOR(colors[4]);

    for (int rowIdx = 0; rowIdx < rowsCount; rowIdx++)
    {
        int dataIdx = (rowIdx + listPos);
        dataIdx = clamp(dataIdx, 0, data->length > 0 ? data->length - 1 : 0);

        int idx = data->imageIds[dataIdx];

        if (idx >= displayBitmapsCount)
        {
            continue;
        }

        int imgPosX = x1 + ((x2 - x1) / 2) - (bitmaps[idx].width / 2);
        int imgPosY = y + (rowH * rowIdx) - (bitmaps[idx].height / 2);

        API_BITMAP_SOURCE(bitmaps[idx].address);
        API_BITMAP_LAYOUT(bitmaps[idx].format, bitmaps[idx].linestride, bitmaps[idx].height);

        // if (style & controlStyleTransformImg && data->imgScaleTransform > 0)
        // {
        //     imgPosX +=
        //       bitmaps[idx].width - bitmaps[idx].width * (256.0f /
        //       (float)data->imgScaleTransform);
        //     imgPosY +=
        //       bitmaps[idx].height - bitmaps[idx].height * (256.0f /
        //       (float)data->imgScaleTransform);
        //
        //     API_BITMAP_TRANSFORM_A(data->imgScaleTransform);
        //     API_BITMAP_TRANSFORM_E(data->imgScaleTransform);
        // }

        API_BITMAP_SIZE(NEAREST, BORDER, BORDER, bitmaps[idx].width, bitmaps[idx].height);

        API_BEGIN(BITMAPS);
        API_VERTEX2F(imgPosX, imgPosY);
        API_END();
    }
}
