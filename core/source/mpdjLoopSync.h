// MPDJ additions (MIT, see NOTICE.md): Loop Sync (step 5g-6).
//
// A program in a loop play mode (Forward / Backward / Pingpong) can have its loop
// length locked to a note value at the project tempo: Loop End = Loop Start + the
// length, made up for the program's Tune / Fine tune so the loop lasts that long
// when it plays (notes at other pitches still change it). Not time-stretching:
// the audio plays at its own speed, only the loop's length is in time. Saved per
// program in the MPDJ block's program slots (strProgramSlot::loopSync, version 6).
#ifndef MPDJ_LOOP_SYNC_H
#define MPDJ_LOOP_SYNC_H

#include <stdint.h>

static constexpr uint8_t kLoopSyncCount = 9; // Free, 1/32 .. 4 bars
const char* loopSyncName(uint8_t v);
// The loop end Loop Sync wants for program idx, or -1 (Free, not a loop mode, no sample)
int32_t loopSyncEnd(int idx);
// Set Loop End where Loop Sync wants it; true if it moved
bool loopSyncApply(int idx);
// Every program, at most every 200 ms (tempo and Tune changes); returns a bit per
// changed program in the first 32 / ... (only the caller's refresh needs it: -1 = some)
bool loopSyncPollAll(uint32_t nowMs);

#endif
