// MPDJ additions (MIT, see NOTICE.md): full-screen visuals drawn at the panel's
// own resolution instead of the tracker's frame (2026-11-16, Mike: a screensaver
// now, and a framework that a visualization screen can share later).
//   - the screensaver: MpcInput starts it after Config's idle time and any control
//     ends it (DisplayDriverImpl draws a new frame every ~33 ms while it runs)
//   - the visuals react to the output level (MpcAudio::outputLevel)
// Started / stopped from the main thread, drawn on the render thread.
#pragma once

#include "eve.h"

#include <atomic>
#include <stdint.h>

class Visuals
{
  public:
    enum Kind : int
    {
        kLogo,  // the MPDJ logo drifting and bouncing, its colour cycling
        kStars, // a starfield flying faster with the music
        kCount
    };
    static const char* name(int kind);

    void start(int kind);
    void stop() { on.store(false); }
    bool active() const { return on.load(); }

    void draw(const eve::Target& t); // render thread

  private:
    void drawLogo(const eve::Target& t, float level);
    void drawStars(const eve::Target& t, float level);

    std::atomic<bool> on{ false };
    std::atomic<int> kind{ kLogo };
    std::atomic<bool> fresh{ false }; // clear the whole target on the next frame

    // render-thread state
    float level = 0.0f;
    float lx = 100, ly = 100, vx = 3.2f, vy = 2.1f, hue = 0.0f;
    int prevX0 = 0, prevY0 = 0, prevX1 = 0, prevY1 = 0;
    static constexpr int kStarsN = 260;
    struct Star
    {
        float x, y, z;
        int px, py, size; // drawn last frame (px < 0: not drawn)
    } stars[kStarsN];
    uint32_t seed = 12345;
    float rnd();
};
