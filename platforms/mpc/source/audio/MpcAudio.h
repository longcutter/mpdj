// Audio for the MPC One+: a real-time thread that renders the Tracker engine in
// 128-frame blocks (which also clocks the sequencer) and plays them on the
// internal codec, ALSA hw:1,0 at 44.1 kHz.
//
// Env:
//   MPDJ_AUDIO=none        no device; a timer clocks the engine (QEMU)
//   MPDJ_AUDIO_WAV=file    no device; write the output to a WAV file in real time
//   MPDJ_AUDIO_CARD / _DEVICE   default: the card named ACVA (card 1), device 0
//   MPDJ_AUDIO_PERIODS     128-frame periods in the ALSA buffer (default 3, ~8.7 ms)
//   MPDJ_AUDIO_IN=0        leave the input closed (default: open the codec's capture
//                          side and feed the Tracker input; confirmed clean 2026-11-09)
//   MPDJ_AUDIO_STATS=1     every 5 s: render time and xruns
#pragma once

class Tracker;

namespace MpcAudio
{
void start(Tracker* tracker);
void stop();

// Called by CodecImpl: the Tracker's master volume (0..1, SGTL5000 headphone
// curve) and headphone mute.
void setVolume(float v);
void setHeadphoneMuted(bool muted);
} // namespace MpcAudio
