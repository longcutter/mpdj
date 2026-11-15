// MPDJ additions (MIT, see NOTICE.md): MPC-style slicing on the Sample Playback screen (step 5g).
//
// The Tracker stores a program's slices as start points only (`slices[]`, sorted,
// `sliceNumber` of them, 1/65536 of the sample). MPDJ keeps that model and the
// files as they are; slices are linked: slice i ends where slice i + 1 starts,
// and the last one ends at the program's End point (`endPoint`, before 5g the
// last slice always ran to the end of the file). Play Thru plays to End too.
#ifndef MPDJ_SLICES_H
#define MPDJ_SLICES_H

#include <stdint.h>

struct strInstrument;

// Where slice i ends (1/65536 of the sample)
uint16_t mpdjSliceEnd(const strInstrument* ins, int i);
// End of the last slice: the program's End, or the file's end if End isn't past the last start
uint16_t mpdjSliceTrimEnd(const strInstrument* ins);
// The slice that contains pos (0 when there are none)
int mpdjSliceAt(const strInstrument* ins, uint16_t pos);

// Move slice i's start / end by delta, clamped by its neighbours (linked: moving an
// end moves the next slice's start; the last slice's end is End). Return true if it moved
bool mpdjSliceMoveStart(strInstrument* ins, int i, int32_t delta);
bool mpdjSliceMoveEnd(strInstrument* ins, int i, int32_t delta);
// No slices yet: make one covering the whole sample (slice 1 at 0, selected)
void mpdjSliceEnsureOne(strInstrument* ins);

// MPDJ_SLICES_TEST=1: checks the rules above, prints, exits (-1 = not asked)
int slicesSelfTest();

#endif
