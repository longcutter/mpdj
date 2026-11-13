// MPDJ additions (MIT, see NOTICE.md): the pad map (milestone 5, step 5b).
//
// The MPC has 16 pads and banks A-D: 64 pads. Each pad plays a program on a
// destination track, MPC drum-program style. Pads that share a track choke
// each other (one voice per track). The default map is bank A pad N ->
// program N -> track N, bank B pad N -> program 16+N -> track N, and so on:
// banks share tracks. The map is saved in the project's MPDJ block (version 3,
// mpdjPrograms.h); older projects and Tracker+ imports get the default map.
// Pad Assign (step 5d) edits it.
#pragma once

#include <stdint.h>

static constexpr int kPadsPerBank = 16;
static constexpr int kPadBanks = 4; // A-D
static constexpr int kPads = kPadsPerBank * kPadBanks;

static constexpr uint8_t kPadNoProgram = 0xFF; // pad plays nothing
static constexpr int8_t kPadNoteDefault = -1;  // the sample's own pitch

enum PadMode : uint8_t
{
    kPadModeAuto = 0,    // Gate when the program's sample loops, else One shot
    kPadModeOneShot = 1, // release does nothing; the sample plays to its end
    kPadModeGate = 2,    // release ends the note (recording writes an OFF)
    kPadModeCount
};

struct strPadSlot
{
    uint8_t program; // slot 0-63, or kPadNoProgram
    uint8_t track;   // 0-15
    int8_t note;     // kPadNoteDefault or a note 0-127
    uint8_t mode;    // PadMode
};
static_assert(sizeof(strPadSlot) == 4);

struct strPadMap
{
    strPadSlot pad[kPads]; // index = bank * 16 + (pad number - 1)

    constexpr strPadMap() : pad{} { setDefault(); }

    constexpr void setDefault()
    {
        for (int i = 0; i < kPads; i++)
            pad[i] = { uint8_t(i), uint8_t(i % kPadsPerBank), kPadNoteDefault, kPadModeAuto };
    }
};

// Pad index from bank (0-3 = A-D) and pad number 1-16 (1 = bottom left)
constexpr int padIndex(int bank, int padNumber) { return bank * kPadsPerBank + padNumber - 1; }

// The current project's map (mtProject.pads), checked: nullptr for a bad index
const strPadSlot* padSlot(int pad);
// Name like "A01" .. "D16"; out needs 4 bytes
void padName(int pad, char out[4]);
const char* padModeName(uint8_t mode);

// Pad engine (step 5b-3). Press: the pad plays its program on its track with
// `velocity` (1-127); returns nullptr, or a short message for the screen when
// there's nothing to play ("A03: slot 03 empty"). Release ends the note for a
// Gate pad and frees the track for a One shot pad. The caller decides where
// pads mean this (MPC: the Pattern screen without popups, not recording).
const char* mpdjPadPress(int pad, uint8_t velocity);
void mpdjPadRelease(int pad);
// Gate or One shot for the pad as it is now (Auto: Gate for MIDI programs and
// samples that loop or sustain: loop modes, wavetable, granular)
bool padIsGate(int pad);

// Project file: copy the map into / out of the MPDJ block's pad table.
// Invalid entries read back as that pad's default.
void padsWrite(strPadSlot* out);
void padsRead(const strPadSlot* in); // nullptr = default map (older files)

// MPDJ_PADS_TEST=1: checks the default map, the file round trip and older
// files, prints the map and exits. Returns the exit code, or -1 when unset.
int padsSelfTest();
