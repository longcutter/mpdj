// MPDJ additions (MIT, see NOTICE.md): the pad map. See mpdjPads.h.
#include "mpdjPads.h"

#include "mpdjPrograms.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "util_math.h"
#include "mtConfig.h"

#include <math.h>
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
    bool recorded = false; // the press was written in live REC (5b-4a)
};
Held s_held[kPads];
// The pad whose hit was written last on each track (5b-4b): only that pad's
// release writes an OFF there (a later hit on the track ends the note anyway)
int8_t s_lastRecPad[kAllTracks] = { -1, -1, -1, -1, -1, -1, -1, -1,
                                    -1, -1, -1, -1, -1, -1, -1, -1 };

bool recTrace()
{
#ifdef PLATFORM_MPC
    static const bool trace = getenv("MPDJ_PAD_REC_TRACE") != nullptr;
    return trace;
#else
    return false;
#endif
}
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
    if (sequencer.mpdjPadRecWrite(p->track, p->program, note, velo, padVelocityToVolume(), &w))
    {
        s_held[pad].recorded = true;
        s_lastRecPad[p->track] = (int8_t)pad;
#ifdef PLATFORM_MPC
        if (recTrace())
            printf("pad rec: %s vel %u -> track %d row %d: %s %02d vol %d micro %d%s\n", name,
                   velocity, p->track + 1, w.row, mtNotes[note], p->program + 1, w.volume,
                   w.micromove, w.skipNext ? " (ahead of the playhead)" : "");
#endif
    }
#ifdef PLATFORM_MPC
    else if (sequencer.isRec() && recTrace())
    {
        printf("pad rec: %s -> track %d not written (%s)\n", name, p->track + 1,
               !sequencer.isRecArmedOnTrack(p->track) ? "track not armed" : "counting in");
    }
#endif
    return nullptr;
}

int mpdjPadStepWrite(int pad, int row, uint8_t velocity)
{
    const strPadSlot* p = padSlot(pad);
    if (!p || p->program == kPadNoProgram || programType(p->program) == kProgramEmpty)
        return -1;
    uint8_t note = p->note < 0 ? (uint8_t)Sequencer::STEP_NOTE_DEFAULT : (uint8_t)p->note;
    uint8_t velo = padVelocity(p->program, velocity ? velocity : 1);
    bool writeVol = padVelocityToVolume();
    int8_t vol = sequencer.mpdjPadStepWrite(p->track, (uint8_t)row, p->program, note, velo, writeVol);
    if (vol < 0)
        return -1;
    if (!writeVol)
        vol = -1;
#ifdef PLATFORM_MPC
    if (recTrace())
    {
        char name[4];
        padName(pad, name);
        printf("pad step: %s vel %u -> track %d row %d: %s %02d vol %d\n", name, velocity,
               p->track + 1, row, mtNotes[note], p->program + 1, vol);
    }
#endif
    return p->track;
}

uint8_t padVelocityScale(uint8_t velocity, uint8_t programVelo, uint8_t curve, int topPercent)
{
    // curve: Soft x^0.5, Normal x, Hard x^2 (x = velocity / 127); a full hit
    // = topPercent of the program's velocity (its volume), capped at 127
    float x = velocity > 127 ? 1.0f : velocity / 127.0f;
    if (curve == 0)
        x = sqrtf(x);
    else if (curve == 2)
        x = x * x;
    float top = programVelo * (topPercent / 100.0f);
    int v = (int)(x * top + 0.5f);
    if (velocity > 0 && v < 1)
        v = 1;
    return (uint8_t)(v > 127 ? 127 : v);
}

uint8_t padVelocity(uint8_t program, uint8_t velocity)
{
#ifdef PLATFORM_MPC
    uint8_t curve = mtConfig.general.padCurve;
    int top = 10 + 5 * mtConfig.general.padTop;
#else
    uint8_t curve = 1;
    int top = 100;
#endif
    return padVelocityScale(velocity, sequencer.getInstrumentVelo(program), curve, top);
}

bool padVelocityToVolume()
{
#ifdef PLATFORM_MPC
    return mtConfig.general.padVelToVolume != 0;
#else
    return true;
#endif
}

void mpdjPadRelease(int pad)
{
    if (pad < 0 || pad >= kPads || s_held[pad].track < 0)
        return;
    uint8_t track = (uint8_t)s_held[pad].track;
    sequencer.mpdjPadNoteOff(track, kPadSourceBase + pad, s_held[pad].gate);

    // REC + playing, Gate pad (5b-4b): the release writes an OFF
    if (s_held[pad].gate && s_held[pad].recorded && s_lastRecPad[track] == pad)
    {
        bool hadNote = false;
        int row = sequencer.mpdjPadRecOff(track, &hadNote);
#ifdef PLATFORM_MPC
        if (recTrace())
        {
            char name[4];
            padName(pad, name);
            if (row >= 0)
                printf("pad rec: %s release -> track %d row %d: OFF\n", name, track + 1, row);
            else
                printf("pad rec: %s release -> track %d: no OFF (%s)\n", name, track + 1,
                       hadNote ? "a note is on that row" : "not recording");
        }
#endif
    }
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

    // velocity scaling (5b-4d): a program at volume 50 (velocity 63)
    {
        const uint8_t pv = 63;
        check(padVelocityScale(127, pv, 1, 100) == pv, "full hit at 100% = program velocity", 0);
        check(padVelocityScale(127, pv, 1, 50) == 32, "full hit at 50%", 0);
        check(padVelocityScale(127, pv, 1, 200) == 126, "full hit at 200%", 0);
        check(padVelocityScale(127, 127, 1, 200) == 127, "200% capped at 127", 0);
        check(padVelocityScale(1, pv, 2, 10) == 1, "lightest hit stays audible", 0);
        for (int c = 0; c < 3; c++)
            for (int v = 1; v < 127; v++)
                check(padVelocityScale(v, pv, c, 100) <= padVelocityScale(v + 1, pv, c, 100),
                      "curve rises with velocity", c * 1000 + v);
        for (int v = 1; v < 127; v++)
            check(padVelocityScale(v, pv, 0, 100) >= padVelocityScale(v, pv, 1, 100) &&
                    padVelocityScale(v, pv, 1, 100) >= padVelocityScale(v, pv, 2, 100),
                  "Soft >= Normal >= Hard", v);
        printf("pad velocity -> volume (program volume 50, full hit 100%%):\n  vel   soft normal hard\n");
        for (int v : { 16, 32, 64, 96, 127 })
            printf("  %3d   %4d %6d %4d\n", v, sequencer.mpdjVelocityToVolume(padVelocityScale(v, pv, 0, 100)),
                   sequencer.mpdjVelocityToVolume(padVelocityScale(v, pv, 1, 100)),
                   sequencer.mpdjVelocityToVolume(padVelocityScale(v, pv, 2, 100)));
    }

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
