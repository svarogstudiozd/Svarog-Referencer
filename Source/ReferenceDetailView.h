#pragma once

#include "ReferenceSlot.h"
#include "WaveformView.h"
#include "AudioFileSupport.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <memory>
#include <functional>

class ReferenceDetailView : public juce::Component,
                            public juce::FileDragAndDropTarget,
                            public juce::ChangeListener,
                            private juce::Timer
{
public:
    ReferenceDetailView (ReferenceMaxAudioProcessor& processor);
    ~ReferenceDetailView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void setSlot (int logicalIndex);
    int  getSlot() const noexcept { return currentLogicalIndex; }

    void openChooserForCurrentSlot();

    void refresh();

private:
    // A TextButton that also fires a callback on double-click. Used by the
    // Match and Offset buttons, which have single-click and double-click
    // actions with different meanings.
    class DoubleClickableButton : public juce::TextButton
    {
    public:
        using juce::TextButton::TextButton;

        std::function<void()> onDoubleClick;

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            juce::ignoreUnused (e);

            if (onDoubleClick != nullptr)
                onDoubleClick();
        }
    };

    void timerCallback() override;
    void updateTimeDisplay();
    void updateFollowButton();
    void updateLoopButtonEnabledState (bool followIsOn);
    void updateReplaceButtonLabel();
    void openFileChooser();
    void attachToSlot();
    void updateGainFromParameter();
    void pushGainToParameter (double newValueDb);
    void toggleFollow();

    void onMatchButtonClicked();
    void startLearning();
    void cancelLearning();
    void finishLearningIfDone();
    void setMatchEnabled (bool shouldMatch);
    void updateMatchButtonText();
    void updateMatchButtonPulse();

    void onOffsetButtonClicked();
    void onOffsetButtonDoubleClicked();
    void updateOffsetButtonState();
    void updateOffsetTooltip();

    bool isNoSlotMode() const;

    ReferenceMaxAudioProcessor& processor;
    int currentLogicalIndex = 0;

    juce::Label titleLabel;
    juce::Label fileLabel;
    juce::TextButton replaceButton { "Replace" };
    juce::TextButton loopButton { "Loop" };
    juce::TextButton followButton { "Follow" };
    DoubleClickableButton matchButton;
    DoubleClickableButton offsetButton { "Offset" };

    juce::Slider gainSlider;
    juce::Label  gainLabel;
    juce::Label  gainValueLabel;

    juce::String statusText;
    juce::Colour statusTextColour { 0xffd8d8d8 };

    juce::RangedAudioParameter* currentGainParam = nullptr;
    juce::RangedAudioParameter* currentFollowParam = nullptr;
    juce::RangedAudioParameter* currentMatchParam = nullptr;
    juce::RangedAudioParameter* currentOffsetParam = nullptr;

    std::unique_ptr<WaveformView> waveform;

    juce::Label playheadTimeLabel;
    juce::Label timeSeparatorLabel;

    juce::Rectangle<int> statusRect;

    bool draggingOver = false;
    std::unique_ptr<juce::FileChooser> chooser;

    bool   isLearning = false;
    double learningEndMs = 0.0;
    double learningPulseStartMs = 0.0;
    double learningAccum = 0.0;
    int    learningSamples = 0;
    bool   learningAutoEngage = true;

    // In-progress loop preview (from WaveformView during a drag).
    // -1 means "not previewing".
    float previewLoopStart = -1.0f;
    float previewLoopEnd   = -1.0f;

    static constexpr double learningDurationMs = 3500.0;
    static constexpr double pulseHz = 1.2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceDetailView)
};