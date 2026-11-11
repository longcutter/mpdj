#include "DisplayDriverImpl.h"

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t microsNow()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000ull + ts.tv_nsec / 1000);
}

DisplayDriverImpl::DisplayDriverImpl()
{
    eveChip.onSwap = &DisplayDriverImpl::onSwap;
    eveChip.onSwapUser = this;
}

void DisplayDriverImpl::init()
{
    fb.open();
    threaded = !getenv("MPDJ_RENDER_INLINE");
    if (threaded)
        renderThread = std::thread([this] { renderLoop(); });
}

DisplayDriverImpl::~DisplayDriverImpl()
{
    if (renderThread.joinable())
    {
        {
            std::lock_guard<std::mutex> lock(renderMutex);
            quitRender = true;
        }
        renderCv.notify_all();
        renderThread.join();
    }
}

void DisplayDriverImpl::renderLoop()
{
    std::unique_lock<std::mutex> lock(renderMutex);
    for (;;)
    {
        renderCv.wait(lock, [this] { return jobReady || quitRender; });
        if (quitRender)
            return;
        jobReady = false;
        rendering = true;
        lock.unlock();
        renderOne();
        lock.lock();
        rendering = false;
    }
}

void DisplayDriverImpl::onSwap(void* self, const uint32_t* dl, int words)
{
    DisplayDriverImpl* d = static_cast<DisplayDriverImpl*>(self);
    d->frameDirty = true;
    if (const char* cap = getenv("MPDJ_EVE_CAPTURE"))
        d->eveChip.saveCapture(cap); // keeps the latest frame for tools/eve_replay
    if (getenv("MPDJ_EVE_TRACE"))
    {
        static int n = 0;
        printf("eve: swap %d, %d words:", ++n, words);
        for (int i = 0; i < words && i < 24; i++)
            printf(" %08x", dl[i]);
        printf("\n");
    }
}

void DisplayDriverImpl::suspend()
{
    suspended = true;
    if (threaded)
    {
        std::unique_lock<std::mutex> lock(renderMutex);
        jobReady = false;
        while (rendering)
        {
            lock.unlock();
            usleep(1000);
            lock.lock();
        }
    }
}

void DisplayDriverImpl::resume()
{
    fb.invalidate(); // the sideload drew over everything
    suspended = false;
    frameDirty = true;
    lastRenderMs = 0;
}

void DisplayDriverImpl::service(uint32_t nowMs)
{
    if (!frameDirty || !powered || suspended || nowMs - lastRenderMs < 16)
        return;
    if (threaded)
    {
        // Hand over the newest frame only when the render thread is free; until
        // then frameDirty stays set and later swaps simply replace this one.
        {
            std::lock_guard<std::mutex> lock(renderMutex);
            if (jobReady || rendering)
                return;
            eveChip.syncTo(renderChip);
            jobReady = true;
        }
        renderCv.notify_one();
    }
    else
    {
        eveChip.syncTo(renderChip);
        renderOne();
    }
    frameDirty = false;
    lastRenderMs = nowMs;
}

// Draws renderChip's front list and shows it. Runs on the render thread.
void DisplayDriverImpl::renderOne()
{
    uint32_t t0 = microsNow();
    fb.beginFrame();
    renderChip.renderer.render(
      renderChip.front, (int)renderChip.frontWords, &fb.target(), renderChip.frontHandles);
    marks.draw(fb.target());
    uint32_t t1 = microsNow();
    fb.present();
    uint32_t t2 = microsNow();
    renderUsTotal += t2 - t0;
    drawUsTotal += t1 - t0;
    presentUsTotal += t2 - t1;
    tilesTotal += (uint32_t)fb.lastTilesWritten();
    diffUsTotal += fb.diffUs;
    writeUsTotal += fb.writeUs;
    flipUsTotal += fb.flipUs;
    frames++;
    reportStats(t2 / 1000);
}

void DisplayDriverImpl::reportStats(uint32_t nowMs)
{
    if (getenv("MPDJ_DISPLAY_STATS") && nowMs - lastReportMs >= 5000)
    {
        uint32_t n = frames ? frames : 1;
        printf("display: %u frames, %u us/frame (draw %u, diff %u, write %u, flip %u), %u tiles/frame\n",
               frames,
               renderUsTotal / n,
               drawUsTotal / n,
               diffUsTotal / n,
               writeUsTotal / n,
               flipUsTotal / n,
               tilesTotal / n);
        frames = renderUsTotal = drawUsTotal = presentUsTotal = tilesTotal = 0;
        diffUsTotal = writeUsTotal = flipUsTotal = 0;
        lastReportMs = nowMs;
    }
}

void DisplayDriverImpl::setPowerState(bool state)
{
    powered = state;
}

void DisplayDriverImpl::setBrightness(int value)
{
    // The Tracker drives REG_PWM_DUTY (0-128). Map it onto the MPC backlight, never fully dark.
    if (getenv("MPDJ_NO_BACKLIGHT"))
        return;
    const char* dir = "/sys/class/backlight/mipi-backlight";
    char path[128];
    snprintf(path, sizeof path, "%s/max_brightness", dir);
    FILE* f = fopen(path, "r");
    if (!f)
        return;
    int maxB = 0;
    if (fscanf(f, "%d", &maxB) != 1)
        maxB = 0;
    fclose(f);
    if (maxB <= 0)
        return;
    if (value > 128)
        value = 128;
    int b = value * maxB / 128;
    if (b < maxB / 10)
        b = maxB / 10;
    snprintf(path, sizeof path, "%s/brightness", dir);
    if ((f = fopen(path, "w")))
    {
        fprintf(f, "%d\n", b);
        fclose(f);
    }
}

void DisplayDriverImpl::getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height)
{
    eveChip.imageProps(addr, width, height);
}

uint32_t DisplayDriverImpl::getDisplayListOffset()
{
    return eveChip.displayListOffset();
}

void DisplayDriverImpl::WriteData32(uint32_t data)
{
    eveChip.write32(data);
}

void DisplayDriverImpl::beginCoProList()
{
}
void DisplayDriverImpl::beginCoProListNoCheck()
{
}

void DisplayDriverImpl::endCoProList()
{
    eveChip.execute();
}

void DisplayDriverImpl::awaitCoProEmpty()
{
    eveChip.execute();
}

uint8_t DisplayDriverImpl::isCoProEmpty()
{
    eveChip.execute();
    return 1;
}

void DisplayDriverImpl::writeDataRAMG(const uint8_t* data, uint32_t size, uint32_t dest)
{
    eveChip.execute();
    eveChip.memWrite(dest, data, size);
}

void DisplayDriverImpl::writeFontRAMG(const uint8_t* data, uint32_t size, uint32_t dest)
{
    // Same layout as the hardware library: the glyph pointer in the metric block
    // (bytes 144-147) is rewritten to point just past the 148-byte block.
    eveChip.execute();
    if (size < 148)
    {
        eveChip.memWrite(dest, data, size);
        return;
    }
    eveChip.memWrite(dest, data, 144);
    uint32_t glyphs = dest + 148;
    uint8_t p[4] = {
        (uint8_t)glyphs, (uint8_t)(glyphs >> 8), (uint8_t)(glyphs >> 16), (uint8_t)(glyphs >> 24)
    };
    eveChip.memWrite(dest + 144, p, 4);
    eveChip.memWrite(dest + 148, data + 148, size - 148);
}

void DisplayDriverImpl::readDataRAMG(uint8_t* data, uint32_t size, uint32_t src)
{
    eveChip.execute();
    eveChip.memRead(src, data, size);
}

void DisplayDriverImpl::readBackDataRAMG(uint8_t* data, uint32_t size, uint32_t src)
{
    readDataRAMG(data, size, src);
}

uint8_t DisplayDriverImpl::sendString(const char* string, uint32_t length)
{
    return (uint8_t)eveChip.writeString(string, length);
}

void DisplayDriverImpl::writeDataToCMD(const uint8_t* data, uint32_t size)
{
    eveChip.writeData(data, size);
}

void DisplayDriverImpl::initStreamDma(uint16_t byteResolution,
                                      uint16_t w,
                                      uint16_t h,
                                      uint8_t* buf,
                                      uint32_t dest)
{
    streamBuf = buf;
    streamSize = (uint32_t)byteResolution * w * h;
    streamDest = dest;
    streamOn = true;
}

bool DisplayDriverImpl::updateStreamDma(bool lastFrame)
{
    if (!streamOn)
        return true;
    if (!lastFrame)
        return false;
    eveChip.execute();
    eveChip.memWrite(streamDest, streamBuf, streamSize);
    return true;
}

void DisplayDriverImpl::stopStreamDma()
{
    streamOn = false;
}
