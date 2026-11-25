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

void ReferenceMaxAudioProcessorEditor::updateUiScale()
{
    // The "design" (100%) size is baseWidth wide and, in the vertical
    // direction, exactly the natural height for the current slot count.
    const int naturalH = computeHeightForSlots (processorRef.getNumVisibleSlots());

    const int w = contentHolder.getWidth();
    const int h = contentHolder.getHeight();

    if (naturalH <= 0 || baseWidth <= 0 || w <= 0 || h <= 0)
        return;

    // The largest scale whose design footprint still fits the window: at
    // that scale the fixed chrome (header, filter bar, meters, slot rows,
    // session strip) exactly fills one axis and the analyser / waveform keep
    // the rest. It is also the ceiling for any growth below, so the chrome
    // can never be squeezed out of the window.
    const float fit = juce::jlimit (minUiScale, maxUiScale,
                                    juce::jmin ((float) w / (float) baseWidth,
                                                (float) h / (float) naturalH));

    if (uiScaleWidth <= 0 || uiScaleHeight <= 0)
    {
        // First layout: nothing to compare against, so use the fit.
        uiScale = fit;
    }
    else
    {
        const float growW = (float) w / (float) uiScaleWidth;
        const float growH = (float) h / (float) uiScaleHeight;

        if (growW > 1.0f && growH > 1.0f)
        {
            // Enlarged in BOTH directions: zoom the whole UI, but only by the
            // smaller of the two relative growths. Growing the chrome by the
            // full width ratio would eat the analyser's / waveform's share of
            // a window that did not get any taller.
            uiScale = juce::jmin (fit, uiScale * juce::jmin (growW, growH));
        }
        else
        {
            // Stretched on a single axis (the usual case: dragging the window
            // wider) keeps the chrome exactly as it is, so the extra space
            // goes to the analyser and waveform — otherwise the fixed chrome
            // grows into their share, they visibly shrink while everything
            // else grows, and the UI only "snaps" once the aspect ratio has
            // caught up. Shrinking in either direction still follows the fit,
            // so the chrome always comes back down with the window.
            uiScale = juce::jmin (uiScale, fit);
        }
    }

    uiScale = juce::jlimit (minUiScale, maxUiScale, uiScale);

    uiScaleWidth  = w;
    uiScaleHeight = h;
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

        // The slot grid is granted a fixed height in layoutLogicalContent()
        // and the waveform absorbs the rest, so the window only needs to
        // change by the grid's own height delta: removing the last slot of
        // the bottom row shrinks the window and closes the gap, adding a
        // slot grows it. Any extra height the user has dragged in is left
        // untouched, so a manually enlarged window keeps its size.
        const float s = safe->getUiScale();

        const int currentCount  = safe->processorRef.getNumVisibleSlots();
        const int previousCount = safe->lastLaidOutSlotCount >= 0
                                      ? safe->lastLaidOutSlotCount
                                      : currentCount;

        const int newGridHeight = SlotGrid::preferredHeightFor (currentCount,  s);
        const int oldGridHeight = SlotGrid::preferredHeightFor (previousCount, s);

        const int targetHeight = juce::jlimit (safe->minWindowHeight(),
                                               safe->maxHeight,
                                               safe->getHeight()
                                                   + newGridHeight - oldGridHeight);

        // Relayout even when the height is unchanged (e.g. 2 slots -> 1
        // slot keeps a single row, or the window is already clamped to its
        // minimum): the grid's granted bounds must always be re-derived from
        // the new slot count, otherwise the deleted row leaves a gap.
        if (safe->getHeight() != targetHeight)
        {
            safe->setSize (safe->getWidth(), targetHeight);
        }
        else
        {
            // setBounds() on an unchanged contentHolder is a no-op, so the
            // layout has to be kicked off by hand.
            safe->resized();
            safe->layoutLogicalContent();
        }
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
    // out, so the parts of the chrome that do scale grow together.
    updateUiScale();
    const float s = uiScale;
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

    // Remember the slot count this layout was built for, so a later slot
    // count change can resize the window by the grid's height delta
    // (see resizeToFitSlotsDeferred()).
    lastLaidOutSlotCount = numSlots;

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

        // Split the flexible height between the analyser and the waveform
        // panel. The analyser takes the larger share (see analyserFlexShare),
        // but each side keeps its own minimum, so the split only decides how
        // the *extra* height is divided and neither area can be squeezed away.
        int spectrumTake = juce::roundToInt ((float) flexHeight * analyserFlexShare);
        spectrumTake = juce::jlimit (minSpec,
                                     juce::jmax (minSpec, flexHeight - minDet),
                                     spectrumTake);
        spectrumTake = juce::jlimit (0, flexHeight, spectrumTake);

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

    // The DAW / Reference toggle sits in the header, where no panel paints
    // shadows for its children — so it gets the same treatment here that the
    // panels give their own buttons: a red glow around the engaged half,
    // plus the standard control shadow for the whole control. Both are
    // drawn behind the button (it is a child of this holder), which is what
    // lets them spill past the button's bounds.
    {
        const auto toggleBounds = modeButton.getBounds().toFloat();

        // Match the button's own integer mid-point so the glow lines up
        // exactly with the half it is drawn under.
        const auto midX = toggleBounds.getX()
                        + (float) modeButton.getLocalBounds().getCentreX();

        const auto activeHalf = modeButton.getActiveIndex() == 0
                                    ? toggleBounds.withRight (midX)
                                    : toggleBounds.withLeft (midX);

        lf.drawControlGlow (g, activeHalf);
        lf.drawControlShadow (g, toggleBounds);
    }

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