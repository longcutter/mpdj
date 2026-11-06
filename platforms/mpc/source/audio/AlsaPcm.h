// Minimal ALSA PCM device on raw kernel ioctls (no libasound on the MPC).
// Interleaved S16_LE, blocking reads and writes.
#pragma once

#include <stdint.h>

class AlsaPcm
{
  public:
    ~AlsaPcm() { close(); }

    // Opens /dev/snd/pcmC<card>D<device>{p,c} and sets rate, channels and the
    // period size / count. On failure returns false and fills `err`.
    bool open(int card, int device, bool capture, unsigned rate, unsigned channels,
              unsigned periodFrames, unsigned periods, char* err, int errLen);
    void close();
    bool isOpen() const { return fd >= 0; }

    bool prepare();
    bool start();
    bool drop();

    // Frames written/read (all of them, since the device blocks), or -errno.
    // -EPIPE = xrun: call prepare() and carry on.
    int write(const int16_t* frames, unsigned count);
    int read(int16_t* frames, unsigned count);

    unsigned rate = 0;
    unsigned channels = 0;
    unsigned periodFrames = 0;
    unsigned bufferFrames = 0;

  private:
    int fd = -1;
    bool capture = false;
};
