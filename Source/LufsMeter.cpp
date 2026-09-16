#include "LufsMeter.h"
#include <cmath>

void LufsMeter::configureKWeighting (Biquad& stage1,
                                     Biquad& stage2,
                                     double sampleRate)
{
    // ITU-R BS.1770-4 K-weighting stage 1: high-shelf "head" filter.
    {
        const double f0 = 1681.974450955533;
        const double G  = 3.999843853973347;
        const double Q  = 0.7071752369554196;

        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);

        const double a0_ = 1.0 + K / Q + K * K;

        const double b0 = (Vh + Vb * K / Q + K * K) / a0_;
        const double b1 = 2.0 * (K * K - Vh) / a0_;
        const double b2 = (Vh - Vb * K / Q + K * K) / a0_;
        const double a1 = 2.0 * (K * K - 1.0) / a0_;
        const double a2 = (1.0 - K / Q + K * K) / a0_;

        stage1.setCoefficients (b0, b1, b2, 1.0, a1, a2);
    }

    // Stage 2: RLB high-pass filter at 38 Hz.
    {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;

        const double K  = std::tan (juce::MathConstants<double>::pi * f0 / sampleRate);
        const double a0_ = 1.0 + K / Q + K * K;

        const double b0 = 1.0;
        const double b1 = -2.0;
        const double b2 = 1.0;
        const double a1 = 2.0 * (K * K - 1.0) / a0_;
        const double a2 = (1.0 - K / Q + K * K) / a0_;

        stage2.setCoefficients (b0 / a0_, b1 / a0_, b2 / a0_, 1.0, a1, a2);
    }
}

void LufsMeter::configureFilters (double sampleRate)
{
    configureKWeighting (stage1, stage2, sampleRate);
}

//==============================================================================

void LufsMeter::prepare (double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;

    configureFilters (sr);

    windowSize = (int) std::round (sr * 0.001 * windowSeconds);
    if (windowSize < 1)
        windowSize = 1;

    windowL.assign ((size_t) windowSize, 0.0f);
    windowR.assign ((size_t) windowSize, 0.0f);

    momentaryWindowSize = (int) std::round (sr * 0.001 * momentaryWindowSeconds);
    if (momentaryWindowSize < 1)
        momentaryWindowSize = 1;

    momentaryWindowL.assign ((size_t) momentaryWindowSize, 0.0f);
    momentaryWindowR.assign ((size_t) momentaryWindowSize, 0.0f);

    msSamplesNeeded = (int) std::round (sr * 0.001);
    if (msSamplesNeeded < 1)
        msSamplesNeeded = 1;

    reset();
}

void LufsMeter::reset()
{
    stage1.reset();
    stage2.reset();

    std::fill (windowL.begin(), windowL.end(), 0.0f);
    std::fill (windowR.begin(), windowR.end(), 0.0f);

    writePos    = 0;
    runningSumL = 0.0;
    runningSumR = 0.0;

    std::fill (momentaryWindowL.begin(), momentaryWindowL.end(), 0.0f);
    std::fill (momentaryWindowR.begin(), momentaryWindowR.end(), 0.0f);

    momentaryWritePos = 0;
    momentarySumL = 0.0;
    momentarySumR = 0.0;

    msSampleCount = 0;
    msAccumL      = 0.0;
    msAccumR      = 0.0;

    shortTermLufs.store (-70.0f);
    momentaryLufs.store (-70.0f);

    for (auto& b : blockEnergy)
        b.store (0.0f);

    blockWritePos.store (0);
    blockTotalCount.store (0);
    blockEmitCounter = 0;

    integratedLufs.store (-70.0f);
    integratedLastComputedMs = 0.0;
}

//==============================================================================

void LufsMeter::pushSamples (const juce::AudioBuffer<float>& buffer,
                             int startSample,
                             int numSamples)
{
    if (numSamples <= 0)
        return;

    const int chans = juce::jmin (buffer.getNumChannels(), numChannels);

    if (chans <= 0)
        return;

    int samplesProcessed = 0;

    while (samplesProcessed < numSamples)
    {
        const int remaining = numSamples - samplesProcessed;
        const int thisChunk = juce::jmin (remaining, msSamplesNeeded - msSampleCount);

        if (thisChunk <= 0)
            break;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const bool validCh = (ch < chans);

            const float* src = validCh
                ? buffer.getReadPointer (ch, startSample + samplesProcessed)
                : nullptr;

            for (int i = 0; i < thisChunk; ++i)
            {
                const double x = validCh ? (double) src[i] : 0.0;

                const double s1 = stage1.process (ch, x);
                const double s2 = stage2.process (ch, s1);

                const double sq = s2 * s2;

                if (ch == 0) msAccumL += sq;
                else         msAccumR += sq;
            }
        }

        msSampleCount += thisChunk;
        samplesProcessed += thisChunk;

        if (msSampleCount >= msSamplesNeeded)
        {
            pushMillisecond (msAccumL, msAccumR, msSampleCount);
            msSampleCount = 0;
            msAccumL      = 0.0;
            msAccumR      = 0.0;
        }
    }
}

void LufsMeter::pushMillisecond (double msSumSqL, double msSumSqR, int /*sampleCount*/)
{
    if (windowSize <= 0 || momentaryWindowSize <= 0)
        return;

    // --- Short-term window (3 s) ---
    runningSumL -= (double) windowL[(size_t) writePos];
    runningSumR -= (double) windowR[(size_t) writePos];

    windowL[(size_t) writePos] = (float) msSumSqL;
    windowR[(size_t) writePos] = (float) msSumSqR;

    runningSumL += msSumSqL;
    runningSumR += msSumSqR;

    writePos = (writePos + 1) % windowSize;

    if (runningSumL < 0.0) runningSumL = 0.0;
    if (runningSumR < 0.0) runningSumR = 0.0;

    {
        const double totalSamples = (double) windowSize * (double) msSamplesNeeded;

        if (totalSamples > 0.0)
        {
            const double zL = runningSumL / totalSamples;
            const double zR = runningSumR / totalSamples;
            const double z  = zL + zR;

            const float lufs = (z > 1.0e-12)
                ? (float) (-0.691 + 10.0 * std::log10 (z))
                : -70.0f;

            shortTermLufs.store (juce::jlimit (-70.0f, 5.0f, lufs));
        }
    }

    // --- Momentary window (400 ms) ---
    momentarySumL -= (double) momentaryWindowL[(size_t) momentaryWritePos];
    momentarySumR -= (double) momentaryWindowR[(size_t) momentaryWritePos];

    momentaryWindowL[(size_t) momentaryWritePos] = (float) msSumSqL;
    momentaryWindowR[(size_t) momentaryWritePos] = (float) msSumSqR;

    momentarySumL += msSumSqL;
    momentarySumR += msSumSqR;

    momentaryWritePos = (momentaryWritePos + 1) % momentaryWindowSize;

    if (momentarySumL < 0.0) momentarySumL = 0.0;
    if (momentarySumR < 0.0) momentarySumR = 0.0;

    {
        const double totalSamples = (double) momentaryWindowSize * (double) msSamplesNeeded;

        if (totalSamples > 0.0)
        {
            const double zL = momentarySumL / totalSamples;
            const double zR = momentarySumR / totalSamples;
            const double z  = zL + zR;

            const float lufs = (z > 1.0e-12)
                ? (float) (-0.691 + 10.0 * std::log10 (z))
                : -70.0f;

            momentaryLufs.store (juce::jlimit (-70.0f, 5.0f, lufs));
        }
    }

    // --- Integrated block emission (every 100 ms) ---
    if (++blockEmitCounter >= integratedOverlapMs)
    {
        blockEmitCounter = 0;
        emitIntegratedBlock();
    }
}

void LufsMeter::emitIntegratedBlock()
{
    if (momentaryWindowSize <= 0 || msSamplesNeeded <= 0)
        return;

    const double totalSamples = (double) momentaryWindowSize * (double) msSamplesNeeded;
    if (totalSamples <= 0.0)
        return;

    // Energy (mean square) over the last 400 ms, summed across channels.
    const double zL = momentarySumL / totalSamples;
    const double zR = momentarySumR / totalSamples;
    const double z  = zL + zR;

    const int pos = blockWritePos.load (std::memory_order_relaxed);
    blockEnergy[(size_t) pos].store ((float) z, std::memory_order_relaxed);

    const int next = (pos + 1) % integratedMaxBlocks;
    blockWritePos.store (next, std::memory_order_relaxed);
    blockTotalCount.fetch_add (1, std::memory_order_relaxed);
}

float LufsMeter::getIntegratedLufs() const noexcept
{
    // Recompute at most every 500 ms so we don't walk the ring on every
    // UI repaint.
    const double nowMs = juce::Time::getMillisecondCounterHiRes();

    if (nowMs - integratedLastComputedMs >= 500.0
        || integratedLastComputedMs == 0.0)
    {
        integratedLastComputedMs = nowMs;
        const float value = computeIntegratedLufs();
        integratedLufs.store (value, std::memory_order_relaxed);
    }

    return integratedLufs.load (std::memory_order_relaxed);
}

float LufsMeter::computeIntegratedLufs() const
{
    const int total = blockTotalCount.load (std::memory_order_relaxed);
    const int n = juce::jmin (total, integratedMaxBlocks);

    if (n <= 0)
        return -70.0f;

    // Pass 1: absolute gate at -70 LUFS.
    double sumAbsolute = 0.0;
    int    countAbsolute = 0;

    for (int i = 0; i < n; ++i)
    {
        const double z = (double) blockEnergy[(size_t) i].load (std::memory_order_relaxed);

        if (z > 0.0)
        {
            const double l = -0.691 + 10.0 * std::log10 (juce::jmax (z, 1.0e-12));

            if (l > -70.0)
            {
                sumAbsolute += z;
                ++countAbsolute;
            }
        }
    }

    if (countAbsolute <= 0)
        return -70.0f;

    const double zAbsMean = sumAbsolute / (double) countAbsolute;
    const double relGate  = -0.691 + 10.0 * std::log10 (juce::jmax (zAbsMean, 1.0e-12)) - 10.0;

    // Pass 2: relative gate.
    double sumRelative = 0.0;
    int    countRelative = 0;

    for (int i = 0; i < n; ++i)
    {
        const double z = (double) blockEnergy[(size_t) i].load (std::memory_order_relaxed);

        if (z > 0.0)
        {
            const double l = -0.691 + 10.0 * std::log10 (juce::jmax (z, 1.0e-12));

            if (l > relGate)
            {
                sumRelative += z;
                ++countRelative;
            }
        }
    }

    if (countRelative <= 0)
        return -70.0f;

    const double zFinal = sumRelative / (double) countRelative;

    const double lufs = -0.691 + 10.0 * std::log10 (juce::jmax (zFinal, 1.0e-12));

    return (float) juce::jlimit (-70.0, 5.0, lufs);
}

//==============================================================================
// Offline whole-file analysis
//==============================================================================

float LufsMeter::analyseFile (const juce::File& file,
                              juce::AudioFormatManager& formats)
{
    if (! file.existsAsFile())
        return -70.0f;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples <= 0)
        return -70.0f;

    const double sampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : 48000.0;
    const int channels = juce::jlimit (1, numChannels, (int) reader->numChannels);

    Biquad s1, s2;
    configureKWeighting (s1, s2, sampleRate);

    constexpr int blockSize = 8192;
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1, channels), blockSize);

    double sumSqL = 0.0;
    double sumSqR = 0.0;
    juce::int64 totalSamples = 0;

    juce::int64 position = 0;

    while (position < reader->lengthInSamples)
    {
        const int thisBlock = (int) juce::jmin ((juce::int64) blockSize,
                                                reader->lengthInSamples - position);

        if (! reader->read (&buffer, 0, thisBlock, position, true, true))
            break;

        for (int i = 0; i < thisBlock; ++i)
        {
            const double xL = buffer.getNumChannels() > 0
                ? (double) buffer.getReadPointer (0)[i] : 0.0;
            const double xR = buffer.getNumChannels() > 1
                ? (double) buffer.getReadPointer (1)[i] : xL;

            const double kL = s2.process (0, s1.process (0, xL));
            const double kR = s2.process (1, s1.process (1, xR));

            sumSqL += kL * kL;
            sumSqR += kR * kR;
        }

        totalSamples += thisBlock;
        position += thisBlock;
    }

    if (totalSamples <= 0)
        return -70.0f;

    const double zL = sumSqL / (double) totalSamples;
    const double zR = sumSqR / (double) totalSamples;
    const double z  = zL + zR;

    if (z <= 1.0e-12)
        return -70.0f;

    const double lufs = -0.691 + 10.0 * std::log10 (z);

    return (float) juce::jlimit (-70.0, 10.0, lufs);
}