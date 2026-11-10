// p8sound: a PICO-8 compatible sound engine (prototype for MPDJ).
// Constants and waveform shapes follow zepto8 (Sam Hocevar, WTFPL).
#include "p8sound.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace p8 {

static float mix(float a, float b, float t) { return a + (b - a) * t; }

float keyToFreq(float key) { return 440.f * std::exp2((key - 33.f) / 12.f); }

bool Sfx::empty() const {
    for (auto &n : notes) if (n.vol) return false;
    return true;
}

// ---- .p8 text cart ------------------------------------------------------

static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int hex2(const std::string &s, size_t i) { return hex(s[i]) * 16 + hex(s[i + 1]); }

bool Cart::load(const std::string &path, std::string *err) {
    std::ifstream f(path);
    if (!f) { if (err) *err = "can't open " + path; return false; }
    std::string line, section;
    int sfxN = 0, songN = 0;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("__", 0) == 0) { section = line; continue; }
        if (section == "__sfx__" && line.size() >= 168 && sfxN < 64) {
            // 4 header bytes (editor/filters, speed, loop start, loop end) then 32 notes of 5 hex digits
            Sfx &s = sfx[sfxN++];
            s.filters = hex2(line, 0);
            s.speed = hex2(line, 2);
            s.loopStart = hex2(line, 4);
            s.loopEnd = hex2(line, 6);
            for (int n = 0; n < 32; ++n) {
                size_t p = 8 + n * 5;
                Note &nt = s.notes[n];
                nt.key = hex2(line, p) & 0x3f;
                int inst = hex(line[p + 2]);
                nt.inst = inst & 7;
                nt.custom = inst & 8;
                nt.vol = hex(line[p + 3]) & 7;
                nt.fx = hex(line[p + 4]) & 7;
            }
        } else if (section == "__music__" && line.size() >= 11 && songN < 64) {
            // "FF AABBCCDD": flags (1 start, 2 loop, 4 stop), then four SFX ids (0x40 = off)
            Song &sg = song[songN++];
            int flags = hex2(line, 0);
            sg.start = flags & 1; sg.loop = flags & 2; sg.stop = flags & 4;
            for (int c = 0; c < 4; ++c) sg.sfx[c] = hex2(line, 3 + c * 2);
        }
    }
    if (sfxN == 0 && err) *err = "no __sfx__ section";
    return sfxN > 0;
}

// ---- filter -----------------------------------------------------------

void Biquad::highShelf(float freq, float q, float gainDb) {
    float w0 = 6.2831853f * freq / kRate, cw = std::cos(w0);
    float a = std::pow(10.f, gainDb / 40);
    float alpha = (std::sin(w0) / 2) * std::sqrt((a + 1 / a) * (1 / q - 1) + 2);
    float sq = 2 * std::sqrt(a) * alpha;
    float a0 = (a + 1) - (a - 1) * cw + sq;
    float a1 = 2 * ((a - 1) - (a + 1) * cw);
    float a2 = (a + 1) - (a - 1) * cw - sq;
    float b0 = a * ((a + 1) + (a - 1) * cw + sq);
    float b1 = -2 * a * ((a - 1) + (a + 1) * cw);
    float b2 = a * ((a + 1) + (a - 1) * cw - sq);
    c1 = b0 / a0; c2 = b1 / a0; c3 = b2 / a0; c4 = a1 / a0; c5 = a2 / a0;
}

float Biquad::run(float x) {
    float y = c1 * x + c2 * x1 + c3 * x2 - c4 * y1 - c5 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
}

// ---- waveforms --------------------------------------------------------

static float frand(uint32_t &s) {  // -1..1, xorshift
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return (s >> 8) * (2.f / 16777216.f) - 1.f;
}

static float wave(Osc &o, uint8_t w, double phi, uint32_t &rng) {
    using std::fabs; using std::fmod;
    float t = (float)fmod(phi, 1.0);
    bool noiz = o.filters & 2, buzz = o.filters & 4;
    float r;
    switch (w) {
    case Triangle:
        r = 1.f - fabs(4.f * t - 2.f);
        if (buzz) {
            const float a = 0.875f;
            float b = t < a ? 2.f * t / a - 1.f : 2.f * (1.f - t) / (1.f - a) - 1.f;
            r = r * 0.75f + b * 0.25f;
        }
        return r * 0.5f;
    case TiltedSaw: {
        float a = buzz ? 0.975f : 0.875f;
        r = t < a ? 2.f * t / a - 1.f : 2.f * (1.f - t) / (1.f - a) - 1.f;
        return r * 0.5f;
    }
    case Saw:
        r = t < 0.5f ? t : t - 1.f;
        if (buzz) r = r * 0.83f - (fabs((float)fmod(phi, 2.0) - 1.f) < 0.5f ? 0.085f : 0.f);
        return 0.653f * r;
    case Square: return t < (buzz ? 0.4f : 0.5f) ? 0.25f : -0.25f;
    case Pulse:  return t < (buzz ? 0.255f : 0.316f) ? 0.25f : -0.25f;
    case Organ:
        r = t < 0.5f ? 3.f - fabs(24.f * t - 6.f) : 1.f - fabs(16.f * t - 12.f);
        if (buzz) {
            r = t < 0.5f ? r * 2.f + 3.f : r;
            r = (t < 0.5f && r > -1.875f) ? r * 0.2f - 1.f : r + 0.5f;
        }
        return r / 9.f;
    case Noise: {
        const float tscale = 8.858923f;  // 22050 / keyToFreq(63)
        float scale = (float)(phi - o.lastAdvance) * tscale;
        float s = (o.lastSample + scale * frand(rng)) / (1.f + scale);
        float k = 1.f - o.key / 63.f;
        r = s * 1.5f * (1.f + k * k);
        if (noiz) r *= 2.f * (t < 0.5f ? t : t - 1.f);
        o.lastAdvance = (float)phi;
        o.lastSample = s;
        return r;
    }
    case Phaser:
        r = 2.f - fabs(8.f * t - 4.f);
        r += 1.f - fabs(4.f * (float)fmod(phi * 109.0 / 110.0, 1.0) - 2.f);
        if (buzz) {
            r += 0.25f - fabs((float)fmod(phi * 2.0 + 0.5, 1.0) - 0.5f);
            r += 0.125f - fabs(0.5f * (float)fmod(phi * 4.0, 1.0) - 0.25f);
        }
        return r / 6.f;
    }
    return 0;
}

float Channel::sample(Osc &o) {
    float v = wave(o, o.wave, o.phi, rng_);
    int detune = (o.filters / 8) % 3;
    if (detune && o.wave != Noise) {
        float f;
        if (o.wave == Triangle) f = detune == 1 ? 3.f / 4.f : 3.f / 2.f;
        else if (o.wave == Organ) f = detune == 1 ? 200.f / 199.f : 800.f / 199.f;
        else if (o.wave == Phaser) f = detune == 1 ? 49.f / 50.f : 400.f / 199.f;
        else f = detune == 1 ? 200.f / 199.f : 400.f / 199.f;
        uint8_t w2 = (detune == 2 && o.wave == Organ) ? Triangle : o.wave;
        v += wave(o, w2, o.phi * f, rng_) * 0.5f;
    }
    return std::clamp(v * o.vol, -1.f, 1.f);
}

// ---- SFX stepping (one sample) -----------------------------------------

void Channel::step(SfxState &s, Osc &o, float freqFactor, bool canLoop, bool music) {
    if (s.sfx < 0) return;
    const Sfx &sfx = cart_->sfx[s.sfx];
    const int speed = std::max(1, (int)sfx.speed);
    const double perSec = kRate / (kTickSamples * speed);
    const double perSample = perSec / kRate;
    double off = s.offset, next = off + perSample, nextTime = s.time + perSample;

    float loopRange = float(sfx.loopEnd - sfx.loopStart);
    if (loopRange > 0 && next >= sfx.loopEnd && canLoop)
        next = std::fmod(next - sfx.loopStart, loopRange) + sfx.loopStart;

    bool hasEnd = false;
    float endTime = 32;
    if (!music && sfx.loopEnd == 0 && sfx.loopStart > 0) { hasEnd = true; endTime = std::min<float>(endTime, sfx.loopStart); }
    if (loopRange <= 0) {
        hasEnd = true;
        if (!music) {
            int last = 0;
            for (int n = 0; n < 32; ++n) if (sfx.notes[n].vol) last = n + 1;
            endTime = std::min(endTime, float(last));
        }
    }

    if (off < 32) {
        int id = (int)off, nextId = (int)next;
        const Note &nt = sfx.notes[id];
        float vol = nt.vol / 7.f;
        float freq = keyToFreq(nt.key) * freqFactor;
        if (vol > 0) {
            float ph = (float)std::fmod(off, 1.0);
            switch (nt.fx) {
            case FxSlide:  // from the previous note and volume
                freq = mix(keyToFreq(s.prevKey), freq, ph);
                if (s.prevVol > 0) vol = mix(s.prevVol, vol, ph);
                break;
            case FxVibrato: {
                float t = std::fabs((float)std::fmod(7.5 * off / perSec, 1.0) - 0.5f) - 0.25f;
                freq = mix(freq, freq * 1.059463094f, t);
                break;
            }
            case FxDrop:    freq *= 1.f - ph; break;
            case FxFadeIn:  vol *= ph; break;
            case FxFadeOut: vol *= 1.f - ph; break;
            case FxArpFast:
            case FxArpSlow: {
                int m = (speed <= 8 ? 32 : 16) / (nt.fx == FxArpFast ? 4 : 8);
                int n = (int)(m * 7.5 * off / perSec);
                freq = keyToFreq(sfx.notes[(id & ~3) | (n & 3)].key);
                break;
            }
            }
            o.key = nt.key; o.freq = freq; o.wave = nt.inst; o.custom = nt.custom;
            o.filters = sfx.filters; o.vol = vol; o.music = music;
            o.phi += freq / kRate;
        }
        if (nextId != id) { s.prevKey = nt.key; s.prevVol = nt.vol / 7.f; }
    }
    s.offset = next;
    s.time = nextTime;
    if (hasEnd && nextTime >= endTime) s.sfx = -1;
}

void Channel::play(const Cart *cart, int sfx, double offset, bool music) {
    cart_ = cart;
    main_ = SfxState{};
    main_.sfx = sfx;
    main_.offset = main_.time = offset;
    music_ = music;
}

float Channel::render() {
    if (!dampInit_) {
        damp1_.highShelf(2400.f, 1.f, -6.f);
        damp2_.highShelf(1000.f, 1.f, -12.f);
        dampInit_ = true;
    }
    Osc o;
    o.phi = last_.phi; o.lastAdvance = last_.lastAdvance; o.lastSample = last_.lastSample;
    float v = 0;
    if (cart_ && main_.sfx >= 0) {
        double base = main_.offset;
        step(main_, o, 1.f, true, music_);
        bool restart = o.wave != lastInst_ || o.key != lastKey_;
        lastInst_ = o.wave; lastKey_ = o.key;
        if (o.vol > 0) {
            if (o.custom) {
                // Custom instrument: SFX 0-7 played underneath, transposed to the note (C-2 = as written)
                if (main_.offset < base) restart = true;
                if (customSfx_.sfx < 0 && std::floor(base) != std::floor(main_.offset)) restart = true;
                if (restart) customSfx_ = SfxState{o.wave, 0, 0, 0, 0};
                o.phi = last_.phi;
                float factor = o.freq / keyToFreq(24);
                float mainVol = o.vol;
                step(customSfx_, o, factor, true, false);
                o.vol *= mainVol;
            }
            v = sample(o);
        }
    }
    // Short crossfade on sudden changes (PICO-8 does this too, it avoids clicks)
    float thr = std::min(o.freq, last_.freq) * 0.01f;
    if (std::fabs(o.vol - last_.vol) > 0.1f || std::fabs(o.freq - last_.freq) > thr || o.wave != last_.wave) {
        if (fade_ <= 0) fadeOsc_ = last_;
        fade_ = 1;
        o.phi = std::fmod(o.phi, 1.f);
    }
    last_ = o;

    int reverb = (o.filters / 24) % 3, dampen = (o.filters / 72) % 3;
    float r1 = reverb == 1, r2 = reverb == 2, d1 = dampen == 1, d2 = dampen == 2;
    if (fade_ > 0) {
        fadeOsc_.phi += fadeOsc_.freq / kRate;
        v = mix(v, sample(fadeOsc_), fade_);
        int fr = (fadeOsc_.filters / 24) % 3, fd = (fadeOsc_.filters / 72) % 3;
        r1 = mix(r1, fr == 1, fade_); r2 = mix(r2, fr == 2, fade_);
        d1 = mix(d1, fd == 1, fade_); d2 = mix(d2, fd == 2, fade_);
        fade_ -= 130.f / kRate;
    }
    // Reverb = two short echoes; zepto8 scales the long one by the short one's amount (kept as measured)
    if (r1 > 0) v += r1 * rev2_[revIdx_ % 366] * 0.5f;
    if (r2 > 0) v += r1 * rev4_[revIdx_ % 732] * 0.5f;
    rev2_[revIdx_ % 366] = v;
    rev4_[revIdx_ % 732] = v;
    ++revIdx_;
    float vd1 = damp1_.run(v);
    if (d1 > 0) v = mix(v, vd1, d1);
    float vd2 = damp2_.run(v);
    if (d2 > 0) v = mix(v, vd2, d2);
    return std::clamp(v, -0.99f, 0.99f);
}

// ---- music --------------------------------------------------------------

void MusicPlayer::start(const Cart *cart, int pattern) {
    cart_ = cart;
    setPattern(pattern);
}

void MusicPlayer::setPattern(int p) {
    for (auto &c : ch) c.stop();
    if (p < 0 || p > 63) { pattern_ = -1; return; }
    const Song &sg = cart_->song[p];
    int loopDur = -1, noLoopDur = -1;
    for (int i = 0; i < 4; ++i) {
        int n = sg.sfx[i];
        if (n & 0x40) continue;
        const Sfx &s = cart_->sfx[n & 0x3f];
        if (s.loopEnd > 0 && s.loopEnd > s.loopStart) {
            loopDur = std::max(loopDur, 32 * (int)s.speed);
        } else {
            int end = 32;
            if (s.loopEnd == 0 && s.loopStart > 0) end = std::min<int>(end, s.loopStart);
            noLoopDur = end * s.speed;
            break;
        }
    }
    int dur = noLoopDur > 0 ? noLoopDur : loopDur;
    if (dur <= 0) { pattern_ = -1; return; }
    pattern_ = p; offset_ = 0; length_ = dur;
    for (int i = 0; i < 4; ++i)
        if (!(sg.sfx[i] & 0x40)) ch[i].play(cart_, sg.sfx[i] & 0x3f, 0, true);
}

float MusicPlayer::render() {
    if (pattern_ >= 0) {
        offset_ += 1.0 / kTickSamples;
        if (offset_ >= length_) {
            const Song &sg = cart_->song[pattern_];
            int next = pattern_ + 1;
            if (sg.stop) next = -1;
            else if (sg.loop) while (--next > 0 && !cart_->song[next].start) {}
            setPattern(next);
        }
    }
    float mixed = 0;
    for (int i = 0; i < 4; ++i) {
        float s = ch[i].render();
        if (!mute[i]) mixed += s;
    }
    return mixed;
}

}  // namespace p8
