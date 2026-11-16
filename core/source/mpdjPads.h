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

// Pad colors (5c-2): 0 = Auto (the pad's track color), 1-16 = kPadPalette
static constexpr int kPadColors = 16;      // the track colors (palette entries 1-16)
static constexpr int kPadColorChoices = 17; // stored pad colors 1-17: + Red (Mike 2026-11-16), kept
                                            // last so saved projects and kits keep their colors
static constexpr uint8_t kPadPalette[kPadColorChoices][3] = { // 0-127 per channel
    { 127, 24, 0 },  { 127, 64, 0 },  { 127, 112, 0 }, { 64, 127, 0 },  { 0, 127, 24 },   { 0, 127, 90 },
    { 0, 110, 127 }, { 0, 52, 127 },  { 24, 0, 127 },  { 72, 0, 127 },  { 127, 0, 127 },  { 127, 0, 64 },
    { 127, 64, 64 }, { 64, 127, 96 }, { 96, 96, 127 }, { 110, 110, 110 }, { 127, 0, 0 },
};
static constexpr const char* kPadColorNames[kPadColorChoices + 1] = {
    "Auto", "Red-orange", "Orange", "Yellow", "Lime", "Green", "Teal", "Cyan", "Sky",
    "Blue", "Violet", "Magenta", "Pink", "Salmon", "Mint", "Lavender", "White", "Red",
};
// the order the color setting steps through: Auto, Red, Red-orange .. White
uint8_t padColorStep(uint8_t color, int dir, bool wrap);

struct strPadMap
{
    strPadSlot pad[kPads]; // index = bank * 16 + (pad number - 1)
    uint8_t color[kPads]; // 5c-2: 0 = Auto, 1-16 = kPadPalette entry + 1
    // 2026-11-16 (Mike: pad dynamics per pad, as Ableton's / the MPC's): 100 - Dynamics %
    // (0 = full range, so zero-filled older data means the default) and the velocity curve
    // (0 Global = Config's, 1 Soft, 2 Normal, 3 Hard)
    uint8_t dynLoss[kPads];
    uint8_t curve[kPads];
    // 2026-11-16 (slice layer to bank): the slice the pad plays, + 1 (0 = none, the whole
    // program); Slice programs play it with the Slice FX, Beat Slice ones by note
    uint8_t slice[kPads];

    constexpr strPadMap() : pad{}, color{}, dynLoss{}, curve{}, slice{} { setDefault(); }

    // Pad N -> program N -> track N, no choke group, no cut on release, Auto
    // colors. (5d-5, Mike 2026-11-14: the hats' built-in group 1 + cut is
    // gone; choke groups and cut are set per pad on Pad Assign.)
    constexpr void setDefault()
    {
        for (int i = 0; i < kPads; i++)
        {
            pad[i] = { uint8_t(i), uint8_t(i % kPadsPerBank), kPadNoteDefault, kPadModeAuto };
            color[i] = 0;
            dynLoss[i] = 0;
            curve[i] = 0;
            slice[i] = 0;
        }
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
// Pad color (5c-2): the stored value (0 Auto, 1-16) and the RGB a pad shows
// for a track (Auto = the track's palette color)
uint8_t padColor(int pad);
void setPadColor(int pad, uint8_t color);
const uint8_t* padColorRgb(int pad, uint8_t track);
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
// the last pad played from the pad map, -1 before any (Kits: the bank to save)
int mpdjLastPadIndex();
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

// Track Mute timing (5e-3): a press queues a flip of the track's mute (again
// = cancel); the audio thread applies queued flips at the next bar / pattern
// start (mpdjMuteTick, from the sequencer before the row plays). The UI picks
// up what was applied (mpdjMuteTakeApplied) to grey the Pattern screen and
// mark the project changed. Bits = tracks 0-15.
void mpdjMuteQueue(int track);
uint16_t mpdjMutePending();
uint16_t mpdjMuteTakePending(); // clears the queue, returns it (apply now)
uint16_t mpdjMuteTakeApplied();
void mpdjMuteTick(int32_t uPos);

// OVER DUB (5f-2, Mike 2026-11-14). Plain live REC only adds: a hit on a row
// that already holds a note is not written. OVER DUB latched: hits replace,
// and from a track's first hit in the pass on, the rows the playhead reaches
// on that track are cleared before they play (note, program, volume,
// micromove, a Roll; other FX stay) unless just written ahead of it. One
// undo step per pass. A pass ends when OVER DUB goes off or REC / playback
// stops (mpdjOverdubPoll on the UI thread).
void mpdjOverdubSet(bool on);
bool mpdjOverdubOn();
uint16_t mpdjOverdubTracks();                // tracks joined in this pass
void mpdjOverdubJoin(int track, int keepRow); // a written hit (keepRow = ahead row or -1)
bool mpdjOverdubKeep(int track, int row);     // audio thread: protected row (consumed)
bool mpdjOverdubTakeUndo();                   // true once per pass: store the undo step now
void mpdjOverdubErased(int track, int row);   // audio thread: a row was cleared
void mpdjOverdubPoll();                       // UI thread

// ERASE + pad (5f-3, Mike 2026-11-14): the pad's hits = notes of its program
// on its track (Notes mode: also its note). Playing: cleared as the playhead
// reaches them while ERASE and the pad are held (one undo step per ERASE
// hold); stopped: all of them at once (one undo step). Returns a popup text.
const char* mpdjErasePadDown(int pad);
void mpdjErasePadUp(int pad);
void mpdjEraseEnd(); // ERASE released
void mpdjEraseTick(int row); // audio thread, before the row plays

// STEP SEQ (5f-4): an x0x step sequencer for one sound = a pad-map pad (the
// last pad hit until one is picked). Toggle = add the sound at `row` (one
// undo step; velocity as a pad hit, FULL LEVEL applies), or remove it if the
// row holds a note of its program. Returns 1 added, 0 removed, -1 nothing.
int mpdjStepSeqPad();
void mpdjStepSeqSetPad(int pad);
int mpdjStepSeqToggle(int row, uint8_t velocity);
bool mpdjStepSeqHas(int row); // the row holds the sound's program on its track
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

// Slice layer (5g-4): SHIFT + pad on the Pattern screen turns the pads into the
// slices of that pad's program (Slice / Beat Slice, at least one slice): bank A
// pad 1 = slice 1 .. C16 = slice 48, on the pad's track with its mode. A Slice
// program plays and records C5 (or the pad's note) + the Slice FX; a Beat Slice
// program the slice's note. Nothing in the pad map changes
// Toggle from SHIFT + pad-map pad `pad`: same pad = off, another = switch to it;
// returns the popup text (a static buffer)
const char* mpdjSliceLayerToggle(int pad);
void mpdjSliceLayerSet(bool on);
bool mpdjSliceLayerOn();
int mpdjSliceLayerPad(); // the source pad-map pad
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
    uint8_t note;     // 5c-3: Notes mode lights the pad of that note
    int8_t slice;     // 5g fix: the Slice FX it played (-1 = none): the slice layer lights that pad
};
void mpdjLightsNoteOn(uint8_t track, uint8_t program, uint8_t velocity, uint8_t note, int slice = -1);
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
// dynamics 0-100: 100 = x as the curve gives it, 0 = every hit at the top
uint8_t padVelocityScaleDyn(uint8_t velocity, uint8_t programVelo, uint8_t curve, int topPercent, int dynamics);
// The track a program plays on outside the pattern (5g fix): the first pad in bank
// order with that program, else program % 16; a muted track gives way to the first
// unmuted one (auditions on Sample Playback must be heard)
uint8_t mpdjProgramTrack(uint8_t program);
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
void padColorsWrite(uint8_t* out);   // 5c-2, block version 4
void padColorsRead(const uint8_t* in); // nullptr = all Auto (older files)
// Per-pad dynamics (2026-11-16, block version 8)
static constexpr int kPadCurveSettings = 4; // Global, Soft, Normal, Hard
uint8_t padDynamics(int pad);             // 0-100 %, 100 = the whole velocity range
void setPadDynamics(int pad, int percent);
// Capture (2026-11-16, like Live's Capture MIDI): pad presses that weren't recorded
// while the pattern played are kept (the latest 1024); mpdjCapture() writes the last
// pattern-length of them into the pattern (nearest row, adds only, one UNDO) and
// returns the popup text
const char* mpdjCapture();
// where a kept hit lands: `at` in microsteps since the pass count started, the pattern's rows
void mpdjCaptureRow(int64_t at, int rows, bool tc, int* row, int8_t* micromove);
// Slice layer to bank (2026-11-16): the slice a pad plays (-1 = none), set / clear it
int padSlice(int pad);
void setPadSlice(int pad, int slice);
void padSlicesWrite(uint8_t* slices);
void padSlicesRead(const uint8_t* slices); // nullptr = none (older files)
// The slice layer's slices layerBank*16 .. +15 onto bank `bank`'s pads, with the source
// pad's settings; returns the popup text (nullptr when the layer isn't on)
const char* mpdjSliceLayerToBank(int layerBank, int bank);
uint8_t padCurveSetting(int pad);         // 0 Global .. 3 Hard
void setPadCurveSetting(int pad, int setting);
const char* padCurveName(uint8_t setting); // "Global", "Soft", ...
void padDynamicsWrite(uint8_t* dynLoss, uint8_t* curve);
void padDynamicsRead(const uint8_t* dynLoss, const uint8_t* curve); // nullptr = defaults
// The pad's own hit: its curve (or Config's) and dynamics; a pad index < 0 = Config only
uint8_t padVelocityFor(int pad, uint8_t program, uint8_t velocity);
int dynamicsPad(int pad); // the pad whose dynamics a press uses (slice layer / Notes: the source pad)

// MPDJ_PADS_TEST=1: checks the default map, the file round trip and older
// files, prints the map and exits. Returns the exit code, or -1 when unset.
int padsSelfTest();
