# p8sound: PICO-8 sound engine (prototype)

A small, PICO-8 compatible sound engine for MPDJ, plus `p8render`, a command-line tool that renders a cart's music or SFX to WAV. Not built into MPDJ yet: the engine is meant to become a synth engine (milestone 6) and a `.p8` cart importer; until then `p8render` turns carts into samples you can load in the Sample Loader.

What it does:

- Reads the `__sfx__` and `__music__` sections of a text `.p8` cart (`.p8.png` carts aren't read; save them as `.p8` in PICO-8 first)
- All 8 waveforms (triangle, tilted saw, saw, square, pulse, organ, noise, phaser), the 7 note effects (slide, vibrato, drop, fade in, fade out, fast and slow arpeggio), the SFX filters (noiz, buzz, detune 1/2, reverb 1/2, dampen 1/2), custom instruments (SFX 0-7), loops and music patterns with start / loop / stop flags
- Renders at PICO-8's 22,050 Hz and writes 44.1 kHz 16-bit stereo, repeating each sample (keeps PICO-8's grit)

## Build (Mac or Linux)

```
make -C tools/p8sound
```

## Use

```
tools/p8sound/p8render mycart.p8 music 0 60 song.wav --stems
```

Plays the music from pattern 0 for up to 60 seconds (stops earlier if the song stops), writes `song.wav` plus `song-ch1.wav` .. `song-ch4.wav`, one per channel, for sampling single parts.

```
tools/p8sound/p8render mycart.p8 sfx 12 sfx12.wav
tools/p8sound/p8render mycart.p8 bench 60
```

`sfx` renders one SFX (up to 30 s if it loops). `bench` times 60 s of music and prints the CPU use.

Copy the WAVs to the card: `tar -C <parent> -cf - <folder> | ssh mpc 'tar -C "/media/MPC SDC/MPDJ" -xf -'`

## Test cart

`python3 tools/p8sound/make_testcart.py test.p8` writes a cart with a waveform tour (SFX 8-15), an effect tour (16-22), a filter tour (23-28) and a short 4-channel song (music pattern 0) that uses a custom instrument (SFX 0).

## Credits and licence

Waveform shapes, filter settings and effect timings follow zepto8 by Sam Hocevar (https://github.com/samhocevar/zepto8, WTFPL), whose author matched them against PICO-8's own WAV exports. This is a rewrite for MPDJ under MPDJ's licence. PICO-8 is a product of Lexaloffle Games; this engine is not affiliated with it. Use your own carts, or carts whose authors allow it.
