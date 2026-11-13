// MPDJ additions (MIT, see NOTICE.md): the pad map. See mpdjPads.h.
#include "mpdjPads.h"

#include "mpdjPrograms.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "util_math.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern strMtProject mtProject;
extern Sequencer sequencer;

// Note source for the sequencer's held-note bookkeeping: Tracker pads use
// 0-47, MIDI keys note + 100; MPDJ pads 200-263
static constexpr int16_t kPadSourceBase = 200;

namespace
{
struct Held
{
    int8_t track = -1; // -1 = not held by the engine
    bool gate = false;
};
Held s_held[kPads];
}

bool padIsGate(int pad)
{
    const strPadSlot* p = padSlot(pad);
    if (!p || p->program == kPadNoProgram)
        return false;
    if (programType(p->program) == kProgramMidi)
        return true; // a MIDI note always needs its note off
    if (p->mode == kPadModeGate)
        return true;
    if (p->mode == kPadModeOneShot)
        return false;
    switch (mtProject.instrument[p->program].playMode)
    {
    case playModeLoopForward:
    case playModeLoopBackward:
    case playModePingpong:
    case playModeWavetable:
    case playModeGranular: return true;
    default: return false; // one shot, slices
    }
}

const char* mpdjPadPress(int pad, uint8_t velocity)
{
    static char msg[32];
    const strPadSlot* p = padSlot(pad);
    if (!p)
        return nullptr;
    char name[4];
    padName(pad, name);
    if (p->program == kPadNoProgram)
    {
        snprintf(msg, sizeof msg, "%s: no program", name);
        return msg;
    }
    if (programType(p->program) == kProgramEmpty)
    {
        snprintf(msg, sizeof msg, "%s: slot %02d empty", name, p->program + 1);
        return msg;
    }
    if (s_held[pad].track >= 0)
        mpdjPadRelease(pad); // a second press before the release (shouldn't happen)

    uint8_t note = p->note < 0 ? (uint8_t)Sequencer::STEP_NOTE_DEFAULT : (uint8_t)p->note;
    uint8_t velo = padVelocity(p->program, velocity ? velocity : 1);
    s_held[pad] = { (int8_t)p->track, padIsGate(pad) };
    sequencer.mpdjPadNoteOn(p->track, p->program, note, velo, kPadSourceBase + pad);

    // REC + playing (5b-4a): write the hit on the pad's track
    Sequencer::MpdjRecWrite w;
    if (sequencer.mpdjPadRecWrite(p->track, p->program, note, velo, &w))
    {
#ifdef PLATFORM_MPC
        static const bool trace = getenv("MPDJ_PAD_REC_TRACE") != nullptr;
        if (trace)
            printf("pad rec: %s vel %u -> track %d row %d: %s %02d vol %d micro %d%s\n", name,
                   velocity, p->track + 1, w.row, mtNotes[note], p->program + 1, w.volume,
                   w.micromove, w.skipNext ? " (ahead of the playhead)" : "");
#endif
    }
#ifdef PLATFORM_MPC
    else if (sequencer.isRec() && getenv("MPDJ_PAD_REC_TRACE"))
    {
        printf("pad rec: %s -> track %d not written (%s)\n", name, p->track + 1,
               !sequencer.isRecArmedOnTrack(p->track) ? "track not armed" : "counting in");
    }
#endif
    return nullptr;
}

uint8_t padVelocity(uint8_t program, uint8_t velocity)
{
    // 5b-4a: linear, a full hit = the program's own volume (Config curve and
    // top come with 5b-4d)
    int v = mapClamped<int>(velocity, 0, 127, 0, sequencer.getInstrumentVelo(program));
    return (uint8_t)(v > 127 ? 127 : v);
}

void mpdjPadRelease(int pad)
{
    if (pad < 0 || pad >= kPads || s_held[pad].track < 0)
        return;
    sequencer.mpdjPadNoteOff((uint8_t)s_held[pad].track, kPadSourceBase + pad, s_held[pad].gate);
    s_held[pad] = {};
}

const strPadSlot* padSlot(int pad)
{
    if (pad < 0 || pad >= kPads)
        return nullptr;
    return &mtProject.pads.pad[pad];
}

void padName(int pad, char out[4])
{
    if (pad < 0 || pad >= kPads)
    {
        memcpy(out, "---", 4);
        return;
    }
    int n = pad % kPadsPerBank + 1;
    out[0] = char('A' + pad / kPadsPerBank);
    out[1] = char('0' + n / 10);
    out[2] = char('0' + n % 10);
    out[3] = 0;
}

const char* padModeName(uint8_t mode)
{
    switch (mode)
    {
    case kPadModeAuto: return "Auto";
    case kPadModeOneShot: return "One shot";
    case kPadModeGate: return "Gate";
    default: return "?";
    }
}

static bool padValid(const strPadSlot& p)
{
    return (p.program < kProgramSlots || p.program == kPadNoProgram) && p.track < kAllTracks &&
           p.note >= kPadNoteDefault && p.mode < kPadModeCount;
}

void padsWrite(strPadSlot* out)
{
    memcpy(out, mtProject.pads.pad, sizeof(mtProject.pads.pad));
}

void padsRead(const strPadSlot* in)
{
    strPadMap def;
    for (int i = 0; i < kPads; i++)
        mtProject.pads.pad[i] = (in && padValid(in[i])) ? in[i] : def.pad[i];
}

#ifdef PLATFORM_MPC
#include <stdio.h>
#include <stdlib.h>

int padsSelfTest()
{
    const char* v = getenv("MPDJ_PADS_TEST");
    if (!v || !*v || *v == '0')
        return -1;

    int failures = 0;
    auto check = [&](bool ok, const char* what, int idx) {
        if (!ok)
        {
            printf("pads test: FAIL %s, pad %d\n", what, idx);
            failures++;
        }
    };

    strPadMap savedMap = mtProject.pads;
    strProgramTable savedPrograms = mtProject.programs;
    uint8_t savedVolumes[8];
    memcpy(savedVolumes, mtProject.trackVolumeHi, sizeof(savedVolumes));

    // 1. The default map: bank A pad N -> program N -> track N, bank B pad N ->
    //    program 16+N -> track N ...; sample pitch; Auto
    strPadMap def;
    for (int b = 0; b < kPadBanks; b++)
        for (int n = 1; n <= kPadsPerBank; n++)
        {
            int i = padIndex(b, n);
            check(def.pad[i].program == b * 16 + n - 1, "default program", i);
            check(def.pad[i].track == n - 1, "default track (banks share tracks)", i);
            check(def.pad[i].note == kPadNoteDefault, "default note", i);
            check(def.pad[i].mode == kPadModeAuto, "default mode", i);
        }
    char name[4];
    padName(padIndex(0, 1), name);
    check(strcmp(name, "A01") == 0, "name A01", 0);
    padName(padIndex(3, 16), name);
    check(strcmp(name, "D16") == 0, "name D16", 63);
    check(padSlot(-1) == nullptr && padSlot(kPads) == nullptr, "padSlot range", -1);

    // 2. Round trip through the MPDJ block (version 3)
    mtProject.pads.pad[0] = { 5, 2, 48, kPadModeGate };          // A01: program 06, track 3, C-3
    mtProject.pads.pad[17] = { kPadNoProgram, 15, -1, kPadModeOneShot }; // B02: nothing, track 16
    mtProject.pads.pad[63] = { 63, 0, 127, kPadModeAuto };       // D16 -> track 1
    strMpdjProjectData file;
    programsWriteToFile(file);
    check(file.version == 3, "block version 3", -1);
    mtProject.pads = strPadMap{};
    programsReadFromFile(file);
    check(mtProject.pads.pad[0].program == 5 &&
            mtProject.pads.pad[0].track == 2 && mtProject.pads.pad[0].note == 48 &&
            mtProject.pads.pad[0].mode == kPadModeGate,
          "A01 after reload", 0);
    check(mtProject.pads.pad[17].program == kPadNoProgram && mtProject.pads.pad[17].track == 15 &&
            mtProject.pads.pad[17].mode == kPadModeOneShot,
          "B02 after reload", 17);
    check(mtProject.pads.pad[63].track == 0 && mtProject.pads.pad[63].note == 127, "D16 after reload", 63);
    check(memcmp(&mtProject.pads.pad[1], &def.pad[1], 4) == 0, "untouched pad after reload", 1);

    // 3. Bad entries read back as that pad's default
    file.pads[3] = { 64, 2, -1, 0 };  // program out of range
    file.pads[4] = { 4, 16, -1, 0 };  // track out of range
    file.pads[5] = { 5, 5, -2, 0 };   // note
    file.pads[6] = { 6, 6, -1, 3 };   // mode
    programsReadFromFile(file);
    for (int i = 3; i <= 6; i++)
        check(memcmp(&mtProject.pads.pad[i], &def.pad[i], 4) == 0, "bad entry -> default", i);

    // 4. Older MPDJ blocks (version 1, 2) and Tracker+ projects: the default map
    for (uint8_t ver = 1; ver <= 2; ver++)
    {
        file.version = ver;
        mtProject.pads.pad[9].track = 12;
        programsReadFromFile(file);
        for (int i = 0; i < kPads; i++)
            check(memcmp(&mtProject.pads.pad[i], &def.pad[i], 4) == 0,
                  ver == 1 ? "version 1 block -> default map" : "version 2 block -> default map", i);
    }
    strMpdjProjectData none = {};
    mtProject.pads.pad[9].track = 12;
    programsReadFromFile(none);
    programsTakeImportNotice();
    for (int i = 0; i < kPads; i++)
        check(memcmp(&mtProject.pads.pad[i], &def.pad[i], 4) == 0, "Tracker+ project -> default map", i);

    mtProject.pads = savedMap;
    mtProject.programs = savedPrograms;
    memcpy(mtProject.trackVolumeHi, savedVolumes, sizeof(savedVolumes));

    printf("pad map (pad: program -> track):\n");
    for (int i = 0; i < kPads; i++)
    {
        const strPadSlot& p = mtProject.pads.pad[i];
        padName(i, name);
        if (p.program == kPadNoProgram)
            printf("  %s: -- -> %2d", name, p.track + 1);
        else
            printf("  %s: %02d -> %2d", name, p.program + 1, p.track + 1);
        if (i % 8 == 7)
            printf("\n");
    }
    printf("pads test: %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
#else
int padsSelfTest() { return -1; }
#endif
