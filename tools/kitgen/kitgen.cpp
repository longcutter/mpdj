// MPDJ additions (MIT, see NOTICE.md): kitgen, the MPDJ default kit.
//
// Renders 16 drum one-shots from scratch (oscillators, noise and filters; no
// recorded audio, so no third-party licence) as 16-bit mono 44.1 kHz WAVs,
// numbered in pad order so the Program Loader lists them that way:
//
//   kitgen <out dir>            writes "01 Kick.wav" .. "16 Boom.wav"
//   kitgen <out dir> --preview  also writes "preview.wav", a two-bar beat
//
// Deterministic: the same build always writes the same files.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

static constexpr double kRate = 44100.0;
static constexpr double kPi = 3.14159265358979323846;

using Buf = std::vector<double>;

// --- building blocks --------------------------------------------------------

struct Noise
{
    uint32_t s;
    explicit Noise(uint32_t seed) : s(seed) {}
    double next() // white, -1..1
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (s / 4294967295.0) * 2.0 - 1.0;
    }
};

// RBJ cookbook biquad
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    enum Type { LP, HP, BP };
    Biquad(Type t, double f, double q)
    {
        double w = 2 * kPi * f / kRate, c = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
        switch (t)
        {
        case LP: b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; break;
        case HP: b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; break;
        case BP: b0 = al; b1 = 0; b2 = -al; break;
        }
        a1 = -2 * c;
        a2 = 1 - al;
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }
    double run(double x)
    {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

static Buf make(double seconds) { return Buf(size_t(seconds * kRate), 0.0); }
static double env(double t, double decay) { return exp(-t / decay); }       // decay = time constant, s
static double attack(double t, double a) { return t < a ? t / a : 1.0; }  // linear rise over a seconds

// Sum of square waves at the 808's six "metal" frequencies, times `ratio`
static double metal(double t, double ratio)
{
    static const double f[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
    double s = 0;
    for (double fi : f)
        s += fmod(t * fi * ratio, 1.0) < 0.5 ? 1.0 : -1.0;
    return s / 6.0;
}

// Sine with an exponential pitch sweep from f0 to f1 (time constant tc)
struct Sweep
{
    double phase = 0;
    double run(double t, double f0, double f1, double tc)
    {
        double f = f1 + (f0 - f1) * exp(-t / tc);
        phase += f / kRate;
        return sin(2 * kPi * phase);
    }
};

// Fades the last 5 ms so no file ends with a click; scales to `peakDb` dBFS
static void finish(Buf& b, double peakDb)
{
    size_t fade = size_t(0.005 * kRate);
    for (size_t i = 0; i < fade && i < b.size(); i++)
        b[b.size() - 1 - i] *= double(i) / fade;
    double peak = 0;
    for (double v : b)
        peak = fmax(peak, fabs(v));
    if (peak > 0)
    {
        double g = pow(10.0, peakDb / 20.0) / peak;
        for (double& v : b)
            v *= g;
    }
}

// --- the 16 sounds ----------------------------------------------------------

static Buf kick()
{
    Buf b = make(0.6);
    Sweep s;
    Noise n(1);
    Biquad click(Biquad::BP, 3500, 0.8);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double body = s.run(t, 160, 48, 0.035) * env(t, 0.18);
        double c = click.run(n.next()) * env(t, 0.003) * 0.5;
        b[i] = tanh(1.6 * body) + c;
    }
    finish(b, -1);
    return b;
}

static Buf snare()
{
    Buf b = make(0.35);
    Noise n(2);
    Biquad hp(Biquad::HP, 1200, 0.7), bp(Biquad::BP, 5000, 0.6);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double tone = (0.6 * sin(2 * kPi * 185 * t) + 0.4 * sin(2 * kPi * 330 * t)) * env(t, 0.05);
        double x = n.next();
        double rattle = (0.6 * hp.run(x) + 0.5 * bp.run(x)) * env(t, 0.09);
        b[i] = 0.55 * tone + rattle;
    }
    finish(b, -2);
    return b;
}

static Buf hat(double seconds, double decay, uint32_t seed, double cutoff)
{
    Buf b = make(seconds);
    Noise n(seed);
    Biquad bp(Biquad::BP, 10000, 1.2), hp(Biquad::HP, cutoff, 0.7);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double x = 0.7 * metal(t, 1.0) + 0.3 * n.next();
        b[i] = hp.run(bp.run(x)) * env(t, decay);
    }
    return b;
}

static Buf closedHat()
{
    Buf b = hat(0.12, 0.022, 3, 7000);
    finish(b, -6);
    return b;
}

static Buf openHat()
{
    Buf b = hat(0.7, 0.16, 4, 6500);
    finish(b, -7);
    return b;
}

static Buf pedalHat()
{
    Buf b = hat(0.08, 0.012, 5, 5000);
    finish(b, -8);
    return b;
}

static Buf clap()
{
    Buf b = make(0.45);
    Noise n(6);
    Biquad bp(Biquad::BP, 1100, 1.4), hp(Biquad::HP, 600, 0.7);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        // three quick bursts, then the room tail
        double e = 0;
        for (int k = 0; k < 3; k++)
        {
            double tk = t - k * 0.011;
            if (tk >= 0)
                e = fmax(e, env(tk, 0.004));
        }
        if (t >= 0.022)
            e = fmax(e, 0.8 * env(t - 0.022, 0.11));
        b[i] = hp.run(bp.run(n.next())) * e;
    }
    finish(b, -2);
    return b;
}

static Buf rim()
{
    Buf b = make(0.08);
    Biquad bp(Biquad::BP, 1700, 2.0);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double x = 0.6 * sin(2 * kPi * 1700 * t) + 0.4 * sin(2 * kPi * 480 * t);
        b[i] = (0.5 * x + bp.run(x)) * env(t, 0.012);
    }
    finish(b, -4);
    return b;
}

static Buf tom(double f0, double f1, uint32_t seed)
{
    Buf b = make(0.6);
    Sweep s;
    Noise n(seed);
    Biquad lp(Biquad::LP, 3000, 0.7);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double body = s.run(t, f0, f1, 0.06) * env(t, 0.16);
        b[i] = tanh(1.3 * body) + 0.15 * lp.run(n.next()) * env(t, 0.02);
    }
    finish(b, -2);
    return b;
}

static Buf crash()
{
    Buf b = make(2.0);
    Noise n(10);
    Biquad hp(Biquad::HP, 3500, 0.7), bp(Biquad::BP, 7000, 0.5);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double x = 0.5 * metal(t, 1.37) + 0.5 * n.next();
        b[i] = (0.6 * hp.run(x) + 0.5 * bp.run(x)) * env(t, 0.55) * attack(t, 0.002);
    }
    finish(b, -6);
    return b;
}

static Buf ride()
{
    Buf b = make(1.6);
    Noise n(11);
    Biquad bp(Biquad::BP, 5200, 2.5), hp(Biquad::HP, 3000, 0.7);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double ping = 0.5 * metal(t, 2.1) * env(t, 0.04);
        double wash = hp.run(0.6 * metal(t, 1.71) + 0.15 * n.next()) * env(t, 0.5);
        b[i] = bp.run(ping) * 2.0 + 0.6 * wash;
    }
    finish(b, -8);
    return b;
}

static Buf cowbell()
{
    Buf b = make(0.35);
    Biquad bp(Biquad::BP, 800, 1.5);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double x = (fmod(t * 540, 1.0) < 0.5 ? 1 : -1) + (fmod(t * 800, 1.0) < 0.5 ? 1 : -1);
        double e = t < 0.01 ? 1.0 : 0.6 * env(t - 0.01, 0.09) + 0.4 * env(t, 0.008);
        b[i] = bp.run(x * 0.5) * e;
    }
    finish(b, -4);
    return b;
}

static Buf clave()
{
    Buf b = make(0.1);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        b[i] = sin(2 * kPi * 2500 * t) * env(t, 0.018);
    }
    finish(b, -5);
    return b;
}

static Buf shaker()
{
    Buf b = make(0.16);
    Noise n(14);
    Biquad hp(Biquad::HP, 6000, 0.7), bp(Biquad::BP, 9000, 0.8);
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double e = t < 0.03 ? t / 0.03 : env(t - 0.03, 0.035);
        b[i] = bp.run(hp.run(n.next())) * e;
    }
    finish(b, -8);
    return b;
}

static Buf boom()
{
    Buf b = make(1.6);
    Sweep s;
    for (size_t i = 0; i < b.size(); i++)
    {
        double t = i / kRate;
        double x = s.run(t, 90, 44, 0.03) * env(t, 0.55);
        b[i] = tanh(2.2 * x) * attack(t, 0.001);
    }
    finish(b, -1);
    return b;
}

// --- output -----------------------------------------------------------------

static bool writeWav(const std::string& path, const Buf& b)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
    {
        perror(path.c_str());
        return false;
    }
    uint32_t n = uint32_t(b.size()), data = n * 2, rate = uint32_t(kRate), byteRate = rate * 2;
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); u32(36 + data); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(rate); u32(byteRate); u16(2); u16(16);
    fwrite("data", 1, 4, f); u32(data);
    for (double v : b)
    {
        double s = fmax(-1.0, fmin(1.0, v)) * 32767.0;
        int16_t x = int16_t(lrint(s));
        fwrite(&x, 2, 1, f);
    }
    return fclose(f) == 0;
}

struct Sound
{
    const char* name;
    Buf (*make)();
};

static Buf lowTom() { return tom(130, 82, 7); }
static Buf midTom() { return tom(180, 118, 8); }
static Buf highTom() { return tom(250, 165, 9); }

// Pad order: pad 1 = bottom left on the MPC
static const Sound kKit[16] = {
    { "Kick", kick },         { "Snare", snare },     { "Closed Hat", closedHat }, { "Open Hat", openHat },
    { "Clap", clap },         { "Rim", rim },         { "Low Tom", lowTom },       { "Mid Tom", midTom },
    { "High Tom", highTom },  { "Crash", crash },     { "Ride", ride },            { "Pedal Hat", pedalHat },
    { "Cowbell", cowbell },   { "Clave", clave },     { "Shaker", shaker },        { "Boom", boom },
};

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: kitgen <out dir> [--preview]\n");
        return 2;
    }
    std::string dir = argv[1];
    bool preview = argc > 2 && strcmp(argv[2], "--preview") == 0;

    Buf sounds[16];
    for (int i = 0; i < 16; i++)
    {
        sounds[i] = kKit[i].make();
        char name[64];
        snprintf(name, sizeof name, "%02d %s.wav", i + 1, kKit[i].name);
        if (!writeWav(dir + "/" + name, sounds[i]))
            return 1;
        printf("%s  %.2f s\n", name, sounds[i].size() / kRate);
    }

    if (preview)
    {
        // Two bars at 100 BPM, 16th notes; one row per sound index + 1 (0 = rest)
        static const char* const kRows[] = {
            "1...1...1..1....1...1...1.....1.", // kick
            "....2.......2.......2.......2..2", // snare
            "3.3.3.3.3.3.3.3.3.3.3.3.3.3.3...", // closed hat
            "..............................4.", // open hat
            "............5...............5...", // clap
            "..........................7.8.9.", // toms
            "A...............................", // crash
        };
        double step = 60.0 / 100 / 4;
        Buf out = make(32 * step + 2.0);
        for (const char* row : kRows)
            for (int s = 0; s < 32; s++)
            {
                char c = row[s];
                int idx = c >= '1' && c <= '9' ? c - '1' : c >= 'A' && c <= 'G' ? c - 'A' + 9 : -1;
                if (idx < 0)
                    continue;
                size_t at = size_t(s * step * kRate);
                for (size_t i = 0; i < sounds[idx].size() && at + i < out.size(); i++)
                    out[at + i] += 0.5 * sounds[idx][i];
            }
        finish(out, -1);
        if (!writeWav(dir + "/preview.wav", out))
            return 1;
        printf("preview.wav  %.2f s\n", out.size() / kRate);
    }
    return 0;
}
