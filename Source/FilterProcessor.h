#pragma once

#include <juce_dsp/juce_dsp.h>

/**
    Splits the input into Low / Mid / High bands using LR4 crossovers
    and sums the requested band(s).

    filterSolo:
        0 = Off  (full-band, no filtering)
        1 = Low  (only the low band)
        2 = Mid  (only the mid band)
        3 = High (only the high band)

    Frequencies are specified in Hz. Both are smoothed internally so
    that live changes don't zipper.
*/
class FilterProcessor
{
public:
    FilterProcessor() = default;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (1, samplesPerBlock);
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

        lowShelfSplit.prepare (spec);   // splits at fLow
        highShelfSplit.prepare (spec);  // splits at fHigh

        lowShelfSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        highShelfSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

        lowBandBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        midBandBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        highBandBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

        // 50 ms smoothing is a good balance for slider drags.
        smoothedLowHz.reset (sampleRate, 0.05);
        smoothedHighHz.reset (sampleRate, 0.05);

        smoothedLowHz.setCurrentAndTargetValue (targetLowHz);
        smoothedHighHz.setCurrentAndTargetValue (targetHighHz);

        lowShelfSplit.setCutoffFrequency (targetLowHz);
        highShelfSplit.setCutoffFrequency (targetHighHz);

        prepared = true;
    }

    void reset()
    {
        lowShelfSplit.reset();
        highShelfSplit.reset();
    }

    // Called from the audio thread once per block.
    void setCrossovers (float lowHz, float highHz)
    {
        lowHz  = juce::jlimit (20.0f, 2000.0f, lowHz);
        highHz = juce::jlimit (500.0f, 20000.0f, highHz);

        if (lowHz >= highHz)
            highHz = juce::jmin (20000.0f, lowHz * 4.0f);

        constexpr float epsilon = 1.0e-4f;

        if (std::abs (lowHz - targetLowHz) > epsilon)
            smoothedLowHz.setTargetValue (lowHz);

        if (std::abs (highHz - targetHighHz) > epsilon)
            smoothedHighHz.setTargetValue (highHz);

        targetLowHz  = lowHz;
        targetHighHz = highHz;
    }

    // 0 = off, 1 = low, 2 = mid, 3 = high
    void setSolo (int soloIndex) noexcept
    {
        solo = juce::jlimit (0, 3, soloIndex);
    }

    int getSolo() const noexcept { return solo; }

    bool isActive() const noexcept { return solo != 0; }

    // In-place processing
    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! prepared || ! isActive())
            return;

        const auto numSamples  = buffer.getNumSamples();
        const auto numChannels = juce::jmin (buffer.getNumChannels(),
                                             lowBandBuffer.getNumChannels());

        if (numSamples <= 0 || numChannels <= 0)
            return;

        // Advance the smoother by the FULL block, and use the smoothed
        // frequency at the end of this block as the coefficient for the
        // whole block. This makes the smoothing time-locked to real audio
        // time instead of "one step per block".
        smoothedLowHz.skip (numSamples);
        smoothedHighHz.skip (numSamples);

        const auto lowHz  = smoothedLowHz.getCurrentValue();
        const auto highHz = smoothedHighHz.getCurrentValue();

        lowShelfSplit.setCutoffFrequency (lowHz);
        highShelfSplit.setCutoffFrequency (highHz);

        // Copy input into our band buffers up front so we can filter in place.
        lowBandBuffer.makeCopyOf (buffer, true);

        // Low band = LP(fLow)
        {
            juce::dsp::AudioBlock<float> block (lowBandBuffer);
            block = block.getSubBlock (0, (size_t) numSamples);
            lowShelfSplit.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // rest = input - low
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* in  = buffer.getReadPointer (ch);
            const auto* lo  = lowBandBuffer.getReadPointer (ch);
            auto*       rst = midBandBuffer.getWritePointer (ch);

            for (int s = 0; s < numSamples; ++s)
                rst[s] = in[s] - lo[s];
        }

        // Mid band = LP(fHigh) applied to rest
        {
            juce::dsp::AudioBlock<float> block (midBandBuffer);
            block = block.getSubBlock (0, (size_t) numSamples);
            highShelfSplit.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        // High band = rest - mid
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* rst = midBandBuffer.getReadPointer (ch);   // now LP'd rest
            const auto* in  = buffer.getReadPointer (ch);
            const auto* lo  = lowBandBuffer.getReadPointer (ch);
            auto*       hi  = highBandBuffer.getWritePointer (ch);

            for (int s = 0; s < numSamples; ++s)
                hi[s] = (in[s] - lo[s]) - rst[s];
        }

        // Sum the requested band(s)
        buffer.clear();

        auto addBand = [&] (const juce::AudioBuffer<float>& src)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.addFrom (ch, 0, src, ch, 0, numSamples);
        };

        switch (solo)
        {
            case 1: addBand (lowBandBuffer);  break;
            case 2: addBand (midBandBuffer);  break;
            case 3: addBand (highBandBuffer); break;
            default: break;
        }
    }

private:
    bool prepared = false;
    int  solo     = 0;

    float targetLowHz  = 250.0f;
    float targetHighHz = 4000.0f;

    juce::dsp::LinkwitzRileyFilter<float> lowShelfSplit;
    juce::dsp::LinkwitzRileyFilter<float> highShelfSplit;

    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedLowHz;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedHighHz;
};