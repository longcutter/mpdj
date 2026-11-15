// MPDJ additions (MIT, see NOTICE.md): slice rules for MPC-style slicing (step 5g)
#include "mpdjSlices.h"
#include "mtStructs.h"

uint32_t mpdjOutputLatencyFrames = 0;

uint16_t mpdjSliceTrimEnd(const strInstrument* ins)
{
    if (ins->sliceNumber == 0)
        return ins->endPoint > 0 ? ins->endPoint : MAX_16BIT;
    uint16_t last = ins->slices[ins->sliceNumber - 1];
    return ins->endPoint > last ? ins->endPoint : MAX_16BIT;
}

uint16_t mpdjSliceEnd(const strInstrument* ins, int i)
{
    if (i < 0 || ins->sliceNumber == 0)
        return mpdjSliceTrimEnd(ins);
    if (i + 1 < ins->sliceNumber)
        return ins->slices[i + 1];
    return mpdjSliceTrimEnd(ins);
}

int mpdjSliceAt(const strInstrument* ins, uint16_t pos)
{
    int at = 0;
    for (int i = 0; i < ins->sliceNumber; i++)
        if (ins->slices[i] <= pos)
            at = i;
    return at;
}

static uint16_t clampMove(uint16_t v, int32_t delta, int32_t lo, int32_t hi)
{
    int32_t n = (int32_t)v + delta;
    if (n < lo)
        n = lo;
    if (n > hi)
        n = hi;
    return (uint16_t)n;
}

bool mpdjSliceMoveStart(strInstrument* ins, int i, int32_t delta)
{
    if (i < 0 || i >= ins->sliceNumber)
        return false;
    int32_t lo = i > 0 ? ins->slices[i - 1] + 1 : 0;
    int32_t hi = (int32_t)mpdjSliceEnd(ins, i) - 1;
    if (hi < lo)
        return false;
    uint16_t n = clampMove(ins->slices[i], delta, lo, hi);
    if (n == ins->slices[i])
        return false;
    ins->slices[i] = n;
    return true;
}

bool mpdjSliceMoveEnd(strInstrument* ins, int i, int32_t delta)
{
    if (i < 0 || i >= ins->sliceNumber)
        return false;
    if (i + 1 < ins->sliceNumber)
        return mpdjSliceMoveStart(ins, i + 1, delta); // linked: the next slice's start
    // the last slice: End
    uint16_t cur = mpdjSliceTrimEnd(ins);
    uint16_t n = clampMove(cur, delta, ins->slices[i] + 1, MAX_16BIT);
    if (n == ins->endPoint && n == cur)
        return false;
    ins->endPoint = n;
    return n != cur;
}

void mpdjSliceEnsureOne(strInstrument* ins)
{
    if (ins->sliceNumber > 0)
        return;
    ins->sliceNumber = 1;
    ins->slices[0] = 0;
    ins->selectedSlice = 0;
    if (ins->endPoint == 0)
        ins->endPoint = MAX_16BIT;
}

#ifdef PLATFORM_MPC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int slicesSelfTest()
{
    const char* v = getenv("MPDJ_SLICES_TEST");
    if (!v || !*v || *v == '0')
        return -1;
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        printf("slices test: %s %s\n", ok ? "ok  " : "FAIL", what);
        if (!ok)
            failures++;
    };

    static strInstrument ins; // big; keep it off the stack
    memset(&ins, 0, sizeof(ins));
    ins.endPoint = MAX_16BIT;

    check(mpdjSliceTrimEnd(&ins) == MAX_16BIT, "no slices: trim end = file end");
    mpdjSliceEnsureOne(&ins);
    check(ins.sliceNumber == 1 && ins.slices[0] == 0, "ensure one: slice 1 at 0");
    check(mpdjSliceEnd(&ins, 0) == MAX_16BIT, "one slice ends at End");

    // four slices at 0, 1000, 2000, 3000, End 4000
    ins.sliceNumber = 4;
    for (int i = 0; i < 4; i++)
        ins.slices[i] = 1000 * i;
    ins.endPoint = 4000;
    check(mpdjSliceEnd(&ins, 1) == 2000 && mpdjSliceEnd(&ins, 3) == 4000, "ends are linked, last = End");
    check(mpdjSliceAt(&ins, 2500) == 2 && mpdjSliceAt(&ins, 0) == 0 && mpdjSliceAt(&ins, 9000) == 3,
          "slice at a position");

    mpdjSliceMoveStart(&ins, 1, -5000);
    check(ins.slices[1] == 1, "start clamped above the previous start");
    mpdjSliceMoveStart(&ins, 1, 50000);
    check(ins.slices[1] == 1999, "start clamped below its end");
    mpdjSliceMoveStart(&ins, 0, 50000);
    check(ins.slices[0] == 1998, "slice 1's start stops before slice 2");
    ins.slices[0] = 0;
    ins.slices[1] = 1000;

    mpdjSliceMoveEnd(&ins, 1, 300);
    check(ins.slices[2] == 2300, "moving slice 2's end moves slice 3's start");
    mpdjSliceMoveEnd(&ins, 3, -50000);
    check(ins.endPoint == 3001, "the last end stops after the last start");
    mpdjSliceMoveEnd(&ins, 3, 70000);
    check(ins.endPoint == MAX_16BIT, "the last end stops at the file end");

    ins.endPoint = 2000; // End before the last start (old project): the last slice runs to the file end
    check(mpdjSliceTrimEnd(&ins) == MAX_16BIT, "End before the last start is ignored");
    mpdjSliceMoveEnd(&ins, 3, -100);
    check(ins.endPoint == MAX_16BIT - 100, "moving it then starts from the file end");

    printf("slices test: %s (%d failure%s)\n", failures ? "FAILED" : "passed", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
#else
int slicesSelfTest() { return -1; }
#endif
