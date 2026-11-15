// MPDJ additions (MIT, see NOTICE.md): Loop Sync (step 5g-6), see mpdjLoopSync.h
#include "mpdjLoopSync.h"
#include "mpdjPrograms.h"
#include "mtAudioEngine.h"
#include "mtStructs.h"
#include "fileManager.h"
#include <math.h>

static const char* const kNames[kLoopSyncCount] = { "Free", "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
static const float kQuarters[kLoopSyncCount] = { 0, 0.125f, 0.25f, 0.5f, 1, 2, 4, 8, 16 }; // 4/4

const char* loopSyncName(uint8_t v)
{
    return kNames[v < kLoopSyncCount ? v : 0];
}

int32_t loopSyncEnd(int idx)
{
    if (idx < 0 || idx >= kProgramSlots)
        return -1;
    uint8_t v = mtProject.programs.slot[idx].loopSync;
    const strInstrument& ins = mtProject.instrument[idx];
    if (v == 0 || v >= kLoopSyncCount || !ins.isActive || ins.sample.length == 0)
        return -1;
    if (ins.playMode != playModeLoopForward && ins.playMode != playModeLoopBackward &&
        ins.playMode != playModePingpong)
        return -1;
    float tempo = mtProject.values.globalTempo > 0 ? mtProject.values.globalTempo : DEFAULT_TEMPO;
    float seconds = 60.0f / tempo * kQuarters[v];
    float rate = powf(2.0f, (ins.tune + ins.fineTune / 100.0f) / 12.0f); // Tune makes it play faster
    float frames = seconds * 44100.0f * rate;
    int32_t len = (int32_t)lrintf(frames / (float)ins.sample.length * MAX_16BIT);
    if (len < 1)
        len = 1;
    int32_t end = ins.loopPoint1 + len;
    if (end >= ins.endPoint)
        end = ins.endPoint - 1; // no room: the loop runs to End
    if (end <= ins.loopPoint1)
        end = ins.loopPoint1 + 1;
    return end;
}

bool loopSyncApply(int idx)
{
    int32_t end = loopSyncEnd(idx);
    if (end < 0 || end == mtProject.instrument[idx].loopPoint2)
        return false;
    mtProject.instrument[idx].loopPoint2 = (uint16_t)end;
    for (uint8_t i = 0; i < audioEngine::kVoices; i++)
        instrumentPlayer[i].setRefreshBit(LP2_MASK); // voices playing it pick up the new end
    newFileManager.setInstrumentStructChanged(idx);
    return true;
}

bool loopSyncPollAll(uint32_t nowMs)
{
    static uint32_t last = 0;
    if (nowMs - last < 200)
        return false;
    last = nowMs;
    bool any = false;
    for (int i = 0; i < kProgramSlots; i++)
        if (mtProject.programs.slot[i].loopSync)
            any |= loopSyncApply(i);
    return any;
}
