// FT812 display driver for the MPC One+: the tracker's display commands go to a
// software EVE chip (eve/eve.h) whose frames are shown on the MPC's panel.
#pragma once

#include "DisplayDriver.h"
#include "eve/FbOutput.h"
#include "eve/eve.h"

#include <stdint.h>

class DisplayDriverImpl : public DisplayDriver
{
  public:
    DisplayDriverImpl();
    void init();
    void service(uint32_t nowMs); // renders the latest swapped frame, at most ~60 per second

    FbOutput& output() { return fb; }
    eve::Chip& chip() { return eveChip; }

    void setPowerState(bool state) override;
    void setBrightness(int value) override;
    void setRotation(int) override {}

    void getImgProps(uint32_t& addr, uint32_t& width, uint32_t& height) override;
    uint32_t getDisplayListOffset() override;
    void IncCMDOffset(uint16_t) override {}
    void WriteData32(uint32_t data) override;

    void beginCoProList() override;
    void beginCoProListNoCheck() override;
    void endCoProList() override;
    void awaitCoProEmpty() override;
    uint8_t isCoProEmpty() override;
    void writeDataRAMG(const uint8_t* data, uint32_t size, uint32_t dest) override;
    void writeFontRAMG(const uint8_t* data, uint32_t size, uint32_t dest) override;
    void readDataRAMG(uint8_t* data, uint32_t size, uint32_t src) override;
    void readBackDataRAMG(uint8_t* data, uint32_t size, uint32_t src) override;
    uint8_t sendString(const char* string, uint32_t length) override;
    void writeDataToCMD(const uint8_t* data, uint32_t size) override;

    void initStreamDma(uint16_t byteResolution,
                       uint16_t w,
                       uint16_t h,
                       uint8_t* buf,
                       uint32_t dest) override;
    bool updateStreamDma(bool lastFrame) override;
    void stopStreamDma() override;

  private:
    static void onSwap(void* self, const uint32_t* dl, int words);

    eve::Chip eveChip;
    FbOutput fb;
    bool frameDirty = false;
    bool powered = true;
    uint32_t lastRenderMs = 0;
    uint32_t frames = 0, renderUsTotal = 0, lastReportMs = 0;

    uint8_t* streamBuf = nullptr;
    uint32_t streamSize = 0, streamDest = 0;
    bool streamOn = false;
};
