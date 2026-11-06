// Entry point for the MPC One+ build of Polyend Tracker CE.
#include "PlatformImpl.h"
#include "Tracker.h"

#include <mutex>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

PlatformImpl platform;
volatile bool g_running = true;
static Tracker* trackerInstance = nullptr;

// ---------------------------------------------------------------- audio glue
// On the Tracker these mask the audio interrupt. Here the audio thread holds
// this lock while it renders, so masking = taking the lock.
static std::recursive_mutex audioLock;
static bool audioOutputEnabled = true;

void disableAudioInterrupt() { audioLock.lock(); }
void enableAudioInterrupt() { audioLock.unlock(); }

void muteAudioOutByKey() { audioOutputEnabled = false; }
void unmuteAudioOutByKey() { audioOutputEnabled = true; }
uint8_t isAudioOutMuted() { return !audioOutputEnabled; }

// CMSIS arm_sin_q15 replacement: input 0..32767 maps to 0..2*pi.
int16_t sin_q15(int16_t value)
{
    float x = (float)(uint16_t)value / 32768.0f * 6.283185307f;
    float s = __builtin_sinf(x) * 32767.0f;
    return (int16_t)s;
}

// ---------------------------------------------------------------- main
static void onSignal(int) { g_running = false; }

int main()
{
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    printf("Tracker CE for MPC One+ starting\n");
    setvbuf(stdout, nullptr, _IOLBF, 0); // line-buffered logs over ssh

    trackerInstance = Tracker::getInstance();
    platform.init();
    if (auto startDisplay = trackerInstance->getStartDisplayFunc())
        startDisplay(&platform); // same point in boot as the Tracker+ hardware init
    trackerInstance->init(&platform);
    printf("Tracker initialized, version %s\n", Tracker::getVersion());

    uint32_t frames = 0, lastReport = platform.getTimer()->millis();
    while (g_running)
    {
        platform.update();
        trackerInstance->update();
        frames++;

        uint32_t now = platform.getTimer()->millis();
        if (now - lastReport >= 5000)
        {
            printf("main loop: %u updates/s\n", frames / 5);
            frames = 0;
            lastReport = now;
        }
        usleep(500);
    }
    printf("Bye.\n");
    return 0;
}
