#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

/**
    Very lightweight per-channel RMS/peak publisher.

    Audio thread: call pushSamples() with the block to meter.
    GUI thread:   read getRmsLinear() / getPeakLinear() and apply your
                  own ballistics for display.
*/
class LevelMeterSource
{
public:
    static constexpr int numChannels = 2;

    LevelMeterSource() = default;

    void reset()
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            rmsLinear[(size_t) ch].store (0.0f);
            peakLinear[(size_t) ch].store (0.0f);
        }
    }

    void pushSamples (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const int chans = juce::jmin (buffer.getNumChannels(), numChannels);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch >= chans)
            {
                rmsLinear[(size_t) ch].store (0.0f);
                peakLinear[(size_t) ch].store (0.0f);
                continue;
            }

            const auto* data = buffer.getReadPointer (ch, startSample);

            float sumSq = 0.0f;
            float peak  = 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                const float s = data[i];
                sumSq += s * s;
                peak = juce::jmax (peak, std::abs (s));
            }

            const float rms = std::sqrt (sumSq / (float) numSamples);

            rmsLinear[(size_t) ch].store (rms);
            peakLinear[(size_t) ch].store (peak);
        }
    }

    float getRmsLinear  (int ch) const noexcept { return rmsLinear[(size_t) juce::jlimit (0, numChannels - 1, ch)].load(); }
    float getPeakLinear (int ch) const noexcept { return peakLinear[(size_t) juce::jlimit (0, numChannels - 1, ch)].load(); }

private:
    std::array<std::atomic<float>, numChannels> rmsLinear  { { {}, {} } };
    std::array<std::atomic<float>, numChannels> peakLinear { { {}, {} } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeterSource)
};