#include "SpectrumAnalyzer.h"

SpectrumAnalyzer::SpectrumAnalyzer()
{
    ringBuffer.setSize (2, fftSize);
    ringBuffer.clear();
    magnitudesDb.fill (-120.0f);
    smoothedDb.fill (-120.0f);
    slowSmoothedDb.fill (-120.0f);
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
    slowSmoothedDb.fill (-120.0f);
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
    // - performFrequencyOnlyForwardTransform returns unnormalized magnitudes.
    // - A full-scale sine produces magnitude ~ fftSize/2 at its bin.
    // - The Hann window has a coherent gain of ~0.5, which halves the peak.
    // - To map "full-scale sine" to 0 dB, we multiply by:
    //     2.0f (undo window coherent gain) * 2.0f / fftSize (undo FFT scale)
    //     = 4.0f / fftSize.
    const float magnitudeScale = 4.0f / (float) fftSize;

    // Fast trace ballistics.
    constexpr float attackMs  = 5.0f;
    constexpr float releaseMs = 200.0f;

    // Slow trace ballistics - the trend line. Asymmetric so transient
    // hits (e.g. a snare's 200 Hz energy) still register at nearly their
    // true peak level, while the trace decays slowly afterwards.
    constexpr float slowAttackMs  = 100.0f;
    constexpr float slowReleaseMs = 800.0f;

    // Below this dB level, a trace is considered to be "at the floor".
    constexpr float floorSnapDb = -119.0f;

    const float blockSeconds = 1.0f / 30.0f;
    const float attackCoeff      = std::exp (-blockSeconds / (attackMs      * 0.001f));
    const float releaseCoeff     = std::exp (-blockSeconds / (releaseMs     * 0.001f));
    const float slowAttackCoeff  = std::exp (-blockSeconds / (slowAttackMs  * 0.001f));
    const float slowReleaseCoeff = std::exp (-blockSeconds / (slowReleaseMs * 0.001f));

    // First pass: compute the raw dB values and detect whether signal has
    // just returned from silence (whole slow trace was at floor).
    std::array<float, numBins> rawDb {};
    bool slowWasAtFloor = true;
    bool signalIsPresent = false;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float rawMag = fftWork[(size_t) bin] * magnitudeScale;
        const float db     = juce::Decibels::gainToDecibels (rawMag, -120.0f);

        rawDb[(size_t) bin] = db;
        magnitudesDb[(size_t) bin] = db;

        if (slowSmoothedDb[(size_t) bin] > floorSnapDb)
            slowWasAtFloor = false;

        if (db > floorSnapDb)
            signalIsPresent = true;
    }

    // If the slow trace was entirely at the floor and signal just appeared,
    // initialise the slow trace to 0 dB across the whole spectrum. The
    // smoothing coefficients then pull each bin down to its true value
    // over the next ~800 ms, producing a "flat line at the top that takes
    // the shape of the signal" behaviour.
    const bool snapSlowToTop = slowWasAtFloor && signalIsPresent;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float db = rawDb[(size_t) bin];

        // Fast smoothing (independent state, does not snap to top).
        {
            const float prev = smoothedDb[(size_t) bin];

            if (prev <= floorSnapDb && db > floorSnapDb)
            {
                // Trace was at the floor; snap straight to the new value.
                smoothedDb[(size_t) bin] = db;
            }
            else
            {
                const float coeff = db > prev ? attackCoeff : releaseCoeff;
                smoothedDb[(size_t) bin] = db + coeff * (prev - db);
            }
        }

        // Slow smoothing (independent state).
        {
            float prev = slowSmoothedDb[(size_t) bin];

            if (snapSlowToTop)
            {
                // Start from the top of the graph; converge downward.
                prev = 0.0f;
            }
            else if (prev <= floorSnapDb && db > floorSnapDb)
            {
                // Signal returned to a bin that was at the floor (but the
                // overall slow trace was not fully silent, so no top snap).
                prev = db;
            }

            const float coeff = db > prev ? slowAttackCoeff : slowReleaseCoeff;
            slowSmoothedDb[(size_t) bin] = db + coeff * (prev - db);
        }
    }
}