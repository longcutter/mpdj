#include "Audio.h"
#include "play_memory.h"
#include "utility/dspinst.h"
#include "mpdjSlices.h"

AUDIO_CORE_FLASH void AudioPlayMemory::playBeatSlice(uint8_t instrIdx, int8_t note)
{
    AudioNoInterrupts();

    uint8_t localSlice;
    FIRST_BEAT_SLICE_NOTE;

    if (note < FIRST_BEAT_SLICE_NOTE)
        localSlice = 0;
    else if (note > (FIRST_BEAT_SLICE_NOTE + mtProject.instrument[instrIdx].sliceNumber - 1))
        localSlice = mtProject.instrument[instrIdx].sliceNumber - 1;
    else
        localSlice = note - FIRST_BEAT_SLICE_NOTE;

    note = 60; // zerowy pitch

    if (instrIdx > kSampleInstrumentsCount - 1)
        instrIdx = kSampleInstrumentsCount - 1;

    lastInstrIdxInPlay = currentInstrIdx;
    currentInstrIdx = instrIdx;

    refreshStartParameters();

    constrainCurrentTune(note);

    setStartPitch(note);

    applyFinetuneOnPitch(note);

    calculateGlidePitch(note);

    lastNote = note;

    uint16_t startSlice, endSlice;

    if (sliceForcedFlag)
    {
        startSlice = (mtProject.instrument[instrIdx].sliceNumber > 0)
                       ? mtProject.instrument[instrIdx].slices[forcedSlice]
                       : 0;
        endSlice = (!mtProject.instrument[instrIdx].slicePlayThru &&
                    (mtProject.instrument[instrIdx].sliceNumber > 1) &&
                    ((mtProject.instrument[instrIdx].sliceNumber - 1) != forcedSlice))
                     ? mtProject.instrument[instrIdx].slices[forcedSlice + 1]
                     : mpdjSliceTrimEnd(&mtProject.instrument[instrIdx]); // MPDJ (5g): the last slice stops at End
    }
    else
    {
        startSlice = (mtProject.instrument[instrIdx].sliceNumber > 0)
                       ? mtProject.instrument[instrIdx].slices[localSlice]
                       : 0;
        endSlice = (!mtProject.instrument[instrIdx].slicePlayThru &&
                    (mtProject.instrument[instrIdx].sliceNumber > 1) &&
                    ((mtProject.instrument[instrIdx].sliceNumber - 1) != localSlice))
                     ? mtProject.instrument[instrIdx].slices[localSlice + 1]
                     : mpdjSliceTrimEnd(&mtProject.instrument[instrIdx]); // MPDJ (5g): the last slice stops at End
    }

    if (samplePointsSource != nullptr)
    {
        pointsInSamples.start = samplePointsSource->pointsInSamples.start;
        pointsInSamples.end = samplePointsSource->pointsInSamples.end;
    }
    else
    {
        pointsInSamples.start = calculatePointPositionAndSnapToZero(
          startSlice, (uint32_t)currentSampleLength, (int16_t*)currentStartAddress);
        pointsInSamples.end = calculatePointPositionAndSnapToZero(
          endSlice, (uint32_t)currentSampleLength, (int16_t*)currentStartAddress);
    }

    pointsInSamples.loop1 = 0;
    pointsInSamples.loop2 = 0;

    constrainsInSamples.endPoint = pointsInSamples.end - pointsInSamples.start;

    next = currentStartAddress + pointsInSamples.start;
    length = currentSampleLength - pointsInSamples.start;
    iPitchCounter = reverseDirectionFlag ? constrainsInSamples.endPoint - 1 : 0;

    if (currentIsStreamed)
    {
        if (!reverseDirectionFlag && mtProject.instrument[instrIdx].sliceNumber > 0)
        {
            uint8_t activeSlice = sliceForcedFlag ? forcedSlice : localSlice;
            voiceStreamManager.getSlicePreCache(instrIdx,
                                                activeSlice,
                                                channel,
                                                pointsInSamples.start,
                                                currentSliceCacheAddress,
                                                currentSliceCacheFrames);
        }
        if (channel == 0 && voiceIndex >= 0)
        {
            voiceStreamManager.notifyNoteOn((uint8_t)voiceIndex,
                                            instrIdx,
                                            pointsInSamples.start,
                                            pointsInSamples.start,
                                            pointsInSamples.end,
                                            pointsInSamples.end,
                                            currentPlayMode,
                                            reverseDirectionFlag != 0,
                                            pitchControl,
                                            currentSliceCacheFrames);
        }
    }

    playing = 1;
    AudioInterrupts();
}

AUDIO_CORE_RAM audio_block_t* AudioPlayMemory::updateBeatSlice()
{
    return updateSingleShot();
}
