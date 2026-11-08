// Stand-alone audio checks for external resampling (milestone 8b). Each runs
// instead of the tracker and returns an exit code. Stop the MPC app first.
//
//   MPDJ_LATENCY_TEST=1    cable from the outs to the ins; plays chirps and
//                          prints the round trip in frames, per channel.
//                          MPDJ_LATENCY_PINGS (default 8), MPDJ_LATENCY_LEVEL
//                          (dBFS, default -12), MPDJ_LATENCY_WAV=file saves
//                          the capture. Uses MPDJ_AUDIO_PERIODS like the tracker.
//   MPDJ_AUDIO_CONTROLS=1  read-only list of the codec's mixer controls and PCM
//                          devices: shows whether the headphone out has its
//                          own path or volume, separate from the line outs.
#pragma once

namespace AudioProbe
{
int latencyTest();
int listControls();
} // namespace AudioProbe
