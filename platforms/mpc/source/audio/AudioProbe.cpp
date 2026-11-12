#include "AudioProbe.h"

#include "AlsaPcm.h"
#include "JackClient.h"
#include "LatencyDetect.h"
#include "MpcAudio.h"

#include <algorithm>
#include <atomic>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sched.h>
#include <sound/asound.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace
{
constexpr unsigned kRate = 44100;
constexpr unsigned kBlock = 128;
constexpr int kChirpLen = 2048;              // ~46 ms
constexpr unsigned kIntervalBlocks = 344;    // ~1 s between pings
constexpr unsigned kFirstBlock = 43;         // ~125 ms of silence first
constexpr int kSearch = 22050;               // look up to 500 ms after each ping

int envInt(const char* name, int def)
{
    const char* v = getenv(name);
    return v && *v ? atoi(v) : def;
}

void writeWav(const char* path, const std::vector<int16_t>& stereo)
{
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        printf("latency: can't write %s: %s\n", path, strerror(errno));
        return;
    }
    uint32_t bytes = (uint32_t)(stereo.size() * 2);
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(kRate);
    u32(kRate * 4);
    u16(4);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(bytes);
    fwrite(stereo.data(), 2, stereo.size(), f);
    fclose(f);
    printf("latency: capture saved to %s\n", path);
}
} // namespace

// ---------------------------------------------------------------- latency
namespace
{
// The probe's output signal at output frame n: chirps at fixed block offsets.
float probeSample(long n, const std::vector<float>& chirp, int pings, float amp)
{
    for (int p = 0; p < pings; p++)
    {
        long k = n - (long)(kFirstBlock + p * kIntervalBlocks) * kBlock;
        if (k >= 0 && k < kChirpLen)
            return amp * chirp[k];
    }
    return 0.0f;
}

// JACK run (MPDJ_AUDIO=jack): the process callback writes the chirps and keeps
// the input on the same frame timeline, so the lag found is JACK's round trip,
// which is what MPDJ sees as a JACK client.
struct JackProbe
{
    const std::vector<float>* chirp;
    std::vector<int16_t>* cap;
    int pings;
    float amp;
    long frame = 0, total = 0;
    std::atomic<bool> done{ false };
};

void jackProbeProcess(const float* inL, const float* inR, const float*, const float*, float* outL,
                      float* outR, unsigned n, void* user)
{
    auto* p = (JackProbe*)user;
    for (unsigned i = 0; i < n; i++, p->frame++)
    {
        float s = p->frame < p->total ? probeSample(p->frame, *p->chirp, p->pings, p->amp) : 0.0f;
        outL[i] = outR[i] = s;
        if (p->frame < p->total)
        {
            auto q = [](float v) { return (int16_t)lrintf(std::max(-1.0f, std::min(1.0f, v)) * 32767.0f); };
            (*p->cap)[(size_t)p->frame * 2] = q(inL[i]);
            (*p->cap)[(size_t)p->frame * 2 + 1] = q(inR[i]);
        }
    }
    if (p->frame >= p->total)
        p->done = true;
}
} // namespace

int AudioProbe::latencyTest()
{
    const int card = envInt("MPDJ_AUDIO_CARD", MpcAudio::codecCard());
    const int device = envInt("MPDJ_AUDIO_DEVICE", 0);
    int periods = envInt("MPDJ_AUDIO_PERIODS", 3);
    if (periods < 2)
        periods = 2;
    int pings = envInt("MPDJ_LATENCY_PINGS", 8);
    if (pings < 1)
        pings = 1;
    const int levelDb = envInt("MPDJ_LATENCY_LEVEL", -12);
    const float amp = powf(10.0f, levelDb / 20.0f);
    const char* mode = getenv("MPDJ_AUDIO");
    const bool useJack = mode && !strcmp(mode, "jack");

    std::vector<float> chirp(kChirpLen);
    LatencyDetect::makeChirp(chirp.data(), kChirpLen, 500.0f, 8000.0f, (float)kRate);

    const unsigned totalBlocks = kFirstBlock + pings * kIntervalBlocks + kIntervalBlocks;
    std::vector<int16_t> cap((size_t)totalBlocks * kBlock * 2);
    unsigned xruns = 0;
    char how[160];

    if (useJack)
    {
        JackProbe jp;
        jp.chirp = &chirp;
        jp.cap = &cap;
        jp.pings = pings;
        jp.amp = amp;
        jp.total = (long)totalBlocks * kBlock;
        if (!JackClient::open("mpdj-latency", jackProbeProcess, &jp))
            return 1;
        if (JackClient::sampleRate() != kRate)
        {
            printf("latency: jackd runs at %u Hz, not %u\n", JackClient::sampleRate(), kRate);
            JackClient::close();
            return 1;
        }
        printf("latency: through JACK, period %u frames, %d pings at %d dBFS, both channels\n",
               JackClient::bufferSize(), pings, levelDb);
        printf("latency: patch a cable (or the pedal chain) from the outs to the ins. "
               "The chirps are audible: turn monitors down.\n");
        if (!JackClient::activate(true))
        {
            JackClient::close();
            return 1;
        }
        unsigned sysCap = JackClient::systemLatency(true), sysPlay = JackClient::systemLatency(false);
        unsigned period = JackClient::bufferSize();
        while (!jp.done && !JackClient::serverGone())
            usleep(20000);
        xruns = JackClient::takeXruns();
        JackClient::close();
        if (!jp.done)
        {
            printf("latency: jackd went away during the test\n");
            return 1;
        }
        snprintf(how, sizeof how,
                 "Offset is JACK's round trip with period %u (JACK itself reports capture %u + playback %u frames)",
                 period, sysCap, sysPlay);
    }
    else
    {
        // Same device setup as the tracker's audio thread, so the measured offset
        // is the one the resampler will see.
        AlsaPcm out, in;
        char err[200];
        if (!out.open(card, device, false, kRate, 2, kBlock, periods, err, sizeof err))
        {
            printf("latency: %s\n", err);
            return 1;
        }
        if (!in.open(card, device, true, kRate, 2, kBlock, periods + 1, err, sizeof err))
        {
            printf("latency: input: %s\n", err);
            return 1;
        }
        printf("latency: hw:%d,%d, %u x %u frames out, %d pings at %d dBFS, both channels\n", card,
               device, out.bufferFrames / out.periodFrames, out.periodFrames, pings, levelDb);
        printf("latency: patch a cable (or the pedal chain) from the outs to the ins. "
               "The chirps are audible: turn monitors down.\n");

        sched_param sp = {};
        sp.sched_priority = 70;
        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
            printf("latency: SCHED_FIFO unavailable (%s); xruns may spoil the result\n",
                   strerror(errno));

        int16_t outBuf[kBlock * 2];
        static const int16_t silence[kBlock * 2] = {};

        // Prefill exactly as audioThread does, then read-then-write per block.
        unsigned fill = out.bufferFrames - kBlock;
        for (unsigned f = 0; f + kBlock <= fill; f += kBlock)
            out.write(silence, kBlock);
        in.start();

        for (unsigned b = 0; b < totalBlocks; b++)
        {
            int16_t* inBlock = &cap[(size_t)b * kBlock * 2];
            if (in.read(inBlock, kBlock) < 0)
            {
                xruns++;
                memset(inBlock, 0, kBlock * 4);
                in.prepare();
                in.start();
            }

            // Output frame index of this block = b * kBlock, the loop's own timeline.
            for (unsigned i = 0; i < kBlock; i++)
            {
                int16_t v = (int16_t)lrintf(probeSample((long)b * kBlock + i, chirp, pings, amp) * 32767.0f);
                outBuf[2 * i] = outBuf[2 * i + 1] = v;
            }

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
                printf("latency: write failed: %s\n", strerror(-w));
                return 1;
            }
        }
        out.drop();
        in.drop();
        snprintf(how, sizeof how,
                 "Offset is from the render loop with %d periods; it changes if MPDJ_AUDIO_PERIODS changes",
                 periods);
    }

    if (const char* wavPath = getenv("MPDJ_LATENCY_WAV"))
        if (*wavPath)
            writeWav(wavPath, cap);

    const int n = (int)(cap.size() / 2);
    std::vector<float> ch(n);
    int rc = 0;
    for (int c = 0; c < 2; c++)
    {
        for (int i = 0; i < n; i++)
            ch[i] = cap[(size_t)i * 2 + c] / 32768.0f;

        std::vector<int> lags;
        float qMin = 1e9f, gainDb = -99.0f;
        int inverted = 0;
        for (int p = 0; p < pings; p++)
        {
            int from = (int)((kFirstBlock + p * kIntervalBlocks) * kBlock);
            auto r = LatencyDetect::find(ch.data(), n, chirp.data(), kChirpLen, from, kSearch);
            qMin = std::min(qMin, r.quality);
            if (r.lag < 0 || r.quality < 8.0f)
                continue;
            lags.push_back(r.lag);
            inverted += r.inverted;
            gainDb = std::max(gainDb, 20.0f * log10f(std::max(r.peak / amp, 1e-6f)));
        }
        const char* name = c ? "R" : "L";
        if ((int)lags.size() < (pings + 1) / 2)
        {
            printf("latency %s: no return found (%d of %d pings). Cable in? Input level up?\n",
                   name, (int)lags.size(), pings);
            rc = 2;
            continue;
        }
        std::sort(lags.begin(), lags.end());
        int med = lags[lags.size() / 2];
        printf("latency %s: %d frames (%.2f ms), spread %d..%d over %d/%d pings, "
               "return %+.1f dB vs sent%s, quality %.0f\n",
               name, med, med * 1000.0 / kRate, lags.front(), lags.back(), (int)lags.size(),
               pings, gainDb, inverted * 2 > (int)lags.size() ? ", polarity inverted" : "", qMin);
    }
    printf("latency: %u xruns%s. %s.\n", xruns,
           xruns ? " (result may be off by a period: run again)" : "", how);
    return rc;
}

// ---------------------------------------------------------------- controls
namespace
{
const char* typeName(int t)
{
    switch (t)
    {
        case SNDRV_CTL_ELEM_TYPE_BOOLEAN: return "bool";
        case SNDRV_CTL_ELEM_TYPE_INTEGER: return "int";
        case SNDRV_CTL_ELEM_TYPE_ENUMERATED: return "enum";
        case SNDRV_CTL_ELEM_TYPE_BYTES: return "bytes";
        case SNDRV_CTL_ELEM_TYPE_IEC958: return "iec958";
        case SNDRV_CTL_ELEM_TYPE_INTEGER64: return "int64";
        default: return "?";
    }
}

const char* ifaceName(int i)
{
    switch (i)
    {
        case SNDRV_CTL_ELEM_IFACE_CARD: return "card";
        case SNDRV_CTL_ELEM_IFACE_MIXER: return "mixer";
        case SNDRV_CTL_ELEM_IFACE_PCM: return "pcm";
        default: return "other";
    }
}

void printFile(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f)
        return;
    printf("--- %s\n", path);
    char line[256];
    while (fgets(line, sizeof line, f))
        printf("  %s", line);
    fclose(f);
}
} // namespace

int AudioProbe::listControls()
{
    const int card = envInt("MPDJ_AUDIO_CARD", MpcAudio::codecCard());

    printFile("/proc/asound/cards");
    printFile("/proc/asound/pcm");
    char dir[64];
    snprintf(dir, sizeof dir, "/proc/asound/card%d", card);
    if (DIR* d = opendir(dir))
    {
        while (dirent* e = readdir(d))
            if (!strncmp(e->d_name, "pcm", 3))
            {
                char path[160];
                snprintf(path, sizeof path, "%s/%s/info", dir, e->d_name);
                printFile(path);
            }
        closedir(d);
    }

    char path[64];
    snprintf(path, sizeof path, "/dev/snd/controlC%d", card);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        printf("controls: open %s: %s\n", path, strerror(errno));
        return 1;
    }

    snd_ctl_elem_list list = {};
    if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_LIST, &list) < 0)
    {
        printf("controls: list: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    std::vector<snd_ctl_elem_id> ids(list.count);
    list.space = list.count;
    list.pids = ids.data();
    if (list.count && ioctl(fd, SNDRV_CTL_IOCTL_ELEM_LIST, &list) < 0)
    {
        printf("controls: list: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    printf("--- %s: %u controls (read only; nothing is changed)\n", path, list.used);

    for (unsigned i = 0; i < list.used; i++)
    {
        snd_ctl_elem_info info = {};
        info.id = ids[i];
        if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_INFO, &info) < 0)
            continue;
        printf("  #%u %s '%s' idx %u: %s x%u", ids[i].numid, ifaceName(ids[i].iface),
               (const char*)ids[i].name, ids[i].index, typeName(info.type), info.count);
        if (info.type == SNDRV_CTL_ELEM_TYPE_INTEGER)
            printf(" [%ld..%ld step %ld]", info.value.integer.min, info.value.integer.max,
                   info.value.integer.step);

        std::vector<std::string> items;
        if (info.type == SNDRV_CTL_ELEM_TYPE_ENUMERATED)
            for (unsigned k = 0; k < info.value.enumerated.items && k < 32; k++)
            {
                snd_ctl_elem_info ei = {};
                ei.id = ids[i];
                ei.value.enumerated.item = k;
                if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_INFO, &ei) == 0)
                    items.push_back(ei.value.enumerated.name);
            }

        if (info.access & SNDRV_CTL_ELEM_ACCESS_READ)
        {
            snd_ctl_elem_value v = {};
            v.id = ids[i];
            if (ioctl(fd, SNDRV_CTL_IOCTL_ELEM_READ, &v) == 0)
            {
                printf(" =");
                unsigned shown = info.count < 8 ? info.count : 8;
                for (unsigned k = 0; k < shown; k++)
                {
                    if (info.type == SNDRV_CTL_ELEM_TYPE_BOOLEAN ||
                        info.type == SNDRV_CTL_ELEM_TYPE_INTEGER)
                        printf(" %ld", v.value.integer.value[k]);
                    else if (info.type == SNDRV_CTL_ELEM_TYPE_INTEGER64)
                        printf(" %lld", (long long)v.value.integer64.value[k]);
                    else if (info.type == SNDRV_CTL_ELEM_TYPE_ENUMERATED)
                    {
                        unsigned it = v.value.enumerated.item[k];
                        printf(" '%s'", it < items.size() ? items[it].c_str() : "?");
                    }
                }
            }
        }
        if (!(info.access & SNDRV_CTL_ELEM_ACCESS_WRITE))
            printf(" (read-only)");
        printf("\n");
        if (!items.empty())
        {
            printf("      options:");
            for (auto& s : items)
                printf(" '%s'", s.c_str());
            printf("\n");
        }
    }
    close(fd);
    return 0;
}
