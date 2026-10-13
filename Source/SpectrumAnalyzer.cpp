#include "SpectrumAnalyzer.h"
#include "Theme.h"

namespace
{
    // Peak-hold ballistics for the stroked line.
    //
    // Attack is instantaneous: whenever the fast smoothed value rises
    // above the currently held peak, the peak jumps to it and the hold
    // timer restarts. After the hold expires, the peak decays
    // exponentially toward the current smoothed value at the release
    // time constant.
    constexpr float peakHoldMs    = 320.0f;
    constexpr float peakReleaseMs = 150.0f;

    // Display-trace ballistics. Very light smoothing so that the low end
    // of the curve keeps its real peaks and valleys. Slow enough that
    // individual FFT frames don't visibly flicker.
    constexpr float displayAttackMs  = 10.0f;
    constexpr float displayReleaseMs = 300.0f;
}

SpectrumAnalyzer::SpectrumAnalyzer()
{
    ringBuffer.setSize (2, fftSize);
    ringBuffer.clear();
    magnitudesDb.fill (-120.0f);
    smoothedDb.fill (-120.0f);
    displayDb.fill (-120.0f);
    peakHoldDb.fill (-120.0f);
    peakHoldStartMs.fill (0);
    peakDb.fill (-120.0f);
}

void SpectrumAnalyzer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    reset();
}

void SpectrumAnalyzer::reset()
{
    fifo.reset();
    ringBuffer.clear();
    fftData.fill (0.0f);
    fftWork.fill (0.0f);
    magnitudesDb.fill (-120.0f);
    smoothedDb.fill (-120.0f);
    displayDb.fill (-120.0f);
    peakHoldDb.fill (-120.0f);
    peakHoldStartMs.fill (0);
    peakDb.fill (-120.0f);
}

void SpectrumAnalyzer::resetPeaks()
{
    peakDb.fill (-120.0f);
}

void SpectrumAnalyzer::pushSamples (const juce::AudioBuffer<float>& buffer,
                                    int startSample,
                                    int numSamples)
{
    if (numSamples <= 0)
        return;

    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    auto writeMonoSum = [&] (int destStart, int srcStart, int count)
    {
        if (count <= 0)
            return;

        auto* dstL = ringBuffer.getWritePointer (0);
        auto* dstR = ringBuffer.getWritePointer (1);
        const int chans = juce::jmin (2, numChannels);

        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int c = 0; c < chans; ++c)
                sum += buffer.getReadPointer (c)[startSample + srcStart + i];
            sum /= (float) chans;

            dstL[destStart + i] = sum;
            dstR[destStart + i] = sum;
        }
    };

    writeMonoSum (start1, 0, size1);
    writeMonoSum (start2, size1, size2);

    fifo.finishedWrite (size1 + size2);
}

void SpectrumAnalyzer::computeSpectrum()
{
    const int available = fifo.getNumReady();
    if (available <= 0)
        return;

    const int toRead = juce::jmin (available, fftSize);

    int start1, size1, start2, size2;
    fifo.prepareToRead (toRead, start1, size1, start2, size2);

    std::array<float, fftSize> timeDomain {};
    timeDomain.fill (0.0f);

    const int padOffset = fftSize - toRead;

    auto readChunk = [&] (int srcStart, int destStart, int count)
    {
        if (count <= 0) return;
        const auto* src = ringBuffer.getReadPointer (0);
        for (int i = 0; i < count; ++i)
            timeDomain[(size_t) (destStart + i)] = src[srcStart + i];
    };

    int destPos = padOffset;
    readChunk (start1, destPos, size1);
    destPos += size1;
    readChunk (start2, destPos, size2);

    fifo.finishedRead (size1 + size2);

    std::copy (timeDomain.begin(), timeDomain.end(), fftData.begin());
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);

    std::fill (fftWork.begin(), fftWork.end(), 0.0f);
    std::copy (fftData.begin(), fftData.end(), fftWork.begin());

    fft.performFrequencyOnlyForwardTransform (fftWork.data(), true);

    // FFT magnitude calibration:
    //
    // 4.0f / fftSize is the textbook normalisation: a full-scale sine
    // that lands exactly on a bin would peak at 0 dB.
    //
    // In practice, real signals spread across several adjacent bins,
    // and the wider the FFT (i.e. the narrower each bin), the more the
    // energy is spread out. That means a full-scale sine peaks well
    // below 0 dB on an 8192-point FFT. Multiply by 2.0f (i.e. +6 dB)
    // to compensate, so real material reaches roughly the same heights
    // on the display as it did at 2048 points.
    const float magnitudeScale = 20.0f / (float) fftSize;

    // Fast trace ballistics (the translucent fill).
    constexpr float attackMs  = 5.0f;
    constexpr float releaseMs = 350.0f;

    // Below this dB level, a trace is considered to be "at the floor".
    constexpr float floorSnapDb = -119.0f;

    const float blockSeconds = Display::blockSeconds;
    const float attackCoeff  = std::exp (-blockSeconds / (attackMs  * 0.001f));
    const float releaseCoeff = std::exp (-blockSeconds / (releaseMs * 0.001f));

    // Coefficient for the peak-hold release phase.
    const float peakReleaseCoeff = std::exp (-blockSeconds / (peakReleaseMs * 0.001f));

    // Display trace coefficients (very light smoothing).
    const float displayAttackCoeff  = std::exp (-blockSeconds / (displayAttackMs  * 0.001f));
    const float displayReleaseCoeff = std::exp (-blockSeconds / (displayReleaseMs * 0.001f));

    const juce::int64 nowMs = juce::Time::currentTimeMillis();

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float rawMag = fftWork[(size_t) bin] * magnitudeScale;
        const float db     = juce::Decibels::gainToDecibels (rawMag, -120.0f);

        magnitudesDb[(size_t) bin] = db;

        // --- Fast smoothing (fill) ---
        {
            const float prev = smoothedDb[(size_t) bin];

            if (prev <= floorSnapDb && db > floorSnapDb)
            {
                smoothedDb[(size_t) bin] = db;
            }
            else
            {
                const float coeff = db > prev ? attackCoeff : releaseCoeff;
                smoothedDb[(size_t) bin] = db + coeff * (prev - db);
            }
        }

        // --- Display trace (very light smoothing) ---
        {
            const float prev = displayDb[(size_t) bin];

            if (prev <= floorSnapDb && db > floorSnapDb)
            {
                displayDb[(size_t) bin] = db;
            }
            else
            {
                const float coeff = db > prev ? displayAttackCoeff : displayReleaseCoeff;
                displayDb[(size_t) bin] = db + coeff * (prev - db);
            }
        }

                const float current = displayDb[(size_t) bin];

        // --- Peak-hold line ---
        {
            float& heldPeak = peakHoldDb[(size_t) bin];
            auto&  holdStart = peakHoldStartMs[(size_t) bin];

            if (current > heldPeak + 0.01f)
            {
                heldPeak = current;
                holdStart = nowMs;
            }
            else
            {
                const juce::int64 elapsed = nowMs - holdStart;

                if (elapsed > (juce::int64) peakHoldMs)
                {
                    heldPeak = current + peakReleaseCoeff * (heldPeak - current);

                    if (heldPeak <= floorSnapDb && current <= floorSnapDb)
                        heldPeak = floorSnapDb;
                }
            }
        }

        // --- Peak-since-reset (tooltip) ---
        {
            if (current > peakDb[(size_t) bin])
                peakDb[(size_t) bin] = current;
        }
    }

    // -----------------------------------------------------------------
    // Cross-bin smoothing pass.
    //
    // The per-bin values are already temporally smoothed (per bin,
    // independent history). But at high frequencies the display shows
    // hundreds of bins packed into a few pixels, and adjacent bins can
    // be at very different levels — one on a harmonic peak, the next in
    // a trough. A light moving average across neighbouring bins smooths
    // the visual envelope without noticeably reducing real peaks.
    //
    // Applies only to smoothedDb and peakHoldDb. The display trace is
    // deliberately left untouched so its detail survives to the renderer.
    // -----------------------------------------------------------------
       {
        std::array<float, numBins> tmpSmoothed {};

        constexpr int halfWindow = 2;

        for (int bin = 0; bin < numBins; ++bin)
        {
            const int lo = juce::jmax (0, bin - halfWindow);
            const int hi = juce::jmin (numBins - 1, bin + halfWindow);

            float sumS = 0.0f;
            int count = 0;

            for (int b = lo; b <= hi; ++b)
            {
                sumS += smoothedDb[(size_t) b];
                ++count;
            }

            tmpSmoothed[(size_t) bin] = sumS / (float) count;
        }

        smoothedDb = tmpSmoothed;
    }
}