#pragma once

#include "AudioFileSupport.h"
#include "ReferenceSlot.h"
#include "FilterProcessor.h"
#include "SpectrumAnalyzer.h"
#include "LevelMeterSource.h"
#include "LufsMeter.h"
#include <array>
#include <atomic>
#include <memory>

class ReferenceMaxAudioProcessor : public juce::AudioProcessor,
                                   public juce::AudioProcessorValueTreeState::Listener,
                                   public juce::ChangeListener
{
public:
    ReferenceMaxAudioProcessor();
    ~ReferenceMaxAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    ReferenceSlot& getSlot (int logicalIndex);
    const ReferenceSlot& getSlot (int logicalIndex) const;

    int physicalIndexFor (int logicalIndex) const noexcept;
    int logicalIndexFor (int physicalIndex) const noexcept;
    int paramNumberFor (int logicalIndex) const noexcept;

    bool isListeningToReference() const;
    int getSoloSlot() const;
    int getNumVisibleSlots() const;

    void addSlot();
    void removeSlot (int logicalIndex);

    // Clear a slot and reset all of its per-reference parameters (gain,
    // follow, offset, match). Does NOT touch the global match target.
    // Call this before loadFileAsync() when the user is replacing a
    // reference with a new file (Replace button or file drop). Do NOT
    // call it during session restore.
    void resetSlotForNewFile (int logicalIndex);

    void syncPlaybackState();
    void syncPlaybackState (bool dawIsPlayingKnown);

    SpectrumAnalyzer& getDawAnalyzer() noexcept { return dawAnalyzer; }
    SpectrumAnalyzer& getRefAnalyzer() noexcept { return refAnalyzer; }
    LevelMeterSource& getOutputMeter() noexcept { return outputMeter; }
    LufsMeter& getLufsMeter() noexcept { return lufsMeter; }
    LufsMeter& getDawLufsMeter() noexcept { return dawLufsMeter; }

    // ---- Match target ----------------------------------------------------

    float getMatchTargetLufs() const noexcept;
    void setMatchTargetLufs (float lufs);
    float captureMatchTargetFromDaw();
    void clearMatchTarget();

    // ---- DAW transport position cache -----------------------------------

    double getLastKnownDawSeconds() const noexcept
    {
        return lastKnownDawSeconds.load();
    }

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int maxReferenceSlots = ReferenceSlot::maxSlots;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 32 };
    juce::TimeSliceThread readAheadThread { "ReferenceMax read-ahead" };
    juce::ThreadPool loadPool { 1 };
    std::array<std::unique_ptr<ReferenceSlot>, maxReferenceSlots> slots;

    // Maps logical slot position (what the user sees) to physical slot
    // index (position in `slots`). Written on the message thread only;
    // read on the audio thread via physicalIndexFor(). Atomic so a
    // concurrent read/write can't tear.
    std::array<std::atomic<int>, maxReferenceSlots> slotOrder;

    std::atomic<float>* listenMode = nullptr;
    std::atomic<float>* soloSlot = nullptr;
    std::atomic<float>* numVisibleSlots = nullptr;
    std::array<std::atomic<float>*, maxReferenceSlots> gainParams {};
    std::array<std::atomic<float>*, maxReferenceSlots> followParams {};
    std::array<std::atomic<float>*, maxReferenceSlots> offsetParams {};
    std::array<std::atomic<float>*, maxReferenceSlots> matchParams {};

    std::atomic<float>* matchTargetParam = nullptr;

    std::atomic<float>* filterSolo   = nullptr;
    std::atomic<float>* filterLowXover  = nullptr;
    std::atomic<float>* filterHighXover = nullptr;

    std::atomic<float>* monoParam   = nullptr;
    std::atomic<float>* swapLRParam = nullptr;

    juce::LinearSmoothedValue<float> gainSmoothed;

    FilterProcessor dawFilter;
    FilterProcessor refFilter;

    SpectrumAnalyzer dawAnalyzer;
    SpectrumAnalyzer refAnalyzer;

    LevelMeterSource outputMeter;
    LufsMeter lufsMeter;
    LufsMeter dawLufsMeter;

    juce::AudioBuffer<float> refAnalysisBuffer;
     // Short fade applied to the reference signal whenever the soloed
    // slot changes, to avoid a click at the source-switch boundary.
    float sessionFade = 1.0f;         // current envelope value, 0..1
    float sessionFadeStep = 0.0f;     // per-sample increment (set in prepareToPlay)
    int   lastSoloedPhysical = -1;    // which physical slot we were playing

    bool lastDawIsPlaying = true;

    std::atomic<double> lastKnownDawSeconds { -1.0 };

    static void applyMono (juce::AudioBuffer<float>& buffer, int n) noexcept;
    static void applySwapLR (juce::AudioBuffer<float>& buffer, int n) noexcept;

    void resetSlotOrderToIdentity() noexcept;
    void clampSoloSlotToVisible();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceMaxAudioProcessor)
};