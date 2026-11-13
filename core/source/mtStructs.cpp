#include "mtStructs.h"

void strMtValues::setDefaultTrackName(int trackIdx)
{
    assert(trackIdx < kMaxAllTracksInTrackers);

    if (trackIdx < kSampleTracks)
    {
        sprintf(TrackNames[trackIdx], "Track %d", trackIdx + 1);
    }
    else
    {
        // MPDJ (step 5a-4): tracks 9-16 play samples too; names fit 7 characters
        sprintf(midiTrackNames[trackIdx - kSampleTracks],
                trackIdx + 1 < 10 ? "Track %d" : "Track%d",
                trackIdx + 1);
    }
}
