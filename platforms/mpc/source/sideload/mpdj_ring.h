/* MPDJ engine ring: how an engine sideload sends audio to MPDJ (sideload test S3,
 * option B). Plain C, header only, no dependencies: copy it into any engine.
 *
 * MPDJ creates the ring (a file in /dev/shm, which the Debian root sees through
 * its bind mount), starts the engine with MPDJ_RING=<path>, and mixes whatever
 * the engine writes into its master like a track, one 128-frame block per audio
 * block. MPDJ's audio thread is the clock: after each read it bumps a futex word
 * and wakes the engine, which tops the ring back up to `target` frames.
 *
 *   mpdj_ring *r = mpdj_ring_open(getenv("MPDJ_RING"));
 *   float buf[MPDJ_RING_MAX_FRAMES * 2];
 *   for (;;) {
 *       int n = mpdj_ring_wait(r, 1000);   // frames wanted, a multiple of r->block
 *       if (n < 0) break;                  // MPDJ stopped reading: exit
 *       render(buf, n);                    // n stereo frames, interleaved, -1..1
 *       mpdj_ring_write(r, buf, n);
 *   }
 *   mpdj_ring_close(r);
 *
 * Added latency = the frames queued in the ring when MPDJ reads (about `target`,
 * MPDJ_ENGINE_FILL, default 256 = 5.8 ms at 44.1 kHz). The engine has until the
 * ring runs dry to answer a wake; if it is late MPDJ pads the block with silence
 * and counts an underrun.
 *
 * Single reader (MPDJ), single writer (the engine). Positions are frame counters
 * that wrap at 2^32; capacity is a power of two.
 */
#ifndef MPDJ_RING_H
#define MPDJ_RING_H

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#define MPDJ_RING_MAGIC 0x5244504Du /* "MPDR" */
#define MPDJ_RING_VERSION 1u
#define MPDJ_RING_HEADER 256u
#define MPDJ_RING_MAX_FRAMES 4096u

typedef struct mpdj_ring
{
    /* fixed, set by MPDJ when it creates the ring */
    uint32_t magic, version;
    uint32_t rate;     /* 44100 */
    uint32_t channels; /* 2, interleaved float -1..1 */
    uint32_t capacity; /* frames, power of two */
    uint32_t block;    /* MPDJ reads this many frames per audio block (128) */
    uint32_t target;   /* frames the engine keeps queued */
    uint32_t pad0[9];

    /* written by MPDJ (reader), own cache line */
    uint32_t readPos;   /* frames consumed */
    uint32_t wake;      /* futex word, +1 after every read */
    uint32_t wakeUs;    /* CLOCK_MONOTONIC us (low 32 bits) of the last wake */
    uint32_t underruns; /* blocks MPDJ found short */
    uint32_t reading;   /* 1 while MPDJ reads; 0 = engine should exit */
    uint32_t pad1[11];

    /* written by the engine (writer), own cache line */
    uint32_t writePos;    /* frames written */
    uint32_t engineState; /* 0 not yet, 1 running, 2 exited */
    uint32_t pad2[14];

    /* samples at offset MPDJ_RING_HEADER */
} mpdj_ring;

static inline float* mpdj_ring_data(mpdj_ring* r)
{
    return (float*)((char*)r + MPDJ_RING_HEADER);
}

static inline size_t mpdj_ring_bytes(uint32_t capacity, uint32_t channels)
{
    return MPDJ_RING_HEADER + (size_t)capacity * channels * sizeof(float);
}

static inline uint32_t mpdj_ring_load(const uint32_t* p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

static inline void mpdj_ring_store(uint32_t* p, uint32_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

static inline uint32_t mpdj_ring_now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u);
}

/* Frames queued (written, not yet read). */
static inline uint32_t mpdj_ring_fill(mpdj_ring* r)
{
    return mpdj_ring_load(&r->writePos) - mpdj_ring_load(&r->readPos);
}

/* Engine side ---------------------------------------------------------------- */

static inline mpdj_ring* mpdj_ring_open(const char* path)
{
    if (!path || !*path)
        return 0;
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return 0;
    off_t size = lseek(fd, 0, SEEK_END);
    if (size < (off_t)MPDJ_RING_HEADER)
    {
        close(fd);
        return 0;
    }
    mpdj_ring* r = (mpdj_ring*)mmap(0, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (r == MAP_FAILED)
        return 0;
    if (r->magic != MPDJ_RING_MAGIC || r->version != MPDJ_RING_VERSION ||
        (size_t)size < mpdj_ring_bytes(r->capacity, r->channels))
    {
        munmap(r, (size_t)size);
        return 0;
    }
    mpdj_ring_store(&r->engineState, 1);
    return r;
}

static inline void mpdj_ring_close(mpdj_ring* r)
{
    if (!r)
        return;
    mpdj_ring_store(&r->engineState, 2);
    munmap(r, mpdj_ring_bytes(r->capacity, r->channels));
}

/* Waits until the ring has room below `target`, then returns how many frames to
 * render (a multiple of block, at least one block). Returns 0 after timeoutMs
 * with nothing to do, -1 once MPDJ has stopped reading. */
static inline int mpdj_ring_wait(mpdj_ring* r, int timeoutMs)
{
    for (int waited = 0;;)
    {
        if (!mpdj_ring_load(&r->reading))
            return -1;
        uint32_t seen = mpdj_ring_load(&r->wake);
        uint32_t fill = mpdj_ring_fill(r);
        if (fill + r->block <= r->target)
        {
            uint32_t n = (r->target - fill) / r->block * r->block;
            return (int)n;
        }
        if (waited >= timeoutMs)
            return 0;
        struct timespec ts = { 0, 100 * 1000000 }; /* re-check `reading` every 100 ms */
        syscall(SYS_futex, &r->wake, FUTEX_WAIT, seen, &ts, 0, 0);
        waited += 100; /* approximate: a wake also counts, which only shortens the timeout */
    }
}

/* Appends `frames` interleaved frames. The caller asked mpdj_ring_wait first, so
 * there is room. */
static inline void mpdj_ring_write(mpdj_ring* r, const float* src, uint32_t frames)
{
    uint32_t ch = r->channels, mask = r->capacity - 1;
    uint32_t w = r->writePos; /* only we write it */
    float* d = mpdj_ring_data(r);
    uint32_t start = w & mask;
    uint32_t first = r->capacity - start;
    if (first > frames)
        first = frames;
    memcpy(d + (size_t)start * ch, src, (size_t)first * ch * sizeof(float));
    if (frames > first)
        memcpy(d, src + (size_t)first * ch, (size_t)(frames - first) * ch * sizeof(float));
    mpdj_ring_store(&r->writePos, w + frames);
}

/* MPDJ side ------------------------------------------------------------------ */

static inline void mpdj_ring_wake(mpdj_ring* r)
{
    __atomic_add_fetch(&r->wake, 1, __ATOMIC_RELEASE);
    syscall(SYS_futex, &r->wake, FUTEX_WAKE, 1, 0, 0, 0);
}

#endif
