#include "mtSliceManager.h"
#include "VoiceStreamManager.h"
#include "mtEffect.h"
#include "mtSliceDetector.h"
#include "mpdjSlices.h"
#include "string.h"
#include <algorithm>

mtSliceManager sliceManager;

void mtSliceManager::addSlice(strInstrument* instr)
{
    if (instr->sliceNumber == MAX_SLICE_NUMBER)
        return;
    if (instr->sliceNumber == 0)
    {
        instr->sliceNumber++;
        instr->selectedSlice = 0;
        instr->slices[0] = 0;
        return;
    }
    if (instr->selectedSlice == (instr->sliceNumber - 1))
    {
        instr->selectedSlice++;
        instr->sliceNumber++;
        instr->slices[instr->selectedSlice] =
          (MAX_16BIT + instr->slices[instr->selectedSlice - 1]) / 2;
        return;
    }
    for (int8_t i = instr->sliceNumber; i > (instr->selectedSlice + 1); i--)
    {
        instr->slices[i] = instr->slices[i - 1];
    }
    instr->selectedSlice++;
    instr->slices[instr->selectedSlice] = ((uint32_t)(instr->slices[instr->selectedSlice - 1] +
                                                      instr->slices[instr->selectedSlice + 1])) /
                                          2;

    instr->sliceNumber++;
}
void mtSliceManager::removeSlice(strInstrument* instr)
{
    if ((instr->sliceNumber == 0) || (instr->sliceNumber <= instr->selectedSlice))
        return;
    if ((instr->sliceNumber - 1) == instr->selectedSlice)
    {
        instr->selectedSlice = instr->sliceNumber == 1 ? 0 : instr->selectedSlice - 1;
        instr->sliceNumber--;
        return;
    }

    for (uint8_t i = instr->selectedSlice; i < instr->sliceNumber - 1; i++)
    {
        instr->slices[i] = instr->slices[i + 1];
    }
    instr->sliceNumber--;
    if (instr->selectedSlice != 0)
        instr->selectedSlice--;
    else
        instr->selectedSlice = 0;
}
void mtSliceManager::adjustSlice(strInstrument* instr, int32_t val)
{
    if ((instr->sliceNumber == 0) || (instr->selectedSlice >= instr->sliceNumber))
        return;

    if ((instr->selectedSlice != (instr->sliceNumber - 1)) && (instr->selectedSlice != 0))
    {
        if (instr->slices[instr->selectedSlice] + val >= instr->slices[instr->selectedSlice + 1])
            instr->slices[instr->selectedSlice] = instr->slices[instr->selectedSlice + 1] - 1;
        else if (instr->slices[instr->selectedSlice] + val <=
                 instr->slices[instr->selectedSlice - 1])
            instr->slices[instr->selectedSlice] = instr->slices[instr->selectedSlice - 1] + 1;
        else
            instr->slices[instr->selectedSlice] += val;
    }
    else if ((instr->selectedSlice == 0) && (instr->selectedSlice == (instr->sliceNumber - 1)))
    {
        if (instr->slices[instr->selectedSlice] + val > MAX_16BIT)
            instr->slices[instr->selectedSlice] = MAX_16BIT;
        else if (instr->slices[instr->selectedSlice] + val < 0)
            instr->slices[instr->selectedSlice] = 0;
        else
            instr->slices[instr->selectedSlice] += val;
    }
    else if (instr->selectedSlice == instr->sliceNumber - 1)
    {
        if (instr->slices[instr->selectedSlice] + val > MAX_16BIT)
            instr->slices[instr->selectedSlice] = MAX_16BIT;
        else if (instr->slices[instr->selectedSlice] + val <=
                 instr->slices[instr->selectedSlice - 1])
            instr->slices[instr->selectedSlice] = instr->slices[instr->selectedSlice - 1] + 1;
        else
            instr->slices[instr->selectedSlice] += val;
    }
    else if (instr->selectedSlice == 0)
    {
        if (instr->slices[instr->selectedSlice] + val >= instr->slices[instr->selectedSlice + 1])
            instr->slices[instr->selectedSlice] = instr->slices[instr->selectedSlice + 1] - 1;
        else if (instr->slices[instr->selectedSlice] + val < 0)
            instr->slices[instr->selectedSlice] = 0;
        else
            instr->slices[instr->selectedSlice] += val;
    }
}
void mtSliceManager::autoSlice(strInstrument* instr)
{
    // MPDJ (2026-11-16, Mike): only between Start and End, as Evenly / Beats do on
    // the slice page: from the first slice's start to End
    uint32_t len = instr->sample.length;
    uint16_t a = instr->sliceNumber > 0 ? instr->slices[0] : 0, b = mpdjSliceTrimEnd(instr);

    memset(instr->slices, 0, MAX_SLICE_NUMBER * 2);
    instr->selectedSlice = 0;

    if (b <= a)
        a = 0, b = MAX_16BIT;
    uint32_t from = (uint32_t)((uint64_t)a * len / MAX_16BIT);
    uint32_t to = (uint32_t)((uint64_t)b * len / MAX_16BIT);
    if (to > len)
        to = len;
    if (to <= from + 1)
        from = 0, to = len, a = 0, b = MAX_16BIT;
    mpdjRangeInstr = (a == 0 && b == MAX_16BIT) ? nullptr : instr;
    mpdjRangeFrom = a;
    mpdjRangeTo = b;

    int instrIdx = (int)(instr - mtProject.instrument);
    if (instrIdx >= 0 && instrIdx < kSampleInstrumentsCount &&
        mtProject.sampleMemory[instrIdx].isStreamed)
    {
        uint32_t maxFrames = std::min<uint32_t>(to - from, (uint32_t)EDIT_BUFF_SIZE);
        if (mpdjRangeInstr && maxFrames < to - from) // the buffer holds less: map what it holds
            mpdjRangeTo = (uint16_t)((uint64_t)(from + maxFrames) * MAX_16BIT / len);
        voiceStreamManager.readInstrumentFramesBlocking(
          instrIdx, 0, from, maxFrames, externalRamEffectsBuff);
        sliceDetector.start(
          externalRamEffectsBuff, maxFrames, instr->slices, &instr->sliceNumber);
        return;
    }

    sliceDetector.start(
      instr->sample.address + from, to - from, instr->slices, &instr->sliceNumber);
}

void mtSliceManager::mpdjAutoSliceDone()
{
    strInstrument* instr = mpdjRangeInstr;
    mpdjRangeInstr = nullptr;
    if (!instr)
        return;
    uint32_t span = (uint32_t)mpdjRangeTo - mpdjRangeFrom;
    for (uint8_t i = 0; i < instr->sliceNumber && i < MAX_SLICE_NUMBER; i++)
        instr->slices[i] = (uint16_t)(mpdjRangeFrom + (uint64_t)instr->slices[i] * span / MAX_16BIT);
}

void mtSliceManager::equalSlice(strInstrument* instr, uint8_t n)
{
    instr->sliceNumber = n;

    for (uint8_t i = 0; i < n; i++)
    {
        instr->slices[i] = (MAX_16BIT * i) / n;
    }
}

uint8_t mtSliceManager::getAutoSliceProgress()
{
    return sliceDetector.getProgress();
}
uint8_t mtSliceManager::getAutoSliceState()
{
    return sliceDetector.getState();
}
