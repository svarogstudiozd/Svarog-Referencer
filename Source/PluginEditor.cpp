#include "PluginEditor.h"
#include "Theme.h"

ReferenceMaxAudioProcessorEditor::ReferenceMaxAudioProcessorEditor (ReferenceMaxAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      filterBar (p.apvts),
      detailView (p),
      slotGrid (p)
{
    // Apply the custom LookAndFeel to this editor. All child components
    // (buttons, sliders, etc.) that don't have their own LookAndFeel set
    // will inherit this.
    setLookAndFeel (&customLookAndFeel);

    auto& lf = getLookAndFeel();
    lf.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    lf.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    lf.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    lf.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    lf.setColour (juce::Slider::thumbColourId,        Theme::greyLight);
    lf.setColour (juce::Slider::trackColourId,        Theme::accentRedBright);
    lf.setColour (juce::Slider::backgroundColourId,   Theme::inset);
    lf.setColour (juce::Slider::textBoxTextColourId,  Theme::textPrimary);
    lf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    lf.setColour (juce::TooltipWindow::backgroundColourId, Theme::background);
    lf.setColour (juce::TooltipWindow::textColourId,       Theme::textPrimary);
    lf.setColour (juce::TooltipWindow::outlineColourId,    Theme::border);

    titleLabel.setText ("Svarog Referencer", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Theme::greyLight);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Mixing and Mastering Reference Check", juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    addAndMakeVisible (subtitleLabel);

    modeButton.setLabels ("DAW", "Reference");
    modeButton.onSelectionChanged = [this] (int index)
    {
        setListenMode (index);
    };
    addAndMakeVisible (modeButton);

    spectrumDisplay.setAnalyzers (&processorRef.getDawAnalyzer(),
                                  &processorRef.getRefAnalyzer());
    spectrumDisplay.setApvts (&processorRef.apvts);
    addAndMakeVisible (spectrumDisplay);

    vuMeter.setSource (&processorRef.getOutputMeter());
    addAndMakeVisible (vuMeter);

    lufsMeter.setSource (&processorRef.getLufsMeter());
    addAndMakeVisible (lufsMeter);

    addAndMakeVisible (filterBar);

    addAndMakeVisible (detailView);
    addAndMakeVisible (slotGrid);

    slotGrid.onGhostClicked = [this]
    {
        const int current = processorRef.getNumVisibleSlots();
        if (current >= ReferenceMaxAudioProcessor::maxReferenceSlots)
            return;

        processorRef.addSlot();

        const int newLogical = processorRef.getSoloSlot();
        detailView.setSlot (newLogical);
        slotGrid.refresh();

        updateResizeLimits();
        resizeToFitSlotsDeferred();

        detailView.openChooserForCurrentSlot();
    };

    processorRef.apvts.addParameterListener ("listen_mode", this);
    processorRef.apvts.addParameterListener ("solo_slot", this);
    processorRef.apvts.addParameterListener ("num_visible_slots", this);

    for (int i = 0; i < ReferenceMaxAudioProcessor::maxReferenceSlots; ++i)
        processorRef.getSlot (i).addChangeListener (this);

    updateModeButtons();

    const int selected = processorRef.getSoloSlot();
    detailView.setSlot (selected);
    slotGrid.refresh();

    const int initialSlots = processorRef.getNumVisibleSlots();

    setResizable (true, true);
    updateResizeLimits();

    setSize (920, computeHeightForSlots (initialSlots));
}

ReferenceMaxAudioProcessorEditor::~ReferenceMaxAudioProcessorEditor()
{
    setLookAndFeel (nullptr);

    processorRef.apvts.removeParameterListener ("listen_mode", this);
    processorRef.apvts.removeParameterListener ("solo_slot", this);
    processorRef.apvts.removeParameterListener ("num_visible_slots", this);

    for (int i = 0; i < ReferenceMaxAudioProcessor::maxReferenceSlots; ++i)
        processorRef.getSlot (i).removeChangeListener (this);
}

//==============================================================================

int ReferenceMaxAudioProcessorEditor::computeHeightForSlots (int numSlots)
{
    numSlots = juce::jlimit (0, ReferenceMaxAudioProcessor::maxReferenceSlots, numSlots);

    const int fixedChrome =
        headerHeight + gap
      + spectrumHeight + gap
      + filterBarHeight + gap
      + detailHeight + gap
      + outerPadding;

    const int gridHeight = SlotGrid::preferredHeightFor (numSlots);

    return fixedChrome + gridHeight;
}

void ReferenceMaxAudioProcessorEditor::updateResizeLimits()
{
    constexpr int minWidth = 920;
    constexpr int maxWidth = 1400;

    const int visible  = processorRef.getNumVisibleSlots();
    const int lockedH  = computeHeightForSlots (visible);

    setResizeLimits (minWidth, lockedH, maxWidth, lockedH);
}

void ReferenceMaxAudioProcessorEditor::resizeToFitSlotsDeferred()
{
    if (resizePending)
        return;

    resizePending = true;

    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer (this)]
    {
        if (safe == nullptr)
            return;

        safe->resizePending = false;

        const int visible = safe->processorRef.getNumVisibleSlots();
        const int targetHeight = computeHeightForSlots (visible);

        if (safe->getHeight() != targetHeight)
            safe->setSize (safe->getWidth(), targetHeight);
    });
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
}

void ReferenceMaxAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (outerPadding / 2);
    auto header = bounds.removeFromTop (headerHeight);
    auto modes  = header.removeFromRight (260);

    titleLabel.setBounds (header.removeFromTop (26));
    subtitleLabel.setBounds (header);

    modeButton.setBounds (modes.reduced (4, 6));

    bounds.removeFromTop (gap);

    auto topRow = bounds.removeFromTop (spectrumHeight);

    // Two meters on the right of the spectrum. Wider than before so the
    // LUFS mode button (labelled "LUFS-S/M/I") fits comfortably.
    auto vuArea = topRow.removeFromRight (meterWidth);
    topRow.removeFromRight (meterGap);

    auto lufsArea = topRow.removeFromRight (meterWidth);
    topRow.removeFromRight (meterGap);

    spectrumDisplay.setBounds (topRow);
    lufsMeter.setBounds (lufsArea);
    vuMeter.setBounds (vuArea);

    bounds.removeFromTop (gap);

    filterBar.setBounds (bounds.removeFromTop (filterBarHeight));
    bounds.removeFromTop (gap);

    detailView.setBounds (bounds.removeFromTop (detailHeight));
    bounds.removeFromTop (gap);

    slotGrid.setBounds (bounds);
}

//==============================================================================

bool ReferenceMaxAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    return AudioFileSupport::containsSupportedFile (files);
}

void ReferenceMaxAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (processorRef.getNumVisibleSlots() <= 0)
        return;

    const int selected = processorRef.getSoloSlot();

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (AudioFileSupport::isSupportedFile (file))
        {
            processorRef.resetSlotForNewFile (selected);
            processorRef.getSlot (selected).loadFileAsync (file);
            break;
        }
    }
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer (this), parameterID]
    {
        if (safe == nullptr)
            return;

        if (parameterID == "listen_mode")
        {
            safe->updateModeButtons();
        }
        else if (parameterID == "solo_slot")
        {
            safe->detailView.setSlot (safe->processorRef.getSoloSlot());
            safe->slotGrid.refresh();
        }
        else if (parameterID == "num_visible_slots")
        {
            // A slot was added or removed. The solo_slot parameter may
            // or may not have changed value as a result, so we cannot
            // rely on the solo_slot handler running. Re-call setSlot so
            // the detail view notices when the physical slot behind
            // the current logical index has changed, then refresh.
            safe->detailView.setSlot (safe->processorRef.getSoloSlot());
            safe->slotGrid.refresh();
            safe->detailView.refresh();
            safe->updateResizeLimits();
            safe->resizeToFitSlotsDeferred();
        }
    });
}

void ReferenceMaxAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer (this)]
    {
        if (safe == nullptr)
            return;

        safe->slotGrid.refresh();
        safe->detailView.refresh();
    });
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::updateModeButtons()
{
    const auto ref = processorRef.isListeningToReference();
    modeButton.setActiveIndex (ref ? 1 : 0);
}

void ReferenceMaxAudioProcessorEditor::setListenMode (int modeIndex)
{
    if (auto* param = processorRef.apvts.getParameter ("listen_mode"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) modeIndex));
        param->endChangeGesture();
    }
}