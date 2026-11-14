// MPDJ (step 5d-1): Pad Assign's 4x4 pad grid, see padGridControl.h
#include "padGridControl.h"

#include "FT812.h"
#include "display.h"

#include <string.h>

namespace
{
constexpr uint32_t kCellBg = 0x1A1A1A;
constexpr uint32_t kCellBorder = 0x333333;
constexpr uint32_t kSelected = 0x3DDC63;
constexpr uint32_t kText = 0xFFFFFF;
constexpr uint32_t kSubText = 0xA8A8A8;
constexpr uint32_t kDimText = 0x5A5A5A;
constexpr int kGap = 5;
} // namespace

cPadGrid::cPadGrid(strControlProperties* properties)
  : cDisplayControl(controlIdMpdjPadGrid, 1)
{
    if (!properties)
        return;
    posX = properties->x;
    posY = properties->y;
    width = properties->w;
    height = properties->h;
    style = properties->style;
    data = (strPadGridData*)properties->data;
}

void cPadGrid::update()
{
    if (!data)
        return;
    const int cw = (width - 3 * kGap) / 4, ch = (height - 3 * kGap) / 4;
    const strFont* big = &fonts[1];
    const strFont* small = &fonts[0];
    for (int i = 0; i < 16; i++)
    {
        const strPadGridData::Cell& c = data->cells[i];
        int col = i % 4, row = 3 - i / 4; // pad 1 bottom left
        int x = posX + col * (cw + kGap), y = posY + row * (ch + kGap);
        bool sel = data->selected == i;

        API_LINE_WIDTH(16);
        API_COLOR(kCellBg);
        API_BEGIN(RECTS);
        API_VERTEX2F(x, y);
        API_VERTEX2F(x + cw, y + ch);
        API_END();

        if (c.colour)
        {
            API_COLOR(c.colour);
            API_BEGIN(RECTS);
            API_VERTEX2F(x, y);
            API_VERTEX2F(x + cw, y + 4);
            API_END();
        }

        API_COLOR(sel ? kSelected : kCellBorder);
        API_LINE_WIDTH(sel ? 24 : 8);
        API_BEGIN(LINE_STRIP);
        API_VERTEX2F(x, y);
        API_VERTEX2F(x + cw, y);
        API_VERTEX2F(x + cw, y + ch);
        API_VERTEX2F(x, y + ch);
        API_VERTEX2F(x, y);
        API_END();

        API_COLOR(c.empty ? kDimText : kText);
        API_CMD_TEXT(x + 6, y + 8, big->handle, 0, c.name);
        API_COLOR(c.empty ? kDimText : kSubText);
        if (c.program[0])
            API_CMD_TEXT(x + 6, y + 8 + big->height + 2, small->handle, 0, c.program,
                         getMaxTextLengthThatFitSpecificWidth(small, c.program, cw - 10));
        API_COLOR(c.empty ? kDimText : kSubText);
        API_CMD_TEXT(x + 6, y + ch - small->height - 6, small->handle, 0, c.marks);
    }
}
