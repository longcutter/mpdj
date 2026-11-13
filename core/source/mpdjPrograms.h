// MPDJ additions (MIT, see NOTICE.md): typed program slots.
//
// MPDJ has one list of 64 program slots, shown as 01-64. Each slot has a type
// (empty, sample, MIDI; later our synths and keygroups) instead of the
// Tracker+'s fixed ranges (samples 1-48, MIDI M01-M16). The pattern still
// stores a program as its slot index, so indices are unchanged.
//
// The type lives in strMtProject::programs; the sequencer's type functions
// (isSampleInstrument, isMidiInstrument, instrumentIdxToMidiChannel in
// mtSequencer.cpp) look it up (step 5a-1). Every slot has sample storage
// (step 5a-2). The project file stores the table in an MPDJ block after the
// Tracker+ data (strMpdjProjectData). A new project starts with all 64 slots
// empty; a Tracker+ project (no MPDJ block) is imported with its samples in
// slots 1-48 and M01-M16 as MIDI programs in slots 49-64 (channels 1-16).
#pragma once

#include "mpdjPads.h"

#include <stddef.h>
#include <stdint.h>

static constexpr int kProgramSlots = 64;
static constexpr int kLegacySampleSlots = 48; // Tracker+ samples 1-48
static constexpr int kLegacyMidiSlots = 16;   // Tracker+ M01-M16

enum ProgramType : uint8_t
{
    kProgramEmpty = 0,
    kProgramSample = 1,
    kProgramMidi = 2,
};

struct strProgramSlot
{
    // Stored type: kProgramMidi, or kProgramEmpty for a slot that takes
    // samples. Whether such a slot holds a sample comes from its sample data
    // (strInstrument::isActive), so the two can't disagree: use programType().
    uint8_t type;
    struct
    {
        uint8_t channel; // 1-16
        uint8_t port;    // 0 = all outputs (ports come with milestone 10)
    } midi;
    uint8_t reserved;
};
static_assert(sizeof(strProgramSlot) == 4);

struct strProgramTable
{
    strProgramSlot slot[kProgramSlots];

    // A new project: all slots empty
    constexpr strProgramTable() : slot{} {}

    // A Tracker+ project: M01-M16 -> slots 49-64, MIDI channels 1-16
    constexpr void setLegacyLayout()
    {
        for (int i = 0; i < kProgramSlots; i++)
        {
            slot[i] = {};
            if (i >= kLegacySampleSlots && i < kLegacySampleSlots + kLegacyMidiSlots)
            {
                slot[i].type = kProgramMidi;
                slot[i].midi.channel = uint8_t(i - kLegacySampleSlots + 1);
            }
        }
    }
};

// Saved in the project file after the Tracker+ data (strMtProjectRemote::mpdj).
// A Tracker+ project file ends before it, so it reads as zeros (no magic).
struct strMpdjProjectData
{
    char magic[4]; // "MPDJ"
    uint8_t version;
    uint8_t reserved[3];
    strProgramSlot programs[kProgramSlots]; // type = programType() when saved
    // version 2 (step 5a-4): per-track data the Tracker+ part only has for
    // tracks 1-8 (mute and names of 9-16 are its midiTrackMute / midiTrackNames)
    uint8_t trackVolumeHi[8]; // tracks 9-16
    uint8_t reservedTracks[24];
    // version 3 (step 5b-1): the pad map, mpdjPads.h
    strPadSlot pads[kPads];
};
static constexpr size_t kMpdjProjectDataV1Size = 8 + 4 * kProgramSlots; // version 1 ended here
static constexpr size_t kMpdjProjectDataV2Size = kMpdjProjectDataV1Size + 32; // version 2 ended here
static_assert(sizeof(strMpdjProjectData) == kMpdjProjectDataV2Size + 4 * kPads);
static constexpr uint8_t kMpdjProjectDataVersion = 3;

// Slot types for the current project (mtProject.programs)
ProgramType programType(int idx);
const char* programTypeName(ProgramType type);

// Ordered slot lists for pickers and the instrument column: all slots, or only
// MIDI programs (Midi tracks). Returns the count; out needs kProgramSlots.
int programListSlots(bool midiOnly, uint8_t* out);
// Step from `actual` by `delta` through that list, clamped at the ends (no
// wrap). delta 0 = `actual` if it's in the list, else the first entry. With
// an empty list, returns `actual` unchanged.
uint8_t programStep(int actual, int delta, bool midiOnly);
// First MIDI program on `channel` (1-16), or -1
int programForMidiChannel(int channel);
// The pad-th entry of that list (pads pick programs in the picker and the
// instrument column), the last one past the end, or -1 for an empty list
int programFromPad(int pad, bool midiOnly);
// Index into mtProject.values.midiInstrument[] (velocity, CCs, offset) for a
// MIDI program: its channel - 1. Those settings belong to the channel, shared
// by every program on it (5a-3 may move them into the slot). 0 otherwise.
int programMidiSettings(int idx);
// Short label for pattern cells: "07" (sample / empty), "M49" (MIDI)
void programCellLabel(int idx, char out[4]);

// Project file: write the table / read it back. Returns false when the file
// has no MPDJ block (a Tracker+ project): the table gets the Tracker+ layout.
void programsWriteToFile(strMpdjProjectData& out);
bool programsReadFromFile(const strMpdjProjectData& in);

// Set when a Tracker+ project was imported; the UI shows a one-time note and
// clears it (programsTakeImportNotice)
bool programsTakeImportNotice();

// MPDJ_PROGRAMS_TEST=1: checks the type functions against the Tracker+'s
// fixed ranges for every index, prints the table and exits. Returns the
// process exit code, or -1 when the variable isn't set.
int programsSelfTest();

// MPDJ_MIDI_OUT_TRACE=1 (MPC build): print every note the sequencer sends to
// MIDI out, with the Config "notes out" mode, so MIDI programs can be checked
// without a cable (DIN and USB MIDI out come with milestone 10).
void mpdjMidiOutTrace(bool noteOn, uint8_t note, uint8_t velocity, uint8_t channel, uint8_t outMode);
