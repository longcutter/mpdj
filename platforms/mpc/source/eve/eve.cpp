// Software FT812 (EVE) emulation for MPDJ. See eve.h for the overview.
#include "eve.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "../3rdparty/stb_image.h"

namespace eve
{

// ------------------------------------------------------------------ constants
enum Prim : uint8_t
{
    kBitmaps = 1,
    kPoints = 2,
    kLines = 3,
    kLineStrip = 4,
    kRects = 9,
};

enum Format : uint8_t
{
    kARGB1555 = 0,
    kL1 = 1,
    kL4 = 2,
    kL8 = 3,
    kRGB332 = 4,
    kARGB2 = 5,
    kARGB4 = 6,
    kRGB565 = 7,
    kPaletted565 = 14,
    kPaletted4444 = 15,
    kPaletted8 = 16,
    kL2 = 17,
};

static int bitsPerPixel(int format)
{
    switch (format)
    {
        case kL1:
            return 1;
        case kL2:
            return 2;
        case kL4:
            return 4;
        case kARGB1555:
        case kARGB4:
        case kRGB565:
            return 16;
        default:
            return 8;
    }
}

static inline int32_t signExtend(uint32_t v, int bits)
{
    uint32_t m = 1u << (bits - 1);
    v &= (1u << bits) - 1;
    return (int32_t)((v ^ m) - m);
}

static inline float clamp01(float v)
{
    return v < 0 ? 0 : (v > 1 ? 1 : v);
}

// ------------------------------------------------------------------ chip
Chip::Chip()
  : renderer(*this)
{
    ramG = (uint8_t*)calloc(kRamGSize, 1);
    memset(dl, 0, sizeof dl);
    memset(front, 0, sizeof front);
}

void Chip::write32(uint32_t w)
{
    if (imageActive)
        finishImage();
    fifo.push_back((uint8_t)w);
    fifo.push_back((uint8_t)(w >> 8));
    fifo.push_back((uint8_t)(w >> 16));
    fifo.push_back((uint8_t)(w >> 24));
}

uint32_t Chip::writeString(const char* s, uint32_t length)
{
    uint32_t real = (uint32_t)strlen(s);
    if (length == 0 || length > real)
        length = real;
    fifo.insert(fifo.end(), s, s + length);
    uint32_t total = (length + 1 + 3) & ~3u;
    fifo.insert(fifo.end(), total - length, 0);
    return total;
}

void Chip::writeData(const uint8_t* data, uint32_t size)
{
    if (imageActive)
    {
        imageData.insert(imageData.end(), data, data + size);
        return;
    }
    fifo.insert(fifo.end(), data, data + size);
    while (fifo.size() & 3)
        fifo.push_back(0);
}

void Chip::execute()
{
    size_t pos = 0;
    while (pos + 4 <= fifo.size())
    {
        size_t start = pos;
        if (!runCommand(pos))
        {
            pos = start;
            break;
        }
    }
    fifo.erase(fifo.begin(), fifo.begin() + pos);
}

uint32_t Chip::displayListOffset()
{
    execute();
    return dlPtr * 4;
}

void Chip::imageProps(uint32_t& endAddr, uint32_t& w, uint32_t& h)
{
    execute();
    endAddr = imgEnd;
    w = imgW;
    h = imgH;
}

void Chip::memWrite(uint32_t addr, const uint8_t* data, uint32_t size)
{
    if (addr < kRamGSize)
        renderer.invalidate(addr, size);
    for (uint32_t i = 0; i < size; i++)
    {
        uint32_t a = addr + i;
        if (a < kRamGSize)
            ramG[a] = data[i];
        else if (a >= kRamDL && a < kRamDL + kDLWords * 4)
        {
            uint32_t off = a - kRamDL, sh = (off & 3) * 8;
            dl[off >> 2] = (dl[off >> 2] & ~(0xFFu << sh)) | ((uint32_t)data[i] << sh);
        }
    }
}

void Chip::memRead(uint32_t addr, uint8_t* data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
    {
        uint32_t a = addr + i;
        if (a < kRamGSize)
            data[i] = ramG[a];
        else if (a >= kRamDL && a < kRamDL + kDLWords * 4)
            data[i] = (uint8_t)(dl[(a - kRamDL) >> 2] >> (((a - kRamDL) & 3) * 8));
        else
            data[i] = 0;
    }
}

int Chip::resolveFont(int handle) const
{
    handle &= 31;
    if (fonts[handle].valid)
        return handle;
    if (handle < 16)
        return handle;
    // ROM fonts aren't available: use the loaded font closest in height.
    static const int romHeights[32] = {
        0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        8, 8, 16, 16, 13, 17, 20, 22, 29, 38, 16, 20, 25, 28, 36, 49
    };
    int best = handle, bestDiff = 1 << 30;
    for (int i = 0; i < 16; i++)
    {
        if (!fonts[i].valid)
            continue;
        int diff = abs((int)rd32(fonts[i].metrics + 140) - romHeights[handle]);
        if (diff < bestDiff)
        {
            bestDiff = diff;
            best = i;
        }
    }
    return best;
}

void Chip::dlWrite(uint32_t w)
{
    if (dlPtr < kDLWords)
        dl[dlPtr++] = w;
}

void Chip::emitText(int x, int y, int fontHandle, uint16_t options, const char* s, int len)
{
    int f = resolveFont(fontHandle);
    if (!fonts[f].valid || len <= 0)
        return;
    uint32_t metrics = fonts[f].metrics;
    int height = (int)rd32(metrics + 140);
    int width = 0;
    for (int i = 0; i < len; i++)
        if ((uint8_t)s[i] < 128)
            width += rd8(metrics + (uint8_t)s[i]);

    if (options & 2048) // OPT_RIGHTX
        x -= width;
    else if (options & 512) // OPT_CENTERX
        x -= width / 2;
    if (options & 1024) // OPT_CENTERY
        y -= height / 2;

    if (len > 4095)
        len = 4095;
    dlWrite(((uint32_t)kOpTextRun << 24) | ((uint32_t)f << 12) | (uint32_t)len);
    dlWrite(((uint32_t)(uint16_t)y << 16) | (uint16_t)x);
    for (int i = 0; i < len; i += 4)
    {
        uint32_t w = 0;
        for (int k = 0; k < 4 && i + k < len; k++)
            w |= (uint32_t)(uint8_t)s[i + k] << (8 * k);
        dlWrite(w);
    }
}

static void matMul(double m[6], double p, double q, double r, double s, double t, double u)
{
    double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5];
    m[0] = a * p + b * s;
    m[1] = a * q + b * t;
    m[2] = a * r + b * u + c;
    m[3] = d * p + e * s;
    m[4] = d * q + e * t;
    m[5] = d * r + e * u + f;
}

bool Chip::runCommand(size_t& pos)
{
    auto have = [&](size_t bytes) { return pos + bytes <= fifo.size(); };
    auto word = [&](size_t i)
    {
        size_t o = pos + i * 4;
        return (uint32_t)fifo[o] | ((uint32_t)fifo[o + 1] << 8) | ((uint32_t)fifo[o + 2] << 16) |
               ((uint32_t)fifo[o + 3] << 24);
    };

    uint32_t w = word(0);
    if ((w & 0xFFFFFF00u) != 0xFFFFFF00u)
    {
        dlWrite(w);
        pos += 4;
        return true;
    }

    switch (w & 0xFF)
    {
        case 0x00: // DLSTART
            dlPtr = 0;
            pos += 4;
            return true;

        case 0x01: // SWAP
            memcpy(front, dl, dlPtr * 4);
            frontWords = dlPtr;
            // Every swapped list runs on the chip, so carry its handle state forward
            // even if this frame is never drawn.
            memcpy(frontHandles, renderer.live, sizeof frontHandles);
            renderer.render(front, (int)frontWords, nullptr, renderer.live, renderer.live);
            pos += 4;
            if (onSwap)
                onSwap(onSwapUser, front, (int)frontWords);
            return true;

        case 0x0C: // TEXT
        {
            if (!have(13))
                return false;
            size_t s = pos + 12, e = s;
            while (e < fifo.size() && fifo[e])
                e++;
            if (e >= fifo.size())
                return false;
            size_t total = 12 + ((e - s + 1 + 3) & ~(size_t)3);
            if (!have(total))
                return false;
            uint32_t xy = word(1), fo = word(2);
            emitText((int16_t)(xy & 0xFFFF),
                     (int16_t)(xy >> 16),
                     (int16_t)(fo & 0xFFFF),
                     (uint16_t)(fo >> 16),
                     (const char*)&fifo[s],
                     (int)(e - s));
            pos += total;
            return true;
        }

        case 0x2E: // NUMBER
        {
            if (!have(16))
                return false;
            uint32_t xy = word(1), fo = word(2);
            uint32_t n = word(3);
            uint16_t opt = (uint16_t)(fo >> 16);
            bool neg = (opt & 256) && (int32_t)n < 0;
            uint32_t v = neg ? (uint32_t)(-(int64_t)(int32_t)n) : n;
            char digits[40];
            int nd = 0;
            int base = numberBase < 2 ? 10 : numberBase;
            do
            {
                int d = (int)(v % (uint32_t)base);
                digits[nd++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
                v /= (uint32_t)base;
            } while (v && nd < 32);
            int minDigits = opt & 0xFF;
            while (nd < minDigits && nd < 32)
                digits[nd++] = '0';
            char text[40];
            int len = 0;
            if (neg)
                text[len++] = '-';
            while (nd)
                text[len++] = digits[--nd];
            emitText(
              (int16_t)(xy & 0xFFFF), (int16_t)(xy >> 16), (int16_t)(fo & 0xFFFF), opt, text, len);
            pos += 16;
            return true;
        }

        case 0x0B: // GRADIENT
            if (!have(20))
                return false;
            dlWrite((uint32_t)kOpGradient << 24);
            for (int i = 1; i <= 4; i++)
                dlWrite(word(i));
            pos += 20;
            return true;

        case 0x1E: // APPEND
        {
            if (!have(12))
                return false;
            uint32_t ptr = word(1), num = word(2);
            for (uint32_t i = 0; i + 4 <= num; i += 4)
            {
                uint8_t b[4];
                memRead(ptr + i, b, 4);
                dlWrite(b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24));
            }
            pos += 12;
            return true;
        }

        case 0x1D: // MEMCPY
        {
            if (!have(16))
                return false;
            uint32_t dst = word(1), src = word(2), num = word(3);
            std::vector<uint8_t> tmp(num);
            memRead(src, tmp.data(), num);
            memWrite(dst, tmp.data(), num);
            pos += 16;
            return true;
        }

        case 0x1C: // MEMZERO
        case 0x1B: // MEMSET
        {
            bool set = (w & 0xFF) == 0x1B;
            if (!have(set ? 16 : 12))
                return false;
            uint32_t ptr = word(1);
            uint8_t value = set ? (uint8_t)word(2) : 0;
            uint32_t num = word(set ? 3 : 2);
            std::vector<uint8_t> tmp(num, value);
            memWrite(ptr, tmp.data(), num);
            pos += set ? 16 : 12;
            return true;
        }

        case 0x1A: // MEMWRITE
        {
            if (!have(12))
                return false;
            uint32_t ptr = word(1), num = word(2);
            size_t total = 12 + ((num + 3) & ~3u);
            if (!have(total))
                return false;
            memWrite(ptr, &fifo[pos + 12], num);
            pos += total;
            return true;
        }

        case 0x3B: // SETFONT2
        case 0x2B: // SETFONT
        {
            bool two = (w & 0xFF) == 0x3B;
            if (!have(two ? 16 : 12))
                return false;
            uint32_t f = word(1) & 31;
            uint32_t metrics = word(2);
            fonts[f].valid = true;
            fonts[f].metrics = metrics;
            fonts[f].firstChar = two ? (uint8_t)word(3) : 0;
            if (two)
            {
                // Like the chip: point the handle at the glyphs so that cell = character code.
                uint32_t fmt = rd32(metrics + 128), stride = rd32(metrics + 132);
                uint32_t gw = rd32(metrics + 136), gh = rd32(metrics + 140);
                uint32_t src = rd32(metrics + 144) - fonts[f].firstChar * stride * gh;
                dlWrite((5u << 24) | f);
                dlWrite((1u << 24) | (src & 0x3FFFFF));
                dlWrite((7u << 24) | ((fmt & 31) << 19) | ((stride & 0x3FF) << 9) | (gh & 0x1FF));
                dlWrite((40u << 24) | (((stride >> 10) & 3) << 2) | ((gh >> 9) & 3));
                dlWrite((8u << 24) | ((gw & 0x1FF) << 9) | (gh & 0x1FF));
                dlWrite((41u << 24) | (((gw >> 9) & 3) << 2) | ((gh >> 9) & 3));
            }
            pos += two ? 16 : 12;
            return true;
        }

        case 0x43: // SETBITMAP
        {
            if (!have(16))
                return false;
            uint32_t src = word(1), fw = word(2), h = word(3) & 0xFFFF;
            uint32_t fmt = fw & 0xFFFF, wpx = fw >> 16;
            uint32_t stride = (wpx * bitsPerPixel((int)fmt) + 7) / 8;
            dlWrite((1u << 24) | (src & 0x3FFFFF));
            dlWrite((7u << 24) | ((fmt & 31) << 19) | ((stride & 0x3FF) << 9) | (h & 0x1FF));
            dlWrite((40u << 24) | (((stride >> 10) & 3) << 2) | ((h >> 9) & 3));
            dlWrite((8u << 24) | ((wpx & 0x1FF) << 9) | (h & 0x1FF));
            dlWrite((41u << 24) | (((wpx >> 9) & 3) << 2) | ((h >> 9) & 3));
            pos += 16;
            return true;
        }

        case 0x26: // LOADIDENTITY
        {
            static const double id[6] = { 1, 0, 0, 0, 1, 0 };
            memcpy(m, id, sizeof m);
            pos += 4;
            return true;
        }

        case 0x27: // TRANSLATE
            if (!have(12))
                return false;
            matMul(m, 1, 0, (int32_t)word(1) / 65536.0, 0, 1, (int32_t)word(2) / 65536.0);
            pos += 12;
            return true;

        case 0x28: // SCALE
            if (!have(12))
                return false;
            matMul(m, (int32_t)word(1) / 65536.0, 0, 0, 0, (int32_t)word(2) / 65536.0, 0);
            pos += 12;
            return true;

        case 0x29: // ROTATE
        {
            if (!have(8))
                return false;
            double a = (int32_t)word(1) / 65536.0 * 2.0 * M_PI;
            double c = cos(a), s = sin(a);
            matMul(m, c, -s, 0, s, c, 0);
            pos += 8;
            return true;
        }

        case 0x2A: // SETMATRIX: bitmap transform = inverse of the current matrix
        {
            double det = m[0] * m[4] - m[1] * m[3];
            if (fabs(det) < 1e-9)
                det = 1e-9;
            double ia = m[4] / det, ib = -m[1] / det, id = -m[3] / det, ie = m[0] / det;
            double ic = -(ia * m[2] + ib * m[5]), iff = -(id * m[2] + ie * m[5]);
            auto fx = [](double v, uint32_t mask)
            { return (uint32_t)(int32_t)lround(v * 256.0) & mask; };
            dlWrite((21u << 24) | fx(ia, 0x1FFFF));
            dlWrite((22u << 24) | fx(ib, 0x1FFFF));
            dlWrite((23u << 24) | fx(ic, 0xFFFFFF));
            dlWrite((24u << 24) | fx(id, 0x1FFFF));
            dlWrite((25u << 24) | fx(ie, 0x1FFFF));
            dlWrite((26u << 24) | fx(iff, 0xFFFFFF));
            pos += 4;
            return true;
        }

        case 0x38: // SETBASE
            if (!have(8))
                return false;
            numberBase = (int)word(1);
            pos += 8;
            return true;

        case 0x25: // GETPROPS: results are read back through imageProps()
            if (!have(16))
                return false;
            pos += 16;
            return true;

        case 0x24: // LOADIMAGE: JPEG data follows through writeData()
        {
            if (!have(12))
                return false;
            imageAddr = word(1);
            imageOptions = word(2);
            imageActive = true;
            imageData.assign(fifo.begin() + pos + 12, fifo.end());
            pos = fifo.size();
            return true;
        }

        case 0x37: // SNAPSHOT2
        {
            if (!have(20))
                return false;
            uint32_t xy = word(3), wh = word(4);
            snapshot2(word(1),
                      word(2),
                      (int16_t)(xy & 0xFFFF),
                      (int16_t)(xy >> 16),
                      (int)(wh & 0xFFFF),
                      (int)(wh >> 16));
            pos += 20;
            return true;
        }

        // Accepted and ignored, with their parameter counts so the stream stays in sync.
        case 0x02: // INTERRUPT
        case 0x09: // BGCOLOR
        case 0x0A: // FGCOLOR
        case 0x34: // GRADCOLOR
        case 0x36: // SETROTATE
        case 0x3C: // SETSCRATCH
            if (!have(8))
                return false;
            pos += 8;
            return true;
        case 0x3F: // ROMFONT
            if (!have(12))
                return false;
            pos += 12;
            return true;
        case 0x17: // STOP
        case 0x42: // SYNC
        case 0x32: // COLDSTART
        case 0x2F: // SCREENSAVER
            pos += 4;
            return true;

        default:
            if (!warnedUnknown)
            {
                fprintf(
                  stderr, "eve: unsupported co-processor command 0x%08x, skipping batch\n", w);
                warnedUnknown = true;
            }
            pos = fifo.size();
            return true;
    }
}

void Chip::finishImage()
{
    imageActive = false;
    int w = 0, h = 0, n = 0;
    bool mono = imageOptions & 1; // OPT_MONO
    uint8_t* px =
      stbi_load_from_memory(imageData.data(), (int)imageData.size(), &w, &h, &n, mono ? 1 : 3);
    imageData.clear();
    imageData.shrink_to_fit();
    if (!px)
    {
        fprintf(stderr, "eve: JPEG decode failed\n");
        imgEnd = imageAddr;
        imgW = imgH = 0;
        return;
    }
    uint32_t bpp = mono ? 1 : 2;
    std::vector<uint8_t> out((size_t)w * h * bpp);
    for (int i = 0; i < w * h; i++)
    {
        if (mono)
            out[i] = px[i];
        else
        {
            uint16_t c = (uint16_t)(((px[i * 3] >> 3) << 11) | ((px[i * 3 + 1] >> 2) << 5) |
                                    (px[i * 3 + 2] >> 3));
            out[i * 2] = (uint8_t)c;
            out[i * 2 + 1] = (uint8_t)(c >> 8);
        }
    }
    stbi_image_free(px);
    memWrite(imageAddr, out.data(), (uint32_t)out.size());
    imgEnd = imageAddr + (uint32_t)out.size();
    imgW = (uint32_t)w;
    imgH = (uint32_t)h;
    if (!(imageOptions & 2)) // without OPT_NODL the chip also sets up a bitmap
    {
        uint32_t fmt = mono ? kL8 : kRGB565, stride = (uint32_t)w * bpp;
        dlWrite((1u << 24) | (imageAddr & 0x3FFFFF));
        dlWrite((7u << 24) | (fmt << 19) | ((stride & 0x3FF) << 9) | ((uint32_t)h & 0x1FF));
        dlWrite((40u << 24) | (((stride >> 10) & 3) << 2) | (((uint32_t)h >> 9) & 3));
        dlWrite((8u << 24) | (((uint32_t)w & 0x1FF) << 9) | ((uint32_t)h & 0x1FF));
        dlWrite((41u << 24) | ((((uint32_t)w >> 9) & 3) << 2) | (((uint32_t)h >> 9) & 3));
    }
}

static const char kCaptureMagic[8] = { 'M', 'P', 'D', 'J', 'E', 'V', 'E', '1' };

bool Chip::saveCapture(const char* path) const
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return false;
    fwrite(kCaptureMagic, 1, 8, f);
    fwrite(&frontWords, 4, 1, f);
    fwrite(front, 4, frontWords, f);
    fwrite(frontHandles, sizeof frontHandles, 1, f);
    fwrite(fonts, sizeof fonts, 1, f);
    fwrite(ramG, 1, kRamGSize, f);
    fclose(f);
    return true;
}

bool Chip::loadCapture(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return false;
    char magic[8];
    bool ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, kCaptureMagic, 8) &&
              fread(&frontWords, 4, 1, f) == 1 && frontWords <= kDLWords &&
              fread(front, 4, frontWords, f) == frontWords &&
              fread(frontHandles, sizeof frontHandles, 1, f) == 1 &&
              fread(fonts, sizeof fonts, 1, f) == 1 && fread(ramG, 1, kRamGSize, f) == kRamGSize;
    fclose(f);
    return ok;
}

std::vector<uint32_t> Chip::renderSnapshot()
{
    std::vector<uint32_t> buf(kNativeW * kNativeH, 0xFF000000u);
    Target t;
    t.px = buf.data();
    t.w = t.stride = kNativeW;
    t.h = kNativeH;
    t.smoothBitmaps = false;
    renderer.render(front, (int)frontWords, &t, frontHandles);
    return buf;
}

void Chip::snapshot2(uint32_t fmt, uint32_t ptr, int x, int y, int w, int h)
{
    std::vector<uint32_t> img = renderSnapshot();
    int bytes = fmt == 0x20 ? 4 : 2;
    if (fmt == 0x20)
        w /= 2; // ARGB8 snapshots take a doubled width
    std::vector<uint8_t> row((size_t)w * bytes);
    for (int j = 0; j < h; j++)
    {
        for (int i = 0; i < w; i++)
        {
            int sx = x + i, sy = y + j;
            uint32_t c =
              (sx >= 0 && sy >= 0 && sx < kNativeW && sy < kNativeH) ? img[sy * kNativeW + sx] : 0;
            uint8_t r = c >> 16, g = c >> 8, b = c, a = c >> 24;
            uint8_t* o = &row[(size_t)i * bytes];
            if (fmt == 0x20)
            {
                o[0] = b;
                o[1] = g;
                o[2] = r;
                o[3] = a;
            }
            else
            {
                uint16_t v =
                  fmt == kARGB4
                    ? (uint16_t)(((a >> 4) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))
                    : (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                o[0] = (uint8_t)v;
                o[1] = (uint8_t)(v >> 8);
            }
        }
        memWrite(ptr + (uint32_t)(j * w * bytes), row.data(), (uint32_t)row.size());
    }
}

// ------------------------------------------------------------------ renderer
Renderer::Clip Renderer::clip() const
{
    float s = t->scale;
    int x0 = (int)lroundf(ctx.scX * s + t->offX);
    int y0 = (int)lroundf(ctx.scY * s + t->offY);
    int x1 = (int)lroundf((ctx.scX + ctx.scW) * s + t->offX);
    int y1 = (int)lroundf((ctx.scY + ctx.scH) * s + t->offY);
    // Nothing draws outside the chip's 800x480 screen.
    int sx0 = (int)lroundf(t->offX), sy0 = (int)lroundf(t->offY);
    int sx1 = (int)lroundf(kNativeW * s + t->offX), sy1 = (int)lroundf(kNativeH * s + t->offY);
    Clip c;
    c.x0 = x0 > sx0 ? x0 : sx0;
    c.y0 = y0 > sy0 ? y0 : sy0;
    c.x1 = x1 < sx1 ? x1 : sx1;
    c.y1 = y1 < sy1 ? y1 : sy1;
    if (c.x0 < 0)
        c.x0 = 0;
    if (c.y0 < 0)
        c.y0 = 0;
    if (c.x1 > t->w)
        c.x1 = t->w;
    if (c.y1 > t->h)
        c.y1 = t->h;
    return c;
}

static inline int blendFactor(int f, int sa, int da)
{
    switch (f)
    {
        case 0:
            return 0;
        case 1:
            return 255;
        case 2:
            return sa;
        case 3:
            return da;
        case 4:
            return 255 - sa;
        case 5:
            return 255 - da;
        default:
            return 0;
    }
}

void Renderer::blendPixel(uint32_t* p, uint32_t rgb, int a) const
{
    if (a <= 0 && ctx.blendDst == 1)
        return;
    if (a >= 255 && ctx.blendSrc == 2 && (ctx.blendDst == 4 || ctx.blendDst == 0) &&
        ctx.colorMask == 0xF)
    {
        *p = 0xFF000000u | rgb;
        return;
    }
    uint32_t d = *p;
    int dr = (d >> 16) & 255, dg = (d >> 8) & 255, db = d & 255, da = d >> 24;
    int sr = (rgb >> 16) & 255, sg = (rgb >> 8) & 255, sb = rgb & 255;
    int fs = blendFactor(ctx.blendSrc, a, da), fd = blendFactor(ctx.blendDst, a, da);
    auto mix = [&](int s, int dd)
    {
        int v = (s * fs + dd * fd + 127) / 255;
        return v > 255 ? 255 : v;
    };
    int r = (ctx.colorMask & 8) ? mix(sr, dr) : dr;
    int g = (ctx.colorMask & 4) ? mix(sg, dg) : dg;
    int b = (ctx.colorMask & 2) ? mix(sb, db) : db;
    int al = (ctx.colorMask & 1) ? mix(a, da) : da;
    *p = ((uint32_t)al << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void Renderer::fillSpan(uint32_t* row, int x0, int x1, uint32_t rgb, int a) const
{
    if (a >= 255 && ctx.blendSrc == 2 && (ctx.blendDst == 4 || ctx.blendDst == 0) &&
        ctx.colorMask == 0xF)
    {
        uint32_t v = 0xFF000000u | rgb;
        for (int x = x0; x < x1; x++)
            row[x] = v;
        return;
    }
    for (int x = x0; x < x1; x++)
        blendPixel(&row[x], rgb, a);
}

void Renderer::invalidate(uint32_t addr, uint32_t size)
{
    if (glyphs.empty() || addr >= glyphHi || addr + size <= glyphLo)
        return;
    glyphs.clear();
    glyphBytes = 0;
    glyphLo = 0xFFFFFFFF;
    glyphHi = 0;
}

void Renderer::clear(bool color)
{
    if (!color)
        return;
    Clip c = clip();
    uint32_t v = ((uint32_t)ctx.clearA << 24) | (ctx.clearRGB & 0xFFFFFF);
    for (int y = c.y0; y < c.y1; y++)
    {
        uint32_t* row = t->px + (size_t)y * t->stride;
        for (int x = c.x0; x < c.x1; x++)
            row[x] = v;
    }
}

// Rectangles: the box between the two vertices (pixel centres), grown by the
// line width (a radius in 1/16 px) minus half a pixel, with rounded corners.
void Renderer::drawRect(float x0, float y0, float x1, float y1)
{
    float s = t->scale;
    float r = ctx.lineWidth / 16.0f;
    float grow = r - 0.5f;
    if (grow < 0)
        grow = 0;
    float bx0 = (x0 < x1 ? x0 : x1) + 0.5f, bx1 = (x0 < x1 ? x1 : x0) + 0.5f;
    float by0 = (y0 < y1 ? y0 : y1) + 0.5f, by1 = (y0 < y1 ? y1 : y0) + 0.5f;
    float cx = (bx0 + bx1) * 0.5f, cy = (by0 + by1) * 0.5f;
    float hx = (bx1 - bx0) * 0.5f, hy = (by1 - by0) * 0.5f;

    Clip c = clip();
    int X0 = (int)floorf((bx0 - grow) * s + t->offX) - 1,
        X1 = (int)ceilf((bx1 + grow) * s + t->offX) + 1;
    int Y0 = (int)floorf((by0 - grow) * s + t->offY) - 1,
        Y1 = (int)ceilf((by1 + grow) * s + t->offY) + 1;
    if (X0 < c.x0)
        X0 = c.x0;
    if (Y0 < c.y0)
        Y0 = c.y0;
    if (X1 > c.x1)
        X1 = c.x1;
    if (Y1 > c.y1)
        Y1 = c.y1;

    uint32_t rgb = ctx.colorRGB & 0xFFFFFF;
    // Within T of the box, coverage is complete: fill those spans directly.
    float T = grow - 0.5f / s;
    float fullRow = T < 0 ? T : 0;
    int spanX0 = (int)ceilf((cx - hx - T) * s + t->offX - 0.5f);
    int spanX1 = (int)floorf((cx + hx + T) * s + t->offX - 0.5f) + 1;
    if (spanX0 < X0)
        spanX0 = X0;
    if (spanX1 > X1)
        spanX1 = X1;
    for (int Y = Y0; Y < Y1; Y++)
    {
        float sy = (Y + 0.5f - t->offY) / s;
        float qy = fabsf(sy - cy) - hy;
        uint32_t* row = t->px + (size_t)Y * t->stride;
        bool spanRow = qy <= fullRow && spanX0 < spanX1;
        if (spanRow)
            fillSpan(row, spanX0, spanX1, rgb, ctx.colorA);
        for (int X = X0; X < X1; X++)
        {
            if (spanRow && X == spanX0)
            {
                X = spanX1 - 1;
                continue;
            }
            float sx = (X + 0.5f - t->offX) / s;
            float qx = fabsf(sx - cx) - hx;
            float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
            float inside = qx > qy ? qx : qy;
            float d = sqrtf(ox * ox + oy * oy) + (inside < 0 ? inside : 0);
            float cov = clamp01((grow - d) * s + 0.5f);
            if (cov > 0)
                blendPixel(&row[X], rgb, (int)(ctx.colorA * cov + 0.5f));
        }
    }
}

void Renderer::drawLine(float x0, float y0, float x1, float y1)
{
    float s = t->scale;
    float r = ctx.lineWidth / 16.0f;
    float ax = x0 + 0.5f, ay = y0 + 0.5f, bx = x1 + 0.5f, by = y1 + 0.5f;
    float pad = r + 1.0f / s;
    Clip c = clip();
    int X0 = (int)floorf(((ax < bx ? ax : bx) - pad) * s + t->offX);
    int X1 = (int)ceilf(((ax < bx ? bx : ax) + pad) * s + t->offX);
    int Y0 = (int)floorf(((ay < by ? ay : by) - pad) * s + t->offY);
    int Y1 = (int)ceilf(((ay < by ? by : ay) + pad) * s + t->offY);
    if (X0 < c.x0)
        X0 = c.x0;
    if (Y0 < c.y0)
        Y0 = c.y0;
    if (X1 > c.x1)
        X1 = c.x1;
    if (Y1 > c.y1)
        Y1 = c.y1;

    float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    uint32_t rgb = ctx.colorRGB & 0xFFFFFF;
    for (int Y = Y0; Y < Y1; Y++)
    {
        float py = (Y + 0.5f - t->offY) / s - ay;
        uint32_t* row = t->px + (size_t)Y * t->stride;
        for (int X = X0; X < X1; X++)
        {
            float px = (X + 0.5f - t->offX) / s - ax;
            float h = len2 > 0 ? clamp01((px * dx + py * dy) / len2) : 0;
            float ex = px - dx * h, ey = py - dy * h;
            float d = sqrtf(ex * ex + ey * ey);
            float cov = clamp01((r - d) * s + 0.5f);
            if (cov > 0)
                blendPixel(&row[X], rgb, (int)(ctx.colorA * cov + 0.5f));
        }
    }
}

void Renderer::drawPoint(float x, float y)
{
    float s = t->scale;
    float r = ctx.pointSize / 16.0f;
    float cx = x + 0.5f, cy = y + 0.5f;
    Clip c = clip();
    int X0 = (int)floorf((cx - r - 1) * s + t->offX), X1 = (int)ceilf((cx + r + 1) * s + t->offX);
    int Y0 = (int)floorf((cy - r - 1) * s + t->offY), Y1 = (int)ceilf((cy + r + 1) * s + t->offY);
    if (X0 < c.x0)
        X0 = c.x0;
    if (Y0 < c.y0)
        Y0 = c.y0;
    if (X1 > c.x1)
        X1 = c.x1;
    if (Y1 > c.y1)
        Y1 = c.y1;
    uint32_t rgb = ctx.colorRGB & 0xFFFFFF;
    for (int Y = Y0; Y < Y1; Y++)
    {
        float py = (Y + 0.5f - t->offY) / s - cy;
        uint32_t* row = t->px + (size_t)Y * t->stride;
        for (int X = X0; X < X1; X++)
        {
            float px = (X + 0.5f - t->offX) / s - cx;
            float cov = clamp01((r - sqrtf(px * px + py * py)) * s + 0.5f);
            if (cov > 0)
                blendPixel(&row[X], rgb, (int)(ctx.colorA * cov + 0.5f));
        }
    }
}

// Fetches one texel as RGBA. Returns false (transparent) outside a BORDER bitmap.
bool Renderer::fetchTexel(const BitmapHandle& b, int cell, int u, int v, uint8_t o[4]) const
{
    int bpp = bitsPerPixel(b.format);
    int layoutW = b.stride * 8 / bpp;
    int layoutH = b.layoutH;
    if (layoutW <= 0 || layoutH <= 0)
        return false;
    if (b.wrapX)
        u = ((u % layoutW) + layoutW) % layoutW;
    else if (u < 0 || u >= layoutW)
        return false;
    if (b.wrapY)
        v = ((v % layoutH) + layoutH) % layoutH;
    else if (v < 0 || v >= layoutH)
        return false;

    uint32_t row =
      (b.source + (uint32_t)cell * b.stride * layoutH + (uint32_t)v * b.stride) & 0x3FFFFF;
    uint32_t addr = (row + (uint32_t)(u * bpp / 8)) & 0x3FFFFF;
    auto argb4 = [&](uint16_t c)
    {
        o[3] = (uint8_t)(((c >> 12) & 15) * 17);
        o[0] = (uint8_t)(((c >> 8) & 15) * 17);
        o[1] = (uint8_t)(((c >> 4) & 15) * 17);
        o[2] = (uint8_t)((c & 15) * 17);
    };
    auto rgb565 = [&](uint16_t c)
    {
        o[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        o[2] = (uint8_t)((c & 31) * 255 / 31);
        o[3] = 255;
    };
    auto lum = [&](uint8_t l)
    {
        o[0] = o[1] = o[2] = 255;
        o[3] = l;
    };
    const Chip& c = chip;
    switch (b.format)
    {
        case kL1:
            lum((c.rd8(addr) >> (7 - (u & 7))) & 1 ? 255 : 0);
            break;
        case kL2:
            lum((uint8_t)(((c.rd8(addr) >> (6 - 2 * (u & 3))) & 3) * 85));
            break;
        case kL4:
        {
            uint8_t byte = c.rd8(addr);
            lum((uint8_t)(((u & 1) ? (byte & 15) : (byte >> 4)) * 17));
            break;
        }
        case kL8:
            lum(c.rd8(addr));
            break;
        case kRGB332:
        {
            uint8_t p = c.rd8(addr);
            o[0] = (uint8_t)(((p >> 5) & 7) * 255 / 7);
            o[1] = (uint8_t)(((p >> 2) & 7) * 255 / 7);
            o[2] = (uint8_t)((p & 3) * 85);
            o[3] = 255;
            break;
        }
        case kARGB2:
        {
            uint8_t p = c.rd8(addr);
            o[3] = (uint8_t)(((p >> 6) & 3) * 85);
            o[0] = (uint8_t)(((p >> 4) & 3) * 85);
            o[1] = (uint8_t)(((p >> 2) & 3) * 85);
            o[2] = (uint8_t)((p & 3) * 85);
            break;
        }
        case kARGB1555:
        {
            uint16_t p = c.rd16(addr);
            o[3] = (p & 0x8000) ? 255 : 0;
            o[0] = (uint8_t)(((p >> 10) & 31) * 255 / 31);
            o[1] = (uint8_t)(((p >> 5) & 31) * 255 / 31);
            o[2] = (uint8_t)((p & 31) * 255 / 31);
            break;
        }
        case kARGB4:
            argb4(c.rd16(addr));
            break;
        case kRGB565:
            rgb565(c.rd16(addr));
            break;
        case kPaletted565:
            rgb565(c.rd16(ctx.paletteSource + c.rd8(addr) * 2u));
            break;
        case kPaletted4444:
            argb4(c.rd16(ctx.paletteSource + c.rd8(addr) * 2u));
            break;
        case kPaletted8:
        {
            uint32_t p = c.rd32(ctx.paletteSource + c.rd8(addr) * 4u);
            o[0] = (uint8_t)(p >> 16);
            o[1] = (uint8_t)(p >> 8);
            o[2] = (uint8_t)p;
            o[3] = (uint8_t)(p >> 24);
            break;
        }
        default:
            return false;
    }
    return true;
}

void Renderer::drawBitmap(float x, float y, int handle, int cell)
{
    const BitmapHandle& b = handles[handle & 31];
    if (b.stride == 0 || b.layoutH == 0)
        return;
    float s = t->scale;
    int w = b.width ? b.width : 2048, h = b.height ? b.height : 2048;

    Clip c = clip();
    int X0 = (int)floorf(x * s + t->offX), X1 = (int)ceilf((x + w) * s + t->offX);
    int Y0 = (int)floorf(y * s + t->offY), Y1 = (int)ceilf((y + h) * s + t->offY);
    if (X0 < c.x0)
        X0 = c.x0;
    if (Y0 < c.y0)
        Y0 = c.y0;
    if (X1 > c.x1)
        X1 = c.x1;
    if (Y1 > c.y1)
        Y1 = c.y1;
    if (X0 >= X1 || Y0 >= Y1)
        return;

    float A = ctx.tA / 256.0f, B = ctx.tB / 256.0f, C = ctx.tC / 256.0f;
    float D = ctx.tD / 256.0f, E = ctx.tE / 256.0f, F = ctx.tF / 256.0f;
    bool integerScale = fabsf(s - roundf(s)) < 1e-3f;
    bool bilinear = b.filter == 1 || (t->smoothBitmaps && !integerScale);

    // Untransformed luminance bitmaps (all the text) go through the glyph cache.
    bool lum = b.format == kL1 || b.format == kL2 || b.format == kL4 || b.format == kL8;
    if (lum && ctx.tA == 256 && ctx.tE == 256 && !ctx.tB && !ctx.tD && !ctx.tC && !ctx.tF &&
        drawCachedGlyph(x, y, b, cell, bilinear))
        return;

    int cr = (ctx.colorRGB >> 16) & 255, cg = (ctx.colorRGB >> 8) & 255, cb = ctx.colorRGB & 255;
    for (int Y = Y0; Y < Y1; Y++)
    {
        float ly = (Y + 0.5f - t->offY) / s - y;
        if (ly < 0 || ly >= h)
            continue;
        uint32_t* row = t->px + (size_t)Y * t->stride;
        for (int X = X0; X < X1; X++)
        {
            float lx = (X + 0.5f - t->offX) / s - x;
            if (lx < 0 || lx >= w)
                continue;
            float u = A * lx + B * ly + C, v = D * lx + E * ly + F;
            float tr, tg, tb, ta;
            if (!bilinear)
            {
                uint8_t o[4];
                if (!fetchTexel(b, cell, (int)floorf(u), (int)floorf(v), o))
                    continue;
                tr = o[0];
                tg = o[1];
                tb = o[2];
                ta = o[3];
            }
            else
            {
                float fu = u - 0.5f, fv = v - 0.5f;
                int u0 = (int)floorf(fu), v0 = (int)floorf(fv);
                float ax = fu - u0, ay = fv - v0;
                float acc[4] = { 0, 0, 0, 0 };
                for (int k = 0; k < 4; k++)
                {
                    uint8_t o[4];
                    if (!fetchTexel(b, cell, u0 + (k & 1), v0 + (k >> 1), o))
                        continue;
                    float wgt = ((k & 1) ? ax : 1 - ax) * ((k >> 1) ? ay : 1 - ay) * o[3];
                    acc[0] += o[0] * wgt;
                    acc[1] += o[1] * wgt;
                    acc[2] += o[2] * wgt;
                    acc[3] += wgt;
                }
                if (acc[3] <= 0.5f)
                    continue;
                tr = acc[0] / acc[3];
                tg = acc[1] / acc[3];
                tb = acc[2] / acc[3];
                ta = acc[3];
            }
            int r = (int)(tr * cr / 255.0f + 0.5f), g = (int)(tg * cg / 255.0f + 0.5f),
                bl = (int)(tb * cb / 255.0f + 0.5f);
            int a = (int)(ta * ctx.colorA / 255.0f + 0.5f);
            blendPixel(&row[X], ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl, a);
        }
    }
}

bool Renderer::drawCachedGlyph(float x, float y, const BitmapHandle& b, int cell, bool bilinear)
{
    float s = t->scale;
    int w = b.width ? b.width : 2048, h = b.height ? b.height : 2048;
    if (w > 256 || h > 256)
        return false; // not a glyph; draw it directly

    // The coverage pattern depends only on the sub-pixel phase of the glyph origin.
    float bx = x * s + t->offX, by = y * s + t->offY;
    int Xi = (int)floorf(bx), Yi = (int)floorf(by);
    int phx = (int)lroundf((bx - Xi) * 16), phy = (int)lroundf((by - Yi) * 16);
    if (phx == 16)
    {
        Xi++;
        phx = 0;
    }
    if (phy == 16)
    {
        Yi++;
        phy = 0;
    }

    GlyphKey key;
    key.addr = (b.source + (uint32_t)cell * b.stride * b.layoutH) & 0x3FFFFF;
    memcpy(&key.scaleBits, &s, 4);
    key.stride = b.stride;
    key.layoutH = b.layoutH;
    key.w = (uint16_t)w;
    key.h = (uint16_t)h;
    key.format = b.format;
    key.flags = (uint8_t)((bilinear ? 1 : 0) | (b.wrapX << 1) | (b.wrapY << 2));
    key.phaseX = (uint8_t)phx;
    key.phaseY = (uint8_t)phy;

    auto it = glyphs.find(key);
    if (it == glyphs.end())
    {
        if (glyphBytes > 16u * 1024 * 1024)
            invalidate(0, 0xFFFFFFFF);
        Glyph g;
        g.w = (int)ceilf(w * s) + 1;
        g.h = (int)ceilf(h * s) + 1;
        g.alpha.assign((size_t)g.w * g.h, 0);
        float fx = phx / 16.0f, fy = phy / 16.0f;
        for (int j = 0; j < g.h; j++)
        {
            float ly = (j + 0.5f - fy) / s;
            if (ly < 0 || ly >= h)
                continue;
            for (int i = 0; i < g.w; i++)
            {
                float lx = (i + 0.5f - fx) / s;
                if (lx < 0 || lx >= w)
                    continue;
                float a = 0;
                uint8_t o[4];
                if (!bilinear)
                {
                    if (fetchTexel(b, cell, (int)floorf(lx), (int)floorf(ly), o))
                        a = o[3];
                }
                else
                {
                    float fu = lx - 0.5f, fv = ly - 0.5f;
                    int u0 = (int)floorf(fu), v0 = (int)floorf(fv);
                    float ax = fu - u0, ay = fv - v0;
                    for (int k = 0; k < 4; k++)
                        if (fetchTexel(b, cell, u0 + (k & 1), v0 + (k >> 1), o))
                            a += ((k & 1) ? ax : 1 - ax) * ((k >> 1) ? ay : 1 - ay) * o[3];
                }
                g.alpha[(size_t)j * g.w + i] = (uint8_t)(a + 0.5f);
            }
        }
        glyphBytes += g.alpha.size() + sizeof(Glyph) + sizeof(GlyphKey);
        uint32_t lo = key.addr, hi = key.addr + (uint32_t)b.stride * b.layoutH;
        if (lo < glyphLo)
            glyphLo = lo;
        if (hi > glyphHi)
            glyphHi = hi;
        it = glyphs.emplace(key, std::move(g)).first;
    }

    const Glyph& g = it->second;
    Clip c = clip();
    uint32_t rgb = ctx.colorRGB & 0xFFFFFF;
    int ca = ctx.colorA;
    for (int j = 0; j < g.h; j++)
    {
        int Y = Yi + j;
        if (Y < c.y0 || Y >= c.y1)
            continue;
        uint32_t* row = t->px + (size_t)Y * t->stride;
        const uint8_t* src = &g.alpha[(size_t)j * g.w];
        int i0 = c.x0 - Xi > 0 ? c.x0 - Xi : 0;
        int i1 = c.x1 - Xi < g.w ? c.x1 - Xi : g.w;
        for (int i = i0; i < i1; i++)
            if (src[i])
                blendPixel(&row[Xi + i], rgb, (src[i] * ca + 127) / 255);
    }
    return true;
}

void Renderer::drawTextRun(const uint32_t* words, int count)
{
    int handle = (words[0] >> 12) & 31;
    float x = (int16_t)(words[1] & 0xFFFF) + ctx.translateX / 16.0f;
    float y = (int16_t)(words[1] >> 16) + ctx.translateY / 16.0f;
    const FontInfo& f = chip.font(handle);
    for (int i = 0; i < count; i++)
    {
        uint8_t ch = (uint8_t)(words[2 + i / 4] >> (8 * (i & 3)));
        if (ch >= 128)
            continue;
        int adv = f.valid ? chip.rd8(f.metrics + ch) : 0;
        if (adv)
            drawBitmap(x, y, handle, ch);
        x += adv;
    }
}

void Renderer::drawGradient(const uint32_t* w)
{
    float x0 = (int16_t)(w[1] & 0xFFFF), y0 = (int16_t)(w[1] >> 16);
    float x1 = (int16_t)(w[3] & 0xFFFF), y1 = (int16_t)(w[3] >> 16);
    uint32_t c0 = w[2], c1 = w[4];
    float dx = x1 - x0, dy = y1 - y0, len2 = dx * dx + dy * dy;
    float s = t->scale;
    Clip c = clip();
    for (int Y = c.y0; Y < c.y1; Y++)
    {
        float py = (Y + 0.5f - t->offY) / s - y0;
        uint32_t* row = t->px + (size_t)Y * t->stride;
        for (int X = c.x0; X < c.x1; X++)
        {
            float px = (X + 0.5f - t->offX) / s - x0;
            float k = len2 > 0 ? clamp01((px * dx + py * dy) / len2) : 0;
            auto ch = [&](int sh)
            {
                float a = (c0 >> sh) & 255, b = (c1 >> sh) & 255;
                return (uint32_t)(a + (b - a) * k + 0.5f);
            };
            blendPixel(&row[X], (ch(16) << 16) | (ch(8) << 8) | ch(0), ctx.colorA);
        }
    }
}

void Renderer::vertex(float x, float y, int handle, int cell)
{
    if (!t)
    {
        nVerts++;
        return;
    }
    switch (prim)
    {
        case kBitmaps:
            drawBitmap(x, y, handle, cell);
            break;
        case kPoints:
            drawPoint(x, y);
            break;
        case kLines:
            if (nVerts & 1)
                drawLine(lastX, lastY, x, y);
            break;
        case kLineStrip:
            if (nVerts > 0)
                drawLine(lastX, lastY, x, y);
            break;
        case kRects:
            if (nVerts & 1)
                drawRect(lastX, lastY, x, y);
            break;
        default:
            break;
    }
    nVerts++;
    lastX = x;
    lastY = y;
}

void Renderer::render(const uint32_t* list,
                      int words,
                      const Target* target,
                      const BitmapHandle* start,
                      BitmapHandle* end)
{
    t = target;
    memcpy(handles, start, sizeof handles);
    runList(list, words);
    if (end)
        memcpy(end, handles, sizeof handles);
}

void Renderer::runList(const uint32_t* list, int words)
{
    ctx = Context();
    stackDepth = 0;
    prim = 0;
    nVerts = 0;

    int callStack[8];
    int callDepth = 0;
    int budget = 65536;

    for (int pc = 0; pc < words && budget-- > 0;)
    {
        uint32_t w = list[pc++];
        uint32_t top = w >> 30;
        if (top == 2) // VERTEX2II
        {
            float x = (float)((w >> 21) & 0x1FF) + ctx.translateX / 16.0f;
            float y = (float)((w >> 12) & 0x1FF) + ctx.translateY / 16.0f;
            vertex(x, y, (w >> 7) & 31, w & 127);
            continue;
        }
        if (top == 1) // VERTEX2F
        {
            float div = (float)(1 << ctx.vertexFormat);
            float x = signExtend(w >> 15, 15) / div + ctx.translateX / 16.0f;
            float y = signExtend(w, 15) / div + ctx.translateY / 16.0f;
            vertex(x, y, ctx.handle, ctx.cell);
            continue;
        }

        uint8_t op = (uint8_t)(w >> 24);
        BitmapHandle& bh = handles[ctx.handle & 31];
        switch (op)
        {
            case 0x00:
                return; // DISPLAY
            case 0x01:
                bh.source = w & 0x3FFFFF;
                break;
            case 0x02:
                ctx.clearRGB = w & 0xFFFFFF;
                break;
            case 0x04:
                ctx.colorRGB = w & 0xFFFFFF;
                break;
            case 0x05:
                ctx.handle = w & 31;
                break;
            case 0x06:
                ctx.cell = w & 127;
                break;
            case 0x07:
                bh.format = (w >> 19) & 31;
                bh.stride = (uint16_t)((bh.stride & ~0x3FF) | ((w >> 9) & 0x3FF));
                bh.layoutH = (uint16_t)((bh.layoutH & ~0x1FF) | (w & 0x1FF));
                break;
            case 0x08:
                bh.filter = (w >> 20) & 1;
                bh.wrapX = (w >> 19) & 1;
                bh.wrapY = (w >> 18) & 1;
                bh.width = (uint16_t)((bh.width & ~0x1FF) | ((w >> 9) & 0x1FF));
                bh.height = (uint16_t)((bh.height & ~0x1FF) | (w & 0x1FF));
                break;
            case 0x09:
                ctx.alphaFunc = (w >> 8) & 7;
                ctx.alphaRef = w & 255;
                break;
            case 0x0B:
                ctx.blendSrc = (w >> 3) & 7;
                ctx.blendDst = w & 7;
                break;
            case 0x0D:
                ctx.pointSize = w & 0x1FFF;
                break;
            case 0x0E:
                ctx.lineWidth = w & 0xFFF;
                break;
            case 0x0F:
                ctx.clearA = w & 255;
                break;
            case 0x10:
                ctx.colorA = w & 255;
                break;
            case 0x15:
                ctx.tA = signExtend(w, 17);
                break;
            case 0x16:
                ctx.tB = signExtend(w, 17);
                break;
            case 0x17:
                ctx.tC = signExtend(w, 24);
                break;
            case 0x18:
                ctx.tD = signExtend(w, 17);
                break;
            case 0x19:
                ctx.tE = signExtend(w, 17);
                break;
            case 0x1A:
                ctx.tF = signExtend(w, 24);
                break;
            case 0x1B:
                ctx.scX = (w >> 11) & 0x7FF;
                ctx.scY = w & 0x7FF;
                break;
            case 0x1C:
                ctx.scW = (w >> 12) & 0xFFF;
                ctx.scH = w & 0xFFF;
                break;
            case 0x1D: // CALL
                if (callDepth < 8)
                {
                    callStack[callDepth++] = pc;
                    pc = w & 0xFFFF;
                }
                break;
            case 0x1E:
                pc = w & 0xFFFF;
                break; // JUMP
            case 0x1F:
                prim = w & 15;
                nVerts = 0;
                break;
            case 0x20:
                ctx.colorMask = w & 15;
                break;
            case 0x21:
                prim = 0;
                nVerts = 0;
                break;
            case 0x22:
                if (stackDepth < 16)
                    stack[stackDepth++] = ctx;
                break;
            case 0x23:
                if (stackDepth > 0)
                    ctx = stack[--stackDepth];
                break;
            case 0x24: // RETURN
                if (callDepth > 0)
                    pc = callStack[--callDepth];
                break;
            case 0x26:
                if (t)
                    clear((w >> 2) & 1);
                break;
            case 0x27:
                ctx.vertexFormat = w & 7;
                break;
            case 0x28:
                bh.stride = (uint16_t)((bh.stride & 0x3FF) | (((w >> 2) & 3) << 10));
                bh.layoutH = (uint16_t)((bh.layoutH & 0x1FF) | ((w & 3) << 9));
                break;
            case 0x29:
                bh.width = (uint16_t)((bh.width & 0x1FF) | (((w >> 2) & 3) << 9));
                bh.height = (uint16_t)((bh.height & 0x1FF) | ((w & 3) << 9));
                break;
            case 0x2A:
                ctx.paletteSource = w & 0x3FFFFF;
                break;
            case 0x2B:
                ctx.translateX = signExtend(w, 17);
                break;
            case 0x2C:
                ctx.translateY = signExtend(w, 17);
                break;
            case kOpGradient:
                if (t && pc + 4 <= words)
                    drawGradient(&list[pc - 1]);
                pc += 4;
                break;
            case kOpTextRun:
            {
                int count = w & 0xFFF;
                int extra = 1 + (count + 3) / 4;
                if (t && pc + extra <= words)
                    drawTextRun(&list[pc - 1], count);
                pc += extra;
                break;
            }
            default:
                break; // TAG, stencil, macros: no visible effect here
        }
    }
}

} // namespace eve
