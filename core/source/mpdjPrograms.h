// MPDJ additions (MIT, see NOTICE.md): typed program slots.
//
// MPDJ has one list of 64 program slots, shown as 01-64. Each slot has a type
// (empty, sample, MIDI; later our synths and keygroups) instead of the
// Tracker+'s fixed ranges (samples 1-48, MIDI M01-M16). The pattern still
// stores a program as its slot index, so indices are unchanged.
//
// Step 5a-1: the type lives in strMtProject::programs; the sequencer's type
// functions (isSampleInstrument, isMidiInstrument, instrumentIdxToMidiChannel
// in mtSequencer.cpp) look it up. Projects don't store types yet (5a-2), so
// every project loads with the Tracker+ layout: slots 1-48 hold samples,
// slots 49-64 are MIDI channels 1-16.
#pragma once

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

    // The Tracker+ layout (also the default for a new project)
    constexpr strProgramTable() : slot{} { setLegacyLayout(); }

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

// Slot types for the current project (mtProject.programs)
ProgramType programType(int idx);
const char* programTypeName(ProgramType type);

// After a project file is read: projects without stored types (all of them
// until 5a-2) get the Tracker+ layout
void programsLoadedFromLegacyProject();

// MPDJ_PROGRAMS_TEST=1: checks the type functions against the Tracker+'s
// fixed ranges for every index, prints the table and exits. Returns the
// process exit code, or -1 when the variable isn't set.
int programsSelfTest();

// MPDJ_MIDI_OUT_TRACE=1 (MPC build): print every note the sequencer sends to
// MIDI out, with the Config "notes out" mode, so MIDI programs can be checked
// without a cable (DIN and USB MIDI out come with milestone 10).
void mpdjMidiOutTrace(bool noteOn, uint8_t note, uint8_t velocity, uint8_t channel, uint8_t outMode);
