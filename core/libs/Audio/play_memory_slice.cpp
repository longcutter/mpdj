#include "Audio.h"
#include "play_memory.h"
#include "utility/dspinst.h"
#include "mpdjSlices.h"

AUDIO_CORE_FLASH void AudioPlayMemory::playSlice(uint8_t instrIdx, int8_t note)
{
    //	__disable_irq();
    AudioNoInterrupts();

    if (instrIdx > kSampleInstrumentsCount - 1)
        instrIdx = kSampleInstrumentsCount - 1;
    if (note > MAX_NOTE)
        note = MAX_NOTE;

    lastInstrIdxInPlay = currentInstrIdxInPlay;
    currentInstrIdxInPlay = instrIdx;
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
        startSlice =
          (mtProject.instrument[instrIdx].sliceNumber > 0)
            ? mtProject.instrument[instrIdx].slices[mtProject.instrument[instrIdx].selectedSlice]
            : 0;
        endSlice = (!mtProject.instrument[instrIdx].slicePlayThru &&
                    (mtProject.instrument[instrIdx].sliceNumber > 1) &&
                    ((mtProject.instrument[instrIdx].sliceNumber - 1) !=
                     mtProject.instrument[instrIdx].selectedSlice))
                     ? mtProject.instrument[instrIdx]
                         .slices[mtProject.instrument[instrIdx].selectedSlice + 1]
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
            uint8_t activeSlice =
              sliceForcedFlag ? forcedSlice : mtProject.instrument[instrIdx].selectedSlice;
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
    //	__enable_irq();
    AudioInterrupts();
}

AUDIO_CORE_RAM audio_block_t* AudioPlayMemory::updateSlice()
{
    return updateSingleShot();
}
