#pragma once

#include "PluginProcessor.h"
#include "ReferenceDetailView.h"
#include "SlotGrid.h"
#include "FilterBar.h"
#include "SpectrumDisplay.h"
#include "LevelMeterComponent.h"
#include "LufsMeterComponent.h"
#include "ModeToggleButton.h"
#include "ReferenceMaxLookAndFeel.h"

class ReferenceMaxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::FileDragAndDropTarget,
                                         public juce::AudioProcessorValueTreeState::Listener,
                                         public juce::ChangeListener
{
public:
    explicit ReferenceMaxAudioProcessorEditor (ReferenceMaxAudioProcessor&);
    ~ReferenceMaxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    void updateModeButtons();
    void setListenMode (int modeIndex);

    static int computeHeightForSlots (int numSlots);
    void updateResizeLimits();
    void resizeToFitSlotsDeferred();

    ReferenceMaxAudioProcessor& processorRef;

    // Custom LookAndFeel applied to the whole editor, so every button
    // (except the mode toggle, which draws itself) inherits it.
    ReferenceMaxLookAndFeel customLookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    ModeToggleButton modeButton;

    SpectrumDisplay spectrumDisplay;
    LevelMeterComponent vuMeter;
    LufsMeterComponent lufsMeter;
    FilterBar filterBar;

    ReferenceDetailView detailView;
    SlotGrid slotGrid;

    juce::TooltipWindow tooltipWindow { this, 600 };

    bool resizePending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceMaxAudioProcessorEditor)
};