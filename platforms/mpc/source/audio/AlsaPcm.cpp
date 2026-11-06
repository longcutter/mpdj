#include "AlsaPcm.h"

#include <errno.h>
#include <fcntl.h>
#include <sound/asound.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

// hw_params helpers, the same way tinyalsa fills the structure: everything
// open, then the fields we care about pinned; SNDRV_PCM_IOCTL_HW_PARAMS refines
// the rest and picks the first valid value for anything still open.
namespace
{
snd_mask* mask(snd_pcm_hw_params* p, int param)
{
    return &p->masks[param - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}

snd_interval* interval(snd_pcm_hw_params* p, int param)
{
    return &p->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

void initParams(snd_pcm_hw_params* p)
{
    memset(p, 0, sizeof *p);
    for (int i = SNDRV_PCM_HW_PARAM_FIRST_MASK; i <= SNDRV_PCM_HW_PARAM_LAST_MASK; i++)
        memset(mask(p, i)->bits, 0xff, sizeof mask(p, i)->bits);
    for (int i = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL; i <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; i++)
    {
        interval(p, i)->min = 0;
        interval(p, i)->max = ~0u;
    }
    p->rmask = ~0u;
    p->cmask = 0;
    p->info = ~0u;
}

void setMask(snd_pcm_hw_params* p, int param, unsigned bit)
{
    snd_mask* m = mask(p, param);
    memset(m->bits, 0, sizeof m->bits);
    m->bits[bit >> 5] |= 1u << (bit & 31);
}

void setInt(snd_pcm_hw_params* p, int param, unsigned value)
{
    snd_interval* i = interval(p, param);
    i->min = i->max = value;
    i->openmin = i->openmax = 0;
    i->integer = 1;
}

unsigned getInt(snd_pcm_hw_params* p, int param)
{
    return interval(p, param)->min;
}
} // namespace

bool AlsaPcm::open(int card, int device, bool cap, unsigned r, unsigned ch, unsigned period,
                   unsigned periods, char* err, int errLen)
{
    close();
    capture = cap;
    char path[64];
    snprintf(path, sizeof path, "/dev/snd/pcmC%dD%d%c", card, device, cap ? 'c' : 'p');
    fd = ::open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        snprintf(err, errLen, "open %s: %s", path, strerror(errno));
        return false;
    }

    snd_pcm_hw_params p;
    initParams(&p);
    setMask(&p, SNDRV_PCM_HW_PARAM_ACCESS, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    setMask(&p, SNDRV_PCM_HW_PARAM_FORMAT, SNDRV_PCM_FORMAT_S16_LE);
    setMask(&p, SNDRV_PCM_HW_PARAM_SUBFORMAT, SNDRV_PCM_SUBFORMAT_STD);
    setInt(&p, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
    setInt(&p, SNDRV_PCM_HW_PARAM_FRAME_BITS, 16 * ch);
    setInt(&p, SNDRV_PCM_HW_PARAM_CHANNELS, ch);
    setInt(&p, SNDRV_PCM_HW_PARAM_RATE, r);
    setInt(&p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, period);
    setInt(&p, SNDRV_PCM_HW_PARAM_PERIODS, periods);

    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &p) < 0)
    {
        snprintf(err, errLen, "%s: hw params %u Hz, %u ch, %u x %u frames: %s", path, r, ch,
                 periods, period, strerror(errno));
        close();
        return false;
    }
    rate = getInt(&p, SNDRV_PCM_HW_PARAM_RATE);
    channels = getInt(&p, SNDRV_PCM_HW_PARAM_CHANNELS);
    periodFrames = getInt(&p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
    bufferFrames = getInt(&p, SNDRV_PCM_HW_PARAM_BUFFER_SIZE);

    // Default software params after HW_PARAMS: start on the first frame,
    // stop on underrun, wake per period. That's what we want.
    if (!prepare())
    {
        snprintf(err, errLen, "%s: prepare: %s", path, strerror(errno));
        close();
        return false;
    }
    return true;
}

void AlsaPcm::close()
{
    if (fd >= 0)
        ::close(fd);
    fd = -1;
}

bool AlsaPcm::prepare()
{
    return fd >= 0 && ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) == 0;
}

bool AlsaPcm::start()
{
    return fd >= 0 && ioctl(fd, SNDRV_PCM_IOCTL_START) == 0;
}

bool AlsaPcm::drop()
{
    return fd >= 0 && ioctl(fd, SNDRV_PCM_IOCTL_DROP) == 0;
}

int AlsaPcm::write(const int16_t* frames, unsigned count)
{
    unsigned done = 0;
    while (done < count)
    {
        snd_xferi x = {};
        x.buf = (void*)(frames + done * channels);
        x.frames = count - done;
        if (ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x) < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        done += (unsigned)x.result;
    }
    return (int)done;
}

int AlsaPcm::read(int16_t* frames, unsigned count)
{
    unsigned done = 0;
    while (done < count)
    {
        snd_xferi x = {};
        x.buf = frames + done * channels;
        x.frames = count - done;
        if (ioctl(fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x) < 0)
        {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        done += (unsigned)x.result;
    }
    return (int)done;
}
