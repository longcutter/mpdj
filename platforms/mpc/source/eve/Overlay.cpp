#include "Overlay.h"

#include <math.h>
#include <vector>

namespace
{
// Tracker screen-button labels: 8 boxes of 100 x ~55 along the bottom of the
// 800x480 UI (label centres at y 424; the row the touchscreen treats as buttons
// starts at 400).
constexpr float kLabelTop = 408.0f, kLabelBottom = 480.0f, kLabelW = 100.0f;
constexpr uint32_t kRed = 0xFFE0282Au;
constexpr uint32_t kPopupBg = 0xFF101010u;

struct Rect
{
    int x0, y0, x1, y1; // [x0, x1) x [y0, y1) in target pixels
};

Rect toTarget(const eve::Target& t, float nx0, float ny0, float nx1, float ny1)
{
    Rect r{ (int)lroundf(t.offX + nx0 * t.scale), (int)lroundf(t.offY + ny0 * t.scale),
            (int)lroundf(t.offX + nx1 * t.scale), (int)lroundf(t.offY + ny1 * t.scale) };
    if (r.x0 < 0)
        r.x0 = 0;
    if (r.y0 < 0)
        r.y0 = 0;
    if (r.x1 > t.w)
        r.x1 = t.w;
    if (r.y1 > t.h)
        r.y1 = t.h;
    return r;
}

void fill(const eve::Target& t, int x0, int y0, int x1, int y1, uint32_t c)
{
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > t.w)
        x1 = t.w;
    if (y1 > t.h)
        y1 = t.h;
    for (int y = y0; y < y1; y++)
    {
        uint32_t* row = t.px + (size_t)y * t.stride;
        for (int x = x0; x < x1; x++)
            row[x] = c;
    }
}

void frame(const eve::Target& t, const Rect& r, int th, uint32_t c)
{
    fill(t, r.x0, r.y0, r.x1, r.y0 + th, c);
    fill(t, r.x0, r.y1 - th, r.x1, r.y1, c);
    fill(t, r.x0, r.y0, r.x0 + th, r.y1, c);
    fill(t, r.x1 - th, r.y0, r.x1, r.y1, c);
}
} // namespace

void Overlay::draw(const eve::Target& t) const
{
    if (!t.px)
        return;
    int th = t.scale >= 1.5f ? 3 : 2;

    int popup = popupLabel.load();
    if (popup >= 0 && popup < 8)
    {
        // Copy the label first (the box below may cross it), then draw it 2x in
        // the middle of the UI with a dark backing and a red frame.
        Rect src = toTarget(t, popup * kLabelW, kLabelTop, (popup + 1) * kLabelW, kLabelBottom);
        int sw = src.x1 - src.x0, sh = src.y1 - src.y0;
        if (sw > 0 && sh > 0)
        {
            std::vector<uint32_t> copy((size_t)sw * sh);
            for (int y = 0; y < sh; y++)
                for (int x = 0; x < sw; x++)
                    copy[(size_t)y * sw + x] = t.px[(size_t)(src.y0 + y) * t.stride + src.x0 + x];

            const int k = 2, pad = 3 * th;
            Rect ui = toTarget(t, 0, 0, eve::kNativeW, eve::kNativeH);
            int cx = (ui.x0 + ui.x1) / 2, cy = (ui.y0 + ui.y1) / 2;
            int dx0 = cx - sw * k / 2, dy0 = cy - sh * k / 2;
            Rect box{ dx0 - pad, dy0 - pad, dx0 + sw * k + pad, dy0 + sh * k + pad };
            fill(t, box.x0, box.y0, box.x1, box.y1, kPopupBg);
            for (int y = 0; y < sh * k; y++)
            {
                int py = dy0 + y;
                if (py < 0 || py >= t.h)
                    continue;
                uint32_t* row = t.px + (size_t)py * t.stride;
                const uint32_t* s = &copy[(size_t)(y / k) * sw];
                for (int x = 0; x < sw * k; x++)
                {
                    int px = dx0 + x;
                    if (px >= 0 && px < t.w)
                        row[px] = s[x / k];
                }
            }
            frame(t, box, th, kRed);
        }
    }

    int group = boxGroup.load();
    if (group == 0 || group == 1)
    {
        Rect r = toTarget(t, group * 4 * kLabelW + 2, kLabelTop, (group + 1) * 4 * kLabelW - 2, kLabelBottom - 1);
        frame(t, r, th, kRed);
    }
}
