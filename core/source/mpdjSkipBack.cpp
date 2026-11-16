// MPDJ (2026-11-16): skip-back sampling, see mpdjSkipBack.h
#include "mpdjSkipBack.h"

#include "fileManager.h"
#include "mpdjPrograms.h"
#include "mtStructs.h"

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <optional>

namespace
{
constexpr uint32_t kRate = 44100, kSeconds = 60, kFrames = kRate * kSeconds;
int16_t s_ring[kFrames * 2]; // interleaved
std::atomic<uint32_t> s_write{ 0 }; // frames written in all (wraps the ring)
// the saved copy, as the file manager wants it (separate channels, kept until it's done)
int16_t s_left[kFrames], s_right[kFrames];
bool s_saving = false;
char s_msg[64];
char s_dir[] = "/Samples/SkipBack";
char s_name[32];
int s_slot = -1;
float s_seconds = 0;

constexpr int kThreshold = 100;            // ~ -50 dBFS
constexpr uint32_t kPre = kRate / 20;      // 50 ms before the first sound
constexpr uint32_t kPost = kRate / 5;      // 200 ms after the last
} // namespace

void mpdjSkipBackPush(const int16_t* in, unsigned frames)
{
    uint32_t w = s_write.load(std::memory_order_relaxed);
    for (unsigned i = 0; i < frames; i++)
    {
        uint32_t at = (w + i) % kFrames;
        s_ring[2 * at] = in[2 * i];
        s_ring[2 * at + 1] = in[2 * i + 1];
    }
    s_write.store(w + frames, std::memory_order_release);
}

bool mpdjSkipBackTrim(const int16_t* l, const int16_t* r, uint32_t frames, uint32_t* from, uint32_t* to)
{
    uint32_t a = 0, b = frames;
    while (a < frames && abs(l[a]) < kThreshold && abs(r[a]) < kThreshold)
        a++;
    if (a == frames)
        return false;
    while (b > a && abs(l[b - 1]) < kThreshold && abs(r[b - 1]) < kThreshold)
        b--;
    *from = a > kPre ? a - kPre : 0;
    *to = b + kPost < frames ? b + kPost : frames;
    return true;
}

const char* mpdjSkipBackSave()
{
    if (s_saving)
        return "Skip-back: still saving";
    uint32_t w = s_write.load(std::memory_order_acquire);
    // the last 59.5 s (the oldest half second may be overwritten while copying)
    uint32_t have = w < kFrames - kRate / 2 ? w : kFrames - kRate / 2;
    if (have == 0)
        return "Skip-back: no input yet";
    for (uint32_t i = 0; i < have; i++)
    {
        uint32_t at = (w - have + i) % kFrames;
        s_left[i] = s_ring[2 * at];
        s_right[i] = s_ring[2 * at + 1];
    }
    uint32_t from, to;
    if (!mpdjSkipBackTrim(s_left, s_right, have, &from, &to))
        return "Skip-back: nothing heard in the last minute";
    // the first empty program slot (none: only the file)
    s_slot = -1;
    for (int i = 0; i < kProgramSlots; i++)
        if (!mtProject.instrument[i].isActive && programType(i) == kProgramEmpty)
        {
            s_slot = i;
            break;
        }
    // a name not taken yet in the folder
    FileSystem* fs = newFileManager.getFileSystem();
    for (int n = 1; n < 1000; n++)
    {
        char path[96];
        snprintf(s_name, sizeof s_name, "Skip %03d", n);
        snprintf(path, sizeof path, "%s/%s.wav", s_dir, s_name);
        if (!fs->exists(path))
            break;
    }
    s_seconds = (to - from) / (float)kRate;
    std::optional<int8_t> slot;
    if (s_slot >= 0)
        slot = (int8_t)s_slot;
    if (!newFileManager.saveRecordedSound(s_dir, s_name, slot, s_left + from, s_right + from, to - from))
        return "Skip-back: busy, try again";
    s_saving = true;
    snprintf(s_msg, sizeof s_msg, "Skip-back: saving %.1f s", s_seconds);
    return s_msg;
}

const char* mpdjSkipBackPoll()
{
    if (!s_saving)
        return nullptr;
    uint8_t st = newFileManager.getStatus();
    if (st == fmSavingRecordedSound || st == fmSavingImportingRecordedSound)
        return nullptr; // still going
    // ended (the screen on show may have taken the End status already)
    if (st < fmError)
    {
        if (st == fmSaveRecordedSoundEnd)
            newFileManager.clearStatus();
        s_saving = false;
        if (s_slot >= 0)
            snprintf(s_msg, sizeof s_msg, "%s (%.1f s) -> program %d", s_name, s_seconds, s_slot + 1);
        else
            snprintf(s_msg, sizeof s_msg, "%s (%.1f s) saved, no empty slot", s_name, s_seconds);
        return s_msg;
    }
    if (st >= fmError)
    {
        newFileManager.clearStatus();
        s_saving = false;
        return "Skip-back: saving failed";
    }
    return nullptr;
}
