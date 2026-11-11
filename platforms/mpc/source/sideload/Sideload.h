// Sideloads: separate programs MPDJ hands the MPC to and takes it back from
// (sideload test S2, the takeover kind). See claude/mpdj-ideas.md and the
// handoff's "Sideload tests" plan.
//
// Hand-over: MPDJ stops the pattern, lets go of every held control, switches
// its LEDs off, stops drawing, closes the touchscreen and releases the codec,
// then runs the sideload command with /bin/sh -c in its own process group.
// While it runs, MPDJ keeps the controller's raw MIDI port and relays it:
//   fd 3 (MPDJ_MIDI_IN_FD=3)   controller MIDI bytes, as read from the port
//   fd 4 (MPDJ_MIDI_OUT_FD=4)  bytes the sideload writes go to the controller (LEDs)
// The sideload opens the panel (/dev/fb0), the touchscreen (/dev/input/event0)
// and the codec (hw:1,0, directly or through jackd) itself.
//
// Way back: the sideload exits, or SHIFT+MAIN held 2 s stops its process group
// (SIGTERM, then SIGKILL after 1.5 s), which works even if it hangs. MPDJ then
// reopens the codec (retrying for up to 3 s), the touchscreen and the screen,
// and repaints everything.
//
// Env:
//   MPDJ_SIDELOAD=<command>      enables SHIFT+MAIN held 2 s in MPDJ to start it
//   MPDJ_SIDELOAD_CYCLE=N        hand over N times on its own, for the S2 test
//                                (the sideload should exit by itself, e.g. after a few s)
//   MPDJ_SIDELOAD_START=s        first automatic hand-over s seconds after start (default 15)
//   MPDJ_SIDELOAD_GAP=s          seconds back in MPDJ between automatic hand-overs (default 5)
#pragma once

#include <stdint.h>

namespace Sideload
{
constexpr uint32_t kComboHoldMs = 2000;

void init();
bool enabled();
void request(); // SHIFT+MAIN held: hand over on the next poll()
void poll();    // main loop, after platform.update()
} // namespace Sideload
