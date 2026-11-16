// MPDJ additions (MIT, see NOTICE.md): Kits (Mike 2026-11-16).
//
// A Kit is a folder, normally in /Kits, holding numbered program files
// ("01 Kick.pti", "02 Snare.pti", ...: the Tracker's own instrument files, so
// each carries its sample and its program settings) and a kit.txt with the pad
// settings for each sound. The Program Loader's Add on a kit folder loads its
// programs into consecutive slots from the selected one (the default pad map
// then puts slots 1-16 on bank A, tracks 1-16) and applies kit.txt to the pads
// that point at those slots. The File screen's Save Kit writes one bank's 16
// slots (empty slots left out) as a kit.
#pragma once
#include <cstdint>

namespace mpdjKits
{
constexpr const char* kFolder = "/Kits";
constexpr const char* kInfoFile = "kit.txt";
constexpr int kMaxSounds = 64;

// a folder (full path, e.g. "/Kits/808") holds a kit.txt
bool isKitFolder(const char* folderPath);

// kit.txt of folderPath -> the pads that point at slots base .. base + count - 1;
// returns the number of pads changed
int applyPadSettings(const char* folderPath, int baseSlot, int count);

// the bank (0-3) of the last pad played from the pad map (A until a pad is hit)
int lastPlayedBank();

// Save slots bank * 16 .. + 15 as /Kits/<name>. Empty slots are left out (the kit
// closes up). Writes a short message for a popup into msg; returns the number of
// sounds saved (0 = nothing saved)
int saveBank(int bank, const char* name, char* msg, int msgSize);

// a kit folder of that name already exists
bool exists(const char* name);
} // namespace mpdjKits
