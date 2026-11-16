// MPDJ additions (MIT, see NOTICE.md): full-screen visuals (see Visuals.h)
#include "Visuals.h"

#include "../audio/MpcAudio.h"

#include <math.h>
#include <string.h>

extern const uint8_t mpdj_logo_320x162_l4[25920]; // MpdjLogo.cpp: 320 x 162, L4

namespace
{
constexpr int kLogoW = 320, kLogoH = 162, kLogoScale = 2; // 640 x 324 on the 1280 x 800 panel

void fillRect(const eve::Target& t, int x0, int y0, int x1, int y1, uint32_t c)
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

uint32_t hsv(float h, float s, float v)
{
    h = fmodf(h, 1.0f) * 6.0f;
    int i = (int)h;
    float f = h - i, p = v * (1 - s), q = v * (1 - s * f), r = v * (1 - s * (1 - f));
    float R, G, B;
    switch (i % 6)
    {
    case 0: R = v, G = r, B = p; break;
    case 1: R = q, G = v, B = p; break;
    case 2: R = p, G = v, B = r; break;
    case 3: R = p, G = q, B = v; break;
    case 4: R = r, G = p, B = v; break;
    default: R = v, G = p, B = q; break;
    }
    return 0xFF000000u | (uint32_t)(R * 255) << 16 | (uint32_t)(G * 255) << 8 | (uint32_t)(B * 255);
}
} // namespace

const char* Visuals::name(int k)
{
    static const char* const kNames[kCount] = { "Logo", "Starfield" };
    return kNames[k >= 0 && k < kCount ? k : 0];
}

void Visuals::start(int k)
{
    kind.store(k >= 0 && k < kCount ? k : 0);
    fresh.store(true);
    on.store(true);
}

float Visuals::rnd()
{
    seed = seed * 1664525u + 1013904223u;
    return (seed >> 8) / 16777216.0f;
}

void Visuals::draw(const eve::Target& t)
{
    // the output level, with a quick attack and a slow fall
    float in = MpcAudio::outputLevel.load();
    level = in > level ? in : level * 0.92f + in * 0.08f;
    if (fresh.exchange(false))
    {
        fillRect(t, 0, 0, t.w, t.h, 0xFF000000u);
        prevX1 = prevX0;
        for (Star& s : stars)
        {
            s.x = rnd() * 2 - 1;
            s.y = rnd() * 2 - 1;
            s.z = 0.05f + rnd();
            s.px = -1;
        }
    }
    if (kind.load() == kStars)
        drawStars(t, level);
    else
        drawLogo(t, level);
}

void Visuals::drawLogo(const eve::Target& t, float lvl)
{
    const int w = kLogoW * kLogoScale, h = kLogoH * kLogoScale;
    // erase where it was, move, bounce off the edges
    fillRect(t, prevX0, prevY0, prevX1, prevY1, 0xFF000000u);
    float speed = 1.0f + 2.0f * lvl;
    lx += vx * speed;
    ly += vy * speed;
    if (lx < 0)
        lx = 0, vx = fabsf(vx);
    if (ly < 0)
        ly = 0, vy = fabsf(vy);
    if (lx + w > t.w)
        lx = (float)(t.w - w), vx = -fabsf(vx);
    if (ly + h > t.h)
        ly = (float)(t.h - h), vy = -fabsf(vy);
    hue += 0.002f + 0.01f * lvl;
    uint32_t c = hsv(hue, 0.65f, 0.6f + 0.4f * (lvl > 1 ? 1 : lvl));
    uint8_t cr = (c >> 16) & 0xFF, cg = (c >> 8) & 0xFF, cb = c & 0xFF;
    int x0 = (int)lx, y0 = (int)ly;
    for (int y = 0; y < h && y0 + y < t.h; y++)
    {
        const uint8_t* src = mpdj_logo_320x162_l4 + (size_t)(y / kLogoScale) * (kLogoW / 2);
        uint32_t* row = t.px + (size_t)(y0 + y) * t.stride + x0;
        for (int x = 0; x < w && x0 + x < t.w; x++)
        {
            int sx = x / kLogoScale;
            uint8_t b = src[sx >> 1];
            int ink = (sx & 1) ? (b & 0x0F) : (b >> 4);
            row[x] = ink ? 0xFF000000u | (uint32_t)(cr * ink / 15) << 16 | (uint32_t)(cg * ink / 15) << 8 |
                             (uint32_t)(cb * ink / 15)
                         : 0xFF000000u;
        }
    }
    prevX0 = x0, prevY0 = y0, prevX1 = x0 + w, prevY1 = y0 + h;
}

void Visuals::drawStars(const eve::Target& t, float lvl)
{
    const float cx = t.w * 0.5f, cy = t.h * 0.5f, f = t.h * 0.6f;
    float speed = 0.006f + 0.05f * (lvl > 1 ? 1 : lvl);
    hue += 0.001f + 0.004f * lvl;
    for (Star& s : stars)
    {
        if (s.px >= 0)
            fillRect(t, s.px, s.py, s.px + s.size, s.py + s.size, 0xFF000000u);
        s.z -= speed;
        if (s.z <= 0.02f)
        {
            s.x = rnd() * 2 - 1;
            s.y = rnd() * 2 - 1;
            s.z = 1.0f;
        }
        int px = (int)(cx + s.x / s.z * f), py = (int)(cy + s.y / s.z * f);
        if (px < 0 || py < 0 || px >= t.w || py >= t.h)
        {
            s.px = -1;
            continue;
        }
        float near = 1.0f - s.z; // 0 far .. 1 near
        int size = 2 + (int)(near * near * 6);
        uint32_t c = hsv(hue + s.x * 0.15f, 0.2f + 0.4f * (lvl > 1 ? 1 : lvl), 0.45f + 0.55f * near);
        fillRect(t, px, py, px + size, py + size, c);
        s.px = px, s.py = py, s.size = size;
    }
}
