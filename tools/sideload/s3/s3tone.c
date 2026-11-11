/* MPDJ sideload test S3 (option B): a tone engine that stands in for an engine
 * sideload. It renders into the ring MPDJ gives it (MPDJ_RING, see
 * platforms/mpc/source/sideload/mpdj_ring.h) and MPDJ mixes it into its master.
 *
 * Sound: a soft sine blip on every beat (TONE_BPM, default 120; the first beat
 * of each bar an octave up), or a steady tone with TONE_BPM=0. Extra voices
 * (TONE_VOICES) are rendered and thrown away, to load the CPU like a bigger
 * engine would.
 *
 * Every 5 s it prints what the engine sees:
 *   wake    time from MPDJ's wake to this process running (us, avg / max)
 *   render  time to render what was asked (us, avg / max)
 *   queued  frames left in the ring when it woke (min: how close it came to an underrun)
 *   underruns  blocks MPDJ found short (counted by MPDJ)
 *
 * Env: MPDJ_RING (set by MPDJ), TONE_HZ (440), TONE_DB (-18, level of the tone),
 * TONE_BPM (120; 0 = steady tone), TONE_VOICES (0 extra voices), TONE_PRIO
 * (SCHED_FIFO priority, default 60, under MPDJ's audio thread at 70; 0 = normal
 * scheduling), TONE_SECONDS (exit after N s; default: run until MPDJ stops it).
 *
 * Build (static, runs inside or outside the Debian root):
 *   zig cc -target arm-linux-musleabihf -mcpu=cortex_a17 -O2 -static \
 *     -I../../../platforms/mpc/source/sideload -o s3tone s3tone.c -lm
 */
#include "mpdj_ring.h"

#include <math.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

static volatile sig_atomic_t stopRequested = 0;
static void onSignal(int s) { (void)s; stopRequested = 1; }

static double envNum(const char* n, double def)
{
    const char* v = getenv(n);
    return v && *v ? atof(v) : def;
}

int main(void)
{
    setvbuf(stdout, 0, _IOLBF, 0);
    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);

    const char* path = getenv("MPDJ_RING");
    mpdj_ring* r = mpdj_ring_open(path);
    if (!r)
    {
        printf("s3tone: can't open the ring (MPDJ_RING=%s)\n", path ? path : "(unset)");
        return 1;
    }

    double hz = envNum("TONE_HZ", 440), db = envNum("TONE_DB", -18), bpm = envNum("TONE_BPM", 120);
    int voices = (int)envNum("TONE_VOICES", 0), prio = (int)envNum("TONE_PRIO", 60);
    double seconds = envNum("TONE_SECONDS", 0);
    if (voices < 0)
        voices = 0;
    if (voices > 512)
        voices = 512;

    mlockall(MCL_CURRENT | MCL_FUTURE);
    if (prio > 0)
    {
        struct sched_param sp = { 0 };
        sp.sched_priority = prio;
        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
            printf("s3tone: SCHED_FIFO %d not available, normal priority\n", prio);
    }

    const double rate = r->rate, twoPi = 6.283185307179586;
    const float level = (float)pow(10.0, db / 20.0);
    printf("s3tone: ring %s, %u Hz, keeps %u frames queued; %.0f Hz at %.0f dB, %s, %d extra voices, prio %d\n",
           path, r->rate, r->target, hz, db, bpm > 0 ? "blips" : "steady", voices, prio);

    static float buf[MPDJ_RING_MAX_FRAMES * 2];
    double phase = 0;
    double* vphase = (double*)calloc((size_t)voices + 1, sizeof(double));
    uint64_t frame = 0; /* frames rendered since start (the beat clock) */
    const uint64_t beatFrames = bpm > 0 ? (uint64_t)(rate * 60.0 / bpm) : 0;
    const uint64_t blipFrames = (uint64_t)(rate * 0.08);
    const uint64_t stopAt = seconds > 0 ? (uint64_t)(seconds * rate) : 0;

    uint32_t lastWake = mpdj_ring_load(&r->wake);
    uint64_t wakeSum = 0, wakeMax = 0, wakes = 0, renderSum = 0, renderMax = 0, renders = 0;
    uint32_t queuedMin = UINT32_MAX, reportUs = mpdj_ring_now_us(), underrunsAtReport = r->underruns;

    while (!stopRequested)
    {
        int n = mpdj_ring_wait(r, 1000);
        if (n < 0)
        {
            printf("s3tone: MPDJ stopped reading, exiting\n");
            break;
        }
        uint32_t t0 = mpdj_ring_now_us();
        uint32_t w = mpdj_ring_load(&r->wake);
        if (n > 0 && w != lastWake)
        {
            uint32_t lat = t0 - mpdj_ring_load(&r->wakeUs);
            if (lat < 1000000) /* ignore the very first, stale timestamp */
            {
                wakeSum += lat;
                wakes++;
                if (lat > wakeMax)
                    wakeMax = lat;
            }
            uint32_t q = r->target - (uint32_t)n;
            if (q < queuedMin)
                queuedMin = q;
        }
        lastWake = w;

        for (int i = 0; i < n; i++, frame++)
        {
            float s;
            if (beatFrames)
            {
                uint64_t inBeat = frame % beatFrames;
                int downbeat = (frame / beatFrames) % 4 == 0;
                double f = downbeat ? hz * 2 : hz;
                double env = inBeat < blipFrames ? 1.0 - (double)inBeat / blipFrames : 0.0;
                s = (float)(sin(phase) * env * env) * level;
                phase += twoPi * f / rate;
            }
            else
            {
                s = (float)sin(phase) * level;
                phase += twoPi * hz / rate;
            }
            if (phase > twoPi)
                phase -= twoPi;

            /* load voices: real work the compiler can't drop, mixed in at zero */
            float sink = 0;
            for (int v = 0; v < voices; v++)
            {
                vphase[v] += twoPi * (110.0 + v) / rate;
                if (vphase[v] > twoPi)
                    vphase[v] -= twoPi;
                sink += (float)sin(vphase[v]);
            }
            s += sink * 0.0f * level;

            buf[2 * i] = s;
            buf[2 * i + 1] = s;
        }
        if (n > 0)
        {
            mpdj_ring_write(r, buf, (uint32_t)n);
            uint32_t dt = mpdj_ring_now_us() - t0;
            renderSum += dt;
            renders++;
            if (dt > renderMax)
                renderMax = dt;
        }

        uint32_t now = mpdj_ring_now_us();
        if (now - reportUs >= 5000000)
        {
            uint32_t u = r->underruns;
            printf("s3tone: wake avg %llu max %llu us, render avg %llu max %llu us (budget %u us per block), "
                   "queued at wake min %u frames, underruns %u\n",
                   (unsigned long long)(wakes ? wakeSum / wakes : 0), (unsigned long long)wakeMax,
                   (unsigned long long)(renders ? renderSum / renders : 0), (unsigned long long)renderMax,
                   (unsigned)(r->block * 1000000.0 / rate), queuedMin == UINT32_MAX ? 0 : queuedMin,
                   u - underrunsAtReport);
            wakeSum = wakeMax = wakes = renderSum = renderMax = renders = 0;
            queuedMin = UINT32_MAX;
            underrunsAtReport = u;
            reportUs = now;
        }
        if (stopAt && frame >= stopAt)
        {
            printf("s3tone: %.0f s done, exiting\n", seconds);
            break;
        }
    }
    mpdj_ring_close(r);
    return 0;
}
