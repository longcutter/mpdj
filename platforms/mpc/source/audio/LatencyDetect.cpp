#include "LatencyDetect.h"

#include <math.h>
#include <vector>

void LatencyDetect::makeChirp(float* out, int len, float f0, float f1, float rate)
{
    const double T = len / (double)rate;
    const double k = (f1 - f0) / T;
    for (int i = 0; i < len; i++)
    {
        double t = i / (double)rate;
        double w = 0.5 - 0.5 * cos(2.0 * M_PI * i / (len - 1));
        out[i] = (float)(w * sin(2.0 * M_PI * (f0 * t + 0.5 * k * t * t)));
    }
}

LatencyDetect::Result LatencyDetect::find(const float* capture, int captureLen, const float* tmpl,
                                          int tmplLen, int from, int searchLen)
{
    Result r;
    if (from < 0 || tmplLen <= 0)
        return r;
    if (from + searchLen + tmplLen > captureLen)
        searchLen = captureLen - from - tmplLen;
    if (searchLen <= 0)
        return r;

    double energy = 0;
    for (int j = 0; j < tmplLen; j++)
        energy += (double)tmpl[j] * tmpl[j];

    std::vector<float> c(searchLen);
    double sumSq = 0;
    float best = 0;
    for (int lag = 0; lag < searchLen; lag++)
    {
        const float* x = capture + from + lag;
        float acc = 0;
        for (int j = 0; j < tmplLen; j++)
            acc += x[j] * tmpl[j];
        c[lag] = acc;
        sumSq += (double)acc * acc;
        if (fabsf(acc) > best)
            best = fabsf(acc);
    }
    double rms = sqrt(sumSq / searchLen);
    r.quality = rms > 0 ? (float)(best / rms) : 0;
    r.peak = (float)(best / energy);
    if (best <= 0)
        return r;

    // Earliest lag over the threshold, then the top of that pulse: the
    // compressed chirp oscillates, so take the largest |c| in the next 24
    // samples (the main lobe is ~6 samples wide for a 0.5-8 kHz sweep).
    // Threshold: a quarter of the strongest peak (a reverb or delay echo can be
    // louder than the direct sound), but well above the correlation noise.
    float thr = 0.25f * best;
    if (thr < 6.0f * (float)rms)
        thr = 6.0f * (float)rms;
    int first = 0;
    while (first < searchLen && fabsf(c[first]) < thr)
        first++;
    if (first >= searchLen)
        return r;
    int lag = first;
    for (int i = first; i < first + 24 && i < searchLen; i++)
        if (fabsf(c[i]) > fabsf(c[lag]))
            lag = i;
    r.lag = lag;
    r.inverted = c[lag] < 0;
    return r;
}
