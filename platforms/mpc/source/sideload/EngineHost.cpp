#include "EngineHost.h"

#include "mpdj_ring.h"

#include <atomic>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace
{
constexpr uint32_t kCapacity = 2048; // frames, power of two (46 ms)
constexpr uint32_t kBlock = 128;

mpdj_ring* ring = nullptr;
char ringPath[256];
pid_t pid = -1;
float gain = 1.0f;
bool stats = false;
std::atomic<bool> active{ false }; // the audio thread mixes while set
bool armed = false;                // audio thread: started reading (ring reached target once)

// Audio thread -> poll(): stats since the last report.
std::atomic<uint32_t> statBlocks{ 0 }, statShort{ 0 }, statFillMin{ UINT32_MAX }, statFillMax{ 0 };
std::atomic<uint64_t> statFillSum{ 0 };
uint64_t lastReportMs = 0;

int envInt(const char* name, int def)
{
    const char* v = getenv(name);
    return v && *v ? atoi(v) : def;
}

uint64_t nowMs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + ts.tv_nsec / 1000000;
}

bool createRing(uint32_t target)
{
    const char* p = getenv("MPDJ_ENGINE_RING");
    snprintf(ringPath, sizeof ringPath, "%s", p && *p ? p : "/dev/shm/mpdj-engine-0");
    unlink(ringPath);
    int fd = open(ringPath, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0)
    {
        printf("engine: can't create %s: %s\n", ringPath, strerror(errno));
        return false;
    }
    fchmod(fd, 0666); // an engine in the Debian root may run as another user
    size_t bytes = mpdj_ring_bytes(kCapacity, 2);
    if (ftruncate(fd, (off_t)bytes) != 0)
    {
        printf("engine: can't size %s: %s\n", ringPath, strerror(errno));
        close(fd);
        return false;
    }
    void* m = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (m == MAP_FAILED)
    {
        printf("engine: mmap failed: %s\n", strerror(errno));
        return false;
    }
    memset(m, 0, bytes);
    mlock(m, bytes); // the audio thread reads it; no page faults there
    ring = (mpdj_ring*)m;
    ring->rate = 44100;
    ring->channels = 2;
    ring->capacity = kCapacity;
    ring->block = kBlock;
    ring->target = target;
    ring->reading = 1;
    ring->version = MPDJ_RING_VERSION;
    mpdj_ring_store(&ring->magic, MPDJ_RING_MAGIC); // last: an engine that opens it early sees a whole header
    return true;
}

void stopEngine(const char* why)
{
    if (pid <= 0)
        return;
    printf("engine: stopping it (%s)\n", why);
    kill(-pid, SIGTERM);
    for (int i = 0; i < 150; i++) // 1.5 s, as for takeover sideloads
    {
        if (waitpid(pid, nullptr, WNOHANG) == pid)
        {
            pid = -1;
            break;
        }
        usleep(10000);
    }
    if (pid > 0)
    {
        printf("engine: still running 1.5 s after SIGTERM, sending SIGKILL\n");
        kill(-pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        pid = -1;
    }
}
} // namespace

void EngineHost::start()
{
    const char* cmd = getenv("MPDJ_ENGINE");
    if (!cmd || !*cmd)
        return;
    stats = envInt("MPDJ_AUDIO_STATS", 0) != 0;
    int fill = envInt("MPDJ_ENGINE_FILL", 256);
    fill = (fill + (int)kBlock / 2) / (int)kBlock * (int)kBlock;
    if (fill < (int)kBlock)
        fill = kBlock;
    if (fill > (int)(kCapacity - kBlock))
        fill = kCapacity - kBlock;
    const char* g = getenv("MPDJ_ENGINE_GAIN");
    gain = g && *g ? powf(10.0f, (float)atof(g) / 20.0f) : 1.0f;

    if (!createRing((uint32_t)fill))
        return;

    pid = fork();
    if (pid == 0)
    {
        setpgid(0, 0);
        for (int fd = 3; fd < 1024; fd++)
            close(fd);
        setenv("MPDJ_RING", ringPath, 1);
        signal(SIGPIPE, SIG_DFL);
        signal(SIGINT, SIG_IGN); // Ctrl+C in the ssh session is for MPDJ; MPDJ then stops us
        execl("/bin/sh", "sh", "-c", cmd, (char*)nullptr);
        _exit(127);
    }
    if (pid < 0)
    {
        printf("engine: fork failed: %s\n", strerror(errno));
        return;
    }
    setpgid(pid, pid);
    printf("engine: %s\n", cmd);
    printf("engine: ring %s, %d frames queued (%.1f ms added), mixed at %.1f dB\n", ringPath, fill,
           fill * 1000.0 / 44100.0, 20.0 * log10(gain));
    lastReportMs = nowMs();
    active = true;
}

void EngineHost::mixBlock(int16_t* left, int16_t* right, unsigned frames)
{
    if (!active.load(std::memory_order_acquire))
        return;
    mpdj_ring* r = ring;
    uint32_t fill = mpdj_ring_fill(r);
    if (!armed)
    {
        // Wait for the engine's first fill, so start-up isn't counted as underruns.
        if (fill < r->target)
            return;
        armed = true;
    }

    uint32_t take = fill < frames ? fill : frames;
    if (take < frames)
        r->underruns++;

    const float* d = mpdj_ring_data(r);
    uint32_t pos = r->readPos, mask = r->capacity - 1;
    const float k = 32767.0f * gain;
    for (uint32_t i = 0; i < take; i++)
    {
        const float* f = d + (size_t)((pos + i) & mask) * 2;
        int l = left[i] + (int)lrintf(f[0] * k);
        int rr = right[i] + (int)lrintf(f[1] * k);
        left[i] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        right[i] = (int16_t)(rr > 32767 ? 32767 : rr < -32768 ? -32768 : rr);
    }
    mpdj_ring_store(&r->readPos, pos + take);
    mpdj_ring_store(&r->wakeUs, mpdj_ring_now_us());
    mpdj_ring_wake(r);

    statBlocks.fetch_add(1, std::memory_order_relaxed);
    if (take < frames)
        statShort.fetch_add(1, std::memory_order_relaxed);
    statFillSum.fetch_add(fill, std::memory_order_relaxed);
    if (fill < statFillMin.load(std::memory_order_relaxed))
        statFillMin.store(fill, std::memory_order_relaxed);
    if (fill > statFillMax.load(std::memory_order_relaxed))
        statFillMax.store(fill, std::memory_order_relaxed);
}

void EngineHost::poll()
{
    if (!ring)
        return;
    if (pid > 0)
    {
        int st;
        if (waitpid(pid, &st, WNOHANG) == pid)
        {
            pid = -1;
            active = false;
            if (WIFEXITED(st))
                printf("engine: exited with status %d\n", WEXITSTATUS(st));
            else if (WIFSIGNALED(st))
                printf("engine: ended by signal %d\n", WTERMSIG(st));
        }
    }

    uint64_t now = nowMs();
    if (now - lastReportMs < 5000)
        return;
    lastReportMs = now;
    uint32_t blocks = statBlocks.exchange(0), shortBlocks = statShort.exchange(0);
    uint64_t sum = statFillSum.exchange(0);
    uint32_t mn = statFillMin.exchange(UINT32_MAX), mx = statFillMax.exchange(0);
    if (!stats && !shortBlocks)
        return;
    if (!blocks)
    {
        printf("engine: nothing mixed yet (ring holds %u of %u frames)\n", mpdj_ring_fill(ring), ring->target);
        return;
    }
    printf("engine: %u blocks mixed, queued at read min %u avg %llu max %u frames (target %u), "
           "underruns %u (total %u)\n",
           blocks, mn, (unsigned long long)(sum / blocks), mx, ring->target, shortBlocks, ring->underruns);
}

void EngineHost::stop()
{
    if (!ring)
        return;
    active = false;
    mpdj_ring_store(&ring->reading, 0);
    mpdj_ring_wake(ring);
    // A well-behaved engine sees `reading` = 0 and exits within ~100 ms; give it
    // a moment before the signals.
    for (int i = 0; i < 30 && pid > 0; i++)
    {
        if (waitpid(pid, nullptr, WNOHANG) == pid)
            pid = -1;
        else
            usleep(10000);
    }
    stopEngine("MPDJ quitting");
    printf("engine: %u underruns in total\n", ring->underruns);
    munmap(ring, mpdj_ring_bytes(ring->capacity, ring->channels));
    ring = nullptr;
    unlink(ringPath);
}
