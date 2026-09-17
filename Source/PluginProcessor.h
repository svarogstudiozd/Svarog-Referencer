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

    void resetSlotForNewFile (int logicalIndex);

    void syncPlaybackState();
    void syncPlaybackState (bool dawIsPlayingKnown);

    SpectrumAnalyzer& getDawAnalyzer() noexcept { return dawAnalyzer; }
    SpectrumAnalyzer& getRefAnalyzer() noexcept { return refAnalyzer; }
    LevelMeterSource& getOutputMeter() noexcept { return outputMeter; }
    LufsMeter& getLufsMeter() noexcept { return lufsMeter; }
    LufsMeter& getDawLufsMeter() noexcept { return dawLufsMeter; }

    float getMatchTargetLufs() const noexcept;
    void setMatchTargetLufs (float lufs);
    float captureMatchTargetFromDaw();
    void clearMatchTarget();

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

    // ------------------------------------------------------------------
    // Output declick state machine
    //
    // Reasons:
    //   ModeSwitch       - 20 ms out, 20 ms in. Both slow, so a source
    //                      change is masked by an inaudible dip.
    //   TransportToggle  - 20 ms out, 20 ms in. Stop is fading into
    //                      silence; play is fading from silence.
    //                      No reason to be fast.
    //   TransportJump    - 5 ms out, 20 ms in. Fast out to reduce the
    //                      click of the new content arriving; slow in
    //                      so the new position fades up gently.
    // ------------------------------------------------------------------
    enum class DeclickReason { None, ModeSwitch, TransportToggle, TransportJump };

    enum class DeclickPhase { idle, fadingOut, fadingIn };

    DeclickPhase declickPhase  = DeclickPhase::idle;
    DeclickReason declickReason = DeclickReason::None;
    float        outFade       = 1.0f;

    // Per-sample ramp steps, set in prepareToPlay.
    float slowStep = 0.0f;   // 20 ms
    float medStep  = 0.0f;   // 5 ms

    int  pendingModeChange        = -1;
    bool effectiveIsReferenceMode = false;

    bool   lastDawIsPlaying        = true;
    double lastDawPositionSeen     = -1.0;
    int    lastListenModeIndex     = 0;
    int    lastRefSourcePhysical   = -1;

    float refSlotFade     = 1.0f;
    float refSlotFadeStep = 0.0f;

    std::atomic<double> lastKnownDawSeconds { -1.0 };

    static void applyMono (juce::AudioBuffer<float>& buffer, int n) noexcept;
    static void applySwapLR (juce::AudioBuffer<float>& buffer, int n) noexcept;

    void resetSlotOrderToIdentity() noexcept;
    void clampSoloSlotToVisible();

    void startDeclick (DeclickReason reason) noexcept
    {
        if (declickPhase != DeclickPhase::idle)
        {
            // Upgrade priority of an in-flight fade: a jump can
            // override a mode switch or a toggle.
            if (reason == DeclickReason::TransportJump
                && declickReason != DeclickReason::TransportJump
                && declickPhase == DeclickPhase::fadingOut)
            {
                declickReason = DeclickReason::TransportJump;
            }
            return;
        }

        declickReason = reason;
        declickPhase  = DeclickPhase::fadingOut;
        outFade       = 1.0f;
    }

    void requestModeSwitch (int newMode) noexcept
    {
        pendingModeChange = newMode;

        if (declickPhase == DeclickPhase::idle)
            startDeclick (DeclickReason::ModeSwitch);
    }

    void requestTransportFade (bool wasJump) noexcept
    {
        if (declickPhase == DeclickPhase::idle)
            startDeclick (wasJump ? DeclickReason::TransportJump
                                  : DeclickReason::TransportToggle);
    }

    // Pick the per-sample step for the current phase and reason.
    float currentStep() const noexcept
    {
        if (declickPhase == DeclickPhase::fadingIn)
            return slowStep;   // always slow fade-in

        // Fading out:
        switch (declickReason)
        {
            case DeclickReason::ModeSwitch:
            case DeclickReason::TransportToggle:
                return slowStep;
            case DeclickReason::TransportJump:
                return medStep;
            case DeclickReason::None:
            default:
                return slowStep;
        }
    }

    void applyOutputDeclick (juce::AudioBuffer<float>& buffer, int n) noexcept
    {
        if (n <= 0)
            return;

        if (declickPhase == DeclickPhase::idle)
            return;

        const bool isFadingOut = (declickPhase == DeclickPhase::fadingOut);
        const int  chans = buffer.getNumChannels();
        const float step = currentStep();

        float env = outFade;

        for (int s = 0; s < n; ++s)
        {
            for (int ch = 0; ch < chans; ++ch)
                buffer.getWritePointer (ch)[s] *= env;

            if (isFadingOut)
                env = juce::jmax (0.0f, env - step);
            else
                env = juce::jmin (1.0f, env + step);
        }

        outFade = env;

        if (isFadingOut && outFade <= 1.0e-6f)
        {
            outFade = 0.0f;

            if (pendingModeChange >= 0)
            {
                effectiveIsReferenceMode = (pendingModeChange == 1);
                pendingModeChange = -1;
            }

            declickPhase = DeclickPhase::fadingIn;
        }
        else if (! isFadingOut && outFade >= 1.0f - 1.0e-6f)
        {
            outFade       = 1.0f;
            declickPhase  = DeclickPhase::idle;
            declickReason = DeclickReason::None;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceMaxAudioProcessor)
};