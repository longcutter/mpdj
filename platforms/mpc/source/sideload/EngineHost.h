// Engine sideloads (sideload test S3, option B): a separate program renders audio
// into a shared-memory ring (mpdj_ring.h) and MPDJ mixes it into the master like
// a track. MPDJ keeps the codec; its audio thread clocks the engine by waking it
// after every 128-frame block it takes.
//
// Env:
//   MPDJ_ENGINE=<command>     start this engine at boot (/bin/sh -c, own process
//                             group, MPDJ_RING=<ring path> in its environment);
//                             stopped when MPDJ quits
//   MPDJ_ENGINE_FILL=frames   frames the engine keeps queued (default 256 = 5.8 ms;
//                             this is the latency the ring adds). Rounded to 128s
//   MPDJ_ENGINE_GAIN=dB       level the engine is mixed at (default 0)
//   MPDJ_ENGINE_RING=path     ring file (default /dev/shm/mpdj-engine-0)
//   MPDJ_AUDIO_STATS=1        every 5 s: blocks mixed, queued frames min/avg/max,
//                             underruns (printed anyway when there are underruns)
#pragma once

#include <stdint.h>

namespace EngineHost
{
void start(); // before MpcAudio::start
void stop();  // after MpcAudio::stop
void poll();  // main loop: notices the engine exiting, prints stats

// Audio thread, after the tracker's block: adds the engine's next block.
void mixBlock(int16_t* left, int16_t* right, unsigned frames);
} // namespace EngineHost
