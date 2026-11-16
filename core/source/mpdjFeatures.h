// MPDJ (2026-11-16): feature switches for testing on the MPC. MPDJ_OFF lists features
// to turn off at launch (comma separated, or "all"), so a misbehaving one can be found
// without a rebuild: MPDJ_OFF=grooves,capture ./tracker-mpc. MIT, see NOTICE.md.
#pragma once

enum class MpdjFeature : unsigned char
{
    Screensaver, // the screensaver (and its dimming)
    Visuals,     // SHIFT+MENU / Config "Show visuals now"
    Backlight,   // writing the backlight (Brightness, dimming)
    TapLists,    // tapping list rows (Program Loader, File, Config, Fill, Song, program list)
    Capture,     // SHIFT+OVER DUB capture (and keeping pad presses)
    Grooves,     // per-track swing, 8ths swing, humanize (the clock swing stays)
    Swing,       // the project swing altogether
    Generators,  // Turing Machine / Drum Grid screens
    SkipBack,    // the input ring and SHIFT+SAMPLER
    Bounce,      // Render Selection's Bounce
    Dynamics,    // per-pad dynamics / curve (Config's curve for every pad)
    PadSlices,   // pads written from the slice layer play their slice
    Fill,        // PLAY held = Fill
    Recovery,    // the launch snapshot / File > Restore (MPDJ_OFF=recovery skips the copy)
    Count
};

bool mpdjOn(MpdjFeature f);
// prints which are off (once, at start)
void mpdjFeaturesReport();
