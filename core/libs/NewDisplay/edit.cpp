#include "FT812.h"

#include "commonControls.h"
#include "display.h"
#include "displayControls.h"
#include "string.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
cEdit::cEdit(strControlProperties* properties)
  : cDisplayControl(controlIdEdit, 1)
{
    colorsCount = 5;
    colors = display.getTheme().editBoxColors;

    if (properties == nullptr)
    {
        posX = 0;
        posY = 0;
        text = nullptr;
        width = 0;
        height = 0;
        style = 0;
        value = 0;
        data = nullptr;
        return;
    }

    colors = (properties->colors == nullptr) ? colors : properties->colors;

    posX = properties->x;
    posY = properties->y;

    text = properties->text;

    value = properties->value;

    data = (properties->data) != nullptr ? (strEditData*)(properties->data) : nullptr;

    width = properties->w;
    height = properties->h;

    setStyle(properties->style);
}

cEdit::~cEdit()
{
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cEdit::setStyle(uint32_t style)
{
    this->style = style;

    textStyle = (style & controlStyleCenterX ? OPT_CENTERX : 0) |
                (style & controlStyleCenterY ? OPT_CENTERY : 0) |
                (style & controlStyleRightX ? OPT_RIGHTX : 0);

    int8_t fontIndex = FONT_INDEX_FROM_STYLE;
    fontIndex = (fontIndex >= 0) ? fontIndex : 0;
    font = &fonts[fontIndex];
}

void cEdit::setText(const char* text)
{
    this->text = text;
}

void cEdit::setValue(int value)
{
    this->value = value;
}

void cEdit::setColors(const uint32_t* colors)
{
    this->colors = colors;
}

void cEdit::setData(void* data)
{
    this->data = (strEditData*)data;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void cEdit::update()
{
    int16_t border_x = posX - 2;

    if (style & controlStyleCenterX)
    {
        border_x = posX - (width / 2) - 1;
    }

    if (style & controlStyleBackground)
    {
        if (style & controlStyleRoundedBorder)
            API_LINE_WIDTH(32);
        else
            API_LINE_WIDTH(16);

        API_COLOR(colors[4]);
        // background
        API_BEGIN(RECTS);
        API_VERTEX2F(border_x, posY);
        API_VERTEX2F(border_x + width, posY + height);

        API_COLOR(colors[1]);
        // conteiner
        API_VERTEX2F(border_x + 10, posY + 10);
        API_VERTEX2F(border_x + width - 10, posY + height - 10);
        API_END();
    }
    if (style & controlStyleBorder)
    {
        API_COLOR(colors[2]);
        API_LINE_WIDTH(8);

        API_BEGIN(LINE_STRIP);
        API_VERTEX2F(border_x - 1, posY - 1);
        API_VERTEX2F(border_x + width + 1, posY - 1);
        API_VERTEX2F(border_x + width + 1, posY + height + 1);
        API_VERTEX2F(border_x - 1, posY + height + 1);
        API_VERTEX2F(border_x - 1, posY - 1);
        API_END();
    }

    if (data != nullptr && data->isFrame)
    {
        API_COLOR(colors[2]);

        API_LINE_WIDTH(kRedFrameWidth);

        API_BEGIN(LINE_STRIP);
        API_VERTEX2F(border_x - 1, posY - 1);
        API_VERTEX2F(border_x + width + 1, posY - 1);
        API_VERTEX2F(border_x + width + 1, posY + height + 1);
        API_VERTEX2F(border_x - 1, posY + height + 1);
        API_VERTEX2F(border_x - 1, posY - 1);
        API_END();
    }

    API_COLOR(colors[2]);
    API_LINE_WIDTH(16);
    API_BEGIN(LINES);
    if (!text)
        text = ""; // MPDJ: null text (address 0 is readable on the MCU)
    uint16_t localTextLength = strlen(text);
    uint16_t localTextWidth = getTextWidth(font, text, localTextLength);
    uint16_t cursorPosition = getTextWidth(font, text, value);
    uint16_t selYPos = posY + height / 2 - font->height / 2;
    uint16_t textPos = posX;

    if (textStyle & OPT_CENTERX)
    {
        textPos = posX - localTextWidth / 2;
        uint16_t xPos = textPos + cursorPosition;
        API_VERTEX2F(xPos, selYPos);
        API_VERTEX2F(xPos, selYPos + font->height);
    }
    else
    {
        uint16_t xPos = textPos + cursorPosition;
        API_VERTEX2F(xPos, selYPos);
        API_VERTEX2F(xPos, selYPos + font->height);
    }

    API_END();

    // MPDJ: the selection box first, the text over it (drawn the other way round
    // the opaque box hid a selected name, e.g. Save As / Save Kit's suggested name)
    if (data != nullptr && data->selectionWidth > 0)
    {
        API_COLOR(colors[3]);
        API_LINE_WIDTH(8);
        API_BEGIN(RECTS);

        uint16_t selStartPosition = textPos + getTextWidth(font, text, data->selectionStart);
        uint16_t selEndPosition =
          textPos + getTextWidth(font, text, data->selectionWidth + data->selectionStart);
        API_VERTEX2F(selStartPosition, selYPos);
        API_VERTEX2F(selEndPosition, selYPos + font->height);
        API_END();
        API_LINE_WIDTH(16);
    }

    if (text != nullptr)
    {
        API_COLOR(colors[0]);
        API_CMD_TEXT(posX, posY + height / 2, font->handle, OPT_CENTER | OPT_CENTERY, text);
    }
}
