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
    uint8_t mode;    // bits 0-1 PadMode, bits 2-6 choke group (0 = none), bit 7 cut on release
};
static_assert(sizeof(strPadSlot) == 4);

static constexpr uint8_t kPadModeMask = 0x03;
static constexpr int kPadGroupShift = 2;
static constexpr int kPadGroups = 31; // 1-31; 0 = no group

constexpr PadMode padModeOf(uint8_t modeByte) { return PadMode(modeByte & kPadModeMask); }
constexpr uint8_t padGroupOf(uint8_t modeByte) { return uint8_t((modeByte >> kPadGroupShift) & 0x1F); }
// Cut on release (5b-6a): a Gate pad's release cuts its track with the 5 ms
// choke fade instead of the envelope release (hi-hat loops)
static constexpr uint8_t kPadCutOnRelease = 0x80;
constexpr bool padCutOf(uint8_t modeByte) { return (modeByte & kPadCutOnRelease) != 0; }
constexpr uint8_t padModeByte(PadMode mode, uint8_t group, bool cut = false)
{
    return uint8_t((mode & kPadModeMask) | ((group > kPadGroups ? 0 : group) << kPadGroupShift) |
                   (cut ? kPadCutOnRelease : 0));
}

// The MPDJ Kit's hats (tools/kitgen): Closed Hat, Open Hat, Pedal Hat
static constexpr int kDefaultHatPads[] = { 2, 3, 11 }; // A03, A04, A12
static constexpr uint8_t kDefaultHatGroup = 1;

struct strPadMap
{
    strPadSlot pad[kPads]; // index = bank * 16 + (pad number - 1)

    constexpr strPadMap() : pad{} { setDefault(); }

    // New projects: hats in group 1 with cut on release (until Pad Assign,
    // 5d, sets both per pad). withGroups = false: the plain map that
    // older files and invalid entries get (existing projects keep no groups)
    constexpr void setDefault(bool withGroups = true)
    {
        for (int i = 0; i < kPads; i++)
            pad[i] = { uint8_t(i), uint8_t(i % kPadsPerBank), kPadNoteDefault, kPadModeAuto };
        if (withGroups)
            for (int i : kDefaultHatPads)
                pad[i].mode = padModeByte(kPadModeAuto, kDefaultHatGroup, true);
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
void setPadGroup(int pad, uint8_t group); // keeps the play mode and cut; > 31 = none
bool padCutOnRelease(int pad);             // 5b-6a
void setPadCutOnRelease(int pad, bool cut);
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

// Note Repeat (step 5b-7): while it's on (NOTE REPEAT held, or latched with
// SHIFT), every held pad retriggers at the division with its press velocity.
// Playing: on the pattern's grid (a uStep position); stopped: free-running
// from the first repeating press. A pad's own press always sounds at once; a
// grid hit closer than half a division to it is skipped.
void mpdjRepeatSet(bool on);
bool mpdjRepeatOn();
// Division in sequencer nanoSteps (576 = one row = 1/16; all multiples of 12)
static constexpr uint16_t kRepeatDefaultPeriod = 576;
void mpdjRepeatSetPeriod(uint16_t nanoSteps);
uint16_t mpdjRepeatPeriod();
// Called by the sequencer on every nanoStep (audio thread). uPos = the
// pattern position in uSteps (row * 48 + uStep - 1) on a uStep boundary
// while playing, else -1
void mpdjRepeatTick(bool playing, int32_t uPos);
// Live REC (5b-7c): a pad pressed with Note Repeat on writes its note with a
// Roll at the division when the Roll has it (1/16, 1/32, 1/32T) and an OFF
// (CUT with cut on release) at its release; at other divisions, or for a pad
// that joined later, each repeat is written as its own hit. The audio thread
// queues those; call this from the main loop to write them
void mpdjPadsPoll();

// Notes mode (5b-8a; SHIFT+16 LEVELS): the 16 pads play one program
// chromatically: pad 1 = the project's root (padBoardRootNote), bank B in the
// octave of the sample's own pitch (C5), A one lower, C / D one / two higher.
// Config "Notes mode plays": the last pad hit (program, track, mode) or the
// title bar's program on the cursor's track. Pad hits, Note Repeat, live REC
// and step entry work as with the pad map
void mpdjNotesSet(bool on);
bool mpdjNotesOn();
int mpdjNotesNote(int pad);
bool mpdjNotesSource(strPadSlot* out); // false: nothing to play
const char* mpdjNotesPress(int pad, uint8_t velocity);
void mpdjSetCursorTrack(int track);

// Timing correct (TC button, 2026-11-14): on = live REC puts hits on the
// nearest row and writes no micromove; off = micromoves recorded. Starts
// from Config > General "Timing correct" (on); a change there applies at once
bool mpdjTcOn();

// Pad lights (5c-1): every note started on a track (pads, repeats, pattern
// playback) is noted here so the platform can light the pads that play it.
// Audio or main thread; lock-free (a counter per track)
struct MpdjTrackHit
{
    uint32_t count;   // bumps on every note on the track
    uint8_t program;  // what played
    uint8_t velocity; // 0-127
};
void mpdjLightsNoteOn(uint8_t track, uint8_t program, uint8_t velocity);
MpdjTrackHit mpdjLightsTrackHit(uint8_t track);
// Pads held right now (pad index -> track, -1 = not held) and what they play
bool mpdjPadHeldSlot(int pad, strPadSlot* out, uint8_t* velocity);
// What a physical pad plays in the current mode (pad map / Notes / 16 Levels)
bool mpdjPadLightSlot(int pad, strPadSlot* out, uint8_t* velocity);
void mpdjTcSet(bool on);

// 16 Levels (5b-9; 16 LEVELS): the 16 pads play what the last hit played
// (a pad-map pad or a Notes-mode note) at 16 velocities, pad 1 softest
// (8) .. pad 16 a full hit (127), before Config's pad sensitivity. Plays,
// repeats and records like any pad. Notes mode and 16 Levels exclude each
// other (the platform switches them)
void mpdjLevelsSet(bool on);
bool mpdjLevelsOn();
uint8_t mpdjLevelsVelocity(int pad);
const char* mpdjLevelsSourceName(); // "A03" / "D5", nullptr before any hit
// The Roll value for a division in nanoSteps, 0 = none fits
uint8_t mpdjRollForPeriod(uint16_t nanoSteps);
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
