#include "MpcAudio.h"

#include "AlsaPcm.h"
#include "Tracker.h"

#include <atomic>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------------------------------------------------------------- audio "interrupt" lock
// On the Tracker the audio engine runs in an interrupt, and the core masks that
// interrupt (DISABLE_GLOBAL_INTERRUPTS, core/config/config.h) around shared
// state. Here the audio thread holds this lock for a whole block, so masking =
// taking the lock, and a block is atomic with respect to those sections, as an
// interrupt would be. Recursive (sections nest), priority-inheriting (the main
// thread must not stall the real-time audio thread behind other work).
namespace
{
pthread_mutex_t audioLock;
thread_local int lockDepth = 0;

struct LockInit
{
    LockInit()
    {
        pthread_mutexattr_t a;
        pthread_mutexattr_init(&a);
        pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutexattr_setprotocol(&a, PTHREAD_PRIO_INHERIT);
        pthread_mutex_init(&audioLock, &a);
        pthread_mutexattr_destroy(&a);
    }
} lockInit;
} // namespace

void disableAudioInterrupt()
{
    pthread_mutex_lock(&audioLock);
    lockDepth++;
}

void enableAudioInterrupt()
{
    if (lockDepth == 0)
        return; // CPSIE without a matching CPSID is harmless on the MCU; keep it so
    lockDepth--;
    pthread_mutex_unlock(&audioLock);
}

// ---------------------------------------------------------------- mute / volume
namespace
{
std::atomic<bool> keyMuted{ false };
std::atomic<bool> headphoneMuted{ false };
std::atomic<float> volumeGain{ 1.0f };
} // namespace

void muteAudioOutByKey() { keyMuted = true; }
void unmuteAudioOutByKey() { keyMuted = false; }
uint8_t isAudioOutMuted() { return keyMuted; }

void MpcAudio::setVolume(float v)
{
    // SGTL5000 headphone volume: 0.01..1 -> -51.5..+12 dB in 0.5 dB steps,
    // 0 = mute; the Tracker's default (81) lands on 0 dB. Digital gain can't go
    // above 0 dB without clipping, so cap there: the MPC's own volume knob
    // does the rest.
    if (v <= 0.0f)
    {
        volumeGain = 0.0f;
        return;
    }
    if (v > 1.0f)
        v = 1.0f;
    float t = (v - 0.01f) / 0.99f;
    if (t < 0.0f)
        t = 0.0f;
    float db = roundf((-51.5f + t * 63.5f) * 2.0f) / 2.0f;
    float g = powf(10.0f, db / 20.0f);
    volumeGain = g > 1.0f ? 1.0f : g;
}

void MpcAudio::setHeadphoneMuted(bool muted)
{
    headphoneMuted = muted;
}

// ---------------------------------------------------------------- audio thread
namespace
{
constexpr unsigned kRate = 44100;
constexpr unsigned kBlock = 128; // AUDIO_BLOCK_SAMPLES

Tracker* tracker = nullptr;
pthread_t thread;
bool threadStarted = false;
std::atomic<bool> running{ false };

AlsaPcm out, in;
FILE* wav = nullptr;
uint32_t wavBytes = 0;
bool stats = false;

int16_t blockL[kBlock], blockR[kBlock];
int16_t inBuf[kBlock * 2];
int16_t outBuf[kBlock * 2];
float currentGain = 1.0f;

int envInt(const char* name, int def)
{
    const char* v = getenv(name);
    return v && *v ? atoi(v) : def;
}

uint64_t nowUs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}

// Card whose /proc/asound id is "ACVA" (the One+ internal codec), else 1.
int findCodecCard()
{
    for (int c = 0; c < 8; c++)
    {
        char path[64], id[32] = "";
        snprintf(path, sizeof path, "/proc/asound/card%d/id", c);
        if (FILE* f = fopen(path, "r"))
        {
            if (fgets(id, sizeof id, f))
                id[strcspn(id, "\n")] = 0;
            fclose(f);
            if (!strcmp(id, "ACVA"))
                return c;
        }
    }
    return 1;
}

void wavHeader(FILE* f, uint32_t dataBytes)
{
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(kRate);
    u32(kRate * 4);
    u16(4);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(dataBytes);
}

// One 128-frame block: input -> engine (sequencer tick, voices, mix) -> outBuf.
void renderBlock()
{
    disableAudioInterrupt();
    tracker->updateInputAudioBuffers(in.isOpen() ? inBuf : nullptr, nullptr, kBlock);
    tracker->updateAudioBuffers(blockL, blockR, kBlock);
    enableAudioInterrupt();

    // Gain ramps across the block so volume changes and mutes don't click.
    float target = (keyMuted || headphoneMuted) ? 0.0f : volumeGain.load();
    float g = currentGain, step = (target - g) / kBlock;
    for (unsigned i = 0; i < kBlock; i++)
    {
        g += step;
        if (g == 1.0f)
        {
            outBuf[2 * i] = blockL[i];
            outBuf[2 * i + 1] = blockR[i];
        }
        else
        {
            outBuf[2 * i] = (int16_t)lrintf(blockL[i] * g);
            outBuf[2 * i + 1] = (int16_t)lrintf(blockR[i] * g);
        }
    }
    currentGain = target;
}

void* audioThread(void*)
{
    uint64_t blocks = 0, xruns = 0, renderSum = 0, renderMax = 0, statBlocks = 0;
    uint64_t lastReport = nowUs();
    timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    const long blockNs = (long)(1e9 * kBlock / kRate);

    static const int16_t silence[kBlock * 2] = {};
    if (out.isOpen())
    {
        // Fill the buffer before the first render: the first write starts the
        // device. With capture, leave one period free, since the capture read
        // paces the loop.
        unsigned fill = out.bufferFrames - (in.isOpen() ? kBlock : 0);
        for (unsigned f = 0; f + kBlock <= fill; f += kBlock)
            out.write(silence, kBlock);
        if (in.isOpen())
            in.start();
    }

    while (running)
    {
        if (in.isOpen())
        {
            int r = in.read(inBuf, kBlock);
            if (r < 0)
            {
                xruns++;
                memset(inBuf, 0, sizeof inBuf);
                in.prepare();
                in.start();
            }
        }

        uint64_t t0 = nowUs();
        renderBlock();
        uint64_t dt = nowUs() - t0;
        renderSum += dt;
        if (dt > renderMax)
            renderMax = dt;
        blocks++;
        statBlocks++;

        if (out.isOpen())
        {
            int w = out.write(outBuf, kBlock);
            if (w == -EPIPE)
            {
                xruns++;
                out.prepare();
                for (unsigned f = 0; f + kBlock <= out.bufferFrames - kBlock; f += kBlock)
                    out.write(silence, kBlock);
                out.write(outBuf, kBlock);
            }
            else if (w < 0)
            {
                fprintf(stderr, "audio: write failed: %s\n", strerror(-w));
                break;
            }
        }
        else
        {
            if (wav)
            {
                fwrite(outBuf, sizeof outBuf, 1, wav);
                wavBytes += sizeof outBuf;
            }
            // No device clock: pace the blocks in real time.
            next.tv_nsec += blockNs;
            if (next.tv_nsec >= 1000000000)
            {
                next.tv_nsec -= 1000000000;
                next.tv_sec++;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);
        }

        uint64_t now = nowUs();
        if (now - lastReport >= 5000000)
        {
            if (stats || xruns)
                printf("audio: %llu blocks, render avg %llu us max %llu us (budget %u us), "
                       "xruns %llu\n",
                       (unsigned long long)statBlocks,
                       (unsigned long long)(statBlocks ? renderSum / statBlocks : 0),
                       (unsigned long long)renderMax, kBlock * 1000000u / kRate,
                       (unsigned long long)xruns);
            renderSum = renderMax = statBlocks = 0;
            xruns = 0;
            lastReport = now;
        }
    }
    return nullptr;
}
} // namespace

void MpcAudio::start(Tracker* t)
{
    tracker = t;
    stats = envInt("MPDJ_AUDIO_STATS", 0) != 0;

    const char* mode = getenv("MPDJ_AUDIO");
    const char* wavPath = getenv("MPDJ_AUDIO_WAV");
    if (wavPath && *wavPath)
    {
        wav = fopen(wavPath, "wb");
        if (wav)
            wavHeader(wav, 0);
        printf("audio: writing %s (no device)\n", wavPath);
    }
    else if (mode && !strcmp(mode, "none"))
    {
        printf("audio: none (timer clock)\n");
    }
    else
    {
        int card = envInt("MPDJ_AUDIO_CARD", -1);
        if (card < 0)
            card = findCodecCard();
        int device = envInt("MPDJ_AUDIO_DEVICE", 0);
        int periods = envInt("MPDJ_AUDIO_PERIODS", 3);
        if (periods < 2)
            periods = 2;
        char err[200];
        if (out.open(card, device, false, kRate, 2, kBlock, periods, err, sizeof err))
            printf("audio: hw:%d,%d %u Hz, %u x %u frames (%.1f ms)\n", card, device, out.rate,
                   out.bufferFrames / out.periodFrames, out.periodFrames,
                   out.bufferFrames * 1000.0 / out.rate);
        else
            printf("audio: %s; running without sound\n", err);

        if (out.isOpen() && envInt("MPDJ_AUDIO_IN", 0))
        {
            if (in.open(card, device, true, kRate, 2, kBlock, periods + 1, err, sizeof err))
                printf("audio: input hw:%d,%d on\n", card, device);
            else
                printf("audio: input: %s\n", err);
        }
    }

    running = true;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    sched_param sp = {};
    sp.sched_priority = envInt("MPDJ_AUDIO_PRIO", 70);
    pthread_attr_setschedparam(&attr, &sp);
    int e = pthread_create(&thread, &attr, audioThread, nullptr);
    pthread_attr_destroy(&attr);
    if (e != 0)
    {
        // Not root, or QEMU without real-time scheduling: run at normal priority.
        printf("audio: SCHED_FIFO unavailable (%s), normal priority\n", strerror(e));
        e = pthread_create(&thread, nullptr, audioThread, nullptr);
    }
    threadStarted = e == 0;
    if (!threadStarted)
        printf("audio: thread failed: %s\n", strerror(e));
}

void MpcAudio::stop()
{
    if (!threadStarted)
        return;
    running = false;
    pthread_join(thread, nullptr);
    threadStarted = false;
    out.drop();
    out.close();
    in.drop();
    in.close();
    if (wav)
    {
        fseek(wav, 0, SEEK_SET);
        wavHeader(wav, wavBytes);
        fclose(wav);
        wav = nullptr;
    }
}
