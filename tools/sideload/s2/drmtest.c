// MPDJ sideload test S2b: can a program show its own pictures through DRM/KMS
// (not /dev/fb0)? Plain kernel ioctls, no libdrm or Mesa, so it separates the
// display path from SDL / Mesa problems.
//
// Becomes DRM master, finds the connected connector (DSI-1) and its CRTC,
// makes two dumb buffers and shows them:
//   1. 3 s: buffer A (red / green / blue bands) set once with SETCRTC
//   2. 3 s: CPU writes a white bar sweeping down into the buffer on screen
//      (shows whether writes to a scanned-out buffer appear: cache / damage)
//   3. 3 s: alternate A and B with PAGE_FLIP (B = blue / green / red bands)
// then puts back whatever was on the CRTC before. Prints every ioctl failure.
// Run with MPDJ and the MPC app stopped.
//
//   drmtest pitch   same as stage 1, but with a padded row pitch (800 px wide
//                   buffer laid out 832 px apart, 3,328 bytes, as Mesa makes them)
//   drmtest peek    no master needed: read the pixels of whatever buffer the
//                   screen shows now (run it while another program draws)
//
// Build: zig cc -target arm-linux-musleabihf -O2 -static -s -I/usr/include/drm -o drmtest drmtest.c
#include <drm.h>
#include <drm_mode.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static int fd;

static int io(unsigned long req, void* arg, const char* name)
{
    int r;
    do
        r = ioctl(fd, req, arg);
    while (r < 0 && (errno == EINTR || errno == EAGAIN));
    if (r < 0)
        printf("drmtest: %s failed: %s\n", name, strerror(errno));
    return r;
}

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

struct Buf
{
    uint32_t handle, fbId, pitch, w, h;
    uint64_t size;
    uint32_t* px;
};

static int makeBuf(struct Buf* b, uint32_t w, uint32_t h, uint32_t allocW)
{
    struct drm_mode_create_dumb c = { .height = h, .width = allocW, .bpp = 32 };
    if (io(DRM_IOCTL_MODE_CREATE_DUMB, &c, "CREATE_DUMB"))
        return -1;
    b->handle = c.handle, b->pitch = c.pitch, b->size = c.size, b->w = w, b->h = h;
    struct drm_mode_fb_cmd f = { .width = w, .height = h, .pitch = c.pitch, .bpp = 32, .depth = 24, .handle = c.handle };
    if (io(DRM_IOCTL_MODE_ADDFB, &f, "ADDFB"))
        return -1;
    b->fbId = f.fb_id;
    struct drm_mode_map_dumb m = { .handle = c.handle };
    if (io(DRM_IOCTL_MODE_MAP_DUMB, &m, "MAP_DUMB"))
        return -1;
    b->px = mmap(0, c.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
    if (b->px == MAP_FAILED)
    {
        printf("drmtest: mmap failed: %s\n", strerror(errno));
        return -1;
    }
    printf("drmtest: buffer fb %u %ux%u pitch %u\n", b->fbId, w, h, b->pitch);
    return 0;
}

static void bands(struct Buf* b, const uint32_t c[3])
{
    for (uint32_t y = 0; y < b->h; y++)
    {
        uint32_t* row = (uint32_t*)((uint8_t*)b->px + (size_t)y * b->pitch);
        uint32_t col = c[y * 3 / b->h];
        for (uint32_t x = 0; x < b->w; x++)
            row[x] = col;
    }
}

static void dirty(struct Buf* b)
{
    struct drm_mode_fb_dirty_cmd d = { .fb_id = b->fbId };
    ioctl(fd, DRM_IOCTL_MODE_DIRTYFB, &d); // only some drivers need it; ignore errors
}

// Reads back the buffer the CRTC shows: format, pitch, and a few pixels.
static int peek(void)
{
    struct drm_mode_card_res res = { 0 };
    uint32_t crtcs[16];
    if (io(DRM_IOCTL_MODE_GETRESOURCES, &res, "GETRESOURCES") || res.count_crtcs > 16)
        return 1;
    res.crtc_id_ptr = (uintptr_t)crtcs;
    res.count_connectors = res.count_encoders = res.count_fbs = 0;
    if (io(DRM_IOCTL_MODE_GETRESOURCES, &res, "GETRESOURCES 2"))
        return 1;
    for (uint32_t i = 0; i < res.count_crtcs; i++)
    {
        struct drm_mode_crtc c = { .crtc_id = crtcs[i] };
        if (io(DRM_IOCTL_MODE_GETCRTC, &c, "GETCRTC") || !c.fb_id)
            continue;
        struct drm_mode_fb_cmd f = { .fb_id = c.fb_id };
        if (io(DRM_IOCTL_MODE_GETFB, &f, "GETFB"))
            continue;
        printf("peek: crtc %u shows fb %u: %ux%u pitch %u bpp %u depth %u handle %u\n", c.crtc_id, f.fb_id, f.width,
               f.height, f.pitch, f.bpp, f.depth, f.handle);
        if (!f.handle)
        {
            printf("peek: no handle (not allowed to read it)\n");
            continue;
        }
        struct drm_mode_map_dumb m = { .handle = f.handle };
        if (io(DRM_IOCTL_MODE_MAP_DUMB, &m, "MAP_DUMB"))
            continue;
        size_t size = (size_t)f.pitch * f.height;
        uint8_t* p = mmap(0, size, PROT_READ, MAP_SHARED, fd, m.offset);
        if (p == MAP_FAILED)
        {
            printf("peek: mmap: %s\n", strerror(errno));
            continue;
        }
        unsigned nonBlack = 0;
        for (uint32_t y = 0; y < f.height; y += 8)
            for (uint32_t x = 0; x < f.width; x += 8)
                if (*(uint32_t*)(p + (size_t)y * f.pitch + x * 4) & 0xFFFFFF)
                    nonBlack++;
        printf("peek: %u of %u sampled pixels are not black\n", nonBlack, ((f.height + 7) / 8) * ((f.width + 7) / 8));
        for (int band = 0; band < 3; band++)
        {
            uint32_t y = f.height * (2 * band + 1) / 6, x = f.width / 2;
            printf("peek: pixel (%u, %u) = 0x%08X\n", x, y, *(uint32_t*)(p + (size_t)y * f.pitch + x * 4));
        }
        munmap(p, size);
    }
    return 0;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        printf("drmtest: open /dev/dri/card0: %s\n", strerror(errno));
        return 1;
    }
    if (argc > 1 && !strcmp(argv[1], "peek"))
        return peek();
    int padded = argc > 1 && !strcmp(argv[1], "pitch");
    if (io(DRM_IOCTL_SET_MASTER, 0, "SET_MASTER (is the MPC app or MPDJ running?)"))
        return 1;

    // Resources
    struct drm_mode_card_res res = { 0 };
    if (io(DRM_IOCTL_MODE_GETRESOURCES, &res, "GETRESOURCES"))
        return 1;
    uint32_t conns[16], crtcs[16], encs[16];
    if (res.count_connectors > 16 || res.count_crtcs > 16 || res.count_encoders > 16)
        return 1;
    res.connector_id_ptr = (uintptr_t)conns;
    res.crtc_id_ptr = (uintptr_t)crtcs;
    res.encoder_id_ptr = (uintptr_t)encs;
    res.fb_id_ptr = 0, res.count_fbs = 0;
    if (io(DRM_IOCTL_MODE_GETRESOURCES, &res, "GETRESOURCES 2"))
        return 1;

    struct drm_mode_modeinfo mode;
    uint32_t connId = 0, crtcId = 0;
    for (uint32_t i = 0; i < res.count_connectors && !connId; i++)
    {
        struct drm_mode_get_connector gc = { .connector_id = conns[i] };
        if (io(DRM_IOCTL_MODE_GETCONNECTOR, &gc, "GETCONNECTOR"))
            continue;
        if (gc.connection != 1 || gc.count_modes == 0)
            continue;
        struct drm_mode_modeinfo modes[16];
        uint32_t encIds[8];
        uint32_t nm = gc.count_modes > 16 ? 16 : gc.count_modes;
        gc.count_modes = nm;
        gc.modes_ptr = (uintptr_t)modes;
        gc.count_props = 0, gc.props_ptr = 0, gc.prop_values_ptr = 0;
        gc.count_encoders = gc.count_encoders > 8 ? 8 : gc.count_encoders;
        gc.encoders_ptr = (uintptr_t)encIds;
        if (io(DRM_IOCTL_MODE_GETCONNECTOR, &gc, "GETCONNECTOR 2"))
            continue;
        mode = modes[0];
        connId = gc.connector_id;
        struct drm_mode_get_encoder ge = { .encoder_id = gc.encoder_id ? gc.encoder_id : encIds[0] };
        if (io(DRM_IOCTL_MODE_GETENCODER, &ge, "GETENCODER") == 0)
            crtcId = ge.crtc_id ? ge.crtc_id : crtcs[0];
        printf("drmtest: connector %u, mode %s %ux%u @ %u Hz, crtc %u\n", connId, mode.name, mode.hdisplay, mode.vdisplay,
               mode.vrefresh, crtcId);
    }
    if (!connId || !crtcId)
    {
        printf("drmtest: no connected connector / crtc\n");
        return 1;
    }

    struct drm_mode_crtc saved = { .crtc_id = crtcId };
    io(DRM_IOCTL_MODE_GETCRTC, &saved, "GETCRTC");
    printf("drmtest: before: crtc %u shows fb %u\n", crtcId, saved.fb_id);

    struct Buf a, b;
    uint32_t allocW = padded ? 832 : mode.hdisplay; // 832 * 4 = 3,328, the pitch Mesa used
    if (makeBuf(&a, mode.hdisplay, mode.vdisplay, allocW) || makeBuf(&b, mode.hdisplay, mode.vdisplay, allocW))
        return 1;
    static const uint32_t ca[3] = { 0xDC2828, 0x28C828, 0x2850E6 }, cb[3] = { 0x2850E6, 0x28C828, 0xDC2828 };
    bands(&a, ca);
    bands(&b, cb);

    struct drm_mode_crtc set = { .crtc_id = crtcId, .fb_id = a.fbId, .set_connectors_ptr = (uintptr_t)&connId,
                                 .count_connectors = 1, .mode = mode, .mode_valid = 1 };
    printf("drmtest: 1/3 SETCRTC buffer A: red / green / blue bands, 3 s\n");
    io(DRM_IOCTL_MODE_SETCRTC, &set, "SETCRTC");
    dirty(&a);
    sleep(3);
    if (padded)
    {
        printf("drmtest: padded pitch %u: that was the only stage\n", a.pitch);
        goto restore;
    }

    printf("drmtest: 2/3 white bar drawn into the buffer on screen, 3 s\n");
    double t0 = now();
    int lastY = -1;
    while (now() - t0 < 3)
    {
        int y = (int)((now() - t0) / 3 * (a.h - 40));
        if (lastY >= 0)
            for (int yy = lastY; yy < lastY + 40; yy++)
            {
                uint32_t* row = (uint32_t*)((uint8_t*)a.px + (size_t)yy * a.pitch);
                for (uint32_t x = 0; x < a.w; x++)
                    row[x] = ca[yy * 3 / a.h];
            }
        for (int yy = y; yy < y + 40; yy++)
        {
            uint32_t* row = (uint32_t*)((uint8_t*)a.px + (size_t)yy * a.pitch);
            for (uint32_t x = 0; x < a.w; x++)
                row[x] = 0xFFFFFF;
        }
        lastY = y;
        dirty(&a);
        usleep(16000);
    }
    bands(&a, ca);

    printf("drmtest: 3/3 PAGE_FLIP between A and B (bands swap), 3 s\n");
    int flips = 0, fails = 0;
    t0 = now();
    while (now() - t0 < 3)
    {
        struct drm_mode_crtc_page_flip pf = { .crtc_id = crtcId, .fb_id = (flips & 1) ? a.fbId : b.fbId };
        if (ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &pf) == 0)
            flips++;
        else if (errno != EBUSY && fails++ < 3)
            printf("drmtest: PAGE_FLIP failed: %s\n", strerror(errno));
        usleep(250000);
    }
    printf("drmtest: %d page flips\n", flips);

restore:
    // Put back what was there (fbdev's buffer), then let go.
    if (saved.fb_id && saved.mode_valid)
    {
        saved.set_connectors_ptr = (uintptr_t)&connId;
        saved.count_connectors = 1;
        io(DRM_IOCTL_MODE_SETCRTC, &saved, "SETCRTC restore");
    }
    ioctl(fd, DRM_IOCTL_DROP_MASTER, 0);
    printf("drmtest: done\n");
    return 0;
}
