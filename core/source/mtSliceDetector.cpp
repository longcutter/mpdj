#include "mtSliceDetector.h"

#include <cr_section_macros.h>

__NOINIT(EXTERNAL_RAM) uint32_t measureBuffer[INSTANT_BUFFER_SIZE];

mtSliceDetector sliceDetector;
float mtSliceDetector::mpdjSensScale = 1.0f;

void mtSliceDetector::start(int16_t* addr, uint32_t len, uint16_t* sliceTab, uint8_t* sliceNumber)
{
    if (len < LOCAL_BUFFER_SIZE)
        return;
    state = detectState::inProgress;
    position = 0;
    lastDetected = 0;
    lastBufferFlag = 0;
    averagePositionHead = 0;
    currentSlice = 0;
    address = addr;
    slice = sliceTab;
    length = len;
    sliceNumberToWrite = sliceNumber;
}
void mtSliceDetector::update()
{
    if (state == detectState::ended)
        return;

    int16_t* localAddr = address + position;
    uint32_t dif = length - position;

    uint16_t constrainSize = (dif < INSTANT_BUFFER_SIZE) ? (uint16_t)dif : INSTANT_BUFFER_SIZE;

    if (constrainSize != INSTANT_BUFFER_SIZE)
        lastBufferFlag = 1;

    for (uint16_t i = 0; i < constrainSize; i++)
    {
        measureBuff[i] = localAddr[i] * localAddr[i];
    }

    currentInstantAverage = calcInstantAverage(measureBuff, INSTANT_BUFFER_SIZE);

    if ((position > LOCAL_BUFFER_SIZE) && (lastBufferFlag == 0))
    {
        localAverage =
          calcLocalAverage(instantAverageBuffer, LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE);
        localVariance = calcLocalVariance(
          instantAverageBuffer, localAverage, LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE);

        sensCoeff = ((-0.00000000015625) * localVariance) + 1.62951;
        if (sensCoeff < 1.2f)
            sensCoeff = 1.2f;
        sensCoeff *= mpdjSensScale; // MPDJ (5g-2): Auto slice sensitivity, 1 = the Tracker's
        if (sensCoeff < 1.02f)
            sensCoeff = 1.02f;

        if ((currentInstantAverage > sensCoeff * localAverage) &&
            (position > (lastDetected + LAST_DETECTED_DEATH_ZONE)))
        {
            if (currentSlice < MAX_SLICE_NUMBER)
            {
                uint32_t precisePosition = preciseDetect(position);
                lastDetected = precisePosition;
                slice[currentSlice++] = (uint16_t)(precisePosition * (MAX_16BIT / (float)length));
            }
        }
    }
    else if (position == LOCAL_BUFFER_SIZE)
    {
        localAverage =
          calcLocalAverage(instantAverageBuffer, LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE);
        localVariance = calcLocalVariance(
          instantAverageBuffer, localAverage, LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE);
        sensCoeff = ((-0.00000000015625) * localVariance) + 1.62951;
        if (sensCoeff < 1.2f)
            sensCoeff = 1.2f;
        sensCoeff *= mpdjSensScale; // MPDJ (5g-2): Auto slice sensitivity, 1 = the Tracker's
        if (sensCoeff < 1.02f)
            sensCoeff = 1.02f;

        for (uint16_t i = 0; i < LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE; i++)
        {
            if ((instantAverageBuffer[i] > sensCoeff * localAverage) &&
                (((i * INSTANT_BUFFER_SIZE) > (lastDetected + LAST_DETECTED_DEATH_ZONE)) ||
                 (i == 0)))
            {
                lastDetected = i * INSTANT_BUFFER_SIZE;
                uint32_t precisePosition = preciseDetect(lastDetected);
                slice[currentSlice++] = (uint16_t)(precisePosition * (MAX_16BIT / (float)length));
            }
        }
        if (currentSlice == 0)
        {
            slice[currentSlice++] = 0;
        }
    }

    instantAverageBuffer[averagePositionHead] = currentInstantAverage;
    averagePositionHead++;
    if (averagePositionHead == LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE)
        averagePositionHead = 0;
    position += constrainSize;
    if ((position == length) || (currentSlice >= MAX_SLICE_NUMBER))
    {
        state = detectState::ended;
        *sliceNumberToWrite = currentSlice;
    }
}
uint8_t mtSliceDetector::getState()
{
    return (uint8_t)state;
}
uint8_t mtSliceDetector::getProgress()
{
    return (position * 100) / length;
}

uint32_t mtSliceDetector::calcInstantAverage(uint32_t* buf, uint16_t size)
{
    uint64_t sum = 0;
    for (uint16_t i = 0; i < size; i++)
    {
        sum += buf[i];
    }
    return (sum / size);
}

uint32_t mtSliceDetector::calcLocalAverage(uint32_t* buf, uint16_t size)
{
    uint64_t sum = 0;
    for (uint16_t i = 0; i < size; i++)
    {
        sum += buf[i];
    }

    return (sum / size);
}
uint32_t mtSliceDetector::calcLocalVariance(uint32_t* buf, uint32_t average, uint16_t size)
{
    uint64_t sum = 0;
    for (uint16_t i = 0; i < size; i++)
    {
        int32_t locDif = buf[i] - average;
        sum += (locDif * locDif);
    }
    return (sum / size);
}

uint32_t mtSliceDetector::preciseDetect(uint32_t presicePosition)
{
    //******************************************************************************************//
    // W niektórych przypadkach średnia sygnału jest na tyle "energetyczna", że gdy peak trafi na
    // koniec bufora i nie zdąży podnieść jego chwilowej energii do poziomu wykrycia to wykrywany
    // jest dopiero kolejny bufor. Dlatego pozycja ustalana jest jako srodek szerszego bufora, w
    // którym prezycyjnie wykrywany jest moment przejscia.
    //******************************************************************************************//
    uint32_t localLength;
    //	if(position + (PRECISE_LOCAL_BUFFER_SIZE/2) > length)  localLength = length - position;
    //	else localLength = PRECISE_LOCAL_BUFFER_SIZE/2;
    //
    //	if((int32_t)(position - (PRECISE_LOCAL_BUFFER_SIZE/2)) < 0 )
    //	{
    //		localLength += position;
    //		position = 0;
    //	}
    //	else
    //	{
    //		localLength += PRECISE_LOCAL_BUFFER_SIZE/2;
    //		position -= (PRECISE_LOCAL_BUFFER_SIZE/2);
    //	}

    if ((int32_t)(presicePosition - (PRECISE_LOCAL_BUFFER_SIZE / 2)) < 0)
    {
        localLength = PRECISE_LOCAL_BUFFER_SIZE - presicePosition;
        presicePosition = 0;
    }
    else if (presicePosition + (PRECISE_LOCAL_BUFFER_SIZE / 2) > length)
    {
        localLength = PRECISE_LOCAL_BUFFER_SIZE - (length - presicePosition);
        presicePosition -= (PRECISE_LOCAL_BUFFER_SIZE / 2);
    }
    else
    {
        localLength = PRECISE_LOCAL_BUFFER_SIZE;
        presicePosition -= (PRECISE_LOCAL_BUFFER_SIZE / 2);
    }

    int16_t* localAddr = address + presicePosition;
    uint32_t presiceMeasureBuf[PRECISE_INSTANT_BUFFER_SIZE];
    uint32_t preciseInstantAverageBuffer[PRECISE_LOCAL_BUFFER_SIZE / PRECISE_INSTANT_BUFFER_SIZE];

    uint16_t localConstrain = localLength / PRECISE_INSTANT_BUFFER_SIZE;

    for (uint16_t j = 0; j < localConstrain; j++)
    {
        for (uint16_t i = 0; i < PRECISE_INSTANT_BUFFER_SIZE; i++)
        {
            presiceMeasureBuf[i] = localAddr[i + j * PRECISE_INSTANT_BUFFER_SIZE] *
                                   localAddr[i + j * PRECISE_INSTANT_BUFFER_SIZE];
        }
        preciseInstantAverageBuffer[j] =
          calcInstantAverage(presiceMeasureBuf, PRECISE_INSTANT_BUFFER_SIZE);
    }

    uint32_t preciseLocalAverage = calcLocalAverage(preciseInstantAverageBuffer, localConstrain);
    uint32_t preciseLocalVariance =
      calcLocalVariance(preciseInstantAverageBuffer, preciseLocalAverage, localConstrain);

    float preciseSensCoeff = ((-0.00000000015625) * preciseLocalVariance) + 1.62951;
    if (preciseSensCoeff < 1.2f)
        preciseSensCoeff = 1.2f;

    for (uint16_t i = 5; i < localConstrain; i++)
    {
        if (preciseInstantAverageBuffer[i] > preciseSensCoeff * preciseLocalAverage)
        {
            if (currentSlice != 0)
            {
                if ((uint32_t)(lastDetected + LAST_DETECTED_DEATH_ZONE) >=
                    (uint32_t)(presicePosition + i * PRECISE_INSTANT_BUFFER_SIZE))
                    continue;
            }
            return presicePosition + (i - 5) * PRECISE_INSTANT_BUFFER_SIZE;
        }
    }

    //	int16_t * localAddr = address + position;
    //	uint32_t presiceMeasureBuf[PRECISE_INSTANT_BUFFER_SIZE];
    //	uint32_t preciseInstantAverageBuffer[INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE];
    //
    //	for(uint16_t j = 0; j < INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE; j++ )
    //	{
    //		for(uint16_t i = 0; i <PRECISE_INSTANT_BUFFER_SIZE ; i++ )
    //		{
    //			presiceMeasureBuf[i] = localAddr[i + j*PRECISE_INSTANT_BUFFER_SIZE] *  localAddr[i +
    // j*PRECISE_INSTANT_BUFFER_SIZE];
    //		}
    //		preciseInstantAverageBuffer[j] =
    // calcInstantAverage(presiceMeasureBuf,PRECISE_INSTANT_BUFFER_SIZE);
    //	}
    //
    //	uint32_t preciseLocalAverage =
    // calcLocalAverage(preciseInstantAverageBuffer,INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE);
    //	uint32_t preciseLocalVariance = calcLocalVariance(preciseInstantAverageBuffer,
    // preciseLocalAverage,INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE);
    //
    //	float preciseSensCoeff = ((-0.00000000015625) * preciseLocalVariance) + 1.62951;
    //	if(preciseSensCoeff < 1.2f) preciseSensCoeff = 1.2f;
    //
    //	for(uint16_t i = 0; i < INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE; i++ )
    //	{
    //		if(preciseInstantAverageBuffer[i] > preciseSensCoeff * preciseLocalAverage)
    //		{
    //				return position + i * PRECISE_INSTANT_BUFFER_SIZE;
    //		}
    //	}

    //******************************************************************************************//
    // W niektórych przypadkach średnia sygnału jest na tyle "energetyczna", że gdy peak trafi na
    // koniec bufora i nie zdąży podnieść jego chwilowej energii do poziomu wykrycia to wykrywany
    // jest dopiero kolejny bufor. W sytuacji gdy precyzyjne wykrywanie pozycji nie wykryje peaku w
    // swoim buforze, warto sprawdzić poprzedni bufor.
    //******************************************************************************************//
    //	if(position != 0)
    //	{
    //		uint32_t decPosition = position - INSTANT_BUFFER_SIZE;
    //
    //		localAddr = address + decPosition;
    //
    //
    //		for(uint16_t j = 0; j < INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE; j++ )
    //		{
    //			for(uint16_t i = 0; i <PRECISE_INSTANT_BUFFER_SIZE ; i++ )
    //			{
    //				presiceMeasureBuf[i] = localAddr[i + j*PRECISE_INSTANT_BUFFER_SIZE] *
    // localAddr[i
    //+ j*PRECISE_INSTANT_BUFFER_SIZE];
    //			}
    //			preciseInstantAverageBuffer[j] =
    // calcInstantAverage(presiceMeasureBuf,PRECISE_INSTANT_BUFFER_SIZE);
    //		}
    //
    //		preciseLocalAverage =
    // calcLocalAverage(preciseInstantAverageBuffer,INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE);
    //		preciseLocalVariance = calcLocalVariance(preciseInstantAverageBuffer,
    // preciseLocalAverage,INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE);
    //
    //		preciseSensCoeff = ((-0.00000000015625) * preciseLocalVariance) + 1.62951;
    //		if(preciseSensCoeff < 1.2f) preciseSensCoeff = 1.2f;
    //
    //		for(uint16_t i = 0; i < INSTANT_BUFFER_SIZE/PRECISE_INSTANT_BUFFER_SIZE; i++ )
    //		{
    //			if(preciseInstantAverageBuffer[i] > preciseSensCoeff * preciseLocalAverage)
    //			{
    //					return decPosition + i * PRECISE_INSTANT_BUFFER_SIZE;
    //			}
    //		}
    //	}

    return presicePosition;
}
