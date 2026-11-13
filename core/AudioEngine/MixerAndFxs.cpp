#include "MixerAndFxs.h"

template<int voicesCount>
MixerAndFxs<voicesCount>::MixerAndFxs()
{

    connections.push_back(new AudioConnection(
      &preFxMixer[FxType::kReverb], 0, &polyverb, (int)MixerAndFxChannelType::kLeft));
    connections.push_back(new AudioConnection(
      &preFxMixer[FxType::kReverb], 0, &polyverb, (int)MixerAndFxChannelType::kRight));

    connections.push_back(new AudioConnection(&preFxMixer[FxType::kDelay], &shortDelay));

    for (int channel = 0; channel < (int)MixerAndFxChannelType::kChannelCount; channel++)
    {
        connections.push_back(new AudioConnection(&tracksAndSynthMixer[channel],
                                                  0,
                                                  &tracksAndSendsMixer[channel],
                                                  TrackAndFxsChannels::kDryMixChannel));

        connections.push_back(new AudioConnection(
          &shortDelay, channel, &tracksAndSendsMixer[channel], TrackAndFxsChannels::kDelayChannel));

        connections.push_back(new AudioConnection(
          &polyverb, channel, &tracksAndSendsMixer[channel], TrackAndFxsChannels::kReverbChannel));

        connections.push_back(new AudioConnection(&polyverb, channel, &polyverbRMS[channel], 0));
        connections.push_back(new AudioConnection(&shortDelay, channel, &delayRMS[channel], 0));
        connections.push_back(
          new AudioConnection(&tracksAndSynthMixer[channel], 0, &dryMixRMS[channel], 0));
    }
}

template<int voicesCount>
AudioMixer<MixerAndFxs<voicesCount>::kSendsInputsCount>* MixerAndFxs<voicesCount>::getPreFxMixer(
  FxType type)
{
    return &preFxMixer[type];
}

template<int voicesCount>
AudioMixer<MixerAndFxs<voicesCount>::kTracksAndSynthVoices>*
MixerAndFxs<voicesCount>::getTracksMixer(MixerAndFxChannelType channel)
{
    return &tracksAndSynthMixer[(int)channel];
}

template<int voicesCount>
AudioMixer<MixerAndFxs<voicesCount>::kTrackAndSendsMixerInputs>*
MixerAndFxs<voicesCount>::getTracksAndSendsMixer(MixerAndFxChannelType channel)
{
    return &tracksAndSendsMixer[(int)channel];
}

template<int voicesCount>
AudioEffectShortDelay* MixerAndFxs<voicesCount>::getDelay()
{
    return &shortDelay;
}

template<int voicesCount>
AudioEffectPolyverb* MixerAndFxs<voicesCount>::getPolyverb()
{
    return &polyverb;
}

template<int voicesCount>
AudioAnalyzeRMS* MixerAndFxs<voicesCount>::getDelayRMS(MixerAndFxChannelType channel)
{
    return &delayRMS[(int)channel];
}

template<int voicesCount>
AudioAnalyzeRMS* MixerAndFxs<voicesCount>::getPolyverbRMS(MixerAndFxChannelType channel)
{
    return &polyverbRMS[(int)channel];
}

template<int voicesCount>
AudioAnalyzeRMS* MixerAndFxs<voicesCount>::getDryMixRMS(MixerAndFxChannelType channel)
{
    return &dryMixRMS[(int)channel];
}

#ifdef FEATURE_SYNTH
template<int voicesCount>
AudioAnalyzeRMS* MixerAndFxs<voicesCount>::getSynthRMS(int synthIdx, MixerAndFxChannelType channel)
{
    return &synthRMS[synthIdx][(int)channel];
}
#endif

template class MixerAndFxs<8>;
template class MixerAndFxs<16>; // MPDJ (step 5a-4)
