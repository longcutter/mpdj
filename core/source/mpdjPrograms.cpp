// MPDJ additions (MIT, see NOTICE.md): typed program slots. See mpdjPrograms.h.
#include "mpdjPrograms.h"

#include "mtSequencer.h"
#include "mtStructs.h"

#include <string.h>

extern strMtProject mtProject;

static_assert(kSampleInstrumentsCount == kProgramSlots, "every slot has sample storage");

static bool s_importNotice = false;

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

int programListSlots(bool midiOnly, uint8_t* out)
{
    int n = 0;
    for (int i = 0; i < kProgramSlots; i++)
    {
        if (!midiOnly || mtProject.programs.slot[i].type == kProgramMidi)
            out[n++] = uint8_t(i);
    }
    return n;
}

uint8_t programStep(int actual, int delta, bool midiOnly)
{
    uint8_t list[kProgramSlots];
    int n = programListSlots(midiOnly, list);
    if (n == 0)
        return uint8_t(actual);

    // position of `actual`, or of the first entry above it
    int pos = 0;
    bool found = false;
    for (; pos < n; pos++)
    {
        if (list[pos] >= actual)
        {
            found = list[pos] == actual;
            break;
        }
    }
    if (delta == 0)
        return found ? list[pos] : list[0];
    if (!found && delta > 0)
        delta--; // the entry above `actual` is already one step up
    pos += delta;
    if (pos < 0)
        pos = 0;
    if (pos > n - 1)
        pos = n - 1;
    return list[pos];
}

int programForMidiChannel(int channel)
{
    for (int i = 0; i < kProgramSlots; i++)
    {
        const strProgramSlot& s = mtProject.programs.slot[i];
        if (s.type == kProgramMidi && s.midi.channel == channel)
            return i;
    }
    return -1;
}

int programFromPad(int pad, bool midiOnly)
{
    uint8_t list[kProgramSlots];
    int n = programListSlots(midiOnly, list);
    if (n == 0 || pad < 0)
        return -1;
    return list[pad < n ? pad : n - 1];
}

int programMidiSettings(int idx)
{
    if (programType(idx) != kProgramMidi)
        return 0;
    int ch = mtProject.programs.slot[idx].midi.channel;
    return (ch >= 1 && ch <= 16) ? ch - 1 : 0;
}

void programCellLabel(int idx, char out[4])
{
    int num = idx + 1;
    int i = 0;
    if (programType(idx) == kProgramMidi)
        out[i++] = 'M'; // 3 characters: the pattern view draws it as MIDI
    out[i++] = char('0' + (num / 10) % 10);
    out[i++] = char('0' + num % 10);
    while (i < 4)
        out[i++] = 0;
}

void programsWriteToFile(strMpdjProjectData& out)
{
    memset(&out, 0, sizeof(out));
    memcpy(out.magic, "MPDJ", 4);
    out.version = kMpdjProjectDataVersion;
    for (int i = 0; i < kProgramSlots; i++)
    {
        out.programs[i] = mtProject.programs.slot[i];
        out.programs[i].type = programType(i);
    }
    memcpy(out.trackVolumeHi, mtProject.trackVolumeHi, sizeof(out.trackVolumeHi));
}

// Tracks 9-16's per-track data: from a version 2 block, else defaults
static void tracksReadFromFile(const strMpdjProjectData* in)
{
    for (int t = 0; t < 8; t++)
    {
        uint8_t v = in ? in->trackVolumeHi[t] : DEFAULT_TRACK_VOLUME;
        mtProject.trackVolumeHi[t] = v > TRACK_VOLUME_MAX ? DEFAULT_TRACK_VOLUME : v;
    }
}

bool programsReadFromFile(const strMpdjProjectData& in)
{
    if (memcmp(in.magic, "MPDJ", 4) != 0)
    {
        mtProject.programs.setLegacyLayout();
        tracksReadFromFile(nullptr);
        s_importNotice = true;
        return false;
    }
    tracksReadFromFile(in.version >= 2 ? &in : nullptr);
    for (int i = 0; i < kProgramSlots; i++)
    {
        strProgramSlot s = in.programs[i];
        // Sample vs empty comes from the slot's sample data once it's loaded
        if (s.type != kProgramMidi)
            s = {};
        else if (s.midi.channel < 1 || s.midi.channel > 16)
            s.midi.channel = 1;
        mtProject.programs.slot[i] = s;
    }
    return true;
}

bool programsTakeImportNotice()
{
    bool n = s_importNotice;
    s_importNotice = false;
    return n;
}

#ifdef PLATFORM_MPC
#include <stdio.h>
#include <stdlib.h>

// The Tracker+'s fixed ranges (samples 0-47, M01-M16 = 48-63, synths 64-66)
static bool oldIsSample(int i) { return i < kLegacySampleSlots; }
static bool oldIsMidi(int i)
{
    return i >= kLegacySampleSlots && i < kLegacySampleSlots + kLegacyMidiSlots;
}
static bool oldIsSynth(int i)
{
    return i >= kLegacySampleSlots + kLegacyMidiSlots &&
           i < kLegacySampleSlots + kLegacyMidiSlots + kSynthInstrumentsCount;
}
static int oldChannel(int i)
{
    if (oldIsSample(i))
        return kNoChannel;
    if (oldIsMidi(i))
        return i - kLegacySampleSlots + 1;
    return i - kLegacySampleSlots - kLegacyMidiSlots + 1;
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

    // 1. An imported Tracker+ project matches the Tracker+ ranges: samples
    //    1-48, M01-M16 = 49-64 (synth indices only exist in FEATURE_SYNTH builds)
    strMpdjProjectData none = {};
    check(!programsReadFromFile(none), "file without MPDJ block = Tracker+ project", -1);
    check(programsTakeImportNotice(), "import notice set", -1);
    check(!programsTakeImportNotice(), "import notice shown once", -1);
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

    // 2. The functions follow the table, not the index; every slot has sample storage
    strProgramTable saved = mtProject.programs;
    mtProject.programs.slot[5].type = kProgramMidi; // program 06 -> MIDI channel 3
    mtProject.programs.slot[5].midi.channel = 3;
    mtProject.programs.slot[50].type = kProgramEmpty; // program 51 -> empty
    mtProject.programs.slot[63].midi.channel = 2;     // program 64 -> channel 2
    check(!isSampleInstrument(5) && isMidiInstrument(5), "slot 6 as MIDI", 5);
    check(instrumentIdxToMidiChannel(5) == 3, "slot 6 channel", 5);
    check(!isMidiInstrument(50) && isSampleInstrument(50), "slot 51 empty takes samples", 50);
    check(instrumentIdxToMidiChannel(50) == kNoChannel, "slot 51 no channel", 50);
    check(programType(50) == kProgramEmpty, "slot 51 type", 50);
    check(instrumentIdxToMidiChannel(63) == 2, "slot 64 channel", 63);
    check(programForMidiChannel(3) == 5, "first program on channel 3", 5);
    check(isSampleInstrument(4) && !isMidiInstrument(4), "slot 5 still a sample slot", 4);

    // 3. Stepping: all slots, or MIDI programs only (6, 49, 50, 52..64)
    check(programStep(4, 1, false) == 5 && programStep(63, 1, false) == 63, "step all", 4);
    check(programStep(0, -1, false) == 0, "step all clamps at 01", 0);
    check(programStep(0, 0, true) == 5, "midi: delta 0 from a sample slot = first MIDI", 0);
    check(programStep(5, 1, true) == 48, "midi: 06 -> 49", 5);
    check(programStep(49, 1, true) == 51, "midi: 50 -> 52 (51 is empty)", 49);
    check(programStep(50, 1, true) == 51 && programStep(50, -1, true) == 49, "midi: from 51", 50);
    check(programStep(63, 5, true) == 63, "midi: clamps at 64", 63);

    // 4. Save and load the table (and, version 2, tracks 9-16's volume)
    uint8_t savedVolumes[8];
    memcpy(savedVolumes, mtProject.trackVolumeHi, sizeof(savedVolumes));
    for (int t = 0; t < 8; t++)
        mtProject.trackVolumeHi[t] = 40 + t;
    strMpdjProjectData file;
    programsWriteToFile(file);
    mtProject.programs = strProgramTable{};
    memset(mtProject.trackVolumeHi, 0, sizeof(mtProject.trackVolumeHi));
    check(programsReadFromFile(file), "MPDJ block read back", -1);
    for (int t = 0; t < 8; t++)
        check(mtProject.trackVolumeHi[t] == 40 + t, "track 9-16 volume after reload", t);
    file.version = 1; // a version 1 block (before 5a-4) has no volumes: defaults
    check(programsReadFromFile(file), "version 1 block read", -1);
    for (int t = 0; t < 8; t++)
        check(mtProject.trackVolumeHi[t] == DEFAULT_TRACK_VOLUME, "version 1: default volume", t);
    file.version = kMpdjProjectDataVersion;
    check(programsReadFromFile(file), "MPDJ block read back again", -1);
    memcpy(mtProject.trackVolumeHi, savedVolumes, sizeof(savedVolumes));
    check(!programsTakeImportNotice(), "no import notice for an MPDJ project", -1);
    for (int i = 0; i < kProgramSlots; i++)
    {
        check(mtProject.programs.slot[i].type == (i == 5 || (i >= 48 && i != 50) ? kProgramMidi : kProgramEmpty),
              "type after reload", i);
    }
    check(instrumentIdxToMidiChannel(5) == 3 && instrumentIdxToMidiChannel(63) == 2, "channels after reload", 5);
    mtProject.programs = saved;

    // 5. A new project starts with every slot empty
    strProgramTable fresh;
    for (int i = 0; i < kProgramSlots; i++)
        check(fresh.slot[i].type == kProgramEmpty, "new project table", i);
    mtProject.programs = fresh;
    check(programStep(7, 1, true) == 7, "midi step with no MIDI programs keeps the value", 7);
    mtProject.programs = saved;

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
