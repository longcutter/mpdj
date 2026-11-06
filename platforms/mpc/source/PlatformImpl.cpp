#include "PlatformImpl.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------- timer
static uint64_t nowUs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}

static const uint64_t startUs = nowUs();

uint32_t TimerImpl::millis() { return (uint32_t)((nowUs() - startUs) / 1000); }
uint32_t TimerImpl::micros() { return (uint32_t)(nowUs() - startUs); }
uint32_t TimerImpl::clockCycles() { return micros(); }
void TimerImpl::delay(int ms)
{
    if (ms > 0)
        usleep((useconds_t)ms * 1000);
}

// ---------------------------------------------------------------- switch groups
bool SwitchGroupImpl::isButtonPressed(int idx)
{
    return idx >= 0 && idx < count && pressed[idx];
}

void SwitchGroupImpl::setActionCallback(ActionType action, void (*cb)(uint8_t))
{
    switch (action)
    {
        case kPress: onPress = cb; break;
        case kRelease: onRelease = cb; break;
        case kHold: onHold = cb; break;
    }
}

void SwitchGroupImpl::press(int idx)
{
    if (idx < 0 || idx >= count || pressed[idx])
        return;
    pressed[idx] = true;
    holdSent[idx] = false;
    pressTime[idx] = platform.getTimer()->millis();
    if (onPress)
        onPress((uint8_t)idx);
}

void SwitchGroupImpl::release(int idx)
{
    if (idx < 0 || idx >= count || !pressed[idx])
        return;
    pressed[idx] = false;
    if (onRelease)
        onRelease((uint8_t)idx);
}

void SwitchGroupImpl::update()
{
    uint32_t now = platform.getTimer()->millis();
    for (int i = 0; i < count; i++)
        if (pressed[i] && !holdSent[i] && now - pressTime[i] >= (uint32_t)holdTimeMs)
        {
            holdSent[i] = true;
            if (onHold)
                onHold((uint8_t)i);
        }
}

// ---------------------------------------------------------------- encoder
void onPotChange(uint8_t n, int16_t value); // core/source/mtActions.cpp

void EncoderImpl::turn(int steps)
{
    if (steps)
        onPotChange(0, (int16_t)steps);
}

// ---------------------------------------------------------------- config
void ConfigManagerImpl::init(const char* p)
{
    path = p;
    memset(mem, 0xFF, sizeof mem); // erased-flash state
    if (FILE* f = fopen(path, "rb"))
    {
        size_t n = fread(mem, 1, sizeof mem, f);
        (void)n;
        fclose(f);
    }
}

bool ConfigManagerImpl::readData(int address, void* data, int size)
{
    if (address < 0 || size < 0 || address + size > kSize)
        return false;
    memcpy(data, mem + address, size);
    return true;
}

void ConfigManagerImpl::writeData(int address, void* data, int size)
{
    if (address < 0 || size < 0 || address + size > kSize)
        return;
    memcpy(mem + address, data, size);
    if (!path)
        return;
    if (FILE* f = fopen(path, "wb"))
    {
        fwrite(mem, 1, sizeof mem, f);
        fclose(f);
    }
}

// ---------------------------------------------------------------- crc32
uint32_t CrcCalculatorImpl::calculate32bitCrc(const uint8_t* data, const uint16_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

// ---------------------------------------------------------------- platform
static constexpr int kButtons = 44; // interfaceButtonsCount
static constexpr int kPads = 48;    // interfacePadsCount

PlatformImpl::PlatformImpl() : buttons(kButtons), pads(kPads)
{
    systemInfo.buildId = "mpc-one-plus";
}

void PlatformImpl::init()
{
    config.init("tracker-config.bin");
    fileSystem.init();
    display.init();
}

void PlatformImpl::update()
{
    buttons.update();
    pads.update();
    display.service(timer.millis());
}

void PlatformImpl::setPowerMode(bool mode)
{
    if (!mode)
        g_running = false; // "power off" quits back to the shell
}

void PlatformImpl::resetMCU()
{
    g_running = false;
}
