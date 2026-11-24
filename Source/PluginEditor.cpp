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

    addAndMakeVisible (contentHolder);

    titleLabel.setText ("Svarog Referencer +", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (22.0f)));
    contentHolder.addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Mixing and Mastering Reference Check", juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (15.0f)));
    contentHolder.addAndMakeVisible (subtitleLabel);

    modeButton.setLabels ("DAW", "Reference");
    modeButton.onSelectionChanged = [this] (int index)
    {
        setListenMode (index);
    };
    contentHolder.addAndMakeVisible (modeButton);

    spectrumDisplay.setAnalyzers (&processorRef.getDawAnalyzer(),
                                  &processorRef.getRefAnalyzer());
    spectrumDisplay.setApvts (&processorRef.apvts);
    spectrumDisplay.onExpandToggled = [this] (bool shouldExpand)
    {
        setExpanded (shouldExpand);
    };
    contentHolder.addAndMakeVisible (spectrumDisplay);

    vuMeter.setSource (&processorRef.getOutputMeter());
    contentHolder.addAndMakeVisible (vuMeter);

    lufsMeter.setSource (&processorRef.getLufsMeter());
    contentHolder.addAndMakeVisible (lufsMeter);

    contentHolder.addAndMakeVisible (filterBar);
    contentHolder.addAndMakeVisible (detailView);
    contentHolder.addAndMakeVisible (slotGrid);
    contentHolder.addAndMakeVisible (sessionStrip);

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
    const int defaultH = computeHeightForSlots (initialSlots);

    setResizable (true, true);
    updateResizeLimits();

    const int savedW = processorRef.getSavedEditorWidth();
    const int savedH = processorRef.getSavedEditorHeight();

    const int initW = (savedW > 0) ? juce::jlimit (minWindowWidth(),  maxWidth,  savedW) : baseWidth;
    const int initH = (savedH > 0) ? juce::jlimit (minWindowHeight(), maxHeight, savedH) : defaultH;

    setSize (initW, initH);

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
      + minSpectrumHeight + gap
      + filterBarHeight + gap
      + minDetailHeight + gap
      + outerPadding
      + sessionStripHeight + gap + 3;

    const int gridHeight = SlotGrid::preferredHeightFor (numSlots);

    return fixedChrome + gridHeight;
}

//==============================================================================

float ReferenceMaxAudioProcessorEditor::getUiScale() const
{
    // The "design" (100%) size is baseWidth wide and, in the vertical
    // direction, exactly the natural height for the current slot count.
    const int naturalH = computeHeightForSlots (processorRef.getNumVisibleSlots());

    if (naturalH <= 0 || baseWidth <= 0)
        return 1.0f;

    const float byWidth  = (float) getWidth()  / (float) baseWidth;
    const float byHeight = (float) getHeight() / (float) naturalH;

    // Taking the min() of both axes means the *tighter* axis limits the
    // scale, so the whole UI (fonts, buttons, meters, slot rows) grows when
    // you enlarge BOTH directions and shrinks when you shrink BOTH — same as
    // Svarog Wider. The window's own minimums are design * minUiScale, so
    // within the legal size range the scale never drops below minUiScale.
    return juce::jlimit (minUiScale, maxUiScale, juce::jmin (byWidth, byHeight));
}

int ReferenceMaxAudioProcessorEditor::minWindowWidth() const
{
    return juce::roundToInt ((float) baseWidth * minUiScale);
}

int ReferenceMaxAudioProcessorEditor::minWindowHeight() const
{
    const int naturalH = computeHeightForSlots (processorRef.getNumVisibleSlots());
    return juce::roundToInt ((float) naturalH * minUiScale);
}

void ReferenceMaxAudioProcessorEditor::applyScaleToChildren (float newScale)
{
    // The shared LookAndFeel paints all TextButton captions through this
    // global factor, so updating it scales every button label in one place.
    UIScale::factor = newScale;

    titleLabel.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold())
                                        .withHeight (22.0f * newScale)));
    subtitleLabel.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular())
                                           .withHeight (15.0f * newScale)));

    // Each of these scales its own fonts + internal geometry.
    spectrumDisplay.setScale (newScale);
    vuMeter.setScale (newScale);
    lufsMeter.setScale (newScale);
    filterBar.setScale (newScale);
    detailView.setScale (newScale);
    slotGrid.setScale (newScale);
    sessionStrip.setScale (newScale);
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

        safe->updateResizeLimits();

        // Grow (but never shrink) to the natural height for the current
        // slot count, so the analyser and waveform keep their size.
        const int naturalH = computeHeightForSlots (safe->processorRef.getNumVisibleSlots());
        const int targetHeight = juce::jmax (safe->getHeight(), naturalH);

        if (safe->getHeight() != targetHeight)
            safe->setSize (safe->getWidth(), targetHeight);
        else
            safe->resized();
    });
}

void ReferenceMaxAudioProcessorEditor::resized()
{
    // First pass after construction: restore the saved size (clamped to
    // the current limits so a saved size can never violate them).
    if (! hasRestoredInitialSize)
    {
        hasRestoredInitialSize = true;

        const int savedW = processorRef.getSavedEditorWidth();
        const int savedH = processorRef.getSavedEditorHeight();

        if (savedW > 0 && savedH > 0)
        {
            setSize (juce::jlimit (minWindowWidth(),  maxWidth,  savedW),
                     juce::jlimit (minWindowHeight(), maxHeight, savedH));
            return;
        }
    }

    // Save the size to the processor
    processorRef.setSavedEditorSize (getWidth(), getHeight());

    // Give contentHolder the full window bounds.
    // NOTE: contentHolder's own resized() will call layoutLogicalContent(),
    // so we don't call it here — doing so would double-up the layout.
    contentHolder.setBounds (0, 0, getWidth(), getHeight());
}

void ReferenceMaxAudioProcessorEditor::layoutLogicalContent()
{
    // If contentHolder hasn't been sized yet, bail out.
    // (This happens on the very first frame before resized() runs.)
    if (contentHolder.getWidth() <= 0 || contentHolder.getHeight() <= 0)
        return;

    // Uniform scale for this frame. Push it into the children (fonts +
    // internal geometry) and the shared LookAndFeel before laying anything
    // out, so every part of the UI grows together.
    const float s = getUiScale();
    applyScaleToChildren (s);

    auto bounds = contentHolder.getLocalBounds();

    // Outer padding — scales with the UI so the margins stay proportional.
    bounds = bounds.reduced (juce::roundToInt ((float) outerPadding * s) / 2);

    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
        return;

    const int gapScaled = juce::roundToInt ((float) gap * s);

    // --- 1. Header (fixed height, scaled) ---
    auto header = bounds.removeFromTop (juce::roundToInt ((float) headerHeight * s));
    auto modes  = header.removeFromRight (juce::roundToInt (260.0f * s));

    titleLabel.setBounds (header.removeFromTop (juce::roundToInt (26.0f * s)));
    subtitleLabel.setBounds (header);
    modeButton.setBounds (modes.reduced (juce::roundToInt (4.0f * s),
                                         juce::roundToInt (6.0f * s)));

    bounds.removeFromTop (gapScaled);

    // --- 2. Bottom fixed elements: session strip + slot grid ---
    auto stripArea = bounds.removeFromBottom (juce::roundToInt ((float) sessionStripHeight * s));
    sessionStrip.setBounds (stripArea);
    bounds.removeFromBottom (gapScaled);

    const int numSlots   = processorRef.getNumVisibleSlots();
    const int gridHeight = SlotGrid::preferredHeightFor (numSlots, s);
    auto gridArea = bounds.removeFromBottom (gridHeight);
    slotGrid.setBounds (gridArea);
    bounds.removeFromBottom (gapScaled);

    // --- 3. Middle sections ---
    if (expanded)
    {
        spectrumDisplay.setBounds (bounds);
    }
    else
    {
        // Analyser on top, filter bar below it, waveform at the bottom.
        const int filterHeight = juce::roundToInt ((float) filterBarHeight * s);

        const int flexHeight = juce::jmax (0, bounds.getHeight() - filterHeight
                                              - gapScaled - gapScaled);

        const int minSpec = juce::roundToInt ((float) minSpectrumHeight * s);
        const int minDet  = juce::roundToInt ((float) minDetailHeight    * s);

        // Guarantee each section its minimum height, then split any extra
        // space 55/45 in favour of the analyser.
        const int surplus      = juce::jmax (0, flexHeight - minSpec - minDet);
        const int spectrumTake = juce::jlimit (0, flexHeight,
                                               minSpec + juce::roundToInt ((float) surplus * 0.55f));

        // Analyser row, with the two meters docked on the right.
        auto spectrumRow = bounds.removeFromTop (spectrumTake);

        const int meterWidthScaled = juce::roundToInt ((float) meterWidth * s);
        const int meterGapScaled   = juce::roundToInt ((float) meterGap   * s);

        auto vuArea = spectrumRow.removeFromRight (meterWidthScaled);
        spectrumRow.removeFromRight (meterGapScaled);
        auto lufsArea = spectrumRow.removeFromRight (meterWidthScaled);
        spectrumRow.removeFromRight (meterGapScaled);

        spectrumDisplay.setBounds (spectrumRow.reduced (juce::roundToInt (1.0f * s), 0));
        lufsMeter.setBounds (lufsArea);
        vuMeter.setBounds (vuArea);

        bounds.removeFromTop (gapScaled);

        // Filter bar.
        auto filterArea = bounds.removeFromTop (filterHeight);
        filterBar.setBounds (filterArea);
        bounds.removeFromTop (gapScaled);

        // Waveform / detail view takes the remaining space.
        detailView.setBounds (bounds);
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

//==============================================================================

void ReferenceMaxAudioProcessorEditor::setExpanded (bool shouldExpand)
{
    if (expanded == shouldExpand)
        return;

    expanded = shouldExpand;

    updateResizeLimits();
    applyVisibilityForCurrentMode();

    // layoutLogicalContent() only sets bounds; the contentHolder's
    // transform/bounds are re-established by resized(). Calling it
    // directly gives a synchronous relayout before the async resize.
    layoutLogicalContent();
    resized();
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::updateResizeLimits()
{
    // The minimum size is the design size scaled down by minUiScale, so the
    // user can shrink the window and watch the whole UI shrink with it (not
    // just the analyser). Width and height remain independent (free resize).
    setResizeLimits (minWindowWidth(), minWindowHeight(), maxWidth, maxHeight);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (0.0);
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::applyVisibilityForCurrentMode()
{
    // In expanded (spectrum) mode the meters and detail controls are
    // hidden so the spectrum fills the available space. In normal mode
    // everything is visible. Keep this in sync with layoutLogicalContent().
    lufsMeter.setVisible (! expanded);
    vuMeter.setVisible (! expanded);

    filterBar.setVisible (! expanded);
    detailView.setVisible (! expanded);
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::paintLogicalContent (juce::Graphics& g)
{
    // Paints into the *logical* (unscaled) coordinate space of
    // contentHolder. Anything drawn here sits behind the child components,
    // which is where the shared depth model expects shadows to live: the
    // panels and tiles draw only their own fill, and the parent draws their
    // shadows (see ReferenceMaxLookAndFeel::drawPanel).
    g.fillAll (Theme::background);

    const auto& lf = customLookAndFeel;

    // Raised panels (filter bar, detail view) leave their shadow to the
    // parent so it can spill outside their bounds. Skip them when hidden
    // (expanded/spectrum mode) so no phantom shadow is left behind.
    if (filterBar.isVisible())
        lf.drawPanelShadow (g, filterBar.getBounds().toFloat());

    if (detailView.isVisible())
        lf.drawPanelShadow (g, detailView.getBounds().toFloat());

    // Slot tiles get their shadow from the parent as well, so the bottom
    // row's shadow can spill into the gap below the grid instead of being
    // clipped at the grid's edge.
    const auto gridOrigin = slotGrid.getPosition();
    slotGrid.forEachVisibleTileBounds ([&] (juce::Rectangle<float> tileBounds)
    {
        lf.drawTileShadow (g, tileBounds.translated ((float) gridOrigin.x,
                                                     (float) gridOrigin.y));
    });

    // Optional: draw the editor background gradient referenced by the
    // build warning in PluginEditor.h (the ColourGradient with
    // area.getX()/getBottom()). If that gradient lives in this method,
    // cast the int coordinates to float to silence the warning:
    //
    //   juce::ColourGradient textGrad (
    //       juce::Colours::white,
    //       (float) area.getX(), (float) area.getY(),
    //       juce::Colour (0xffa0a0a0),
    //       (float) area.getX(), (float) area.getBottom(),
    //       false);
    //   g.setGradientFill (textGrad);
    //   ... draw whatever the gradient was for ...
}

//==============================================================================

void ReferenceMaxAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill the background color
    g.fillAll (Theme::background); 
}