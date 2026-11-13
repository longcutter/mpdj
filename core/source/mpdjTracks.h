// MPDJ (step 5a-4): 16 equal tracks. Every track has a sample voice
// (audioEngine::kVoices = 16) and plays any program. The project file keeps
// the Tracker+ layout, which stores per-track data for tracks 1-8 and 9-16 in
// different arrays (trackMute / midiTrackMute, TrackNames / midiTrackNames,
// trackVolume / the MPDJ block); these helpers hide that split.
// MPDJ's own code, MIT licence (NOTICE.md, "MPDJ additions").
#pragma once

#include "mtStructs.h"

extern strMtProject mtProject;

inline bool isUpperTrack(int track) { return track >= kSampleTracks; } // 9-16
inline int trackPageOf(int track) { return track / kTrackPageSize; }

inline uint8_t& trackMuteRef(int track)
{
    return track < kSampleTracks ? mtProject.values.trackMute[track]
                                 : mtProject.values.midiTrackMute[track - kSampleTracks];
}

inline uint8_t& trackVolumeRef(int track)
{
    return track < kSampleTracks ? mtProject.values.trackVolume[track]
                                 : mtProject.trackVolumeHi[track - kSampleTracks];
}

// Room for TRACK_NAME_LENGTH characters + 0 either way
inline char* trackNameRef(int track)
{
    return track < kSampleTracks ? mtProject.values.TrackNames[track]
                                 : mtProject.values.midiTrackNames[track - kSampleTracks];
}
