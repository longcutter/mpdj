// MPDJ additions (MIT, see NOTICE.md): the pad map. See mpdjPads.h.
#include "mpdjPads.h"

#include "mpdjPrograms.h"
#include "mtSequencer.h"
#include "mtAudioEngine.h"
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
    bool cut = false;      // Gate + cut on release (5b-6a)
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
    if (padModeOf(p->mode) == kPadModeGate)
        return true;
    if (padModeOf(p->mode) == kPadModeOneShot)
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
    s_held[pad] = { (int8_t)p->track, padIsGate(pad), padIsGate(pad) && padCutOf(p->mode) };

    // choke groups (5b-5c): the pad's own group cuts the rest of the group,
    // also when this track is muted
    sequencer.mpdjChoke(p->track, padGroupOf(p->mode), name);
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
    sequencer.mpdjPadNoteOff(track, kPadSourceBase + pad, s_held[pad].gate, s_held[pad].cut);
#ifdef PLATFORM_MPC
    // MPDJ_CHOKE_TEST=1 (5b-5b): a pad's release chokes its own track, to hear
    // the fast cut before choke groups use it
    static int chokeTest = -1;
    if (chokeTest < 0)
    {
        const char* v = getenv("MPDJ_CHOKE_TEST");
        chokeTest = (v && *v && *v != '0') ? 1 : 0;
    }
    if (chokeTest)
    {
        engine.chokeTrack(track);
        printf("choke test: track %d cut\n", track + 1);
    }
#endif

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
    switch (padModeOf(mode))
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
           p.note >= kPadNoteDefault && padModeOf(p.mode) < kPadModeCount;
}

uint8_t padGroup(int pad)
{
    const strPadSlot* p = padSlot(pad);
    return p ? padGroupOf(p->mode) : 0;
}

void setPadGroup(int pad, uint8_t group)
{
    if (pad < 0 || pad >= kPads)
        return;
    strPadSlot& p = mtProject.pads.pad[pad];
    p.mode = padModeByte(padModeOf(p.mode), group, padCutOf(p.mode));
}

bool padCutOnRelease(int pad)
{
    const strPadSlot* p = padSlot(pad);
    return p && padCutOf(p->mode);
}

void setPadCutOnRelease(int pad, bool cut)
{
    if (pad < 0 || pad >= kPads)
        return;
    strPadSlot& p = mtProject.pads.pad[pad];
    p.mode = padModeByte(padModeOf(p.mode), padGroupOf(p.mode), cut);
}

uint8_t padGroupFor(uint8_t program, uint8_t track)
{
    for (int i = 0; i < kPads; i++)
    {
        const strPadSlot& p = mtProject.pads.pad[i];
        if (p.program == program && p.track == track)
            return padGroupOf(p.mode);
    }
    return 0;
}

void padsWrite(strPadSlot* out)
{
    memcpy(out, mtProject.pads.pad, sizeof(mtProject.pads.pad));
}

void padsRead(const strPadSlot* in)
{
    strPadMap def;
    def.setDefault(false); // existing projects keep no groups
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
    //    program 16+N -> track N ...; sample pitch; Auto. New projects: the
    //    kit's hats (A03, A04, A12) in choke group 1, every other pad none
    strPadMap def;
    strPadMap plain;
    plain.setDefault(false);
    for (int b = 0; b < kPadBanks; b++)
        for (int n = 1; n <= kPadsPerBank; n++)
        {
            int i = padIndex(b, n);
            check(def.pad[i].program == b * 16 + n - 1, "default program", i);
            check(def.pad[i].track == n - 1, "default track (banks share tracks)", i);
            check(def.pad[i].note == kPadNoteDefault, "default note", i);
            check(padModeOf(def.pad[i].mode) == kPadModeAuto, "default mode", i);
            bool hat = b == 0 && (n == 3 || n == 4 || n == 12);
            check(padGroupOf(def.pad[i].mode) == (hat ? 1 : 0), "default group (hats in 1)", i);
            check(padCutOf(def.pad[i].mode) == hat, "default cut on release (hats)", i);
            check(plain.pad[i].mode == kPadModeAuto, "plain default: no group", i);
        }
    char name[4];
    padName(padIndex(0, 1), name);
    check(strcmp(name, "A01") == 0, "name A01", 0);
    padName(padIndex(3, 16), name);
    check(strcmp(name, "D16") == 0, "name D16", 63);
    check(padSlot(-1) == nullptr && padSlot(kPads) == nullptr, "padSlot range", -1);

    // 1b. Group bits don't change the play mode
    mtProject.pads = def;
    check(padGroup(2) == 1 && padGroup(3) == 1 && padGroup(11) == 1 && padGroup(0) == 0,
          "padGroup on the default map", -1);
    check(padGroup(-1) == 0 && padGroup(kPads) == 0, "padGroup range", -1);
    for (int m = 0; m < kPadModeCount; m++)
        for (int g : { 0, 1, 17, 31 })
        {
            mtProject.pads.pad[40].mode = padModeByte(PadMode(m), (uint8_t)g);
            check(padModeOf(mtProject.pads.pad[40].mode) == m && padGroup(40) == g, "mode and group apart",
                  m * 100 + g);
            check(strcmp(padModeName(mtProject.pads.pad[40].mode), padModeName((uint8_t)m)) == 0,
                  "mode name ignores the group", m * 100 + g);
        }
    mtProject.pads.pad[40].mode = padModeByte(kPadModeGate, 0);
    setPadGroup(40, 9);
    check(padModeOf(mtProject.pads.pad[40].mode) == kPadModeGate && padGroup(40) == 9, "setPadGroup keeps the mode", 40);
    setPadGroup(40, 0);
    check(mtProject.pads.pad[40].mode == kPadModeGate, "setPadGroup 0 = none", 40);
    setPadGroup(40, 32);
    check(mtProject.pads.pad[40].mode == kPadModeGate, "setPadGroup > 31 = none", 40);
    // a Gate pad on a sample program (no loop) stays Gate with a group; One shot stays One shot
    mtProject.programs.slot[40] = {};
    uint8_t savedPlayMode = mtProject.instrument[40].playMode;
    uint8_t savedActive = mtProject.instrument[40].isActive;
    mtProject.instrument[40].isActive = 1;
    mtProject.instrument[40].playMode = playModeSingleShot;
    mtProject.pads.pad[40] = { 40, 8, -1, padModeByte(kPadModeGate, 5) };
    check(padIsGate(40), "Gate with a group is still Gate", 40);
    mtProject.pads.pad[40].mode = padModeByte(kPadModeOneShot, 5);
    check(!padIsGate(40), "One shot with a group is still One shot", 40);
    mtProject.pads.pad[40].mode = padModeByte(kPadModeAuto, 31);
    check(!padIsGate(40), "Auto with a group follows the play mode", 40);
    mtProject.instrument[40].playMode = playModeLoopForward;
    check(padIsGate(40), "Auto with a group on a loop = Gate", 40);
    mtProject.instrument[40].playMode = savedPlayMode;
    mtProject.instrument[40].isActive = savedActive;

    // 1c. A (program, track)'s group: the first pad in bank order decides
    mtProject.pads = def;
    check(padGroupFor(2, 2) == 1 && padGroupFor(3, 3) == 1 && padGroupFor(11, 11) == 1, "hats by (program, track)", -1);
    check(padGroupFor(0, 0) == 0 && padGroupFor(2, 3) == 0, "no group / no pad", -1);
    mtProject.pads.pad[padIndex(1, 5)] = { 2, 2, -1, padModeByte(kPadModeAuto, 7) }; // B05: closed hat, group 7
    check(padGroupFor(2, 2) == 1, "A03 comes before B05", -1);
    mtProject.pads.pad[2].program = 5; // A03 now plays something else
    check(padGroupFor(2, 2) == 7, "then B05 decides", -1);
    mtProject.pads.pad[padIndex(0, 16)] = { 2, 2, -1, kPadModeAuto }; // A16: same pair, no group
    check(padGroupFor(2, 2) == 0, "A16 (no group) comes before B05", -1);

    // 2. Round trip through the MPDJ block (version 3), groups included
    mtProject.pads = def;
    mtProject.pads.pad[0] = { 5, 2, 48, kPadModeGate };          // A01: program 06, track 3, C-3
    mtProject.pads.pad[17] = { kPadNoProgram, 15, -1, kPadModeOneShot }; // B02: nothing, track 16
    mtProject.pads.pad[63] = { 63, 0, 127, kPadModeAuto };       // D16 -> track 1
    mtProject.pads.pad[20] = { 20, 4, -1, padModeByte(kPadModeGate, 31) }; // B05: Gate, group 31
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
    check(padGroup(20) == 31 && padModeOf(mtProject.pads.pad[20].mode) == kPadModeGate, "group 31 after reload", 20);
    check(padGroup(2) == 1 && padGroup(3) == 1 && padGroup(11) == 1, "hats' group after reload", -1);
    check(padCutOnRelease(2) && padCutOnRelease(3) && padCutOnRelease(11) && !padCutOnRelease(0),
          "hats' cut on release after reload", -1);

    // 2b. Cut on release (5b-6a) is kept apart from mode and group
    mtProject.pads.pad[41].mode = padModeByte(kPadModeGate, 9, true);
    check(padModeOf(mtProject.pads.pad[41].mode) == kPadModeGate && padGroup(41) == 9 && padCutOnRelease(41),
          "cut with mode and group", 41);
    setPadGroup(41, 3);
    check(padCutOnRelease(41) && padGroup(41) == 3, "setPadGroup keeps cut", 41);
    setPadCutOnRelease(41, false);
    check(!padCutOnRelease(41) && padGroup(41) == 3 && padModeOf(mtProject.pads.pad[41].mode) == kPadModeGate,
          "setPadCutOnRelease keeps mode and group", 41);
    setPadCutOnRelease(41, true);
    programsWriteToFile(file);
    mtProject.pads.pad[41] = {};
    programsReadFromFile(file);
    check(padCutOnRelease(41) && padGroup(41) == 3, "cut round trip", 41);
    check(strcmp(padModeName(mtProject.pads.pad[41].mode), "Gate") == 0, "mode name ignores cut", 41);
    mtProject.pads = def;
    programsWriteToFile(file);

    // 3. Bad entries read back as that pad's plain default (no group)
    file.pads[3] = { 64, 2, -1, 0 };  // program out of range
    file.pads[4] = { 4, 16, -1, 0 };  // track out of range
    file.pads[5] = { 5, 5, -2, 0 };   // note
    file.pads[6] = { 6, 6, -1, 3 };   // mode
    file.pads[7] = { 7, 7, -1, uint8_t(3 | kPadCutOnRelease) }; // mode 3 with cut
    file.pads[8] = { 8, 8, -1, uint8_t(3 | (4 << kPadGroupShift)) }; // mode 3 with a group
    programsReadFromFile(file);
    for (int i = 3; i <= 8; i++)
        check(memcmp(&mtProject.pads.pad[i], &plain.pad[i], 4) == 0, "bad entry -> plain default", i);

    // 4. Older MPDJ blocks (version 1, 2) and Tracker+ projects: the default
    //    map without groups (existing projects keep no groups)
    for (uint8_t ver = 1; ver <= 2; ver++)
    {
        file.version = ver;
        mtProject.pads.pad[9].track = 12;
        programsReadFromFile(file);
        for (int i = 0; i < kPads; i++)
            check(memcmp(&mtProject.pads.pad[i], &plain.pad[i], 4) == 0,
                  ver == 1 ? "version 1 block -> default map, no groups" : "version 2 block -> default map, no groups", i);
    }
    strMpdjProjectData none = {};
    mtProject.pads.pad[9].track = 12;
    programsReadFromFile(none);
    programsTakeImportNotice();
    for (int i = 0; i < kPads; i++)
        check(memcmp(&mtProject.pads.pad[i], &plain.pad[i], 4) == 0, "Tracker+ project -> default map, no groups", i);
    check(padGroupFor(2, 2) == 0, "older file: hats in no group", -1);

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

    printf("pad map (pad: program -> track, gN = choke group):\n");
    for (int i = 0; i < kPads; i++)
    {
        const strPadSlot& p = mtProject.pads.pad[i];
        padName(i, name);
        if (p.program == kPadNoProgram)
            printf("  %s: -- -> %2d", name, p.track + 1);
        else
            printf("  %s: %02d -> %2d", name, p.program + 1, p.track + 1);
        if (padGroupOf(p.mode))
            printf(" g%-2u", padGroupOf(p.mode));
        else
            printf("    ");
        if (i % 8 == 7)
            printf("\n");
    }
    printf("pads test: %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
#else
int padsSelfTest() { return -1; }
#endif
