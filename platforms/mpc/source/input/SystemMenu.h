// MPDJ (2026-11-16): the System menu, + and - held together ~1.5 s (Mike).
//
// Restart MPDJ (takes a new build waiting as tracker-mpc.new next to the running one),
// Switch to the MPC app, Start up in (what the power button boots into, once the startup
// service is installed), Power off, Close. Main encoder chooses, ENCODER PUSH does it;
// any other button or a touch closes it. Switch and Power off ask for a second push.
//
// The exits save the workspace first (main.cpp, SystemMenu::finish after the loop).
#pragma once

#include <stdint.h>

namespace SystemMenu
{
// main.cpp, at start: where the running binary lives (for the restart and boot-mode file)
void setExecutable(const char* path, char** argv);

bool isOpen();
void open();
void close();
void turn(int dir);
void push();
void poll(uint32_t nowMs); // re-shows the popup before it times out

// What the menu asked for when the loop ended; main.cpp calls finish() after the
// clean-up (it doesn't return for Restart and Power off when they work)
enum class Exit : uint8_t
{
    None,
    Restart,
    SwitchToMpc,
    PowerOff,
};
Exit requested();
int finish();

// "mpdj" / "mpc" in <binary dir>/boot-mode; true = MPDJ
bool bootIntoMpdj();
} // namespace SystemMenu
