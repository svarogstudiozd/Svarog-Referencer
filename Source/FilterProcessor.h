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

    ------------------------------------------------------------------
    Declick design
    ------------------------------------------------------------------

    Two independent crossfades, both at the mix stage, both with
    per-sample ramps over ~10 ms:

    1. Dry/wet crossfade: `mix` goes 0 -> 1 when the filter turns on,
       and 1 -> 0 when it turns off. The output at any moment is
       dry * (1 - mix) + selectedBand * mix. This makes Off<->On
       inaudible.

    2. Band crossfade: `bandMix` smoothly moves from the previously
       selected band to the newly selected band. When the user
       switches Low->Mid, the old band's output and the new band's
       output are both computed on every sample, and blended with
       an envelope that goes 1->0 for the old and 0->1 for the new.
       This makes band<->band inaudible.

    Both crossfades can be in progress at the same time.

    The filters themselves are always fed the same input, so their
    internal state stays coherent across switches.
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

        lowShelfSplit.prepare (spec);
        highShelfSplit.prepare (spec);

        lowShelfSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        highShelfSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);

        dryBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        lowBandBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        midBandBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        highBandBuffer.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);

        smoothedLowHz.reset (sampleRate, 0.05);
        smoothedHighHz.reset (sampleRate, 0.05);

        smoothedLowHz.setCurrentAndTargetValue (targetLowHz);
        smoothedHighHz.setCurrentAndTargetValue (targetHighHz);

        lowShelfSplit.setCutoffFrequency (targetLowHz);
        highShelfSplit.setCutoffFrequency (targetHighHz);

        // Both ramps are ~10 ms, one step per sample.
        const float rampStep = 1.0f / (0.010f * (float) juce::jmax (1.0, sampleRate));
        mixRampStep  = rampStep;
        bandRampStep = rampStep;

        // Reset both crossfades to a settled state consistent with `solo`.
        currentMix = (solo != 0) ? 1.0f : 0.0f;
        targetMix  = currentMix;

        prevSolo   = solo;
        activeSolo = solo;
        bandMix    = 1.0f;   // fully on `activeSolo`

        prepared = true;
    }

    void reset()
    {
        lowShelfSplit.reset();
        highShelfSplit.reset();

        currentMix = targetMix;
        prevSolo   = solo;
        activeSolo = solo;
        bandMix    = 1.0f;
    }

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
        const int clamped = juce::jlimit (0, 3, soloIndex);

        if (clamped == solo)
            return;

        const int oldSolo = solo;
        solo = clamped;

        // Dry/wet target only moves when crossing the off/on boundary.
        // Inside the non-zero set, the dry/wet stays fully wet and the
        // band crossfade takes over.
        const float newTargetMix = (solo != 0) ? 1.0f : 0.0f;

        if (newTargetMix != targetMix)
            targetMix = newTargetMix;

        // If both the old and the new solo are non-zero and different,
        // start a band crossfade. `prevSolo` is the band we are fading
        // *from*; `activeSolo` becomes the band we are fading *to*.
        //
        // If we were in the middle of a previous band crossfade, snap
        // it to done first (take the current `activeSolo` as the new
        // starting point) to keep the logic simple and prevent
        // oscillating fades.
        if (oldSolo != 0 && solo != 0 && oldSolo != solo)
        {
            if (bandMix < 1.0f)
            {
                // We were mid-fade. Finish it before starting a new one.
                prevSolo = activeSolo;
            }
            else
            {
                prevSolo = oldSolo;
            }

            activeSolo = solo;
            bandMix    = 0.0f;   // fully on `prevSolo`, ramp toward `activeSolo`
        }
        else if (solo == 0)
        {
            // Going to Off. Snap the band crossfade to settled; the
            // dry/wet crossfade will handle the audible transition.
            prevSolo   = activeSolo;
            bandMix    = 1.0f;
        }
        else if (oldSolo == 0 && solo != 0)
        {
            // Coming from Off. No previous band to fade from; snap to
            // fully on the new band. The dry/wet crossfade covers the
            // Off->On transition.
            prevSolo   = solo;
            activeSolo = solo;
            bandMix    = 1.0f;
        }
    }

    int getSolo() const noexcept { return solo; }

    bool isActive() const noexcept { return solo != 0; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! prepared)
            return;

        const auto numSamples  = buffer.getNumSamples();
        const auto numChannels = juce::jmin (buffer.getNumChannels(),
                                             lowBandBuffer.getNumChannels());

        if (numSamples <= 0 || numChannels <= 0)
            return;

        // Fully dry and settled: nothing to do at all.
        const bool fullyDry = (solo == 0)
                           && (currentMix <= 0.0f)
                           && (targetMix  <= 0.0f);

        if (fullyDry)
            return;

        // Advance the frequency smoother and set both crossovers.
        smoothedLowHz.skip (numSamples);
        smoothedHighHz.skip (numSamples);

        const auto lowHz  = smoothedLowHz.getCurrentValue();
        const auto highHz = smoothedHighHz.getCurrentValue();

        lowShelfSplit.setCutoffFrequency (lowHz);
        highShelfSplit.setCutoffFrequency (highHz);

        // Keep dry signal and run the crossovers.
        dryBuffer.makeCopyOf (buffer, true);
        lowBandBuffer.makeCopyOf (buffer, true);

        {
            juce::dsp::AudioBlock<float> block (lowBandBuffer);
            block = block.getSubBlock (0, (size_t) numSamples);
            lowShelfSplit.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* in  = dryBuffer.getReadPointer (ch);
            const auto* lo  = lowBandBuffer.getReadPointer (ch);
            auto*       rst = midBandBuffer.getWritePointer (ch);

            for (int s = 0; s < numSamples; ++s)
                rst[s] = in[s] - lo[s];
        }

        {
            juce::dsp::AudioBlock<float> block (midBandBuffer);
            block = block.getSubBlock (0, (size_t) numSamples);
            highShelfSplit.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* rst = midBandBuffer.getReadPointer (ch);
            const auto* in  = dryBuffer.getReadPointer (ch);
            const auto* lo  = lowBandBuffer.getReadPointer (ch);
            auto*       hi  = highBandBuffer.getWritePointer (ch);

            for (int s = 0; s < numSamples; ++s)
                hi[s] = (in[s] - lo[s]) - rst[s];
        }

        // Read a band's sample for channel ch, index s. Off returns 0.
        auto bandSample = [&] (int band, int ch, int s) -> float
        {
            switch (band)
            {
                case 1: return lowBandBuffer .getReadPointer (ch)[s];
                case 2: return midBandBuffer .getReadPointer (ch)[s];
                case 3: return highBandBuffer.getReadPointer (ch)[s];
                default: return 0.0f;
            }
        };

        float mix = currentMix;
        float bm  = bandMix;

        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float dry = dryBuffer.getReadPointer (ch)[s];

                // Band crossfade: blend prevSolo's band with activeSolo's
                // band. If the two are the same, this reduces to a single
                // band's output.
                const float prevBand = bandSample (prevSolo,   ch, s);
                const float nextBand = bandSample (activeSolo, ch, s);
                const float wet      = prevBand + (nextBand - prevBand) * bm;

                buffer.getWritePointer (ch)[s] = dry + (wet - dry) * mix;
            }

            // Advance band crossfade.
            if (bm < 1.0f)
                bm = juce::jmin (1.0f, bm + bandRampStep);

            // Advance dry/wet crossfade.
            if (mix < targetMix)
                mix = juce::jmin (targetMix, mix + mixRampStep);
            else if (mix > targetMix)
                mix = juce::jmax (targetMix, mix - mixRampStep);
        }

        currentMix = mix;
        bandMix    = bm;

        // When the band crossfade is complete, collapse prevSolo to
        // activeSolo so the "same band on both sides" fast path applies.
        if (bandMix >= 1.0f)
            prevSolo = activeSolo;

        // Snap fully-dry residue to exactly 0 so the fast path triggers.
        if (solo == 0 && currentMix <= mixRampStep)
            currentMix = 0.0f;
    }

private:
    bool prepared = false;
    int  solo     = 0;

    float targetLowHz  = 250.0f;
    float targetHighHz = 4000.0f;

    juce::dsp::LinkwitzRileyFilter<float> lowShelfSplit;
    juce::dsp::LinkwitzRileyFilter<float> highShelfSplit;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> midBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedLowHz;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedHighHz;

    // Dry/wet crossfade (Off <-> On).
    float currentMix    = 0.0f;
    float targetMix     = 0.0f;
    float mixRampStep   = 0.0f;

    // Band crossfade (Band A <-> Band B). `bandMix` goes 0 -> 1, at 0
    // the output is fully `prevSolo`, at 1 fully `activeSolo`.
    int   prevSolo      = 0;
    int   activeSolo    = 0;
    float bandMix       = 1.0f;
    float bandRampStep  = 0.0f;
};