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
    if (fbMem)
        munmap(fbMem, fbSize);
    if (fd >= 0)
        close(fd);
}

void FbOutput::open()
{
    const char* dev = getenv("MPDJ_FB");
    if (!dev)
        dev = "/dev/fb0";

    if (strcmp(dev, "none") != 0)
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

void FbOutput::writeRotated(uint8_t* dst)
{
    auto pack = [&](uint32_t c) -> uint32_t
    {
        uint32_t r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
        if (bpp == 16)
            return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        return (r << rOff) | (g << gOff) | (b << bOff) |
               (bpp == 32 ? 0xFF000000u & ~((255u << rOff) | (255u << gOff) | (255u << bOff)) : 0);
    };

    // Walk the panel in 32x32 tiles so reads from the landscape buffer stay cache-friendly.
    const int T = 32;
    for (int ty = 0; ty < yres; ty += T)
        for (int tx = 0; tx < xres; tx += T)
            for (int py = ty; py < ty + T && py < yres; py++)
            {
                uint8_t* row = dst + (size_t)py * lineLength;
                for (int px = tx; px < tx + T && px < xres; px++)
                {
                    int x, y;
                    panelToScreen(px, py, x, y);
                    uint32_t v = pack(buffer[(size_t)y * logicalW + x]);
                    if (bpp == 32)
                        ((uint32_t*)row)[px] = v;
                    else
                        ((uint16_t*)row)[px] = (uint16_t)v;
                }
            }
}

void FbOutput::present()
{
    frameNo++;
    if (fbMem)
    {
        int next = pages > 1 ? (page + 1) % pages : 0;
        writeRotated(fbMem + (size_t)next * yres * lineLength);
        if (pages > 1)
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
    if (dumpDir && dumped < dumpMax && (frameNo % dumpEvery) == 0)
        dumpFrame();
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
