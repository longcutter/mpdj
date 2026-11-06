// Software emulation of the BridgeTek FT812 (EVE) graphics chip for MPDJ.
//
// The Tracker UI talks to an FT812 over SPI: it streams co-processor commands,
// the chip turns them into a display list (RAM_DL) and renders that list.
// Here, Chip plays the co-processor and memory, and Renderer rasterizes a
// display list into a 32-bit pixel buffer at any scale and offset.
//
// Two private display-list opcodes (0x3A, 0x3B) carry gradients and packed text
// runs. They keep display lists at least as compact as the real chip's, which
// matters because the UI budgets each frame's lists against the 8 KB RAM_DL.
#pragma once

#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace eve
{

constexpr uint32_t kRamGSize = 1024 * 1024;
constexpr uint32_t kRamDL = 0x300000;
constexpr uint32_t kDLWords = 2048; // 8 KB
constexpr int kNativeW = 800;
constexpr int kNativeH = 480;

// Private display-list opcodes (FT81x leaves 0x30-0x3F unused).
constexpr uint8_t kOpGradient = 0x3A; // + 4 words: xy0, rgb0, xy1, rgb1
constexpr uint8_t kOpTextRun = 0x3B;  // handle<<12 | count, then xy, then chars packed 4/word

// Output surface. Pixels are 0xAARRGGBB.
struct Target
{
    uint32_t* px = nullptr;
    int w = 0, h = 0, stride = 0; // stride in pixels
    float scale = 1.0f;           // output = native * scale + offset
    float offX = 0, offY = 0;
    bool smoothBitmaps = true; // bilinear-filter bitmaps when the scale isn't an integer
};

struct BitmapHandle
{
    uint32_t source = 0;
    uint8_t format = 0;
    uint16_t stride = 0, layoutH = 0;
    uint8_t filter = 0, wrapX = 0, wrapY = 0;
    uint16_t width = 0, height = 0;
};

struct FontInfo
{
    bool valid = false;
    uint32_t metrics = 0; // RAM_G address of the 148-byte metric block
    uint8_t firstChar = 0;
};

class Chip;

class Renderer
{
  public:
    explicit Renderer(Chip& chip)
      : chip(chip)
    {
    }
    // Runs a display list. Bitmap handle state starts from `start` and, if `end` is
    // given, is written back there. With no target, only state is tracked (no drawing).
    void render(const uint32_t* dl,
                int words,
                const Target* t,
                const BitmapHandle* start,
                BitmapHandle* end = nullptr);

    // Handle state after the most recently swapped display list, as on the chip.
    BitmapHandle live[32];

    // Drops cached glyphs whose source memory overlaps a write to chip memory.
    void invalidate(uint32_t addr, uint32_t size);

  private:
    struct Context
    {
        uint8_t alphaFunc = 7, alphaRef = 0;
        uint8_t blendSrc = 2, blendDst = 4; // SRC_ALPHA, ONE_MINUS_SRC_ALPHA
        uint8_t cell = 0, handle = 0;
        uint32_t clearRGB = 0;
        uint8_t clearA = 0;
        uint32_t colorRGB = 0xFFFFFF;
        uint8_t colorA = 255;
        uint8_t colorMask = 0xF; // r g b a
        uint16_t lineWidth = 16, pointSize = 16;
        int scX = 0, scY = 0, scW = 2048, scH = 2048;
        int32_t tA = 256, tB = 0, tC = 0, tD = 0, tE = 256, tF = 0;
        uint8_t vertexFormat = 4;
        int32_t translateX = 0, translateY = 0; // 1/16 px
        uint32_t paletteSource = 0;
    };

    struct Clip
    {
        int x0, y0, x1, y1; // output pixels, half-open
    };

    void runList(const uint32_t* dl, int words);
    void vertex(float x, float y, int handle, int cell);
    void drawRect(float x0, float y0, float x1, float y1);
    void drawLine(float x0, float y0, float x1, float y1);
    void drawPoint(float x, float y);
    void drawBitmap(float x, float y, int handle, int cell);
    void drawTextRun(const uint32_t* words, int count);
    void drawGradient(const uint32_t* words);
    void clear(bool color);

    // Rasterized luminance bitmaps (glyphs) at one scale and sub-pixel phase.
    struct GlyphKey
    {
        uint32_t addr, scaleBits;
        uint16_t stride, layoutH, w, h;
        uint8_t format, flags, phaseX, phaseY;
        bool operator==(const GlyphKey& o) const
        {
            return addr == o.addr && scaleBits == o.scaleBits && stride == o.stride &&
                   layoutH == o.layoutH && w == o.w && h == o.h && format == o.format &&
                   flags == o.flags && phaseX == o.phaseX && phaseY == o.phaseY;
        }
    };
    struct GlyphKeyHash
    {
        size_t operator()(const GlyphKey& k) const
        {
            uint64_t v = k.addr * 0x9E3779B97F4A7C15ull ^ k.scaleBits ^ ((uint64_t)k.w << 20) ^
                         ((uint64_t)k.h << 32) ^ ((uint64_t)k.phaseX << 44) ^
                         ((uint64_t)k.phaseY << 48) ^ ((uint64_t)k.format << 52) ^
                         ((uint64_t)k.flags << 58) ^ ((uint64_t)k.stride << 7) ^ k.layoutH;
            return (size_t)(v ^ (v >> 29));
        }
    };
    struct Glyph
    {
        int w = 0, h = 0;
        std::vector<uint8_t> alpha;
    };
    bool drawCachedGlyph(float x, float y, const BitmapHandle& b, int cell, bool bilinear);

    Clip clip() const;
    void fillSpan(uint32_t* row, int x0, int x1, uint32_t rgb, int a) const;
    void blendPixel(uint32_t* p, uint32_t rgb, int a) const;
    bool fetchTexel(const BitmapHandle& b, int cell, int u, int v, uint8_t out[4]) const;

    Chip& chip;
    const Target* t = nullptr;
    Context ctx;
    Context stack[16];
    int stackDepth = 0;
    uint8_t prim = 0;
    int nVerts = 0;
    float lastX = 0, lastY = 0;
    BitmapHandle handles[32];

    std::unordered_map<GlyphKey, Glyph, GlyphKeyHash> glyphs;
    size_t glyphBytes = 0;
    uint32_t glyphLo = 0xFFFFFFFF, glyphHi = 0; // source memory covered by the cache
};

class Chip
{
  public:
    Chip();

    // Command FIFO, fed by the FT812 API layer through DisplayDriver.
    void write32(uint32_t w);
    uint32_t writeString(const char* s, uint32_t length); // returns bytes written (padded)
    void writeData(const uint8_t* data, uint32_t size);   // raw data after LOADIMAGE / MEMWRITE
    void execute();                                       // run every complete command

    uint32_t displayListOffset();
    void imageProps(uint32_t& endAddr, uint32_t& w, uint32_t& h);

    // Memory access in chip address space (RAM_G and RAM_DL).
    void memWrite(uint32_t addr, const uint8_t* data, uint32_t size);
    void memRead(uint32_t addr, uint8_t* data, uint32_t size);
    uint8_t rd8(uint32_t addr) const { return addr < kRamGSize ? ramG[addr] : 0; }
    uint16_t rd16(uint32_t addr) const { return rd8(addr) | (rd8(addr + 1) << 8); }
    uint32_t rd32(uint32_t addr) const { return rd16(addr) | ((uint32_t)rd16(addr + 2) << 16); }

    const FontInfo& font(int handle) const { return fonts[handle & 31]; }
    int resolveFont(int handle) const; // maps ROM font handles to loaded ones

    // Called on CMD_SWAP with the finished display list.
    void (*onSwap)(void* user, const uint32_t* dl, int words) = nullptr;
    void* onSwapUser = nullptr;

    // Saves / restores everything needed to re-render the last swapped frame
    // (display list, handle state, fonts, RAM_G). Used by tools/eve_replay.
    bool saveCapture(const char* path) const;
    bool loadCapture(const char* path);

    // Renders the last swapped list at native size (for CMD_SNAPSHOT2).
    std::vector<uint32_t> renderSnapshot();

    // The last swapped display list and the bitmap handle state it starts from.
    uint32_t frontWords = 0;
    uint32_t front[kDLWords];
    BitmapHandle frontHandles[32];

    Renderer renderer;

    // Render-thread support: copy everything a render needs (RAM_G written since
    // the last sync, fonts, the front list and its handle state) into `dst`, a
    // chip used only for rendering. Its glyph cache is invalidated as needed.
    void syncTo(Chip& dst);

  private:
    void markDirty(uint32_t addr, uint32_t size);
    std::vector<std::pair<uint32_t, uint32_t>> dirty; // RAM_G [start, end) since last sync
    bool allDirty = true;

    bool runCommand(size_t& pos); // false if the command is incomplete
    void dlWrite(uint32_t w);
    void emitText(int x, int y, int font, uint16_t options, const char* s, int len);
    void finishImage();
    void snapshot2(uint32_t fmt, uint32_t ptr, int x, int y, int w, int h);

    uint8_t* ramG;
    uint32_t dl[kDLWords];
    uint32_t dlPtr = 0; // words
    std::vector<uint8_t> fifo;

    FontInfo fonts[32];
    double m[6] = { 1, 0, 0, 0, 1, 0 }; // a b c / d e f: screen = M * texture
    int numberBase = 10;

    bool imageActive = false;
    uint32_t imageAddr = 0, imageOptions = 0;
    std::vector<uint8_t> imageData;
    uint32_t imgEnd = 0, imgW = 0, imgH = 0;

    uint32_t memWritePtr = 0, memWriteLeft = 0;
    bool warnedUnknown = false;
};

} // namespace eve
