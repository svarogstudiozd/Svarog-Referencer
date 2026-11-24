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

class EngravedTitleLabel : public juce::Label
{
public:
    EngravedTitleLabel() = default;

    void paint (juce::Graphics& g) override
    {
        const auto text = getText();
        if (text.isEmpty())
            return;

        const auto border = getBorderSize();
        auto area = getLocalBounds();
        area.removeFromTop    (border.getTop());
        area.removeFromLeft   (border.getLeft());
        area.removeFromBottom (border.getBottom());
        area.removeFromRight  (border.getRight());

        juce::Graphics::ScopedSaveState save (g);
        g.setFont (getFont());

    juce::ColourGradient textGrad (
    juce::Colours::white,
    (float) area.getX(), (float) area.getY(),
    juce::Colour (0xffa0a0a0),
    (float) area.getX(), (float) area.getBottom(),
    false);

        g.setGradientFill (textGrad);
        g.drawFittedText (text, area, getJustificationType(), 1);
    }
};



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
    class ScaledContentHolder : public juce::Component
    {
    public:
        explicit ScaledContentHolder (ReferenceMaxAudioProcessorEditor& ownerEditor)
            : editor (ownerEditor) {}

        void paint (juce::Graphics& g) override { editor.paintLogicalContent (g); }
        void resized() override { editor.layoutLogicalContent(); }

    private:
        ReferenceMaxAudioProcessorEditor& editor;
    };

    static constexpr int baseWidth          = 920;
    static constexpr int maxWidth           = 2560;
    static constexpr int maxHeight          = 1600;
    static constexpr float minUiScale       = 0.75f; // shrink floor: window may go below the design size
    static constexpr float maxUiScale       = 2.5f;  // cap so fonts/controls stay sane at huge sizes
    static constexpr int headerHeight       = 48;
    static constexpr int gap                = 4;
    static constexpr int minSpectrumHeight  = 150;  // analyser can shrink this far before the window stops
    static constexpr int filterBarHeight    = 40;
    static constexpr int minDetailHeight    = 140;  // waveform can shrink this far before the window stops
    static constexpr int outerPadding       = 32;
    static constexpr int meterWidth         = 56;
    static constexpr int meterGap           = 6;
    static constexpr int sessionStripHeight = SessionStrip::preferredHeight;

    void updateModeButtons();
    void setListenMode (int modeIndex);

    static int computeHeightForSlots (int numSlots);

    // Uniform UI scale — grows when the window grows and shrinks when it
    // shrinks (in BOTH directions), mirroring Svarog Wider's
    // min(width/base, height/base), clamped to [minUiScale, maxUiScale].
    float getUiScale() const;
    void applyScaleToChildren (float newScale);

    // Smallest window the user can drag to: the design size scaled down by
    // minUiScale, so the whole UI shrinks with the window (not just the analyser).
    int minWindowWidth()  const;
    int minWindowHeight() const;

    void updateResizeLimits();
    void resizeToFitSlotsDeferred();

    void paintLogicalContent (juce::Graphics& g);
    void layoutLogicalContent();

    void setExpanded (bool shouldBeExpanded);
    void applyVisibilityForCurrentMode();

    ReferenceMaxAudioProcessor& processorRef;
    ReferenceMaxLookAndFeel customLookAndFeel;

    ScaledContentHolder contentHolder { *this };

    EngravedTitleLabel titleLabel;
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

    double currentAspectRatio = 1.0; 
    bool hasRestoredInitialSize = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceMaxAudioProcessorEditor)
};