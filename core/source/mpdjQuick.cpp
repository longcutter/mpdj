// MPDJ additions (MIT, see NOTICE.md): Q-Link mode 4 quick controls (see mpdjQuick.h)
#include "mpdjQuick.h"
#include "fileManager.h"
#include "mtSequencer.h"
#include "patternEditor/patternEditor.h"
#include "util_math.h"

namespace mpdjQuick
{
using F = Sequencer::strFxConsts;
const uint8_t kFxType[kCount] = {
    F::FX_TYPE_VOLUME,          F::FX_TYPE_MICROMOVE,      F::FX_TYPE_PANNING,
    F::FX_TYPE_REVERSE_PLAYBACK, F::FX_TYPE_FILTER_LOWPASS, F::FX_TYPE_FILTER_HIGHPASS,
    F::FX_TYPE_FILTER_BANDPASS, F::FX_TYPE_REVERB_SEND,    F::FX_TYPE_DELAY_SEND,
    F::FX_TYPE_OVERDRIVE,       F::FX_TYPE_BIT_DEPTH,      F::FX_TYPE_TUNE,
    F::FX_TYPE_MICROTUNING,     F::FX_TYPE_POSITION,       F::FX_TYPE_GATE_LENGTH,
    F::FX_TYPE_CHANCE,
};
const char* const kName[kCount] = {
    "Volume",   "Micromove", "Panning",   "Reverse",   "Low-pass",  "High-pass",
    "Band-pass", "Reverb send", "Delay send", "Overdrive", "Bit depth", "Tune",
    "Micro-tune", "Position", "Gate length", "Chance",
};
} // namespace mpdjQuick

// Change one quick control on the current selection (the pattern editor sets
// it: the cursor step, or the marked block). Volume / Micromove use the
// Tracker's own column edits (an empty cell starts from the program's volume /
// 0). Other effects: a step that already has the effect in an FX column gets it
// changed; a single step without it gets it in a free FX column (FX1 first)
// starting from the effect's default; in a block only steps that already have
// it change (as the Tracker's own column edits do). *value = the first step's
// value afterwards (stored form, -1 = none). Returns false when a single step
// had no free FX column.
bool Sequencer::mpdjQuickChange(uint8_t fxType, int16_t delta, int16_t* value)
{
    *value = -1;
    strSelection* sel = &selection;
    if (!isSelectionCorrect(sel))
        return true;
    strPattern::strTrack::strStep* first = &currentPattern->track[sel->firstTrack].step[sel->firstStep];

    if (fxType == fx.FX_TYPE_VOLUME)
    {
        changeSelectionVolume(delta);
        *value = first->volume;
        return true;
    }
    if (fxType == fx.FX_TYPE_MICROMOVE)
    {
        changeSelectionMicromove(delta);
        *value = first->micromove;
        return true;
    }

    bool multi = isMultiSelection();
    bool changed = false, full = false;
    int16_t lo = getFxMin(fxType), hi = getFxMax(fxType);
    newFileManager.storePatternUndoRevision();
    for (uint8_t t = sel->firstTrack; t <= sel->lastTrack; t++)
    {
        for (uint8_t s = sel->firstStep; s <= sel->lastStep; s++)
        {
            strPattern::strTrack::strStep* step = &currentPattern->track[t].step[s];
            int slot = -1;
            for (int k : { 1, 0 }) // fx[1] is the FX1 column on screen
                if (slot < 0 && step->fx[k].type == fxType)
                    slot = k;
            if (slot >= 0)
            {
                step->fx[slot].value = (uint8_t)clamp<int>(step->fx[slot].value + delta, lo, hi);
            }
            else
            {
                if (multi)
                    continue;
                for (int k : { 1, 0 })
                    if (slot < 0 && step->fx[k].type == fx.FX_TYPE_NONE)
                        slot = k;
                if (slot < 0)
                {
                    full = true;
                    continue;
                }
                step->fx[slot].type = fxType;
                step->fx[slot].value = (uint8_t)clamp<int>(getFxDefault(fxType) + delta, lo, hi);
            }
            changed = true;
            if (step == first)
                *value = step->fx[slot].value;
        }
    }
    if (changed)
    {
        newFileManager.setActualPatternStructChanged();
        if (!isPlay())
            playSelection();
    }
    return !full;
}
