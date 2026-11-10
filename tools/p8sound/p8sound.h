// p8sound: a PICO-8 compatible sound engine (prototype for MPDJ)
//
// Waveform shapes, filter constants and effect timings follow zepto8 by
// Sam Hocevar (WTFPL), which measured them against PICO-8's own WAV
// exports. This code is a rewrite for MPDJ; credit zepto8 if it ships.
#pragma once
#include <cstdint>
#include <string>

namespace p8 {

constexpr int kRate = 22050;          // PICO-8's native sample rate
constexpr double kTickSamples = 183;  // samples per speed unit per note

enum Wave { Triangle, TiltedSaw, Saw, Square, Pulse, Organ, Noise, Phaser };
enum Fx { FxNone, FxSlide, FxVibrato, FxDrop, FxFadeIn, FxFadeOut, FxArpFast, FxArpSlow };

struct Note {
    uint8_t key = 0;     // 0..63, 33 = A-2 = 440 Hz (PICO-8 naming)
    uint8_t inst = 0;    // 0..7 waveform, or SFX 0..7 when custom
    uint8_t vol = 0;     // 0..7
    uint8_t fx = 0;      // 0..7
    bool custom = false;
};

struct Sfx {
    Note notes[32];
    uint8_t filters = 0;  // bit1 noiz, bit2 buzz, detune (f/8)%3, reverb (f/24)%3, dampen (f/72)%3
    uint8_t speed = 16;
    uint8_t loopStart = 0, loopEnd = 0;
    bool empty() const;
};

struct Song {
    uint8_t sfx[4] = {0x41, 0x42, 0x43, 0x44};  // bit 0x40 = channel off
    bool start = false, loop = false, stop = false;
};

struct Cart {
    Sfx sfx[64];
    Song song[64];
    bool load(const std::string &path, std::string *err = nullptr);
};

float keyToFreq(float key);

// Second-order filter (RBJ cookbook), for the dampen setting.
struct Biquad {
    float c1 = 1, c2 = 0, c3 = 0, c4 = 0, c5 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void highShelf(float freq, float q, float gainDb);
    float run(float x);
};

struct Osc {
    uint8_t wave = 0, key = 0, filters = 0;
    float freq = 0, vol = 0, phi = 0;
    bool custom = false, music = false;
    float lastAdvance = 0, lastSample = 0;  // noise state
};

// One PICO-8 channel: plays an SFX (with custom-instrument SFX underneath).
class Channel {
public:
    void play(const Cart *cart, int sfx, double offset = 0, bool music = false);
    void stop() { main_.sfx = -1; }
    bool playing() const { return main_.sfx >= 0; }
    float render();  // one sample at 22050 Hz, roughly -1..1

private:
    struct SfxState { int sfx = -1; double offset = 0, time = 0; int prevKey = 0; float prevVol = 0; };
    void step(SfxState &s, Osc &o, float freqFactor, bool canLoop, bool music);
    float sample(Osc &o);

    const Cart *cart_ = nullptr;
    SfxState main_, customSfx_;
    Osc last_, fadeOsc_;
    float fade_ = 0;
    uint8_t lastInst_ = 0, lastKey_ = 0;
    bool music_ = false;
    int revIdx_ = 0;
    float rev2_[366] = {}, rev4_[732] = {};
    Biquad damp1_, damp2_;
    bool dampInit_ = false;
    uint32_t rng_ = 0x12345678;
};

// Plays a cart's music from a pattern, four channels, like music(n).
class MusicPlayer {
public:
    void start(const Cart *cart, int pattern);
    bool playing() const { return pattern_ >= 0; }
    int pattern() const { return pattern_; }
    float render();  // mixed, 22050 Hz
    Channel ch[4];
    bool mute[4] = {};

private:
    void setPattern(int p);
    const Cart *cart_ = nullptr;
    int pattern_ = -1;
    double offset_ = 0, length_ = 0;
};

}  // namespace p8
