#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 13;            // 8192
    static constexpr int fftSize  = 1 << fftOrder; // 8192
    static constexpr int numBins  = fftSize / 2;   // 4096

    SpectrumAnalyzer();

    void prepare (double sampleRate);
    void reset();

    void pushSamples (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void computeSpectrum();

    // Fast trace (5 ms attack / 200 ms release) — the translucent fill.
    float getMagnitudeDb (int bin) const noexcept { return smoothedDb[(size_t) bin]; }

    // Peak-hold trace — the stroked line.
    //
    // Ballistics: instantaneous attack (jumps to a new higher value
    // immediately), a fixed hold at the peak for kPeakHoldMs, then a
    // linear/exponential release at kPeakReleaseMs. This produces the
    // "peak marker that lingers then falls" behaviour that DAW peak
    // meters use, and it tracks the fast fill closely the rest of the
    // time.
    float getPeakHoldDb (int bin) const noexcept { return peakHoldDb[(size_t) bin]; }

    // Peak-since-reset trace, used by the display's hover tooltip.
    // Independent of the peak-hold line: this one never releases, it
    // just keeps climbing until resetPeaks() is called.
    float getPeakDb (int bin) const noexcept { return peakDb[(size_t) bin]; }

    // Display trace — a lightly smoothed version used for drawing the
    // low end of the curve. Much faster release than the fill so that
    // detail (peaks and valleys) is preserved, but slow enough to
    // prevent frame-to-frame flicker.
    float getDisplayDb (int bin) const noexcept { return displayDb[(size_t) bin]; }

    // Zero out the peak-since-reset trace. Called by the display when
    // the mouse enters the spectrum plot so the tooltip shows peaks
    // accumulated since entry.
    void resetPeaks();

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
    std::array<float, numBins> displayDb {};

    // Peak-hold line state.
    std::array<float, numBins>     peakHoldDb {};
    // Per-bin timestamp (ms) of the last time the peak-hold line
    // captured a new high. Used to know when the hold expires.
    std::array<juce::int64, numBins> peakHoldStartMs {};

    // Peak-since-reset (tooltip) state.
    std::array<float, numBins>     peakDb {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};