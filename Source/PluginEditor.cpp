#include "PluginEditor.h"
#include "Theme.h"

ReferenceMaxAudioProcessorEditor::ReferenceMaxAudioProcessorEditor (ReferenceMaxAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),


      
      filterBar (p.apvts),
      detailView (p),
      slotGrid (p),
      sessionStrip (p.getSessionTimer(), p.apvts)
{

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

    titleLabel.setText ("Svarog Referencer +", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (22.0f)));

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
    spectrumDisplay.onExpandToggled = [this] (bool shouldExpand)
    {
        setExpanded (shouldExpand);
    };
    addAndMakeVisible (spectrumDisplay);

    vuMeter.setSource (&processorRef.getOutputMeter());
    addAndMakeVisible (vuMeter);

    lufsMeter.setSource (&processorRef.getLufsMeter());
    addAndMakeVisible (lufsMeter);

    addAndMakeVisible (filterBar);

    addAndMakeVisible (detailView);
    addAndMakeVisible (slotGrid);
    addAndMakeVisible (sessionStrip);



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

    applyVisibilityForCurrentMode();
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
      + outerPadding
      + sessionStripHeight + gap + 3;

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
// Expanded mode

void ReferenceMaxAudioProcessorEditor::setExpanded (bool shouldBeExpanded)
{
    expanded = shouldBeExpanded;

    // Keep the button state in sync in case this was triggered by
    // anything other than the button itself.
    spectrumDisplay.setExpanded (expanded);

    applyVisibilityForCurrentMode();
    resized();
    repaint();
}

void ReferenceMaxAudioProcessorEditor::applyVisibilityForCurrentMode()
{
    // Expanded: hide the chrome between the header and the slot grid.
    // Header, spectrum, slot grid, and session strip stay visible.
    const bool showChrome = ! expanded;

    titleLabel.setVisible (true);
    subtitleLabel.setVisible (true);
    modeButton.setVisible (true);

    vuMeter.setVisible (showChrome);
    lufsMeter.setVisible (showChrome);
    filterBar.setVisible (showChrome);
    detailView.setVisible (showChrome);

    spectrumDisplay.setVisible (true);
    slotGrid.setVisible (true);
    sessionStrip.setVisible (true);
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    // Draw shadows for the main panels. The panels themselves cannot
    // draw outside their own bounds — JUCE clips them — so we draw the
    // shadows here, in the parent, where there's room.
    if (auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel()))
    {
        if (filterBar.isVisible())
            lf->drawPanelShadow (g, filterBar.getBounds().toFloat());

        if (detailView.isVisible())
            lf->drawPanelShadow (g, detailView.getBounds().toFloat());

               // Regular shadow under the whole toggle.
        lf->drawControlShadow (g, modeButton.getBounds().toFloat());

        // Red glow around the active half only.
        {
            const auto toggleBounds = modeButton.getBounds().toFloat();
            const int midX = modeButton.getLocalBounds().getCentreX();

            // Compute the active half's bounds in editor coordinates.
            juce::Rectangle<float> activeHalf;

            if (modeButton.getActiveIndex() == 0)
            {
                // DAW is active: left half.
                activeHalf = toggleBounds.withRight (toggleBounds.getX()
                                                    + (float) midX);
            }
            else
            {
                // Reference is active: right half.
                activeHalf = toggleBounds.withLeft (toggleBounds.getX()
                                                   + (float) midX);
            }

            lf->drawControlGlow (g, activeHalf);
        }

        // Shadows for the slot tiles. Drawn from the editor (which has
        // more room) instead of the grid (which would clip them at its
        // own bounds).
        const auto gridOffset = slotGrid.getPosition();

        slotGrid.forEachVisibleTileBounds ([&] (juce::Rectangle<float> tileBoundsInGrid)
        {
            tileBoundsInGrid.translate ((float) gridOffset.x, (float) gridOffset.y);
            lf->drawTileShadow (g, tileBoundsInGrid);
        });
    }

    
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

    // --- Bottom rows: always the same in both modes ---
    //
    // We take the slot grid and session strip off the bottom first, so
    // the expanded spectrum gets exactly the space that's left.

    auto stripArea = bounds.removeFromBottom (sessionStripHeight);
    bounds.removeFromBottom (gap + 3);   // <-- extra 5 px above the strip
    sessionStrip.setBounds (stripArea);

    const int numSlots = processorRef.getNumVisibleSlots();
    const int gridHeight = SlotGrid::preferredHeightFor (numSlots);

    auto gridArea = bounds.removeFromBottom (gridHeight);
    bounds.removeFromBottom (gap);
    slotGrid.setBounds (gridArea);

    // --- Top area: depends on expanded state ---

    if (expanded)
    {
        // Spectrum fills everything above the slot grid.
        spectrumDisplay.setBounds (bounds);
    }
    else
    {
        auto topRow = bounds.removeFromTop (spectrumHeight);

        auto vuArea = topRow.removeFromRight (meterWidth);
        topRow.removeFromRight (meterGap);

        auto lufsArea = topRow.removeFromRight (meterWidth);
        topRow.removeFromRight (meterGap);

         spectrumDisplay.setBounds (topRow.reduced (1, 0));
        lufsMeter.setBounds (lufsArea);
        vuMeter.setBounds (vuArea);

        bounds.removeFromTop (gap);

        filterBar.setBounds (bounds.removeFromTop (filterBarHeight));
        bounds.removeFromTop (gap);

        detailView.setBounds (bounds.removeFromTop (detailHeight));
    }
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

    repaint();
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