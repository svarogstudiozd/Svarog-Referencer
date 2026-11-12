#include "ReferenceSlot.h"
#include "LufsMeter.h"

struct LoadReferenceJob final : juce::ThreadPoolJob
{
    LoadReferenceJob (juce::AudioFormatManager& formatsToUse,
                      juce::WeakReference<ReferenceSlot> slotToUse,
                      juce::File fileToLoad,
                      uint32_t tokenToUse)
        : ThreadPoolJob ("LoadReference"),
          formats (formatsToUse),
          slot (std::move (slotToUse)),
          file (std::move (fileToLoad)),
          token (tokenToUse)
    {
    }

    JobStatus runJob() override
    {
        auto decoded = std::unique_ptr<juce::AudioFormatReader> (
            formats.createReaderFor (file));

        juce::MessageManager::callAsync (
            [slotToNotify = slot,
             fileToLoad = file,
             loadGeneration = token,
             formatReader = std::move (decoded)]() mutable
            {
                if (slotToNotify != nullptr)
                    slotToNotify->finishLoad (fileToLoad,
                                              loadGeneration,
                                              std::move (formatReader));
            });

        return jobHasFinished;
    }

    juce::AudioFormatManager& formats;
    juce::WeakReference<ReferenceSlot> slot;
    juce::File file;
    uint32_t token = 0;
};

//==============================================================================

struct MeasureLufsJob final : juce::ThreadPoolJob
{
    MeasureLufsJob (juce::AudioFormatManager& formatsToUse,
                    juce::WeakReference<ReferenceSlot> slotToUse,
                    juce::File fileToMeasure,
                    uint32_t tokenToUse)
        : ThreadPoolJob ("MeasureLufs"),
          formats (formatsToUse),
          slot (std::move (slotToUse)),
          file (std::move (fileToMeasure)),
          token (tokenToUse)
    {
    }

    JobStatus runJob() override
    {
        const float lufs = LufsMeter::analyseFile (file, formats);

        juce::MessageManager::callAsync (
            [slotToNotify = slot, loadGeneration = token, result = lufs]() mutable
            {
                if (slotToNotify != nullptr)
                {
                    if (slotToNotify->loadToken.load() == loadGeneration)
                    {
                        slotToNotify->integratedLufs.store (result);
                        slotToNotify->sendChangeMessage();
                    }
                }
            });

        return jobHasFinished;
    }

    juce::AudioFormatManager& formats;
    juce::WeakReference<ReferenceSlot> slot;
    juce::File file;
    uint32_t token = 0;
};

//==============================================================================

ReferenceSlot::ReferenceSlot (juce::AudioFormatManager& formats,
                              juce::AudioThumbnailCache& thumbnailCache,
                              juce::TimeSliceThread& readAhead,
                              juce::ThreadPool& pool)
    : formatManager (formats),
      readAheadThread (readAhead),
      loadPool (pool),
      thumbnail (512, formats, thumbnailCache)
{
    transport.setLooping (false);
    shouldBePlaying.store (false);
    pausedByTransport.store (false);
    transportRunning.store (false);
}

ReferenceSlot::~ReferenceSlot()
{
    ++loadToken;

    const juce::SpinLock::ScopedLockType lock (callbackLock);

    transport.setSource (nullptr);
    readerSource.reset();
    thumbnail.setSource (nullptr);
}

//==============================================================================

void ReferenceSlot::prepare (double sampleRate,
                             int samplesPerBlock,
                             int numChannels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    currentNumChannels = numChannels;

    prepared = true;

    transport.prepareToPlay (samplesPerBlock, sampleRate);
    transport.start();
    transportRunning.store (true);

    lastBlockBuffer.setSize (juce::jmax (2, numChannels),
                             juce::jmax (1, samplesPerBlock),
                             false, true, true);
    lastBlockBuffer.clear();
    hasLastBlock = false;
    pendingPauseFade = false;
    pauseFadeLevel = 1.0f;
}

void ReferenceSlot::releaseResources()
{
    transport.stop();
    transportRunning.store (false);
    transport.releaseResources();
    prepared = false;
}

//==============================================================================

void ReferenceSlot::reset()
{
    ++loadToken;

    loading.store (false);
    loaded.store (false);

    {
        const juce::SpinLock::ScopedLockType lock (callbackLock);

        transport.stop();
        transport.setSource (nullptr);
        readerSource.reset();
        thumbnail.setSource (nullptr);

        currentFile = juce::File();
        missingFilePath = juce::File();
        statusText = "Empty";

        missingFile.store (false);

        pendingPosition.store (-1.0f);

        loopEnabled.store (false);
        loopStartNormalized.store (0.0f);
        loopEndNormalized.store (1.0f);

        shouldBePlaying.store (false);
        pausedByTransport.store (false);
        pausedPosition.store (0.0);

        followMode.store (false);
        followTargetSeconds.store (-1.0);

        integratedLufs.store (-70.0f);

        transport.start();
        transportRunning.store (true);
    }

    sendChangeMessage();
}

//==============================================================================

void ReferenceSlot::loadFileAsync (const juce::File& file)
{
    // If the file doesn't exist (deleted, moved, external drive
    // unmounted, etc.), record the intent so the tile and detail view
    // can surface a "missing file" state and offer to relink.
    if (! file.existsAsFile())
    {
        const auto token = ++loadToken;

        loading.store (false);
        loaded.store (false);
        missingFile.store (true);

        {
            const juce::SpinLock::ScopedLockType lock (callbackLock);

            transport.stop();
            transport.setSource (nullptr);
            readerSource.reset();
            thumbnail.setSource (nullptr);

            currentFile = juce::File();
            missingFilePath = file;
            statusText = "Missing: " + file.getFileName();
            pendingPosition.store (-1.0f);
            integratedLufs.store (-70.0f);

            transport.start();
            transportRunning.store (true);
        }

        juce::ignoreUnused (token);
        sendChangeMessage();
        return;
    }

    const auto token = ++loadToken;

    loading.store (true);
    loaded.store (false);
    missingFile.store (false);
    missingFilePath = juce::File();
    integratedLufs.store (-70.0f);

    statusText = "Loading " + file.getFileName() + "...";
    sendChangeMessage();

    loadPool.addJob (
        new LoadReferenceJob (formatManager, this, file, token),
        true);

    loadPool.addJob (
        new MeasureLufsJob (formatManager, this, file, token),
        true);
}

void ReferenceSlot::finishLoad (const juce::File& file,
                                uint32_t token,
                                std::unique_ptr<juce::AudioFormatReader> reader)
{
    if (token != loadToken.load())
        return;

    if (reader == nullptr)
    {
        loading.store (false);
        statusText = "Couldn't read " + file.getFileName();
        sendChangeMessage();
        return;
    }

    const auto sourceSampleRate = reader->sampleRate;

    {
        const juce::SpinLock::ScopedLockType lock (callbackLock);

        transport.stop();
        transport.setSource (nullptr);

        readerSource = std::make_unique<juce::AudioFormatReaderSource> (
            reader.release(), true);

        currentFile = file;

        transport.setSource (readerSource.get(),
                             32768,
                             &readAheadThread,
                             sourceSampleRate,
                             currentNumChannels);

        if (prepared)
            transport.prepareToPlay (currentBlockSize, currentSampleRate);

        const auto pending = pendingPosition.exchange (-1.0f);
        const auto length = transport.getLengthInSeconds();

        const auto position = pending >= 0.0f
                                ? juce::jlimit (0.0f, 1.0f, pending)
                                : 0.0f;

        transport.setPosition (position * length);

        thumbnail.setSource (new juce::FileInputSource (file));

        transport.start();
        transportRunning.store (true);
    }

    loaded.store (true);
    loading.store (false);
    missingFile.store (false);

    statusText = file.getFileName();

    sendChangeMessage();
}

//==============================================================================

void ReferenceSlot::setPendingNormalizedPosition (float zeroToOne)
{
    pendingPosition.store (
        juce::jlimit (0.0f, 1.0f, zeroToOne));
}

//==============================================================================

void ReferenceSlot::setFollowMode (bool shouldFollow)
{
    followMode.store (shouldFollow);

    if (! shouldFollow)
        followTargetSeconds.store (-1.0);
}

//==============================================================================

void ReferenceSlot::handleTransportEnd()
{
    const auto length = transport.getLengthInSeconds();

    if (length <= 0.0)
        return;

    const auto position = transport.getCurrentPosition();

    // "At the end" means position is very close to length. A small
    // tolerance is used because floating-point positions do not
    // land exactly on `length`.
    const bool atEnd = (length - position) < 0.05;

    if (! atEnd)
        return;

    if (shouldBePlaying.load() && ! pausedByTransport.load())
    {
        // Still supposed to be playing. Rewind and restart.
        transport.setPosition (0.0);
        transport.start();
    }
    else
    {
        // Not supposed to be playing. Rewind so the next play starts
        // from the top.
        transport.setPosition (0.0);
    }
}

void ReferenceSlot::ensureTransportRunning()
{
    if (shouldBePlaying.load() && ! pausedByTransport.load())
    {
        if (! transport.isPlaying())
            transport.start();
    }
}

//==============================================================================

void ReferenceSlot::getNextAudioBlock (
    const juce::AudioSourceChannelInfo& info)
{
    const int numSamples = info.numSamples;

    if (! prepared || ! loaded.load() || readerSource == nullptr || numSamples <= 0)
    {
        info.clearActiveBufferRegion();
        return;
    }

    const juce::SpinLock::ScopedTryLockType lock (callbackLock);

    if (! lock.isLocked())
    {
        info.clearActiveBufferRegion();
        return;
    }

    // ---------- Pending pause: replay last block, fade to zero ----------
    if (pendingPauseFade.load())
    {
        const int outChans = info.buffer->getNumChannels();
        const int srcChans = lastBlockBuffer.getNumChannels();
        const int chans = juce::jmin (outChans, srcChans);

        if (hasLastBlock && lastBlockBuffer.getNumSamples() >= numSamples)
        {
            for (int ch = 0; ch < chans; ++ch)
                info.buffer->copyFrom (ch, info.startSample,
                                       lastBlockBuffer, ch, 0, numSamples);

            // Zero any extra channels we don't have source for.
            for (int ch = chans; ch < outChans; ++ch)
                info.buffer->clear (ch, info.startSample, numSamples);
        }
        else
        {
            info.clearActiveBufferRegion();
        }

        const float step = 1.0f / (0.005f * (float) currentSampleRate);
        float env = pauseFadeLevel.load();

        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < outChans; ++ch)
                info.buffer->getWritePointer (ch)[info.startSample + s] *= env;

            env = juce::jmax (0.0f, env - step);
        }

        pauseFadeLevel.store (env);

        if (env <= 1.0e-6f)
        {
            pendingPauseFade.store (false);
            pauseFadeLevel.store (1.0f);
            shouldBePlaying.store (false);
            pausedByTransport.store (true);
            hasLastBlock = false;
            lastBlockBuffer.clear();
        }

        return;
    }

    // ---------- Guard: paused or stopped ----------
    const bool shouldPlay = shouldBePlaying.load();
    const bool isPaused = pausedByTransport.load();

    if (! shouldPlay || isPaused)
    {
        info.clearActiveBufferRegion();
        hasLastBlock = false;
        return;
    }

    const auto length = transport.getLengthInSeconds();

    // ---------- Follow transport mode ----------
    if (followMode.load())
    {
        const double target = followTargetSeconds.load();

        if (target < 0.0 || length <= 0.0 || target >= length)
        {
            info.clearActiveBufferRegion();
            hasLastBlock = false;
            return;
        }

        const double current = transport.getCurrentPosition();
        const double delta = target - current;

        constexpr double seekThresholdSeconds = 0.05;

        if (std::abs (delta) > seekThresholdSeconds)
            transport.setPosition (target);

        ensureTransportRunning();
        transport.getNextAudioBlock (info);
        handleTransportEnd();
    }
    else if (! loopEnabled.load())
    {
        // ---------- Free-running mode ----------
        ensureTransportRunning();
        transport.getNextAudioBlock (info);
        handleTransportEnd();
    }
    else
    {
        // ---------- Loop mode ----------
        if (length <= 0.0)
        {
            info.clearActiveBufferRegion();
            hasLastBlock = false;
            return;
        }

        const auto loopStart = juce::jlimit (
            0.0,
            length,
            (double) loopStartNormalized.load() * length);

        const auto loopEnd = juce::jlimit (
            loopStart,
            length,
            (double) loopEndNormalized.load() * length);

        if (loopEnd <= loopStart + 0.000001)
        {
            ensureTransportRunning();
            transport.getNextAudioBlock (info);
            handleTransportEnd();
        }
        else
        {
            ensureTransportRunning();

            auto samplesRemaining = numSamples;
            auto bufferOffset = 0;

            while (samplesRemaining > 0)
            {
                auto currentPosition = transport.getCurrentPosition();

                if (currentPosition < loopStart || currentPosition >= loopEnd)
                {
                    transport.setPosition (loopStart);
                    currentPosition = loopStart;
                }

                const auto secondsUntilLoopEnd = loopEnd - currentPosition;
                auto samplesUntilLoopEnd = juce::jmax (
                    1,
                    (int) std::ceil (secondsUntilLoopEnd * currentSampleRate));

                const auto samplesToProcess = juce::jmin (samplesRemaining,
                                                          samplesUntilLoopEnd);

                juce::AudioSourceChannelInfo chunk (
                    info.buffer,
                    info.startSample + bufferOffset,
                    samplesToProcess);

                transport.getNextAudioBlock (chunk);

                bufferOffset += samplesToProcess;
                samplesRemaining -= samplesToProcess;

                if (samplesToProcess >= samplesUntilLoopEnd)
                    transport.setPosition (loopStart);
            }
        }
    }

    // ---------- Save this block for potential replay on pause ----------
    if (lastBlockBuffer.getNumSamples() >= numSamples)
    {
        const int outChans = info.buffer->getNumChannels();
        const int srcChans = lastBlockBuffer.getNumChannels();
        const int chans = juce::jmin (outChans, srcChans);

        for (int ch = 0; ch < chans; ++ch)
            lastBlockBuffer.copyFrom (ch, 0,
                                      *info.buffer, ch, info.startSample,
                                      numSamples);

        for (int ch = chans; ch < srcChans; ++ch)
            lastBlockBuffer.clear (ch, 0, numSamples);

        hasLastBlock = true;
    }
}

//==============================================================================

void ReferenceSlot::setPlaying (bool shouldPlay)
{
    shouldBePlaying.store (shouldPlay);

    if (! shouldPlay)
    {
        pausedByTransport.store (false);
        pausedPosition.store (0.0);
    }
}

bool ReferenceSlot::isPlaying() const
{
    return shouldBePlaying.load() && ! pausedByTransport.load();
}

//==============================================================================

void ReferenceSlot::setNormalizedPosition (float zeroToOne)
{
    const juce::SpinLock::ScopedLockType lock (callbackLock);

    const auto length = transport.getLengthInSeconds();

    if (length <= 0.0)
        return;

    transport.setPosition (
        juce::jlimit (0.0, 1.0, (double) zeroToOne) * length);

    // If the transport had stopped at the end of the file, setting the
    // position does not restart it. Do so explicitly if we are supposed
    // to be playing.
    ensureTransportRunning();
}

float ReferenceSlot::getNormalizedPosition() const
{
    const auto length = transport.getLengthInSeconds();

    if (length <= 0.0)
        return 0.0f;

    return (float) juce::jlimit (
        0.0,
        1.0,
        transport.getCurrentPosition() / length);
}

//==============================================================================

void ReferenceSlot::setLoopEnabled (bool shouldLoop)
{
    loopEnabled.store (shouldLoop);

    // If enabling loop and the transport was stopped at the end, rewind
    // to the loop start (or the file start) and restart.
    if (shouldLoop)
    {
        const juce::SpinLock::ScopedLockType lock (callbackLock);

        const auto length = transport.getLengthInSeconds();

        if (length > 0.0)
        {
            const auto loopStart = juce::jlimit (
                0.0,
                length,
                (double) loopStartNormalized.load() * length);

            const auto current = transport.getCurrentPosition();

            if (current < loopStart || current >= length - 0.05)
                transport.setPosition (loopStart);
        }

        ensureTransportRunning();
    }

    sendChangeMessage();
}

bool ReferenceSlot::isLoopEnabled() const
{
    return loopEnabled.load();
}

void ReferenceSlot::setLoopRangeNormalized (float start,
                                             float end)
{
    start = juce::jlimit (0.0f, 1.0f, start);
    end   = juce::jlimit (0.0f, 1.0f, end);

    if (start > end)
        std::swap (start, end);

    if (end - start < 0.001f)
        return;

    loopStartNormalized.store (start);
    loopEndNormalized.store (end);

    if (loopEnabled.load())
    {
        const juce::SpinLock::ScopedLockType lock (callbackLock);

        const auto current = getNormalizedPosition();

        if (current < start || current >= end)
        {
            const auto length = transport.getLengthInSeconds();

            if (length > 0.0)
                transport.setPosition ((double) start * length);

            ensureTransportRunning();
        }
    }

    sendChangeMessage();
}

float ReferenceSlot::getLoopStartNormalized() const
{
    return loopStartNormalized.load();
}

float ReferenceSlot::getLoopEndNormalized() const
{
    return loopEndNormalized.load();
}

//==============================================================================

juce::File ReferenceSlot::getFile() const
{
    return currentFile;
}

juce::File ReferenceSlot::getMissingFilePath() const   // <-- ADDED
{
    return missingFilePath;
}

juce::String ReferenceSlot::getStatusText() const
{
    return statusText;
}

double ReferenceSlot::getLengthSeconds() const
{
    return transport.getLengthInSeconds();
}

//==============================================================================

void ReferenceSlot::transportPaused()
{
    const auto pos = transport.getCurrentPosition();
    pausedPosition.store (pos);
    pausedByTransport.store (true);
    shouldBePlaying.store (false);
}

void ReferenceSlot::transportResumed()
{
    const double pos = pausedPosition.load();

    pendingPauseFade.store (false);
    pauseFadeLevel.store (1.0f);

    pausedByTransport.store (false);
    shouldBePlaying.store (true);

    if (pos > 0.0)
    {
        transport.setPosition (pos);
        pausedPosition.store (0.0);
    }

    ensureTransportRunning();
}

void ReferenceSlot::syncTransport (bool isPlaying)
{
    if (isPlaying)
        transportResumed();
    else
        transportPaused();
}