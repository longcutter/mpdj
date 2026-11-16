// MPDJ (2026-11-16): MPDJ_BATCH_TEST=1 runs every stand-alone self-test (programs,
// pads, slices) and checks the logic behind the features built on 2026-11-16 that
// can run without the screen or audio: Chance conditions, Euclidean rotation,
// per-track swing's delay, Capture's row / micromove, the Turing Machine. Prints one
// line per check that fails and a summary; exits. MIT, see NOTICE.md (MPDJ additions).
#include "mpdjBatchTest.h"

#include "mpdjPads.h"
#include "mpdjPrograms.h"
#include "mpdjSlices.h"
#include "mtSequencer.h"
#include "mtStructs.h"
#include "turing/turing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_MPC
namespace
{
int s_fail = 0, s_pass = 0;
const char* s_section = "";

void check(bool ok, const char* what)
{
    if (ok)
        s_pass++;
    else
    {
        s_fail++;
        printf("batch test: FAIL [%s] %s\n", s_section, what);
    }
}

// run one of the older self-tests the way its env variable would
int runOld(const char* env, int (*fn)())
{
    setenv(env, "1", 1);
    int rc = fn();
    unsetenv(env);
    return rc;
}

void chance()
{
    s_section = "chance";
    auto& f = sequencer.fx;
    check(sequencer.mpdjTestChance(0, f.CHANCE_FIRST, 0, false), "1ST plays on the first pass");
    check(!sequencer.mpdjTestChance(0, f.CHANCE_FIRST, 1, false), "1ST silent on the second");
    check(!sequencer.mpdjTestChance(0, f.CHANCE_NOT_FIRST, 0, false), "!1S silent on the first");
    check(sequencer.mpdjTestChance(0, f.CHANCE_NOT_FIRST, 3, false), "!1S plays later");
    check(sequencer.mpdjTestChance(0, f.CHANCE_FILL, 2, true), "FIL plays with Fill");
    check(!sequencer.mpdjTestChance(0, f.CHANCE_FILL, 2, false), "FIL silent without");
    check(!sequencer.mpdjTestChance(0, f.CHANCE_NOT_FILL, 2, true), "!FL silent with Fill");
    check(sequencer.mpdjTestChance(0, f.CHANCE_NOT_FILL, 2, false), "!FL plays without");
    check(sequencer.mpdjTestChance(0, 100, 5, false), "100 % always plays");
    check(!sequencer.mpdjTestChance(0, 0, 5, false), "0 % never plays");
    check(f.CHANCE_MAX < 256, "Chance values fit a byte");
}

void euclid()
{
    s_section = "euclid";
    auto saved = sequencer.selection;
    sequencer.selection.firstStep = 0;
    sequencer.selection.lastStep = 15;
    auto hits = [](int rot) {
        sequencer.mpdjEuclidRotate = (int16_t)rot;
        uint32_t m = 0;
        for (int s = 0; s < 16; s++)
            if (sequencer.isEuclideanStep((uint8_t)s, 4))
                m |= 1u << s;
        sequencer.mpdjEuclidRotate = 0;
        return m;
    };
    check(hits(0) == 0x1111, "4 in 16: steps 1, 5, 9, 13");
    check(hits(2) == 0x4444, "rotate 2: steps 3, 7, 11, 15");
    check(hits(16) == hits(0), "rotate by the length = none");
    check(hits(17) == hits(1), "rotation wraps");
    sequencer.selection = saved;
}

void swing()
{
    s_section = "swing";
    check(Sequencer::mpdjSwingDelay(50) == 0, "50 % = no delay");
    check(Sequencer::mpdjSwingDelay(66) == 15, "66 % = 15 / 48 of a row");
    check(Sequencer::mpdjSwingDelay(75) == 24, "75 % = half a row");
}

void capture()
{
    s_section = "capture";
    int row;
    int8_t mm;
    mpdjCaptureRow(10 * 48 + 30, 32, true, &row, &mm);
    check(row == 11 && mm == -1, "TC on: a late hit goes to the next row");
    mpdjCaptureRow(10 * 48 + 10, 32, true, &row, &mm);
    check(row == 10 && mm == -1, "TC on: an early hit stays");
    mpdjCaptureRow(10 * 48 + 30, 32, false, &row, &mm);
    check(row == 10 && mm > 50, "TC off: its row + micromove");
    mpdjCaptureRow(31 * 48 + 40, 32, true, &row, &mm);
    check(row == 0, "TC on: the last row's late hit wraps to row 1");
    mpdjCaptureRow((int64_t)5 * 32 * 48 + 7 * 48, 32, true, &row, &mm);
    check(row == 7, "later passes fold onto the pattern");
}

void turingGen()
{
    s_section = "turing";
    uint8_t keepScale = mtProject.values.padBoardScale, keepRoot = mtProject.values.padBoardRootNote;
    mtProject.values.padBoardScale = 0; // chromatic
    int16_t a[64], b[64];
    cTuring::generate(0xA5C3, 8, 0, 100, 48, 2, 1234, 64, a);
    bool loops = true, inRange = true;
    for (int r = 0; r < 64; r++)
    {
        if (r >= 8 && a[r] != a[r - 8])
            loops = false;
        if (a[r] < 48 || a[r] >= 72)
            inRange = false;
    }
    check(loops, "Change 0 (locked): an 8-step loop repeats");
    check(inRange, "Density 100: every row a note inside Low + Range");
    cTuring::generate(0xA5C3, 8, 0, 0, 48, 2, 1234, 64, b);
    bool none = true;
    for (int r = 0; r < 64; r++)
        none = none && b[r] < 0;
    check(none, "Density 0: no hits");
    cTuring::generate(0xA5C3, 8, 40, 50, 48, 2, 99, 64, a);
    cTuring::generate(0xA5C3, 8, 40, 50, 48, 2, 99, 64, b);
    check(memcmp(a, b, sizeof a) == 0, "same seed + settings = same rows");
    mtProject.values.padBoardScale = 2; // Major
    mtProject.values.padBoardRootNote = 0;
    cTuring::generate(0x1234, 16, 30, 100, 48, 3, 7, 64, a);
    static const bool kMajor[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
    bool inScale = true;
    for (int r = 0; r < 64; r++)
        if (a[r] >= 0 && !kMajor[a[r] % 12])
            inScale = false;
    check(inScale, "C major: only scale notes");
    mtProject.values.padBoardScale = keepScale;
    mtProject.values.padBoardRootNote = keepRoot;
}
} // namespace

int batchSelfTest()
{
    const char* v = getenv("MPDJ_BATCH_TEST");
    if (!v || !*v || *v == '0')
        return -1;
    printf("batch test: the older self-tests first\n");
    int older = 0;
    older += runOld("MPDJ_PROGRAMS_TEST", programsSelfTest) != 0;
    older += runOld("MPDJ_PADS_TEST", padsSelfTest) != 0;
    older += runOld("MPDJ_SLICES_TEST", slicesSelfTest) != 0;
    chance();
    euclid();
    swing();
    capture();
    turingGen();
    printf("batch test: programs / pads / slices %s; %d checks passed, %d failed\n",
           older ? "FAILED (see above)" : "passed", s_pass, s_fail);
    printf("batch test: %s\n", (older || s_fail) ? "FAILED" : "passed");
    return (older || s_fail) ? 1 : 0;
}
#else
int batchSelfTest() { return -1; }
#endif
