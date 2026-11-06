#include "FbOutput.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

static int envInt(const char* name, int def)
{
    const char* v = getenv(name);
    return v && *v ? atoi(v) : def;
}

FbOutput::~FbOutput()
{
    if (fbMem && fakeFb)
        free(fbMem);
    else if (fbMem)
        munmap(fbMem, fbSize);
    if (fd >= 0)
        close(fd);
}

void FbOutput::open()
{
    const char* dev = getenv("MPDJ_FB");
    if (!dev)
        dev = "/dev/fb0";

    if (strcmp(dev, "fake") == 0)
    {
        // Test mode: memory laid out like the MPC One+ panel, no ioctls.
        xres = 800;
        yres = 1280;
        bpp = 32;
        lineLength = 3200;
        rOff = 16;
        gOff = 8;
        bOff = 0;
        pages = 3;
        fbSize = (size_t)lineLength * yres * pages;
        fbMem = (uint8_t*)calloc(1, fbSize);
        fakeFb = true;
        printf("fb: fake %dx%d %dbpp, %d page(s)\n", xres, yres, bpp, pages);
    }
    else if (strcmp(dev, "none") != 0)
    {
        fd = ::open(dev, O_RDWR);
        fb_var_screeninfo var;
        fb_fix_screeninfo fix;
        if (fd >= 0 && ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0 &&
            ioctl(fd, FBIOGET_FSCREENINFO, &fix) == 0 &&
            (var.bits_per_pixel == 32 || var.bits_per_pixel == 16))
        {
            xres = (int)var.xres;
            yres = (int)var.yres;
            bpp = (int)var.bits_per_pixel;
            lineLength = (int)fix.line_length;
            rOff = (int)var.red.offset;
            gOff = (int)var.green.offset;
            bOff = (int)var.blue.offset;
            pages = (int)(var.yres_virtual / var.yres);
            if (pages < 1)
                pages = 1;
            if (pages > 3)
                pages = 3;
            fbSize = fix.smem_len;
            fbMem = (uint8_t*)mmap(nullptr, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (fbMem == MAP_FAILED)
            {
                fbMem = nullptr;
                fprintf(stderr, "fb: mmap failed\n");
            }
            if ((size_t)lineLength * yres * pages > fbSize)
                pages = 1;
            printf("fb: %s %dx%d %dbpp, %d page(s)\n", dev, xres, yres, bpp, pages);
        }
        else
        {
            fprintf(stderr, "fb: %s not usable, running without a screen\n", dev);
            if (fd >= 0)
                close(fd);
            fd = -1;
        }
    }

    if (fbMem)
    {
        rotate = envInt("MPDJ_ROTATE", xres < yres ? 270 : 0);
        bool sideways = rotate == 90 || rotate == 270;
        logicalW = sideways ? yres : xres;
        logicalH = sideways ? xres : yres;
    }

    dumpDir = getenv("MPDJ_DUMP");
    dumpEvery = envInt("MPDJ_DUMP_EVERY", 1);
    dumpMax = envInt("MPDJ_DUMP_MAX", 200);

    buffer.assign((size_t)logicalW * logicalH, 0xFF000000u);
    // prev starts different from any real frame so the first frame fills every tile.
    prev.assign(buffer.size(), 0x00123456u);
    tilesX = (logicalW + kTile - 1) / kTile;
    tilesY = (logicalH + kTile - 1) / kTile;
    tileMask.assign((size_t)tilesX * tilesY, 0);
    fullPresent = envInt("MPDJ_FULL_PRESENT", 0) != 0;
    verify = envInt("MPDJ_FB_VERIFY", 0) != 0;
    if (bpp == 32)
        alphaBits = 0xFFFFFFFFu & ~((255u << rOff) | (255u << gOff) | (255u << bOff));
    directPack = bpp == 32 && rOff == 16 && gOff == 8 && bOff == 0;
    tgt.px = buffer.data();
    tgt.w = tgt.stride = logicalW;
    tgt.h = logicalH;

    float fit = fminf((float)logicalW / eve::kNativeW, (float)logicalH / eve::kNativeH);
    const char* s = getenv("MPDJ_SCALE");
    tgt.scale = s && *s ? (float)atof(s) : fit;
    tgt.offX = (float)envInt("MPDJ_OFFSET_X", (int)lroundf((logicalW - eve::kNativeW * tgt.scale) / 2));
    tgt.offY = (float)envInt("MPDJ_OFFSET_Y", (int)lroundf((logicalH - eve::kNativeH * tgt.scale) / 2));
    tgt.smoothBitmaps = envInt("MPDJ_SMOOTH", 1) != 0;
    printf("display: %dx%d, UI scale %.3f at (%.0f, %.0f), rotate %d\n",
           logicalW,
           logicalH,
           tgt.scale,
           tgt.offX,
           tgt.offY,
           rotate);
}

void FbOutput::panelToScreen(int px, int py, int& x, int& y) const
{
    switch (rotate)
    {
        case 90:
            x = py;
            y = logicalH - 1 - px;
            break;
        case 180:
            x = logicalW - 1 - px;
            y = logicalH - 1 - py;
            break;
        case 270:
            x = logicalW - 1 - py;
            y = px;
            break;
        default:
            x = px;
            y = py;
            break;
    }
}

void FbOutput::toNative(float x, float y, float& nx, float& ny) const
{
    nx = (x - tgt.offX) / tgt.scale;
    ny = (y - tgt.offY) / tgt.scale;
}

void FbOutput::beginFrame()
{
    // The renderer only touches the UI area; keep the margins black.
    int x0 = (int)tgt.offX, y0 = (int)tgt.offY;
    int x1 = (int)lroundf(tgt.offX + eve::kNativeW * tgt.scale);
    int y1 = (int)lroundf(tgt.offY + eve::kNativeH * tgt.scale);
    for (int y = 0; y < logicalH; y++)
    {
        uint32_t* row = &buffer[(size_t)y * logicalW];
        if (y < y0 || y >= y1)
        {
            for (int x = 0; x < logicalW; x++)
                row[x] = 0xFF000000u;
            continue;
        }
        for (int x = 0; x < x0 && x < logicalW; x++)
            row[x] = 0xFF000000u;
        for (int x = x1 < 0 ? 0 : x1; x < logicalW; x++)
            row[x] = 0xFF000000u;
    }
}

uint32_t FbOutput::pack(uint32_t c) const
{
    uint32_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (bpp == 16)
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    return (r << rOff) | (g << gOff) | (b << bOff) | alphaBits;
}

// Copy logical rect [lx0,lx1) x [ly0,ly1) of the landscape buffer to the panel.
void FbOutput::writeTile(uint8_t* dst, int lx0, int ly0, int lx1, int ly1)
{
    const uint32_t* src = buffer.data();
    const int W = logicalW;
    if (rotate == 270 && bpp == 32 && directPack)
    {
        // Logical (x, y) -> panel (px, py) = (y, W - 1 - x): each logical column
        // becomes a panel row, written left to right in one sequential run.
        for (int lx = lx0; lx < lx1; lx++)
        {
            uint32_t* out = (uint32_t*)(dst + (size_t)(W - 1 - lx) * lineLength) + ly0;
            const uint32_t* in = src + (size_t)ly0 * W + lx;
            int n = ly1 - ly0;
            for (int i = 0; i < n; i++)
            {
                out[i] = *in | alphaBits;
                in += W;
            }
        }
        return;
    }
    // Generic path: any rotation and pixel format.
    for (int ly = ly0; ly < ly1; ly++)
        for (int lx = lx0; lx < lx1; lx++)
        {
            int px, py;
            switch (rotate)
            {
                case 90: px = logicalH - 1 - ly; py = lx; break;
                case 180: px = logicalW - 1 - lx; py = logicalH - 1 - ly; break;
                case 270: px = ly; py = logicalW - 1 - lx; break;
                default: px = lx; py = ly; break;
            }
            uint32_t v = pack(src[(size_t)ly * W + lx]);
            uint8_t* row = dst + (size_t)py * lineLength;
            if (bpp == 32)
                ((uint32_t*)row)[px] = v;
            else
                ((uint16_t*)row)[px] = (uint16_t)v;
        }
}

// Compare the new frame with the last one, tile by tile. A changed tile is
// marked stale on every page; each page catches up when it is next drawn.
void FbOutput::diffTiles()
{
    const int W = logicalW, H = logicalH;
    const uint8_t allPages = (uint8_t)((1u << pages) - 1);
    tilesChanged = 0;
    for (int ty = 0; ty < tilesY; ty++)
    {
        int y0 = ty * kTile, y1 = y0 + kTile < H ? y0 + kTile : H;
        for (int tx = 0; tx < tilesX; tx++)
        {
            int x0 = tx * kTile, x1 = x0 + kTile < W ? x0 + kTile : W;
            size_t bytes = (size_t)(x1 - x0) * 4;
            bool changed = false;
            for (int y = y0; y < y1; y++)
            {
                size_t o = (size_t)y * W + x0;
                // Rows above the first difference are already identical.
                if (changed || memcmp(&prev[o], &buffer[o], bytes) != 0)
                {
                    changed = true;
                    memcpy(&prev[o], &buffer[o], bytes);
                }
            }
            if (changed)
            {
                tileMask[ty * tilesX + tx] = allPages;
                tilesChanged++;
            }
        }
    }
}

void FbOutput::writeRotated(uint8_t* dst, int pageIdx)
{
    const uint8_t bit = (uint8_t)(1u << pageIdx);
    tilesWritten = 0;
    for (int ty = 0; ty < tilesY; ty++)
        for (int tx = 0; tx < tilesX; tx++)
        {
            uint8_t& m = tileMask[ty * tilesX + tx];
            if (!fullPresent && !(m & bit))
                continue;
            m &= (uint8_t)~bit;
            int x0 = tx * kTile, y0 = ty * kTile;
            int x1 = x0 + kTile < logicalW ? x0 + kTile : logicalW;
            int y1 = y0 + kTile < logicalH ? y0 + kTile : logicalH;
            writeTile(dst, x0, y0, x1, y1);
            tilesWritten++;
        }
}

void FbOutput::present()
{
    frameNo++;
    if (fbMem)
    {
        int next = pages > 1 ? (page + 1) % pages : 0;
        if (!fullPresent)
            diffTiles();
        writeRotated(fbMem + (size_t)next * yres * lineLength, next);
        if (fakeFb && tilesWritten > 0)
            page = next;
        else if (pages > 1 && tilesWritten > 0)
        {
            fb_var_screeninfo var;
            if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0)
            {
                var.yoffset = (uint32_t)(next * yres);
                if (ioctl(fd, FBIOPAN_DISPLAY, &var) == 0)
                    page = next;
                else
                    pages = 1; // panning unsupported: draw to the visible page from now on
            }
        }
    }
    if (fbMem && verify)
        verifyPage();
    if (dumpDir && dumped < dumpMax && (frameNo % dumpEvery) == 0)
        dumpFrame();
}

// MPDJ_FB_VERIFY=1: check the visible page matches the frame exactly.
void FbOutput::verifyPage()
{
    const uint8_t* base = fbMem + (size_t)page * yres * lineLength;
    int bad = 0;
    for (int py = 0; py < yres; py++)
        for (int px = 0; px < xres; px++)
        {
            int x, y;
            panelToScreen(px, py, x, y);
            uint32_t want = pack(buffer[(size_t)y * logicalW + x]);
            uint32_t got = bpp == 32 ? ((const uint32_t*)(base + (size_t)py * lineLength))[px]
                                     : ((const uint16_t*)(base + (size_t)py * lineLength))[px];
            bad += got != want;
        }
    printf("fb verify: frame %d page %d tiles %d/%d, %d wrong pixels\n",
           frameNo, page, tilesWritten, tilesChanged, bad);
}

void FbOutput::dumpFrame()
{
    char path[512];
    snprintf(path, sizeof path, "%s/frame_%05d.ppm", dumpDir, frameNo);
    FILE* f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", logicalW, logicalH);
    std::vector<uint8_t> row((size_t)logicalW * 3);
    for (int y = 0; y < logicalH; y++)
    {
        for (int x = 0; x < logicalW; x++)
        {
            uint32_t c = buffer[(size_t)y * logicalW + x];
            row[x * 3] = (uint8_t)(c >> 16);
            row[x * 3 + 1] = (uint8_t)(c >> 8);
            row[x * 3 + 2] = (uint8_t)c;
        }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f);
    dumped++;
}
