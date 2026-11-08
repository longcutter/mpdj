// Round-trip latency detection: find where a known chirp comes back in a
// capture. Pure functions, no device access, so they can be tested on a host.
#pragma once

namespace LatencyDetect
{
// Hann-windowed linear sweep from f0 to f1 Hz, `len` samples long, peak 1.0.
void makeChirp(float* out, int len, float f0, float f1, float rate);

struct Result
{
    int lag = -1;          // first arrival, in samples from the search start; -1 = none
    float quality = 0;     // correlation peak / correlation RMS (higher = cleaner)
    float peak = 0;        // matched-filter peak, normalised: 1.0 = template at full scale
    bool inverted = false; // return arrives with flipped polarity
};

// Matched filter over capture[from .. from + searchLen + tmplLen). The first
// arrival is the earliest correlation peak reaching a quarter of the strongest one, so
// a delay or reverb echo louder than the direct sound doesn't win. quality
// below ~8 means no usable return (no cable, or the signal is buried).
Result find(const float* capture, int captureLen, const float* tmpl, int tmplLen, int from,
            int searchLen);
} // namespace LatencyDetect
