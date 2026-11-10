// p8render: render a .p8 cart's SFX or music to a 44.1 kHz WAV (prototype tool)
//   p8render cart.p8 music <pattern> <seconds> out.wav [--stems]
//   p8render cart.p8 sfx <n> out.wav
//   p8render cart.p8 bench <seconds>
#include "p8sound.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

// 22050 -> 44100 by repeating each sample: keeps PICO-8's grit, which is part of the sound
static bool writeWav(const std::string &path, const std::vector<float> &s22) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    uint32_t frames = s22.size() * 2, bytes = frames * 4;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, 44100); put32(f, 44100 * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    for (float v : s22) {
        int16_t x = (int16_t)(std::max(-1.f, std::min(1.f, v)) * 32767.f);
        for (int k = 0; k < 4; ++k) fwrite(&x, 2, 1, f);  // L, R, twice
    }
    fclose(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: see source\n"); return 1; }
    p8::Cart cart;
    std::string err;
    if (!cart.load(argv[1], &err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    std::string mode = argv[2];

    if (mode == "sfx") {
        p8::Channel ch;
        ch.play(&cart, atoi(argv[3]));
        std::vector<float> out;
        while (ch.playing() && out.size() < (size_t)p8::kRate * 30) out.push_back(ch.render());
        for (int i = 0; i < p8::kRate / 4; ++i) out.push_back(ch.render());  // tail
        return writeWav(argv[4], out) ? 0 : 1;
    }
    if (mode == "music") {
        double secs = atof(argv[4]);
        bool stems = argc > 6 && !strcmp(argv[6], "--stems");
        size_t n = (size_t)(secs * p8::kRate);
        p8::MusicPlayer mp;
        mp.start(&cart, atoi(argv[3]));
        std::vector<float> mixOut;
        for (size_t i = 0; i < n && mp.playing(); ++i) mixOut.push_back(mp.render() * 0.5f);
        if (!writeWav(argv[5], mixOut)) return 1;
        if (stems) {
            for (int c = 0; c < 4; ++c) {
                p8::MusicPlayer m2;
                for (int k = 0; k < 4; ++k) m2.mute[k] = k != c;
                m2.start(&cart, atoi(argv[3]));
                std::vector<float> o;
                for (size_t i = 0; i < n && m2.playing(); ++i) o.push_back(m2.render() * 0.5f);
                std::string p = argv[5];
                p = p.substr(0, p.size() - 4) + "-ch" + std::to_string(c + 1) + ".wav";
                writeWav(p, o);
            }
        }
        return 0;
    }
    if (mode == "bench") {
        double secs = atof(argv[3]);
        size_t n = (size_t)(secs * p8::kRate);
        p8::MusicPlayer mp;
        float acc = 0;
        auto t0 = std::chrono::steady_clock::now();
        size_t done = 0;
        while (done < n) {
            mp.start(&cart, 0);
            while (mp.playing() && done < n) { acc += mp.render(); ++done; }
            if (!mp.playing() && done == 0) break;
        }
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        printf("%zu samples (%.1f s of audio) in %.1f ms: %.0fx real time, %.1f%% of one core (chk %g)\n",
               done, done / (double)p8::kRate, ms, done / (double)p8::kRate * 1000 / ms,
               100 * ms / (done / (double)p8::kRate * 1000), acc);
        return 0;
    }
    return 1;
}
