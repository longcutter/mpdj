// MPDJ (step 5d-1): the Pad Assign screen's 4x4 pad grid. Each cell shows the
// pad's name, its program and a line of marks (track, choke group, cut), with
// its colour as a bar along the top; the selected pad is framed. Laid out like
// the MPC's pads: pad 1 bottom left. MIT, see NOTICE.md (MPDJ additions).
#pragma once

#include "displayControls.h"

struct strPadGridData
{
    struct Cell
    {
        char name[4];     // "A03"
        char program[24]; // "03 Closed Hat", "" = no program
        char marks[16];   // "T3  G1 Cut"
        uint32_t colour;  // 0xRRGGBB bar; 0 = none
        bool empty;       // no program / empty slot: dimmed
    } cells[16];          // index = pad number - 1
    int8_t selected = -1; // 0-15
};

class cPadGrid : public cDisplayControl
{
  public:
    cPadGrid(strControlProperties* properties = nullptr);
    virtual ~cPadGrid() {}

    virtual void update();

    virtual void setStyle(uint32_t style) { this->style = style; }
    virtual void setText(const char* text) { this->text = text; }
    virtual void setValue(int value) { this->value = value; }
    virtual void setColors(const uint32_t* colors) { this->colors = colors; }
    virtual void setData(void* data) { this->data = (strPadGridData*)data; }

  private:
    strPadGridData* data = nullptr;
};
