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
#include "SessionStrip.h"

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
    static constexpr int headerHeight    = 48;
    static constexpr int gap             = 4;
    static constexpr int spectrumHeight  = 240;
    static constexpr int filterBarHeight = 40;
    static constexpr int detailHeight    = 200;
    static constexpr int outerPadding    = 32;
    static constexpr int meterWidth      = 56;
    static constexpr int meterGap        = 6;
    static constexpr int sessionStripHeight = SessionStrip::preferredHeight;

    void updateModeButtons();
    void setListenMode (int modeIndex);

    static int computeHeightForSlots (int numSlots);
    void updateResizeLimits();
    void resizeToFitSlotsDeferred();

    // Expanded mode: hide filter bar, detail view, and meters; grow the
    // spectrum to fill the freed space. Header, slot grid, and session
    // strip stay visible.
    void setExpanded (bool shouldBeExpanded);
    void applyVisibilityForCurrentMode();

    ReferenceMaxAudioProcessor& processorRef;

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

    SessionStrip sessionStrip;

    juce::TooltipWindow tooltipWindow { this, 600 };

    bool resizePending = false;
    bool expanded      = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceMaxAudioProcessorEditor)
};