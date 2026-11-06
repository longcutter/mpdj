// Presents rendered frames on the MPC One+ panel (/dev/fb0), or dumps them to
// PPM files for testing off-device.
//
// The panel is 800x1280 portrait; the tracker draws into a 1280x800 landscape
// buffer that is rotated on the way out. Environment overrides:
//   MPDJ_FB=/dev/fb0         framebuffer device ("none" for headless)
//   MPDJ_ROTATE=270          0, 90, 180 or 270 (default 270 on a portrait panel)
//   MPDJ_SCALE=1.6           tracker UI scale (default: fit 800x480 into the screen)
//   MPDJ_OFFSET_X / _Y       UI position in landscape pixels (default: centred)
//   MPDJ_DUMP=dir            also write frames as dir/frame_00001.ppm
//   MPDJ_DUMP_EVERY=n, MPDJ_DUMP_MAX=n
#pragma once

#include "eve.h"

#include <stdint.h>
#include <vector>

class FbOutput
{
  public:
    ~FbOutput();
    void open();
    eve::Target& target() { return tgt; }
    void beginFrame(); // clears the margins around the UI
    void present();

    int width() const { return logicalW; }
    int height() const { return logicalH; }

    // Maps a landscape screen point to the tracker's 800x480 coordinates (for touch).
    void toNative(float x, float y, float& nx, float& ny) const;

    // Physical panel (0 when headless) and panel pixel -> landscape screen pixel.
    int panelWidth() const { return xres; }
    int panelHeight() const { return yres; }
    void panelToScreen(int px, int py, int& x, int& y) const;

    // Stats for the last present(): tiles that changed / tiles copied to the fb.
    int lastTilesChanged() const { return tilesChanged; }
    int lastTilesWritten() const { return tilesWritten; }

  private:
    static constexpr int kTile = 32;
    void writeRotated(uint8_t* dst, int pageIdx);
    void writeTile(uint8_t* dst, int lx0, int ly0, int lx1, int ly1);
    void diffTiles();
    void verifyPage();
    uint32_t pack(uint32_t c) const;
    void dumpFrame();

    int fd = -1;
    uint8_t* fbMem = nullptr;
    size_t fbSize = 0;
    int xres = 0, yres = 0, lineLength = 0, bpp = 0, pages = 1, page = 0;
    int rOff = 16, gOff = 8, bOff = 0;
    int rotate = 0;

    int logicalW = 1280, logicalH = 800;
    std::vector<uint32_t> buffer;
    std::vector<uint32_t> prev;    // last presented frame, for the tile diff
    std::vector<uint8_t> tileMask; // per tile: bit p = page p still shows an old copy
    int tilesX = 0, tilesY = 0, tilesChanged = 0, tilesWritten = 0;
    bool fullPresent = false, directPack = false, fakeFb = false, verify = false;
    uint32_t alphaBits = 0;
    eve::Target tgt;

    const char* dumpDir = nullptr;
    int dumpEvery = 1, dumpMax = 200, frameNo = 0, dumped = 0;
};
