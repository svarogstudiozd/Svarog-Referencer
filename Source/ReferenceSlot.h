#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class ReferenceSlot : public juce::ChangeBroadcaster
{
public:
    static constexpr int maxSlots = 8;

    ReferenceSlot (juce::AudioFormatManager& formats,
                   juce::AudioThumbnailCache& thumbnailCache,
                   juce::TimeSliceThread& readAheadThread,
                   juce::ThreadPool& loadPool);

    ~ReferenceSlot() override;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels);
    void releaseResources();

    void loadFileAsync (const juce::File& file);
    void setPendingNormalizedPosition (float zeroToOne);

    void reset();

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info);

    void setPlaying (bool shouldPlay);
    bool isPlaying() const;
    bool isActive() const { return shouldBePlaying.load(); }
    bool isPausedByTransport() const { return pausedByTransport.load(); }

    void setNormalizedPosition (float zeroToOne);
    float getNormalizedPosition() const;

    void transportPaused();
    void transportResumed();
    void syncTransport (bool isPlaying);

    void setLoopEnabled (bool shouldLoop);
    bool isLoopEnabled() const;

    void setLoopRangeNormalized (float start, float end);
    float getLoopStartNormalized() const;
    float getLoopEndNormalized() const;

    void setFollowMode (bool shouldFollow);
    bool isFollowMode() const noexcept { return followMode.load(); }

    void setFollowTargetSeconds (double seconds) noexcept
    {
        followTargetSeconds.store (seconds);
    }

    bool isLoaded() const noexcept { return loaded.load(); }
    bool isLoading() const noexcept { return loading.load(); }

    // Measured integrated loudness of the loaded file, in LUFS.
    // -70 means "not measured / silent". Updated asynchronously after
    // a file is loaded.
    float getIntegratedLufs() const noexcept { return integratedLufs.load(); }

    juce::File getFile() const;
    juce::String getStatusText() const;
    double getLengthSeconds() const;

    juce::AudioThumbnail& getThumbnail() noexcept { return thumbnail; }

private:
    void finishLoad (const juce::File& file,
                     uint32_t token,
                     std::unique_ptr<juce::AudioFormatReader> reader);

    // Detect end-of-file and put the transport back into a usable
    // state so subsequent play attempts are audible.
    void handleTransportEnd();

    // If we are supposed to be playing, ensure the transport is
    // actually running. Safe to call after any position change.
    void ensureTransportRunning();

    friend struct LoadReferenceJob;
    friend struct MeasureLufsJob;

    juce::AudioFormatManager& formatManager;
    juce::TimeSliceThread& readAheadThread;
    juce::ThreadPool& loadPool;

    juce::AudioThumbnail thumbnail;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;

    juce::SpinLock callbackLock;

    juce::File currentFile;
    juce::String statusText { "Empty" };

    std::atomic<bool> loaded { false };
    std::atomic<bool> loading { false };
    std::atomic<uint32_t> loadToken { 0 };
    std::atomic<float> pendingPosition { -1.0f };

    std::atomic<bool> shouldBePlaying { false };
    std::atomic<bool> pausedByTransport { false };
    std::atomic<double> pausedPosition { 0.0 };
    std::atomic<bool> transportRunning { false };

    std::atomic<bool> loopEnabled { false };
    std::atomic<float> loopStartNormalized { 0.0f };
    std::atomic<float> loopEndNormalized { 1.0f };

    std::atomic<bool> followMode { false };
    std::atomic<double> followTargetSeconds { -1.0 };

    std::atomic<float> integratedLufs { -70.0f };

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int currentNumChannels = 2;
    bool prepared = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ReferenceSlot)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceSlot)
};