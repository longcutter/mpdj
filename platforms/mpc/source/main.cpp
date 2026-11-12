// Entry point for the MPC One+ build of Polyend Tracker CE.
#include "PlatformImpl.h"
#include "Tracker.h"
#include "audio/AudioProbe.h"
#include "audio/MpcAudio.h"
#include "input/LedProbe.h"
#include "sideload/EngineHost.h"
#include "sideload/Sideload.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

PlatformImpl platform;
volatile bool g_running = true;
static Tracker* trackerInstance = nullptr;

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

    // Stand-alone checks: run and exit instead of the tracker.
    auto envOn = [](const char* n) { const char* v = getenv(n); return v && *v && *v != '0'; };
    if (envOn("MPDJ_LED_PROBE"))
        return runLedProbe();
    if (envOn("MPDJ_AUDIO_CONTROLS"))
        return AudioProbe::listControls();
    if (envOn("MPDJ_LATENCY_TEST"))
        return AudioProbe::latencyTest();

    trackerInstance = Tracker::getInstance();
    platform.init();
    if (auto startDisplay = trackerInstance->getStartDisplayFunc())
        startDisplay(&platform); // same point in boot as the Tracker+ hardware init
    trackerInstance->init(&platform);
    printf("Tracker initialized, version %s\n", Tracker::getVersion());
    EngineHost::start();
    MpcAudio::start(trackerInstance); // audio callback also clocks the sequencer
    Sideload::init();

    uint32_t frames = 0, lastReport = platform.getTimer()->millis();
    while (g_running)
    {
        platform.update();
        Sideload::poll();
        EngineHost::poll();
        MpcAudio::poll();
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
    MpcAudio::stop();
    EngineHost::stop();
    printf("Bye.\n");
    return 0;
}
