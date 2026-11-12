// MPDJ additions (MIT, see NOTICE.md): typed program slots. See mpdjPrograms.h.
#include "mpdjPrograms.h"

#include "mtSequencer.h"
#include "mtStructs.h"

extern strMtProject mtProject;

static_assert(kProgramSlots == kSampleInstrumentsCount + kMidiInstrumentsCount,
              "5a-1 keeps the Tracker+ index ranges; 64 sample slots come with 5a-2");

ProgramType programType(int idx)
{
    if (idx < 0 || idx >= kProgramSlots)
        return kProgramEmpty;
    if (mtProject.programs.slot[idx].type == kProgramMidi)
        return kProgramMidi;
    if (idx < kSampleInstrumentsCount && mtProject.instrument[idx].isActive)
        return kProgramSample;
    return kProgramEmpty;
}

const char* programTypeName(ProgramType type)
{
    switch (type)
    {
    case kProgramSample: return "sample";
    case kProgramMidi: return "MIDI";
    default: return "empty";
    }
}

void programsLoadedFromLegacyProject() { mtProject.programs.setLegacyLayout(); }

#ifdef PLATFORM_MPC
#include <stdio.h>
#include <stdlib.h>

// The Tracker+'s fixed ranges, as the five functions computed them before 5a-1
static bool oldIsSample(int i) { return i < kSampleInstrumentsCount; }
static bool oldIsMidi(int i)
{
    return i >= kSampleInstrumentsCount && i < kSampleInstrumentsCount + kMidiInstrumentsCount;
}
static bool oldIsSynth(int i)
{
    return i >= kSampleInstrumentsCount + kMidiInstrumentsCount &&
           i < kSampleInstrumentsCount + kMidiInstrumentsCount + kSynthInstrumentsCount;
}
static int oldChannel(int i)
{
    if (oldIsSample(i))
        return kNoChannel;
    if (oldIsMidi(i))
        return i - kSampleInstrumentsCount + 1;
    return i - kSampleInstrumentsCount - kMidiInstrumentsCount + 1;
}

void mpdjMidiOutTrace(bool noteOn, uint8_t note, uint8_t velocity, uint8_t channel, uint8_t outMode)
{
    static const bool on = [] {
        const char* v = getenv("MPDJ_MIDI_OUT_TRACE");
        return v && *v && *v != '0';
    }();
    static const char* const kModes[] = { "off", "usb", "din", "usb+din" };
    if (on)
        printf("midi out: %s ch %u note %u vel %u (notes out: %s)\n",
               noteOn ? "note on " : "note off",
               channel,
               note,
               velocity,
               outMode < 4 ? kModes[outMode] : "?");
}

int programsSelfTest()
{
    const char* v = getenv("MPDJ_PROGRAMS_TEST");
    if (!v || !*v || *v == '0')
        return -1;

    int failures = 0;
    auto check = [&](bool ok, const char* what, int idx) {
        if (!ok)
        {
            printf("programs test: FAIL %s, index %d\n", what, idx);
            failures++;
        }
    };

    // 1. A loaded older project matches the Tracker+ ranges for every index
    //    (synth indices only exist in FEATURE_SYNTH builds)
    programsLoadedFromLegacyProject();
    for (int i = 0; i < kTotalInstrumentsCount; i++)
    {
        check(isSampleInstrument(i) == oldIsSample(i), "legacy isSampleInstrument", i);
        check(isMidiInstrument(i) == oldIsMidi(i), "legacy isMidiInstrument", i);
        check(isSynthInstrument(i) == oldIsSynth(i), "legacy isSynthInstrument", i);
        check(instrumentIdxToMidiChannel(i) == oldChannel(i), "legacy MIDI channel", i);
    }
    for (int i = 0; i < kProgramSlots; i++)
    {
        bool midi = programType(i) == kProgramMidi;
        check(midi == (i >= 48), "legacy type", i);
        if (midi)
            check(mtProject.programs.slot[i].midi.channel == i - 47, "legacy channel 49-64 = 1-16", i);
    }

    // 2. The functions follow the table, not the index
    strProgramTable saved = mtProject.programs;
    mtProject.programs.slot[5].type = kProgramMidi; // program 06 -> MIDI channel 3
    mtProject.programs.slot[5].midi.channel = 3;
    mtProject.programs.slot[50].type = kProgramEmpty; // program 51 -> empty
    mtProject.programs.slot[63].midi.channel = 2;     // program 64 -> channel 2
    check(!isSampleInstrument(5) && isMidiInstrument(5), "slot 6 as MIDI", 5);
    check(instrumentIdxToMidiChannel(5) == 3, "slot 6 channel", 5);
    check(!isMidiInstrument(50) && !isSampleInstrument(50), "slot 51 empty (no sample storage yet)", 50);
    check(instrumentIdxToMidiChannel(50) == kNoChannel, "slot 51 no channel", 50);
    check(programType(50) == kProgramEmpty, "slot 51 type", 50);
    check(instrumentIdxToMidiChannel(63) == 2, "slot 64 channel", 63);
    check(isSampleInstrument(4) && !isMidiInstrument(4), "slot 5 still a sample slot", 4);
    mtProject.programs = saved;

    // 3. The table a fresh project starts with is the legacy layout
    strProgramTable fresh;
    for (int i = 0; i < kProgramSlots; i++)
    {
        check(fresh.slot[i].type == saved.slot[i].type &&
                fresh.slot[i].midi.channel == saved.slot[i].midi.channel,
              "default table", i);
    }

    printf("programs table (program: type, MIDI channel):\n");
    for (int i = 0; i < kProgramSlots; i++)
    {
        ProgramType t = programType(i);
        if (t == kProgramMidi)
            printf("  %02d: MIDI %2d", i + 1, mtProject.programs.slot[i].midi.channel);
        else
            printf("  %02d: %-7s", i + 1, programTypeName(t));
        if (i % 8 == 7)
            printf("\n");
    }
    printf("programs test: %s (%d failure%s, %d indices checked)\n",
           failures ? "FAILED" : "passed",
           failures,
           failures == 1 ? "" : "s",
           int(kTotalInstrumentsCount));
    return failures ? 1 : 0;
}
#else
void mpdjMidiOutTrace(bool, uint8_t, uint8_t, uint8_t, uint8_t) {}
int programsSelfTest() { return -1; }
#endif
