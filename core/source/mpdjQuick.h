// MPDJ additions (MIT, see NOTICE.md): Q-Link mode 4, quick controls.
// Each Q-Link writes one effect into the step under the Pattern cursor (or every
// step of the selection): Volume and Micromove into their own columns, the rest
// into an FX column (Mike 2026-11-15: four effects picked in a menu, defaults
// Volume, Micromove, Panning, Reverse). The pick lives in Config > Q-Links.
#pragma once
#include <cstdint>

// Mike 2026-11-16: mode 4 off until it gets more testing and a decision (Q-LINK
// cycles 1-3, no Config > Q-Links group). 1 brings both back.
#define MPDJ_QLINK_MODE4 0

namespace mpdjQuick
{
constexpr int kCount = 16;
constexpr int kKnobs = 4;
extern const uint8_t kFxType[kCount];   // Sequencer FX type per entry
extern const char* const kName[kCount]; // menu and popup name
constexpr uint8_t kDefault[kKnobs] = { 0, 1, 2, 3 }; // Volume, Micromove, Panning, Reverse
} // namespace mpdjQuick
