#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/**
    ITU-R BS.1770-4 loudness meter.

    Live use:
        prepare(), then pushSamples() from the audio thread. Read any of
        getShortTermLufs() / getMomentaryLufs() / getIntegratedLufs() from
        the GUI thread.

    - Short-term: 3-second sliding window.
    - Momentary: 400 ms sliding window.
    - Integrated: gated whole-session loudness per BS.1770-4, using
      400 ms blocks emitted every 100 ms, absolute gate at -70 LUFS and
      relative gate at (mean - 10 LU). Stored in a wrap-around ring, so
      very long sessions measure the last ~13.6 minutes.

    Offline use:
        analyseFile() runs a one-shot whole-file measurement and returns
        the loudness in LUFS. Used for gain-matching references.

    Note: analyseFile() uses a simplified (un-gated) mean square, which is
    within ~1 dB of true integrated LUFS for typical music. Fine for
    level-matching purposes.
*/
class LufsMeter
{
public:
    static constexpr double windowSeconds = 3.0;
    static constexpr double momentaryWindowSeconds = 0.4;
    static constexpr int    numChannels   = 2;

    LufsMeter() = default;

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    void pushSamples (const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // GUI thread getters. Each returns -70 (silence floor) when silent.
    float getShortTermLufs()  const noexcept { return shortTermLufs.load(); }
    float getMomentaryLufs()  const noexcept { return momentaryLufs.load(); }
    float getIntegratedLufs() const noexcept;   // cached; recomputed every ~500 ms

    // Offline: measure a file's integrated loudness using K-weighting +
    // whole-file mean square. Returns -70.0f on failure.
    static float analyseFile (const juce::File& file,
                              juce::AudioFormatManager& formats);

private:
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;

        struct State { double z1 = 0.0, z2 = 0.0; };
        std::array<State, numChannels> states {};

        void setCoefficients (double B0, double B1, double B2,
                              double A0, double A1, double A2) noexcept
        {
            b0 = B0 / A0;
            b1 = B1 / A0;
            b2 = B2 / A0;
            a1 = A1 / A0;
            a2 = A2 / A0;
        }

        void reset() noexcept
        {
            for (auto& s : states)
                s = {};
        }

        inline double process (int ch, double x) noexcept
        {
            auto& s = states[(size_t) ch];
            const double y = b0 * x + s.z1;
            s.z1 = b1 * x - a1 * y + s.z2;
            s.z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // Shared between live and offline paths.
    static void configureKWeighting (Biquad& stage1, Biquad& stage2, double sampleRate);

    void configureFilters (double sampleRate);
    void pushMillisecond (double msSumSqL, double msSumSqR, int sampleCount);
    void emitIntegratedBlock();

    // Recompute the gated integrated value by walking the block ring.
    float computeIntegratedLufs() const;

    double sr = 48000.0;

    Biquad stage1;
    Biquad stage2;

    // --- Short-term (3 s) ---
    std::vector<float> windowL;
    std::vector<float> windowR;
    int    windowSize = 0;
    int    writePos   = 0;
    double runningSumL = 0.0;
    double runningSumR = 0.0;

    // --- Momentary (400 ms) ---
    std::vector<float> momentaryWindowL;
    std::vector<float> momentaryWindowR;
    int    momentaryWindowSize = 0;
    int    momentaryWritePos   = 0;
    double momentarySumL = 0.0;
    double momentarySumR = 0.0;

    // --- Per-ms accumulators ---
    int    msSampleCount = 0;
    int    msSamplesNeeded = 0;
    double msAccumL = 0.0;
    double msAccumR = 0.0;

    // --- Integrated (gated, ring of block energies) ---
    static constexpr int integratedBlockMs   = 400;
    static constexpr int integratedOverlapMs = 100;
    static constexpr int integratedMaxBlocks = 8192;

    std::array<std::atomic<float>, integratedMaxBlocks> blockEnergy {};
    std::atomic<int> blockWritePos { 0 };
    std::atomic<int> blockTotalCount { 0 };

    // Audio-thread-only counter for emitting a block every 100 ms.
    int blockEmitCounter = 0;

    // Cached gated result, refreshed periodically.
    mutable std::atomic<float> integratedLufs { -70.0f };
    mutable double integratedLastComputedMs = 0.0;

    // Live readouts (written from the audio thread).
    std::atomic<float> shortTermLufs { -70.0f };
    std::atomic<float> momentaryLufs { -70.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LufsMeter)
};