// MPDJ additions (MIT, see NOTICE.md): the pad map (milestone 5, step 5b).
//
// The MPC has 16 pads and banks A-D: 64 pads. Each pad plays a program on a
// destination track, MPC drum-program style. Pads that share a track choke
// each other (one voice per track). The default map is bank A pad N ->
// program N -> track N, bank B pad N -> program 16+N -> track N, and so on:
// banks share tracks. The map is saved in the project's MPDJ block (version 3,
// mpdjPrograms.h); older projects and Tracker+ imports get the default map.
// Pad Assign (step 5d) edits it.
//
// Choke groups (step 5b-5): pads in the same group 1-31 cut each other across
// tracks (open / closed hats). The group lives in the mode byte's bits 2-6
// (bits 0-1 = the play mode), so files without groups read as group 0 = none.
// New projects put the MPDJ Kit's hats (A03, A04, A12) in group 1.
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
    uint8_t mode;    // bits 0-1 PadMode, bits 2-6 choke group (0 = none), bit 7 = 0
};
static_assert(sizeof(strPadSlot) == 4);

static constexpr uint8_t kPadModeMask = 0x03;
static constexpr int kPadGroupShift = 2;
static constexpr int kPadGroups = 31; // 1-31; 0 = no group

constexpr PadMode padModeOf(uint8_t modeByte) { return PadMode(modeByte & kPadModeMask); }
constexpr uint8_t padGroupOf(uint8_t modeByte) { return uint8_t((modeByte >> kPadGroupShift) & 0x1F); }
constexpr uint8_t padModeByte(PadMode mode, uint8_t group)
{
    return uint8_t((mode & kPadModeMask) | ((group > kPadGroups ? 0 : group) << kPadGroupShift));
}

// The MPDJ Kit's hats (tools/kitgen): Closed Hat, Open Hat, Pedal Hat
static constexpr int kDefaultHatPads[] = { 2, 3, 11 }; // A03, A04, A12
static constexpr uint8_t kDefaultHatGroup = 1;

struct strPadMap
{
    strPadSlot pad[kPads]; // index = bank * 16 + (pad number - 1)

    constexpr strPadMap() : pad{} { setDefault(); }

    // New projects: hats in group 1. withGroups = false: the plain map that
    // older files and invalid entries get (existing projects keep no groups)
    constexpr void setDefault(bool withGroups = true)
    {
        for (int i = 0; i < kPads; i++)
            pad[i] = { uint8_t(i), uint8_t(i % kPadsPerBank), kPadNoteDefault, kPadModeAuto };
        if (withGroups)
            for (int i : kDefaultHatPads)
                pad[i].mode = padModeByte(kPadModeAuto, kDefaultHatGroup);
    }
};

// Pad index from bank (0-3 = A-D) and pad number 1-16 (1 = bottom left)
constexpr int padIndex(int bank, int padNumber) { return bank * kPadsPerBank + padNumber - 1; }

// The current project's map (mtProject.pads), checked: nullptr for a bad index
const strPadSlot* padSlot(int pad);
// Name like "A01" .. "D16"; out needs 4 bytes
void padName(int pad, char out[4]);
const char* padModeName(uint8_t mode); // takes the mode byte (group bits ignored)

// Choke groups (5b-5a): a pad's group 0-31 (0 = none, also for a bad index)
uint8_t padGroup(int pad);
void setPadGroup(int pad, uint8_t group); // keeps the play mode; > 31 = none
// The group a (program, track) plays in, for notes that don't come from a
// pad (pattern playback): the first pad in bank order A01 -> D16 with that
// program and track decides; 0 = none
uint8_t padGroupFor(uint8_t program, uint8_t track);

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
// A pad's velocity 1-127 scaled for its program: a full hit = the program's
// own volume (velocity setting for MIDI). The pad plays and records this.
uint8_t padVelocity(uint8_t program, uint8_t velocity);
// The scaling behind it (Config: curve 0 Soft / 1 Normal / 2 Hard, top %)
uint8_t padVelocityScale(uint8_t velocity, uint8_t programVelo, uint8_t curve, int topPercent);
// Config "Pad vel to volume": recording writes the velocity into the volume column
bool padVelocityToVolume();
// Edit mode (5b-4c): write the pad's hit at a row of its own track (the pad
// was just pressed through mpdjPadPress). Returns the track, -1 = nothing.
int mpdjPadStepWrite(int pad, int row, uint8_t velocity);

// Project file: copy the map into / out of the MPDJ block's pad table.
// Invalid entries read back as that pad's default.
void padsWrite(strPadSlot* out);
void padsRead(const strPadSlot* in); // nullptr = default map (older files)

// MPDJ_PADS_TEST=1: checks the default map, the file round trip and older
// files, prints the map and exits. Returns the exit code, or -1 when unset.
int padsSelfTest();
