// MPDJ as a JACK client (sideload test S3, option A: JACK hub). libjack is
// loaded at run time with dlopen, so the normal build has no JACK dependency;
// it only works where libjack.so.0 can be loaded, i.e. with MPDJ running inside
// the Debian root next to jackd (tools/sideload/s3a/s3a-run.sh).
//
// Ports: in_1/in_2 (connected to system:capture_1/2), out_1/out_2 (connected to
// system:playback_1/2), aux_1/aux_2 (not connected: anything a sideload plays
// can be routed into MPDJ here with jack_connect; MPDJ mixes it into its master).
#pragma once

#include <stdint.h>

namespace JackClient
{
// One JACK period. Buffers are JACK's own, valid only during the call.
using Process = void (*)(const float* inL, const float* inR, const float* auxL, const float* auxR,
                         float* outL, float* outR, unsigned frames, void* user);

// Loads libjack, opens client `name` (no server start), registers the ports.
// Prints why on failure.
bool open(const char* name, Process process, void* user);
bool activate(bool connectInputs); // also connects the system ports
void deactivate();
void close();
bool isOpen();

unsigned sampleRate();
unsigned bufferSize();
float cpuLoad();          // JACK's DSP load, percent
unsigned takeXruns();     // xruns JACK reported since the last call
bool serverGone();        // jackd shut down under us
// Capture -> playback latency JACK reports for the system ports (frames).
unsigned systemLatency(bool capture);
} // namespace JackClient
