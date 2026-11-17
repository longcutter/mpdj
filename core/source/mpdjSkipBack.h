// MPDJ (2026-11-16): skip-back sampling, as the SP-404 MKII's: the audio thread keeps
// the last 60 s of the input in RAM; SHIFT+SAMPLER saves what was heard (silence at
// both ends trimmed) as Samples/SkipBack/Skip NNN.wav and loads it into the first empty
// program slot. MIT, see NOTICE.md (MPDJ additions).
#pragma once

#include <stdint.h>

// audio thread: one block of interleaved stereo input
void mpdjSkipBackPush(const int16_t* interleaved, unsigned frames);
// UI thread: start the save; returns the popup text
const char* mpdjSkipBackSave();
// UI thread, every poll: finishes a save (file manager status); returns a popup text
// when it ended, else nullptr
const char* mpdjSkipBackPoll();

// Saves the programs changed since the last save (their settings) to the workspace and
// starts the Tracker's sample reload, which loads every slot's sound into sample memory
// (after a sound was written into a slot through saveRecordedSound). False if the file
// manager is busy. Ends with fmReloadSamplesEnd (screens may take that status).
bool mpdjReloadSamples();
// for the self-test: where the sound starts and ends in a buffer (frames), false if silent
bool mpdjSkipBackTrim(const int16_t* l, const int16_t* r, uint32_t frames, uint32_t* from, uint32_t* to);
