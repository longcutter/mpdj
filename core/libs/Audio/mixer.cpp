/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "mixer.h"
#include "dspApplyGain.h"

template<int mixerSize>
AUDIO_CORE_RAM void AudioMixer<mixerSize>::update(void)
{

    audio_block_t *in = nullptr, *out = nullptr;

    int32_t mixedBuffer[AUDIO_BLOCK_SAMPLES];

    for (int channel = 0; channel < mixerSize; channel++)
    {
        if (nullptr == out)
        {
            out = receiveWritable(channel);
            if (out)
            {
                applyGainAndCopy(
                  mixedBuffer, out->data, multiplier[channel], previousMultiplier[channel]);
            }
        }
        else
        {
            in = receiveReadOnly(channel);
            if (nullptr != in)
            {
                applyGainThenAdd(
                  mixedBuffer, in->data, multiplier[channel], previousMultiplier[channel]);
                release(in);
            }
        }
    }
    if (nullptr != out)
    {
        saturate(out->data, mixedBuffer, saturationType);

        transmit(out);
        release(out);
    }
}

// if you use this template with other value you should add it in this place to force compilation
// for every class in one .cpp file (to avoid symbol visibility issues)
template class AudioMixer<2>;
template class AudioMixer<4>;
template class AudioMixer<7>;
template class AudioMixer<8>;
template class AudioMixer<10>;
#ifdef PLATFORM_IMX
template class AudioMixer<18>;
template class AudioMixer<11>;
template class AudioMixer<21>;
// MPDJ (step 5a-4): MixerAndFxs<16> = 16 voices (+ synth parts), 2 inputs + 2 x 16 (+ synths)
template class AudioMixer<16>;
template class AudioMixer<34>;
#if defined(FEATURE_SYNTH)
template class AudioMixer<19>;
template class AudioMixer<37>;
#endif
#endif
