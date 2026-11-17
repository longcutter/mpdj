// MPDJ additions (MIT, see NOTICE.md): the pad map. See mpdjPads.h.
#include "mpdjPads.h"
#include "mpdjFeatures.h"

#include "mpdjPrograms.h"
#include "mtSequencer.h"
#include "mtAudioEngine.h"
#include "mtStructs.h"
#include "util_math.h"
#include "mtConfig.h"
#include "fileManager.h"
#include "scales.h"

#include "Audio.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include "mpdjTracks.h"

// ---- Track Mute timing (5e-3) ----------------------------------------------
static std::atomic<uint16_t> s_mutePending{ 0 }, s_muteApplied{ 0 };
static constexpr int kMuteUSteps = 48; // uSteps per row (Sequencer::kuStepsInStep)

// ---- OVER DUB (5f-2) ---------------------------------------------------------
static std::atomic<bool> s_overdub{ false }, s_odErased{ false };
static std::atomic<uint16_t> s_odTracks{ 0 };
static std::atomic<int16_t> s_odKeep[16][2];
static bool s_odUndoStored = false;

static void overdubEndPass()
{
    s_odTracks.store(0);
    for (auto& k : s_odKeep)
        k[0].store(-1), k[1].store(-1);
    s_odUndoStored = false;
}

void mpdjOverdubSet(bool on)
{
    overdubEndPass(); // a new pass starts clean (protected rows -1)
    s_overdub.store(on);
}
bool mpdjOverdubOn() { return s_overdub.load(); }
uint16_t mpdjOverdubTracks() { return s_overdub.load() ? s_odTracks.load() : 0; }

void mpdjOverdubJoin(int track, int keepRow)
{
    if (!s_overdub.load() || track < 0 || track >= 16)
        return;
    if (keepRow >= 0)
    {
        // two protected rows per track (a note and its OFF); replace the older
        int slot = s_odKeep[track][0].load() < 0 ? 0 : 1;
        if (slot == 1 && s_odKeep[track][1].load() >= 0)
            s_odKeep[track][0].store(s_odKeep[track][1].load());
        s_odKeep[track][slot].store((int16_t)keepRow);
    }
    s_odTracks.fetch_or((uint16_t)(1u << track));
}

bool mpdjOverdubKeep(int track, int row)
{
    for (auto& k : s_odKeep[track])
    {
        int16_t r = row;
        if (k.compare_exchange_strong(r, (int16_t)-1))
            return true;
    }
    return false;
}

bool mpdjOverdubTakeUndo()
{
    if (s_odUndoStored)
        return false;
    s_odUndoStored = true;
    return true;
}

void mpdjOverdubErased(int track, int row)
{
    s_odErased.store(true);
#ifdef PLATFORM_MPC
    static const bool trace = [] { const char* v = getenv("MPDJ_PAD_REC_TRACE"); return v && *v && *v != '0'; }();
    if (trace)
        printf("pad rec: over dub cleared track %d row %d\n", track + 1, row);
#else
    (void)track;
    (void)row;
#endif
}

void mpdjOverdubPoll()
{
    if (s_odErased.exchange(false))
        newFileManager.setActualPatternStructChanged();
    if ((s_odTracks.load() || s_odUndoStored) && (!sequencer.isRec() || !sequencer.isPlay()))
        overdubEndPass();
}

void mpdjMuteQueue(int track)
{
    if (track >= 0 && track < 16)
        s_mutePending.fetch_xor((uint16_t)(1u << track));
}

uint16_t mpdjMutePending() { return s_mutePending.load(); }
uint16_t mpdjMuteTakePending() { return s_mutePending.exchange(0); }
uint16_t mpdjMuteTakeApplied() { return s_muteApplied.exchange(0); }

void mpdjMuteTick(int32_t uPos)
{
    if (uPos < 0 || uPos % kMuteUSteps != 0 || !s_mutePending.load())
        return;
    int row = uPos / kMuteUSteps;
    int perBar = sequencer.getMetronomeDenominator() * sequencer.getMetronomeNumerator();
    bool due = mtConfig.general.trackMuteTiming == 2 ? row == 0
                                                       : (perBar > 0 && row % perBar == 0);
    if (!due)
        return;
    uint16_t m = s_mutePending.exchange(0);
    for (int t = 0; t < 16; t++)
        if (m & (1u << t))
        {
            trackMuteRef(t) = !trackMuteRef(t);
            engine.muteTrack(t, trackMuteRef(t));
        }
    s_muteApplied.fetch_or(m);
}
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
    bool recorded = false;
    // Note Repeat (5b-7a): what a retrigger plays, captured at the press
    uint8_t program = 0;
    uint8_t note = 0;
    uint8_t velo = 0;
    uint8_t group = 0;
    int8_t slice = -1;   // slice layer (5g-4): the Slice FX the press plays, -1 = none
    std::atomic<bool> repeat{ false };
    bool rolled = false; // live REC wrote the press with a Roll (5b-7c)
    uint32_t lastHit = 0; // s_nano of the last hit // the press was written in live REC (5b-4a)
};
Held s_held[kPads];

// Note Repeat state (5b-7a)
std::atomic<bool> s_repeatOn{ false };
std::atomic<uint16_t> s_repeatPeriod{ kRepeatDefaultPeriod };
std::atomic<uint32_t> s_nano{ 0 }; // nanoSteps since start (audio thread)
uint32_t s_freeNano = 0;           // stopped: phase from the first repeating press
std::atomic<int> s_repeating{ 0 }; // pads repeating

// Repeat hits to write in live REC (5b-7c): audio thread -> main loop
constexpr int kRecQueue = 64;
std::atomic<uint8_t> s_recQueue[kRecQueue];
std::atomic<uint32_t> s_recHead{ 0 }, s_recTail{ 0 };

// Wait until the audio thread isn't inside a block: it holds the audio lock
// for the whole block, so after this no retrigger can still be on its way
void audioBarrier()
{
    AudioNoInterrupts();
    AudioInterrupts();
}

void startRepeat(int pad)
{
    Held& h = s_held[pad];
    if (h.track < 0 || h.repeat.load())
        return;
    if (s_repeating.load() == 0)
        s_freeNano = 0; // read only by the audio thread, which is idle with 0 repeating
    h.lastHit = s_nano.load();
    h.repeat.store(true);
    s_repeating.fetch_add(1);
}

void stopRepeat(int pad)
{
    Held& h = s_held[pad];
    if (!h.repeat.load())
        return;
    h.repeat.store(false);
    s_repeating.fetch_sub(1);
    audioBarrier();
}
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

namespace
{
bool slotIsGate(const strPadSlot* p)
{
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

int s_cursorTrack = 0; // the Pattern screen's cursor track (set by the platform)
int s_lastPad = -1; // the last pad hit in the pad map (5b-8a: Notes mode's default source)
bool s_notesMode = false;
bool s_levelsMode = false;   // 16 Levels (5b-9)
bool s_sliceMode = false;    // slice layer (5g-4)
int s_slicePad = -1;         // its source pad-map pad
int s_resolvedSlice = -1;    // the Slice FX of the press resolvePad last resolved
strPadSlot s_lastSlot{};     // what the last hit played (pad map or Notes), 16 Levels' source
bool s_lastSlotValid = false;
char s_lastName[8] = "";
const char* padPress(int pad, const strPadSlot* p, uint8_t velocity);
}

bool padIsGate(int pad)
{
    return slotIsGate(padSlot(pad));
}

// What a press of `pad` plays in the current mode, and with what velocity
// (16 Levels replaces it); false = nothing (msg says why, if anything)
static bool resolvePad(int pad, strPadSlot* out, uint8_t* velocity, const char** msg)
{
    *msg = nullptr;
    s_resolvedSlice = -1;
    if (s_sliceMode && s_slicePad >= 0)
    {
        // slice layer (5g-4): pad index = slice; the source pad's program, track, mode
        *out = *padSlot(s_slicePad);
        const strInstrument& ins = mtProject.instrument[out->program < kProgramSlots ? out->program : 0];
        if (out->program == kPadNoProgram || !ins.isActive || pad >= ins.sliceNumber)
            return false; // no slice on this pad: nothing plays, its light is dark
        if (ins.playMode == playModeBeatSlice)
            out->note = (int8_t)(FIRST_BEAT_SLICE_NOTE + pad);
        else
            s_resolvedSlice = pad;
        if (mtProject.mpdjFullLevel)
            *velocity = 127;
        return true;
    }
    if (s_levelsMode)
    {
        if (!s_lastSlotValid)
        {
            return false; // nothing to play yet (the 16 LEVELS popup says so)
        }
        *out = s_lastSlot;
        *velocity = mpdjLevelsVelocity(pad);
        return true;
    }
    if (s_notesMode)
    {
        if (!mpdjNotesSource(out))
        {
            // nothing to play (the SHIFT+16 LEVELS popup says so)
            return false;
        }
        out->note = (int8_t)mpdjNotesNote(pad);
        if (mtProject.mpdjFullLevel)
            *velocity = 127; // FULL LEVEL (5f-1): every hit at the full-hit level
        return true;
    }
    *out = *padSlot(pad);
    // a pad written from the slice layer (2026-11-16) plays its slice
    if (int slice = padSlice(pad); slice >= 0 && out->program < kProgramSlots)
    {
        const strInstrument& ins = mtProject.instrument[out->program];
        if (ins.playMode == playModeBeatSlice)
            out->note = (int8_t)(FIRST_BEAT_SLICE_NOTE + slice);
        else if (ins.playMode == playModeSlice)
            s_resolvedSlice = slice;
    }
    if (mtProject.mpdjFullLevel)
        *velocity = 127; // FULL LEVEL (5f-1)
    return true;
}

const char* mpdjPadPress(int pad, uint8_t velocity)
{
    if (pad < 0 || pad >= kPads)
        return nullptr;
    strPadSlot slot;
    const char* msg;
    if (!resolvePad(pad, &slot, &velocity, &msg))
        return msg;
    if (!s_levelsMode && !s_sliceMode)
    {
        if (!s_notesMode)
            s_lastPad = pad;
        s_lastSlot = slot; // 16 Levels plays this
        s_lastSlotValid = true;
        padName(pad, s_lastName);
        if (s_notesMode)
        {
            int n = slot.note < 0 ? Sequencer::STEP_NOTE_DEFAULT : slot.note;
            snprintf(s_lastName, sizeof s_lastName, "%s", mtNotes[n]);
        }
    }
    return padPress(pad, &slot, velocity);
}

uint8_t mpdjProgramTrack(uint8_t program)
{
    int track = program % kAllTracks;
    for (int i = 0; i < kPads; i++)
        if (padSlot(i)->program == program)
        {
            track = padSlot(i)->track;
            break;
        }
    if (trackMuteRef(track))
        for (int t = 0; t < kAllTracks; t++)
            if (!trackMuteRef(t))
                return (uint8_t)t;
    return (uint8_t)track;
}

// ---------------------------------------------------------------- slice layer (5g-4)
void mpdjSliceLayerSet(bool on)
{
    s_sliceMode = on && s_slicePad >= 0;
}

bool mpdjSliceLayerOn()
{
    return s_sliceMode;
}

int mpdjSliceLayerPad()
{
    return s_slicePad;
}

const char* mpdjSliceLayerToggle(int pad)
{
    static char msg[48]; // the popup keeps the pointer
    char name[4];
    if (pad < 0 || pad >= kPads)
        return nullptr;
    padName(pad, name);
    if (s_sliceMode && pad == s_slicePad)
    {
        s_sliceMode = false;
        snprintf(msg, sizeof msg, "Slices off");
        return msg;
    }
    const strPadSlot* p = padSlot(pad);
    const strInstrument* ins =
      (p->program != kPadNoProgram && p->program < kProgramSlots) ? &mtProject.instrument[p->program] : nullptr;
    if (!ins || !ins->isActive || (ins->playMode != playModeSlice && ins->playMode != playModeBeatSlice) ||
        ins->sliceNumber == 0)
    {
        snprintf(msg, sizeof msg, "%s: not sliced", name);
        return msg;
    }
    s_slicePad = pad;
    s_sliceMode = true;
    snprintf(msg, sizeof msg, "Slices: %s, %d on track %d", name, ins->sliceNumber, p->track + 1);
    return msg;
}

// ---------------------------------------------------------------- 16 Levels
void mpdjLevelsSet(bool on)
{
    s_levelsMode = on;
}

bool mpdjLevelsOn()
{
    return s_levelsMode;
}

uint8_t mpdjLevelsVelocity(int pad)
{
    int n = pad % kPadsPerBank + 1; // pad 1 softest .. pad 16 a full hit
    return (uint8_t)((n * 127 + 8) / 16);
}

const char* mpdjLevelsSourceName()
{
    return s_lastSlotValid ? s_lastName : nullptr;
}

namespace
{
// A press of physical pad `pad` playing slot `p` (the pad map's own slot, or
// what Notes mode makes of it)
// ---- Capture (2026-11-16) ----
struct CaptureHit
{
    int64_t at; // pass * rows * 48 + row * 48 + microstep, on the hit's track
    uint8_t track, program, note, velo;
    int8_t slice;
    uint8_t chance; // the pad's Chance % (100 = always)
};
constexpr int kCaptureMax = 1024;
CaptureHit s_capture[kCaptureMax];
int s_captureCount = 0, s_captureHead = 0; // ring: the oldest at head - count
int64_t s_captureLast = -1;

void captureKeep(uint8_t track, uint8_t program, uint8_t note, uint8_t velo, int8_t slice, uint8_t chance)
{
    uint32_t pass;
    int row, u, last;
    sequencer.mpdjPlayhead(track, &pass, &row, &u, &last);
    int64_t at = ((int64_t)pass * (last + 1) + row) * 48 + u;
    if (at < s_captureLast - 48) // playback started again: an older run doesn't line up
        s_captureCount = 0;
    s_captureLast = at;
    s_capture[s_captureHead] = { at, track, program, note, velo, slice, chance };
    s_captureHead = (s_captureHead + 1) % kCaptureMax;
    if (s_captureCount < kCaptureMax)
        s_captureCount++;
}

const char* padPress(int pad, const strPadSlot* p, uint8_t velocity)
{
    if (!p)
        return nullptr;
    char name[4];
    padName(pad, name);
    // an empty pad plays nothing and says nothing: its light is dark (Mike
    // 2026-11-14, the popup went with the pad lights)
    if (p->program == kPadNoProgram || programType(p->program) == kProgramEmpty)
        return nullptr;
    if (s_held[pad].track >= 0)
        mpdjPadRelease(pad); // a second press before the release (shouldn't happen)

    uint8_t note = p->note < 0 ? (uint8_t)Sequencer::STEP_NOTE_DEFAULT : (uint8_t)p->note;
    uint8_t velo = padVelocityFor(dynamicsPad(pad), p->program, velocity ? velocity : 1);
    Held& h = s_held[pad];
    h.track = (int8_t)p->track;
    h.gate = slotIsGate(p);
    h.cut = h.gate && padCutOf(p->mode);
    h.recorded = false;
    h.rolled = false;
    h.program = p->program;
    h.note = note;
    h.velo = velo;
    h.group = padGroupOf(p->mode);
    h.slice = (int8_t)s_resolvedSlice;

    // choke groups (5b-5c): the pad's own group cuts the rest of the group,
    // also when this track is muted
    sequencer.mpdjChoke(p->track, padGroupOf(p->mode), name);
    sequencer.mpdjPadNoteOn(p->track, p->program, note, velo, kPadSourceBase + pad, h.slice);

    // REC + playing (5b-4a): write the hit on the pad's track
    Sequencer::MpdjRecWrite w;
    if (sequencer.mpdjPadRecWrite(p->track, p->program, note, velo, padVelocityToVolume(), &w))
    {
        s_held[pad].recorded = true;
        s_lastRecPad[p->track] = (int8_t)pad;
        if (s_held[pad].slice >= 0)
            sequencer.mpdjSetSlice(p->track, (uint8_t)w.row, (uint8_t)s_held[pad].slice); // 5g-4
        sequencer.mpdjSetChance(p->track, (uint8_t)w.row, (uint8_t)padChance(dynamicsPad(pad)));
        // Note Repeat (5b-7c): the press carries a Roll at the division
        uint8_t roll = s_repeatOn.load() ? mpdjRollForPeriod(s_repeatPeriod.load()) : 0;
        s_held[pad].rolled = roll != 0;
        if (roll)
        {
            sequencer.mpdjSetRoll(p->track, (uint8_t)w.row, roll);
#ifdef PLATFORM_MPC
            if (recTrace())
                printf("pad rec: %s -> track %d row %d: Roll %u (note repeat)\n", name, p->track + 1, w.row, roll);
#endif
        }
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
               !sequencer.isRecArmedOnTrack(p->track) ? "track not armed"
               : sequencer.mpdjCountingIn()           ? "counting in"
                                                      : "the row has a note; Over Dub off adds only");
    }
#endif
    if (!s_held[pad].recorded && sequencer.isPlay() && mpdjOn(MpdjFeature::Capture))
        captureKeep(p->track, p->program, note, velo, s_held[pad].slice, (uint8_t)padChance(dynamicsPad(pad)));
    if (s_repeatOn.load())
        startRepeat(pad); // Note Repeat (5b-7a): the press was the first hit
    return nullptr;
}
}

// ---------------------------------------------------------------- pad lights
namespace
{
std::atomic<uint32_t> s_hitCount[kAllTracks];
std::atomic<uint32_t> s_hitWhat[kAllTracks]; // (slice + 1) << 24 | note << 16 | program << 8 | velocity
}

void mpdjLightsNoteOn(uint8_t track, uint8_t program, uint8_t velocity, uint8_t note, int slice)
{
    if (track >= kAllTracks)
        return;
    uint32_t s = (slice >= 0 && slice < 64) ? (uint32_t)(slice + 1) : 0;
    s_hitWhat[track].store(s << 24 | (uint32_t)(note & 0x7F) << 16 | (uint32_t)program << 8 | (velocity & 0x7F));
    s_hitCount[track].fetch_add(1);
}

MpdjTrackHit mpdjLightsTrackHit(uint8_t track)
{
    if (track >= kAllTracks)
        return { 0, 0, 0, 0, -1 };
    uint32_t w = s_hitWhat[track].load();
    return { s_hitCount[track].load(), (uint8_t)(w >> 8), (uint8_t)(w & 0x7F), (uint8_t)((w >> 16) & 0x7F),
             (int8_t)((int)(w >> 24) - 1) };
}

bool mpdjPadHeldSlot(int pad, strPadSlot* out, uint8_t* velocity)
{
    if (pad < 0 || pad >= kPads || s_held[pad].track < 0)
        return false;
    const Held& h = s_held[pad];
    *out = { h.program, (uint8_t)h.track, (int8_t)h.note, 0 };
    *velocity = h.velo;
    return true;
}

bool mpdjPadLightSlot(int pad, strPadSlot* out, uint8_t* velocity)
{
    const char* msg;
    *velocity = 127;
    return pad >= 0 && pad < kPads && resolvePad(pad, out, velocity, &msg);
}

// ---------------------------------------------------------------- TC
namespace
{
int s_tc = -1;            // -1 = not read yet
uint8_t s_tcConfig = 0xFF; // the Config value last seen
}

bool mpdjTcOn()
{
#ifdef PLATFORM_MPC
    if (mtConfig.general.tcDefault != s_tcConfig) // start, or changed in Config
    {
        s_tcConfig = mtConfig.general.tcDefault;
        s_tc = s_tcConfig ? 1 : 0;
    }
    return s_tc == 1;
#else
    return false;
#endif
}

void mpdjTcSet(bool on)
{
    mpdjTcOn(); // read the Config value first, so it doesn't override this later
    s_tc = on ? 1 : 0;
}

// ---------------------------------------------------------------- Notes mode
void mpdjSetCursorTrack(int track)
{
    if (track >= 0 && track < kAllTracks)
        s_cursorTrack = track;
}

void mpdjNotesSet(bool on)
{
    s_notesMode = on;
}

bool mpdjNotesOn()
{
    return s_notesMode;
}

int mpdjNotesNote(int pad)
{
    // pad 1 of bank B = the root in the octave that plays the sample at its
    // own pitch (C5 = 60); bank A an octave lower, C / D one and two higher;
    // pads go up chromatically
    // pads go up chromatically, or through the project scale (5b-8b:
    // padBoardScale, the Tracker's scale list; pad n = the n-th degree)
    int bank = pad / kPadsPerBank, n = pad % kPadsPerBank;
    int root = mtProject.values.padBoardRootNote % 12;
    int note = Sequencer::STEP_NOTE_DEFAULT + root + (bank - 1) * 12;
    uint8_t scale = mtProject.values.padBoardScale;
    if (scale == 0 || scale > MAX_SCALE)
        note += n;
    else
        for (int degree = 0; degree < n && note < 127;)
            if (isInScale((uint8_t)++note, (uint8_t)root, scale))
                degree++;
    return note > 127 ? 127 : note;
}

bool mpdjNotesSource(strPadSlot* out)
{
    if (mtConfig.general.notesSource == 1)
    {
        // the title bar's program on the cursor's track
        out->program = mtProject.values.lastUsedInstrument;
        out->track = (uint8_t)s_cursorTrack;
        out->note = kPadNoteDefault;
        out->mode = kPadModeAuto;
        return out->program < kProgramSlots;
    }
    const strPadSlot* src = padSlot(s_lastPad >= 0 ? s_lastPad : 0); // A01 until a pad is hit
    *out = *src;
    return src->program != kPadNoProgram;
}

const char* mpdjNotesPress(int pad, uint8_t velocity)
{
    strPadSlot slot;
    if (!mpdjNotesSource(&slot))
        return nullptr;
    slot.note = (int8_t)mpdjNotesNote(pad);
    return padPress(pad, &slot, velocity);
}

// ---------------------------------------------------------------- Note Repeat
void mpdjRepeatSet(bool on)
{
    if (on == s_repeatOn.load())
        return;
    s_repeatOn.store(on);
    for (int pad = 0; pad < kPads; pad++)
    {
        if (on)
            startRepeat(pad); // pads already held join in (no extra hit now)
        else
            stopRepeat(pad); // held pads keep their last note
    }
}

bool mpdjRepeatOn()
{
    return s_repeatOn.load();
}

void mpdjRepeatSetPeriod(uint16_t nanoSteps)
{
    if (nanoSteps < 12)
        nanoSteps = 12;
    s_repeatPeriod.store(nanoSteps);
}

uint16_t mpdjRepeatPeriod()
{
    return s_repeatPeriod.load();
}

// Audio thread, every sequencer nanoStep (holds the audio lock)
void mpdjRepeatTick(bool playing, int32_t uPos)
{
    uint32_t now = s_nano.fetch_add(1) + 1;
    if (s_repeating.load() == 0)
        return;
    uint32_t period = s_repeatPeriod.load();
    bool fire;
    if (playing)
    {
        uint32_t uPeriod = period / 12; // uSteps
        fire = uPos >= 0 && uPeriod && (uint32_t)uPos % uPeriod == 0;
    }
    else
    {
        s_freeNano++;
        fire = s_freeNano % period == 0;
    }
    if (!fire)
        return;
    for (int pad = 0; pad < kPads; pad++)
    {
        Held& h = s_held[pad];
        if (!h.repeat.load() || h.track < 0)
            continue;
        if (now - h.lastHit < period / 2)
            continue; // just pressed: no flam
        h.lastHit = now;
        sequencer.mpdjChoke((uint8_t)h.track, h.group, nullptr);
        sequencer.mpdjPadNoteOn((uint8_t)h.track, h.program, h.note, h.velo, kPadSourceBase + pad, h.slice);
        if (playing && !h.rolled && sequencer.isRec())
        {
            uint32_t head = s_recHead.load();
            if (head - s_recTail.load() < kRecQueue) // full: drop the write, not the sound
            {
                s_recQueue[head % kRecQueue].store((uint8_t)pad);
                s_recHead.store(head + 1);
            }
        }
    }
}

uint8_t mpdjRollForPeriod(uint16_t nanoSteps)
{
    switch (nanoSteps)
    {
    case 576: return Sequencer::strFxConsts::ROLL_PERIOD_1_1; // 1/16
    case 288: return Sequencer::strFxConsts::ROLL_PERIOD_1_2; // 1/32
    case 192: return Sequencer::strFxConsts::ROLL_PERIOD_1_3; // 1/32T
    default: return 0;
    }
}

// Main loop: write the repeat hits the audio thread queued (5b-7c)
void mpdjPadsPoll()
{
    uint32_t tail = s_recTail.load();
    while (tail != s_recHead.load())
    {
        int pad = s_recQueue[tail % kRecQueue].load();
        tail++;
        s_recTail.store(tail);
        Held& h = s_held[pad];
        if (h.track < 0)
            continue; // released meanwhile
        Sequencer::MpdjRecWrite w;
        if (sequencer.mpdjPadRecWrite((uint8_t)h.track, h.program, h.note, h.velo, padVelocityToVolume(), &w))
        {
            if (h.slice >= 0)
                sequencer.mpdjSetSlice((uint8_t)h.track, (uint8_t)w.row, (uint8_t)h.slice); // 5g-4
            sequencer.mpdjSetChance((uint8_t)h.track, (uint8_t)w.row, (uint8_t)padChance(dynamicsPad(pad)));
            h.recorded = true;
            s_lastRecPad[h.track] = (int8_t)pad;
#ifdef PLATFORM_MPC
            if (recTrace())
            {
                char name[4];
                padName(pad, name);
                printf("pad rec: %s repeat -> track %d row %d: %s %02d vol %d micro %d\n", name,
                       h.track + 1, w.row, mtNotes[h.note], h.program + 1, w.volume, w.micromove);
            }
#endif
        }
    }
}

int mpdjPadStepWrite(int pad, int row, uint8_t velocity)
{
    // Notes mode (5b-8a): the pad's note; 16 Levels (5b-9): the last hit at
    // the pad's velocity
    strPadSlot slot;
    const char* msg;
    if (pad < 0 || pad >= kPads || !resolvePad(pad, &slot, &velocity, &msg))
        return -1;
    const strPadSlot* p = &slot;
    if ( p->program == kPadNoProgram || programType(p->program) == kProgramEmpty)
        return -1;
    uint8_t note = p->note < 0 ? (uint8_t)Sequencer::STEP_NOTE_DEFAULT : (uint8_t)p->note;
    uint8_t velo = padVelocityFor(dynamicsPad(pad), p->program, velocity ? velocity : 1);
    bool writeVol = padVelocityToVolume();
    int slice = s_resolvedSlice;
    int8_t vol = sequencer.mpdjPadStepWrite(p->track, (uint8_t)row, p->program, note, velo, writeVol);
    if (vol < 0)
        return -1;
    if (slice >= 0)
        sequencer.mpdjSetSlice(p->track, (uint8_t)row, (uint8_t)slice, true); // 5g-4: the slice layer's slice
    sequencer.mpdjSetChance(p->track, (uint8_t)row, (uint8_t)padChance(dynamicsPad(pad)), true);
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

uint8_t padVelocityScaleDyn(uint8_t velocity, uint8_t programVelo, uint8_t curve, int topPercent, int dynamics)
{
    float x = velocity > 127 ? 1.0f : velocity / 127.0f;
    if (curve == 0)
        x = sqrtf(x);
    else if (curve == 2)
        x = x * x;
    float d = (dynamics < 0 ? 0 : dynamics > 100 ? 100 : dynamics) / 100.0f;
    x = (1.0f - d) + d * x; // less dynamics: the hit stays nearer the top
    float top = programVelo * (topPercent / 100.0f);
    int v = (int)(x * top + 0.5f);
    if (velocity > 0 && v < 1)
        v = 1;
    return (uint8_t)(v > 127 ? 127 : v);
}

uint8_t padDynamics(int pad)
{
    if (pad < 0 || pad >= kPads)
        return 100;
    int loss = mtProject.pads.dynLoss[pad];
    return (uint8_t)(loss > 100 ? 0 : 100 - loss);
}

void setPadDynamics(int pad, int percent)
{
    if (pad >= 0 && pad < kPads)
        mtProject.pads.dynLoss[pad] = (uint8_t)(100 - (percent < 0 ? 0 : percent > 100 ? 100 : percent));
}

uint8_t padCurveSetting(int pad)
{
    return (pad >= 0 && pad < kPads && mtProject.pads.curve[pad] < kPadCurveSettings) ? mtProject.pads.curve[pad] : 0;
}

void setPadCurveSetting(int pad, int setting)
{
    if (pad >= 0 && pad < kPads && setting >= 0 && setting < kPadCurveSettings)
        mtProject.pads.curve[pad] = (uint8_t)setting;
}

const char* padCurveName(uint8_t setting)
{
    static const char* const kNames[kPadCurveSettings] = { "Global", "Soft", "Normal", "Hard" };
    return kNames[setting < kPadCurveSettings ? setting : 0];
}

int padSlice(int pad)
{
    if (!mpdjOn(MpdjFeature::PadSlices))
        return -1;
    return (pad >= 0 && pad < kPads && mtProject.pads.slice[pad]) ? mtProject.pads.slice[pad] - 1 : -1;
}

void setPadSlice(int pad, int slice)
{
    if (pad >= 0 && pad < kPads)
        mtProject.pads.slice[pad] = (slice >= 0 && slice < MAX_SLICE_NUMBER) ? (uint8_t)(slice + 1) : 0;
}

int padChance(int pad)
{
    return (pad >= 0 && pad < kPads) ? 100 - mtProject.pads.chanceLoss[pad] : 100;
}

void setPadChance(int pad, int chance)
{
    if (pad >= 0 && pad < kPads)
        mtProject.pads.chanceLoss[pad] = (uint8_t)(100 - (chance < 5 ? 5 : (chance > 100 ? 100 : chance)));
}

void padChanceWrite(uint8_t* loss)
{
    memcpy(loss, mtProject.pads.chanceLoss, kPads);
}

void padChanceRead(const uint8_t* loss)
{
    for (int i = 0; i < kPads; i++)
        mtProject.pads.chanceLoss[i] = (loss && loss[i] <= 95) ? loss[i] : 0;
}

void padSlicesWrite(uint8_t* slices)
{
    memcpy(slices, mtProject.pads.slice, kPads);
}

void padSlicesRead(const uint8_t* slices)
{
    for (int i = 0; i < kPads; i++)
        mtProject.pads.slice[i] = (slices && slices[i] <= MAX_SLICE_NUMBER) ? slices[i] : 0;
}

// TC on: the nearest row; TC off: the row it fell in + its micromove (as live REC)
void mpdjCaptureRow(int64_t at, int rows, bool tc, int* row, int8_t* micromove)
{
    *row = (int)((tc ? (at + 24) / 48 : at / 48) % rows);
    *micromove = tc ? -1 : sequencer.mpdjMicromoveFor((int)(at % 48));
}

static char s_captureMsg[40];

const char* mpdjCapture()
{
#ifdef PLATFORM_MPC
    int toTrack = mtConfig.general.captureTo ? s_cursorTrack : -1;
#else
    int toTrack = -1;
#endif
    if (!s_captureCount)
        return "Nothing to capture";
    uint32_t pass;
    int row, u, last;
    sequencer.mpdjPlayhead(0, &pass, &row, &u, &last);
    int rows = last + 1;
    // the last pattern-length of playing, ending at the newest hit
    int64_t from = s_captureLast - (int64_t)rows * 48 + 24;
    newFileManager.storePatternUndoRevision();
    int written = 0, skipped = 0;
    for (int k = 0; k < s_captureCount; k++)
    {
        const CaptureHit& h = s_capture[(s_captureHead - s_captureCount + k + kCaptureMax) % kCaptureMax];
        if (h.at < from)
            continue;
        int r;
        int8_t mm;
        mpdjCaptureRow(h.at, rows, mpdjTcOn(), &r, &mm);
        int8_t vol = padVelocityToVolume() ? sequencer.mpdjVelocityToVolume(h.velo) : -1;
        uint8_t track = toTrack >= 0 ? (uint8_t)toTrack : h.track;
        bool ok = sequencer.mpdjAddHit(track, (uint8_t)r, h.program, h.note, vol, mm);
#ifdef PLATFORM_MPC
        if (recTrace())
            printf("capture: track %d row %d micro %d (%lld/48 after its start) %s\n", track + 1, r + 1, mm,
                   (long long)(h.at % ((int64_t)rows * 48)), ok ? "written" : "row taken");
#endif
        if (ok)
        {
            if (h.slice >= 0)
                sequencer.mpdjSetSlice(track, (uint8_t)r, (uint8_t)h.slice, true);
            sequencer.mpdjSetChance(track, (uint8_t)r, h.chance, true);
            written++;
        }
        else
            skipped++;
    }
    s_captureCount = 0;
    if (written)
        newFileManager.setActualPatternStructChanged();
    if (skipped)
        snprintf(s_captureMsg, sizeof s_captureMsg, "Captured %d hits (%d rows taken)", written, skipped);
    else
        snprintf(s_captureMsg, sizeof s_captureMsg, written == 1 ? "Captured 1 hit" : "Captured %d hits", written);
    return s_captureMsg;
}

static char s_toBankMsg[40];

const char* mpdjSliceLayerToBank(int layerBank, int bank)
{
    if (!s_sliceMode || s_slicePad < 0 || bank < 0 || bank >= kPads / kPadsPerBank)
        return nullptr;
    const strPadSlot src = *padSlot(s_slicePad);
    if (src.program >= kProgramSlots)
        return nullptr;
    const strInstrument& ins = mtProject.instrument[src.program];
    int first = layerBank * kPadsPerBank, n = 0;
    for (int i = 0; i < kPadsPerBank && first + i < ins.sliceNumber; i++, n++)
    {
        int dst = bank * kPadsPerBank + i;
        mtProject.pads.pad[dst] = src;
        mtProject.pads.color[dst] = mtProject.pads.color[s_slicePad];
        mtProject.pads.dynLoss[dst] = mtProject.pads.dynLoss[s_slicePad];
        mtProject.pads.curve[dst] = mtProject.pads.curve[s_slicePad];
        setPadSlice(dst, first + i);
    }
    if (!n)
    {
        snprintf(s_toBankMsg, sizeof s_toBankMsg, "No slices %d-%d", first + 1, first + kPadsPerBank);
        return s_toBankMsg;
    }
    newFileManager.setProjectStructChanged();
    snprintf(s_toBankMsg, sizeof s_toBankMsg, "Slices %d-%d -> bank %c", first + 1, first + n, 'A' + bank);
    return s_toBankMsg;
}

void padDynamicsWrite(uint8_t* dynLoss, uint8_t* curve)
{
    memcpy(dynLoss, mtProject.pads.dynLoss, kPads);
    memcpy(curve, mtProject.pads.curve, kPads);
}

void padDynamicsRead(const uint8_t* dynLoss, const uint8_t* curve)
{
    for (int i = 0; i < kPads; i++)
    {
        mtProject.pads.dynLoss[i] = (dynLoss && dynLoss[i] <= 100) ? dynLoss[i] : 0;
        mtProject.pads.curve[i] = (curve && curve[i] < kPadCurveSettings) ? curve[i] : 0;
    }
}

// whose dynamics a press uses: the slice layer's source pad, Notes mode's (the last pad-map
// pad played, as its program is), else the pad itself
int dynamicsPad(int pad)
{
    if (s_sliceMode && s_slicePad >= 0)
        return s_slicePad;
    if (s_notesMode)
        return s_lastPad;
    return pad;
}

uint8_t padVelocityFor(int pad, uint8_t program, uint8_t velocity)
{
    if (!mpdjOn(MpdjFeature::Dynamics))
        pad = -1; // Config's curve, the whole range
    // 16 Levels sets the velocities itself: the pad's own dynamics stay out of it
    if (pad < 0 || pad >= kPads || s_levelsMode)
        return padVelocity(program, velocity);
#ifdef PLATFORM_MPC
    uint8_t curve = padCurveSetting(pad) ? padCurveSetting(pad) - 1 : mtConfig.general.padCurve;
    int top = 10 + 5 * mtConfig.general.padTop;
#else
    uint8_t curve = padCurveSetting(pad) ? padCurveSetting(pad) - 1 : 1;
    int top = 100;
#endif
    return padVelocityScaleDyn(velocity, sequencer.getInstrumentVelo(program), curve, top, padDynamics(pad));
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
    stopRepeat(pad); // no retrigger after the note off below
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

    // REC + playing, Gate pad (5b-4b) or a Roll (5b-7c): the release writes an OFF
    if ((s_held[pad].gate || s_held[pad].rolled) && s_held[pad].recorded && s_lastRecPad[track] == pad)
    {
        bool hadNote = false;
        int row = sequencer.mpdjPadRecOff(track, &hadNote, s_held[pad].cut);
#ifdef PLATFORM_MPC
        if (recTrace())
        {
            char name[4];
            padName(pad, name);
            if (row >= 0)
                printf("pad rec: %s release -> track %d row %d: %s\n", name, track + 1, row,
                       s_held[pad].cut ? "CUT" : "OFF");
            else
                printf("pad rec: %s release -> track %d: no OFF (%s)\n", name, track + 1,
                       hadNote ? "a note is on that row" : "not recording");
        }
#endif
    }
    Held& h = s_held[pad];
    h.track = -1;
    h.gate = h.cut = h.recorded = h.rolled = false;
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

void padColorsWrite(uint8_t* out)
{
    memcpy(out, mtProject.pads.color, sizeof(mtProject.pads.color));
}

void padColorsRead(const uint8_t* in)
{
    for (int i = 0; i < kPads; i++)
        mtProject.pads.color[i] = (in && in[i] <= kPadColorChoices) ? in[i] : 0;
}

uint8_t padColor(int pad)
{
    return (pad >= 0 && pad < kPads) ? mtProject.pads.color[pad] : 0;
}

void setPadColor(int pad, uint8_t color)
{
    if (pad >= 0 && pad < kPads && color <= kPadColorChoices)
        mtProject.pads.color[pad] = color;
}

uint8_t padColorStep(uint8_t color, int dir, bool wrap)
{
    static const uint8_t kOrder[kPadColorChoices + 1] = { 0, 17, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18 };
    constexpr int n = kPadColorChoices + 1;
    int pos = 0;
    for (int i = 0; i < n; i++)
        if (kOrder[i] == color)
            pos = i;
    pos += dir > 0 ? 1 : -1;
    if (wrap)
        pos = (pos + n) % n;
    else
        pos = pos < 0 ? 0 : pos >= n ? n - 1 : pos;
    return kOrder[pos];
}

const uint8_t* padColorRgb(int pad, uint8_t track)
{
    uint8_t c = padColor(pad);
    return c ? kPadPalette[c - 1] : padAutoColorRgb(pad, track);
}

const uint8_t* padAutoColorRgb(int pad, uint8_t track)
{
    // the rainbow: pad 1 (bottom left) red .. pad 16 (top right) violet, one hue per diagonal
    static const uint8_t kRainbow[7][3] = {
        { 127, 0, 0 }, { 127, 60, 0 }, { 120, 120, 0 }, { 0, 127, 20 },
        { 0, 100, 127 }, { 20, 20, 127 }, { 100, 0, 127 },
    };
    switch (mtConfig.general.padColorMode)
    {
        case kPadAutoRainbow:
        {
            int n = (pad < 0 ? 0 : pad) % 16; // row from the bottom = n / 4, column = n % 4
            return kRainbow[n / 4 + n % 4];
        }
        case kPadAutoTrack: return kPadPalette[track % kPadColors];
        default:
            return kPadPalette[mtConfig.general.padDefaultColor < kPadColorChoices ? mtConfig.general.padDefaultColor
                                                                                   : kPadDefaultColor];
    }
}

const char* padAutoColorName(uint8_t track)
{
    switch (mtConfig.general.padColorMode)
    {
        case kPadAutoRainbow: return "Rainbow";
        case kPadAutoTrack: return kPadColorNames[track % kPadColors + 1];
        default:
            return kPadColorNames[(mtConfig.general.padDefaultColor < kPadColorChoices ? mtConfig.general.padDefaultColor
                                                                                        : kPadDefaultColor) +
                                  1];
    }
}

void padsRead(const strPadSlot* in)
{
    strPadMap def;
    def.setDefault();
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
    //    program 16+N -> track N ...; sample pitch; Auto; no groups or cut (5d-5)
    strPadMap def;
    strPadMap hats = def; // the tests' hats: A03, A04, A12 in group 1 with cut
    for (int i : { 2, 3, 11 })
        hats.pad[i].mode = padModeByte(kPadModeAuto, 1, true);
    for (int b = 0; b < kPadBanks; b++)
        for (int n = 1; n <= kPadsPerBank; n++)
        {
            int i = padIndex(b, n);
            check(def.pad[i].program == b * 16 + n - 1, "default program", i);
            check(def.pad[i].track == n - 1, "default track (banks share tracks)", i);
            check(def.pad[i].note == kPadNoteDefault, "default note", i);
            check(padModeOf(def.pad[i].mode) == kPadModeAuto, "default mode", i);
            check(def.pad[i].mode == kPadModeAuto, "default: no group, no cut", i);
        }
    char name[4];
    padName(padIndex(0, 1), name);
    check(strcmp(name, "A01") == 0, "name A01", 0);
    padName(padIndex(3, 16), name);
    check(strcmp(name, "D16") == 0, "name D16", 63);
    check(padSlot(-1) == nullptr && padSlot(kPads) == nullptr, "padSlot range", -1);

    // 1b. Group bits don't change the play mode
    mtProject.pads = def;
    for (int i : { 2, 3, 11 })
        setPadGroup(i, 1);
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
    mtProject.pads = hats;
    check(padGroupFor(2, 2) == 1 && padGroupFor(3, 3) == 1 && padGroupFor(11, 11) == 1, "hats by (program, track)", -1);
    check(padGroupFor(0, 0) == 0 && padGroupFor(2, 3) == 0, "no group / no pad", -1);
    mtProject.pads.pad[padIndex(1, 5)] = { 2, 2, -1, padModeByte(kPadModeAuto, 7) }; // B05: closed hat, group 7
    check(padGroupFor(2, 2) == 1, "A03 comes before B05", -1);
    mtProject.pads.pad[2].program = 5; // A03 now plays something else
    check(padGroupFor(2, 2) == 7, "then B05 decides", -1);
    mtProject.pads.pad[padIndex(0, 16)] = { 2, 2, -1, kPadModeAuto }; // A16: same pair, no group
    check(padGroupFor(2, 2) == 0, "A16 (no group) comes before B05", -1);

    // 2. Round trip through the MPDJ block (version 4), groups included
    mtProject.pads = hats;
    mtProject.pads.pad[0] = { 5, 2, 48, kPadModeGate };          // A01: program 06, track 3, C-3
    mtProject.pads.pad[17] = { kPadNoProgram, 15, -1, kPadModeOneShot }; // B02: nothing, track 16
    mtProject.pads.pad[63] = { 63, 0, 127, kPadModeAuto };       // D16 -> track 1
    mtProject.pads.pad[20] = { 20, 4, -1, padModeByte(kPadModeGate, 31) }; // B05: Gate, group 31
    strMpdjProjectData file;
    programsWriteToFile(file);
    check(file.version == kMpdjProjectDataVersion, "block version", -1);
    check(file.flags == 0, "FULL LEVEL off -> flags 0", -1);
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

    // 2c. Pad colors (5c-2): round trip; a version 3 block or bad values -> Auto
    mtProject.pads = def;
    // Auto follows Config "Pad colors" (2026-11-16): One color (Light gray by default), Rainbow, Track
    uint8_t keepMode = mtConfig.general.padColorMode, keepDefault = mtConfig.general.padDefaultColor;
    mtConfig.general.padColorMode = kPadAutoOne;
    mtConfig.general.padDefaultColor = kPadDefaultColor;
    check(padColor(5) == 0 && padColorRgb(5, 3) == kPadPalette[17] && padColorRgb(40, 9) == kPadPalette[17],
          "Auto = One color, Light gray", 5);
    mtConfig.general.padDefaultColor = 6;
    check(padColorRgb(5, 3) == kPadPalette[6], "One color follows Pad default color", 5);
    mtConfig.general.padColorMode = kPadAutoRainbow;
    check(padColorRgb(0, 3)[0] == 127 && padColorRgb(0, 3)[1] == 0 && padColorRgb(15, 3)[2] == 127 &&
            padColorRgb(3, 0) == padColorRgb(12, 0) && padColorRgb(16, 0) == padColorRgb(0, 0) &&
            padColorRgb(1, 0) != padColorRgb(0, 0),
          "Rainbow: red bottom left, violet top right, one hue per diagonal, every bank alike", 5);
    mtConfig.general.padColorMode = kPadAutoTrack;
    check(padColorRgb(5, 3) == kPadPalette[3], "Track: the track's color", 5);
    mtConfig.general.padColorMode = keepMode;
    mtConfig.general.padDefaultColor = keepDefault;
    setPadColor(5, 7);
    setPadColor(63, 16);
    setPadColor(6, 19); // out of range: ignored
    check(padColor(5) == 7 && padColorRgb(5, 3) == kPadPalette[6] && padColor(6) == 0, "set color", 5);
    setPadColor(7, 17); // Red (2026-11-16)
    check(padColor(7) == 17 && padColorRgb(7, 0)[0] == 127 && padColorRgb(7, 0)[1] == 0 &&
            padColorRgb(7, 0)[2] == 0,
          "Red", 7);
    check(padColorStep(0, 1, false) == 17 && padColorStep(17, 1, false) == 1 && padColorStep(1, -1, false) == 17 &&
            padColorStep(16, 1, false) == 18 && padColorStep(18, 1, false) == 18 && padColorStep(18, 1, true) == 0 &&
            padColorStep(0, -1, true) == 18,
          "color order Auto, Red, Red-orange .. White, Light gray", 7);
    setPadColor(7, 0);
    // per-pad dynamics + curve (2026-11-16)
    check(padDynamics(9) == 100 && padCurveSetting(9) == 0, "dynamics default 100 %, Global", 9);
    check(padVelocityScaleDyn(64, 50, 1, 100, 100) == padVelocityScale(64, 50, 1, 100), "100 % = as before", 9);
    check(padVelocityScaleDyn(1, 50, 1, 100, 0) == 50 && padVelocityScaleDyn(127, 50, 1, 100, 0) == 50,
          "0 %: every hit at the top", 9);
    check(padVelocityScaleDyn(0, 50, 1, 100, 50) == 25 && padVelocityScaleDyn(127, 50, 1, 100, 50) == 50,
          "50 %: the softest hit at half", 9);
    setPadDynamics(9, 40);
    setPadCurveSetting(9, 3);
    setPadDynamics(10, 130); // clamps to 100
    check(padDynamics(9) == 40 && padCurveSetting(9) == 3 && padDynamics(10) == 100, "set dynamics / curve", 9);
    programsWriteToFile(file);
    padDynamicsRead(nullptr, nullptr);
    check(padDynamics(9) == 100 && padCurveSetting(9) == 0, "older file: defaults", 9);
    programsReadFromFile(file);
    check(padDynamics(9) == 40 && padCurveSetting(9) == 3, "dynamics round trip", 9);
    file.version = 7;
    programsReadFromFile(file);
    check(padDynamics(9) == 100 && padCurveSetting(9) == 0, "version 7 block: defaults", 9);
    // pad slices (version 9, slice layer to bank)
    file.version = kMpdjProjectDataVersion;
    setPadSlice(20, 5);
    check(padSlice(20) == 5 && padSlice(21) == -1, "set pad slice", 20);
    programsWriteToFile(file);
    setPadSlice(20, -1);
    programsReadFromFile(file);
    check(padSlice(20) == 5, "pad slice round trip", 20);
    file.version = 8;
    programsReadFromFile(file);
    check(padSlice(20) == -1, "version 8 block: no pad slices", 20);
    setPadSlice(20, -1);
    file.version = kMpdjProjectDataVersion;
    setPadDynamics(9, 100);
    setPadCurveSetting(9, 0);
    programsWriteToFile(file);
    mtProject.pads.color[5] = mtProject.pads.color[63] = 0;
    programsReadFromFile(file);
    check(padColor(5) == 7 && padColor(63) == 16, "color round trip", 63);
    file.padColor[9] = 40;
    programsReadFromFile(file);
    check(padColor(9) == 0, "bad color -> Auto", 9);
    file.version = 3;
    programsReadFromFile(file);
    check(padColor(5) == 0 && padColor(63) == 0, "version 3 block -> Auto", 5);
    file.version = 4;
    file.padColor[9] = 0;
    mtProject.pads = def;
    programsWriteToFile(file);

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
        check(memcmp(&mtProject.pads.pad[i], &def.pad[i], 4) == 0, "bad entry -> plain default", i);

    // 4. Older MPDJ blocks (version 1, 2) and Tracker+ projects: the default
    //    map without groups (existing projects keep no groups)
    for (uint8_t ver = 1; ver <= 2; ver++)
    {
        file.version = ver;
        mtProject.pads.pad[9].track = 12;
        programsReadFromFile(file);
        for (int i = 0; i < kPads; i++)
            check(memcmp(&mtProject.pads.pad[i], &def.pad[i], 4) == 0,
                  ver == 1 ? "version 1 block -> default map, no groups" : "version 2 block -> default map, no groups", i);
    }
    strMpdjProjectData none = {};
    mtProject.pads.pad[9].track = 12;
    programsReadFromFile(none);
    programsTakeImportNotice();
    for (int i = 0; i < kPads; i++)
        check(memcmp(&mtProject.pads.pad[i], &def.pad[i], 4) == 0, "Tracker+ project -> default map, no groups", i);
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

// ---- ERASE + pad (5f-3) ------------------------------------------------------
// one entry per pad held with ERASE: track << 16 | program << 8 | (note + 1), 0 = none
static std::atomic<uint32_t> s_erase[kPads];
static bool s_eraseUndoStored = false;
static char s_eraseMsg[40];

// bits: 31 set, 24-30 slice + 1 (0 = any), 16-23 track, 8-15 program, 0-7 note + 1 (0 = any)
static uint32_t erasePack(const strPadSlot& s, bool withNote, int slice = -1)
{
    return 0x80000000u | (uint32_t)(slice >= 0 ? (slice + 1) & 0x7F : 0) << 24 |
           (uint32_t)s.track << 16 | (uint32_t)s.program << 8 |
           (uint32_t)(withNote && s.note >= 0 ? s.note + 1 : 0);
}

const char* mpdjErasePadDown(int pad)
{
    if (pad < 0 || pad >= kPads)
        return nullptr;
    strPadSlot slot;
    uint8_t vel = 0;
    const char* msg;
    if (s_levelsMode || !resolvePad(pad, &slot, &vel, &msg) || slot.program == kPadNoProgram)
        return nullptr;
    // slice layer (2026-11-16, Mike): only that slice's hits (Slice: its Slice FX;
    // Beat Slice: its note)
    int slice = s_resolvedSlice; // the slice layer, or a pad written from it
    bool withNote = s_notesMode || ((s_sliceMode || padSlice(pad) >= 0) && slice < 0);
    int note = withNote ? slot.note : -1;
    char name[12];
    if (s_sliceMode)
        snprintf(name, sizeof name, "Slice %d", pad + 1);
    else
        padName(pad, name);
    if (sequencer.isPlay())
    {
        if (!s_eraseUndoStored)
        {
            newFileManager.storePatternUndoRevision();
            s_eraseUndoStored = true;
        }
        s_erase[pad].store(erasePack(slot, withNote, slice));
        snprintf(s_eraseMsg, sizeof s_eraseMsg, "Erasing %s", name);
        return s_eraseMsg;
    }
    newFileManager.storePatternUndoRevision();
    Sequencer::strPattern* pt = sequencer.getPatternToUI();
    int n = 0;
    for (int row = 0; row <= pt->track[0].length; row++)
        n += sequencer.mpdjEraseHit(pt, slot.track, row, slot.program, note, slice);
    if (n)
        newFileManager.setActualPatternStructChanged();
    snprintf(s_eraseMsg, sizeof s_eraseMsg, n == 1 ? "%s: 1 hit erased" : "%s: %d hits erased", name, n);
    return s_eraseMsg;
}

void mpdjErasePadUp(int pad)
{
    if (pad >= 0 && pad < kPads)
        s_erase[pad].store(0);
}

void mpdjEraseEnd()
{
    for (auto& e : s_erase)
        e.store(0);
    s_eraseUndoStored = false;
}

void mpdjEraseTick(int row)
{
    for (int p = 0; p < kPads; p++)
    {
        uint32_t e = s_erase[p].load();
        if (!e)
            continue;
        int track = (e >> 16) & 0xFF, program = (e >> 8) & 0xFF, note = (int)(e & 0xFF) - 1;
        int slice = (int)((e >> 24) & 0x7F) - 1;
        if (sequencer.mpdjEraseHit(sequencer.getPatternToUI(), track, row, program, note, slice))
            s_odErased.store(true); // the UI marks the pattern changed (mpdjOverdubPoll)
    }
}

// ---- STEP SEQ (5f-4) -----------------------------------------------------------
static int s_stepPad = -1;

int mpdjLastPadIndex() { return s_lastPad; } // MPDJ Kits: Save Kit's default bank
int mpdjStepSeqPad() { return s_stepPad >= 0 ? s_stepPad : (s_lastPad >= 0 ? s_lastPad : 0); }

void mpdjStepSeqSetPad(int pad)
{
    if (pad >= 0 && pad < kPads)
        s_stepPad = pad;
}

bool mpdjStepSeqHas(int row)
{
    const strPadSlot* p = padSlot(mpdjStepSeqPad());
    Sequencer::strPattern* pt = sequencer.getPatternToUI();
    if (!p || p->program == kPadNoProgram || !pt || row < 0 || row > pt->track[0].length)
        return false;
    const auto& st = pt->track[p->track].step[row];
    return st.note >= 0 && st.instrument == p->program;
}

int mpdjStepSeqToggle(int row, uint8_t velocity)
{
    const strPadSlot* p = padSlot(mpdjStepSeqPad());
    Sequencer::strPattern* pt = sequencer.getPatternToUI();
    if (!p || p->program == kPadNoProgram || programType(p->program) == kProgramEmpty || !pt || row < 0 ||
        row > pt->track[0].length)
        return -1;
    if (mpdjStepSeqHas(row))
    {
        newFileManager.storePatternUndoRevision();
        sequencer.mpdjEraseHit(pt, p->track, row, p->program, -1);
        newFileManager.setActualPatternStructChanged();
        return 0;
    }
    uint8_t note = p->note < 0 ? (uint8_t)Sequencer::STEP_NOTE_DEFAULT : (uint8_t)p->note;
    if (mtProject.mpdjFullLevel)
        velocity = 127;
    uint8_t velo = padVelocityFor(mpdjStepSeqPad(), p->program, velocity ? velocity : 1);
    if (sequencer.mpdjPadStepWrite(p->track, (uint8_t)row, p->program, note, velo, padVelocityToVolume()) < 0)
        return -1;
    sequencer.mpdjSetChance(p->track, (uint8_t)row, (uint8_t)padChance(mpdjStepSeqPad()), true);
    return 1;
}
