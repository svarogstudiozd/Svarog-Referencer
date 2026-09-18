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
// Offline LUFS measurement job.
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
        statusText = "Empty";

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
    if (! file.existsAsFile())
        return;

    const auto token = ++loadToken;

    loading.store (true);
    loaded.store (false);
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

void ReferenceSlot::getNextAudioBlock (
    const juce::AudioSourceChannelInfo& info)
{
    if (! prepared || ! loaded.load() || readerSource == nullptr)
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

    const auto numSamples = info.numSamples;

    if (numSamples <= 0)
        return;

    const bool shouldPlay = shouldBePlaying.load();
    const bool isPaused = pausedByTransport.load();

    if (! shouldPlay || isPaused)
    {
        info.clearActiveBufferRegion();
        return;
    }

    const auto length = transport.getLengthInSeconds();

    // ---------- Follow transport mode ----------
    if (followMode.load())
    {
        const double target = followTargetSeconds.load();

        if (target < 0.0 || length <= 0.0)
        {
            info.clearActiveBufferRegion();
            return;
        }

        if (target >= length)
        {
            info.clearActiveBufferRegion();
            return;
        }

        const double current = transport.getCurrentPosition();
        const double delta = target - current;

        constexpr double seekThresholdSeconds = 0.05;

        if (std::abs (delta) > seekThresholdSeconds)
            transport.setPosition (target);

        transport.getNextAudioBlock (info);
        return;
    }

    // ---------- Free-running mode ----------
    if (! loopEnabled.load())
    {
        transport.getNextAudioBlock (info);
        return;
    }

    if (length <= 0.0)
    {
        info.clearActiveBufferRegion();
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
        transport.getNextAudioBlock (info);
        return;
    }

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

        const auto secondsUntilLoopEnd =
            loopEnd - currentPosition;

        auto samplesUntilLoopEnd =
            juce::jmax (
                1,
                (int) std::ceil (
                    secondsUntilLoopEnd * currentSampleRate));

        const auto samplesToProcess =
            juce::jmin (samplesRemaining,
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
        const auto current = getNormalizedPosition();

        if (current < start || current >= end)
        {
            const juce::SpinLock::ScopedLockType lock (callbackLock);

            const auto length = transport.getLengthInSeconds();

            if (length > 0.0)
                transport.setPosition ((double) start * length);
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
    pausedByTransport.store (false);
    shouldBePlaying.store (true);

    if (pos > 0.0)
    {
        transport.setPosition (pos);
        pausedPosition.store (0.0);
    }
}

void ReferenceSlot::syncTransport (bool isPlaying)
{
    if (isPlaying)
        transportResumed();
    else
        transportPaused();
}