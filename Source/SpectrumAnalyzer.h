#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 11;            // 2048
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    static constexpr int numBins  = fftSize / 2;   // 1024

    SpectrumAnalyzer();

    void prepare (double sampleRate);
    void reset();

    void pushSamples (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void computeSpectrum();

    // Fast trace (5 ms attack / 200 ms release).
    float getMagnitudeDb (int bin) const noexcept { return smoothedDb[(size_t) bin]; }

    // Slow trace (150 ms attack / 800 ms release) - the "trend" line.
    float getSlowMagnitudeDb (int bin) const noexcept { return slowSmoothedDb[(size_t) bin]; }

    double getSampleRate() const noexcept { return sampleRate; }
    int    getNumBins()    const noexcept { return numBins; }

private:
    double sampleRate = 48000.0;

    juce::AbstractFifo fifo { fftSize };
    juce::AudioBuffer<float> ringBuffer;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann,
                                                 false };

    std::array<float, fftSize>     fftData {};
    std::array<float, fftSize * 2> fftWork {};
    std::array<float, numBins>     magnitudesDb {};
    std::array<float, numBins>     smoothedDb {};
    std::array<float, numBins>     slowSmoothedDb {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};