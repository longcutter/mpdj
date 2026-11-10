"""Write mpdj-test.p8: a small cart that exercises every waveform, effect and filter.

SFX 0       custom instrument "pluck" (used by the song)
SFX 8-15    waveform tour: each of the 8 waveforms, C major arpeggio
SFX 16-22   effect tour on the square wave: slide, vibrato, drop, fade in, fade out, arp fast, arp slow
SFX 23-28   filter tour on the saw: plain, buzz, detune 1, detune 2, reverb 2, dampen 2
SFX 32-39   a short 4-channel song (music patterns 0-1, looping)
"""
import sys

KEY = {n: i for i, n in enumerate("C C# D D# E F F# G G# A A# B".split())}
def k(name, octave):  # PICO-8 naming: C-0 = key 0, A-2 = 440 Hz
    return KEY[name] + 12 * octave

def sfx_line(notes, speed=16, filters=0, loop=(0, 0)):
    """notes: list of up to 32 (key, inst, vol, fx, custom) tuples or None for rest"""
    out = "%02x%02x%02x%02x" % (filters, speed, loop[0], loop[1])
    for i in range(32):
        n = notes[i] if i < len(notes) else None
        if n is None:
            out += "00000"
        else:
            key, inst, vol, fx = n[:4]
            custom = n[4] if len(n) > 4 else False
            out += "%02x%x%x%x" % (key, inst + (8 if custom else 0), vol, fx)
    return out

FILT_BUZZ, DET1, DET2, REV1, REV2, DAMP1, DAMP2 = 4, 8, 16, 24, 48, 72, 144

sfx = ["0" * 168] * 64
def put(i, line):
    sfx[i] = line

# SFX 0: pluck instrument (a decaying saw blip, transposed by the note that plays it)
put(0, sfx_line([(k("C", 2), 2, v, 0) for v in (7, 6, 5, 4, 3, 2, 2, 1, 1)], speed=1))

arp = [k("C", 2), k("E", 2), k("G", 2), k("C", 3)]
for w in range(8):  # waveform tour
    put(8 + w, sfx_line([(arp[i // 2 % 4], w, 5, 0) if i % 2 == 0 else None for i in range(16)], speed=12))

eff = [  # effect tour, square wave
    [(k("C", 2), 3, 5, 0), None, (k("G", 2), 3, 5, 1), None, (k("C", 3), 3, 5, 1), None, (k("C", 2), 3, 5, 1)],
    [(k("A", 2), 3, 5, 2)] * 8,
    [(k("C", 4), 3, 6, 3), None, (k("C", 4), 3, 6, 3), None, (k("G", 3), 3, 6, 3)],
    [(k("C", 3), 3, 6, 4)] * 4,
    [(k("C", 3), 3, 6, 5)] * 4,
    [(k("C", 2), 3, 5, 6), (k("E", 2), 3, 5, 6), (k("G", 2), 3, 5, 6), (k("C", 3), 3, 5, 6)] * 2,
    [(k("C", 2), 3, 5, 7), (k("E", 2), 3, 5, 7), (k("G", 2), 3, 5, 7), (k("C", 3), 3, 5, 7)] * 2,
]
for i, notes in enumerate(eff):
    put(16 + i, sfx_line(notes, speed=16))

line = [(arp[i // 2 % 4], 2, 5, 0) if i % 2 == 0 else None for i in range(16)]
for i, f in enumerate([0, FILT_BUZZ, DET1, DET2, REV2, DAMP2]):  # filter tour, saw
    put(23 + i, sfx_line(line, speed=12, filters=f))

# --- song: A minor, Am F C G, two patterns ---
chords = [("A", [k("A", 1), k("C", 2), k("E", 2), k("A", 2)]),
          ("F", [k("F", 1), k("A", 1), k("C", 2), k("F", 2)]),
          ("C", [k("C", 2), k("E", 2), k("G", 2), k("C", 3)]),
          ("G", [k("G", 1), k("B", 1), k("D", 2), k("G", 2)])]
roots = [k("A", 0), k("F", 0), k("C", 1), k("G", 0)]

def bass():
    n = []
    for c in range(4):
        for s in range(8):
            n.append((roots[c], 0, 6, 0) if s in (0, 3, 6) else (roots[c] + 12, 0, 3, 5) if s == 7 else None)
    return n

def pads():
    n = []
    for c in range(4):
        n += [(x, 3, 2, 6) for x in chords[c][1]] * 2
    return n

def drums(fill):
    n = []
    for s in range(32):
        if s % 8 == 0 or (fill and s in (27, 30)): n.append((k("C", 2), 6, 7, 3))   # kick: noise with drop
        elif s % 8 == 4: n.append((k("C", 4), 6, 5, 5))                            # snare: noise, fade out
        elif s % 2 == 1: n.append((k("C", 5), 6, 2, 5))                            # hat
        else: n.append(None)
    return n

mel1 = ["E3", None, "C3", None, "A2", None, "C3", "D3", "C3", None, "A2", None, "F2", None, None, None,
        "G2", None, "C3", None, "E3", None, "G3", "E3", "D3", None, "B2", None, "G2", None, None, None]
mel2 = ["A3", None, "G3", "E3", None, "C3", None, "E3", "F3", None, "E3", "C3", None, "A2", None, None,
        "G3", None, "E3", "C3", None, "E3", "G3", None, "B3", None, "A3", "G3", None, "D3", None, None]

def melody(m, inst, fx_long, custom=False):
    n = []
    for i, x in enumerate(m):
        if x is None:
            n.append(None)
        else:
            nm, octv = x[:-1], int(x[-1])
            long_note = i + 1 < len(m) and m[i + 1] is None
            fx = fx_long if long_note else (1 if i > 0 and m[i - 1] is not None else 0)
            n.append((k(nm, octv), inst, 5, fx, custom))
    return n

SP = 14
put(32, sfx_line(bass(), SP, filters=DAMP1))
put(33, sfx_line(pads(), SP))
put(34, sfx_line(melody(mel1, 7, 2), SP, filters=REV1))        # phaser lead, vibrato on long notes
put(35, sfx_line(drums(False), SP))
put(36, sfx_line(bass(), SP, filters=DAMP1))
put(37, sfx_line(pads(), SP, filters=DET1))
put(38, sfx_line(melody(mel2, 0, 0, custom=True), SP, filters=REV2))  # custom pluck instrument
put(39, sfx_line(drums(True), SP))

music = ["01 20212223", "02 24252627"] + ["00 41424344"] * 62

with open(sys.argv[1] if len(sys.argv) > 1 else "mpdj-test.p8", "w") as f:
    f.write("pico-8 cartridge // http://www.pico-8.com\nversion 41\n__lua__\n-- mpdj sound test\n")
    f.write("__sfx__\n" + "\n".join(sfx) + "\n")
    f.write("__music__\n" + "\n".join(music) + "\n")
