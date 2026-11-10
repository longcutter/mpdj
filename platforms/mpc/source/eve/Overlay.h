// MPDJ-only marks drawn over the tracker's frame, after the software FT812 has
// rendered it and before it goes to the panel, so no core screen needs to know:
//   - a red box around the screen-button labels the Q-Links drive (modes 2-3)
//   - a value popup: the label under a parameter that is being changed, copied
//     and magnified in the middle of the screen (a finger on the label or the
//     knob row hides the original)
// Set from the main thread, drawn on the render thread.
#pragma once

#include "eve.h"

#include <atomic>
#include <stdint.h>

class Overlay
{
  public:
    void setBoxGroup(int group) { boxGroup.store(group); } // -1 none, 0 = labels 1-4, 1 = 5-8
    void setPopupLabel(int idx) { popupLabel.store(idx); } // -1 none, 0..7
    int getBoxGroup() const { return boxGroup.load(); }
    int getPopupLabel() const { return popupLabel.load(); }

    void draw(const eve::Target& t) const; // render thread

  private:
    std::atomic<int> boxGroup{ -1 };
    std::atomic<int> popupLabel{ -1 };
};
