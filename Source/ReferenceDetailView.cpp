#include "ReferenceDetailView.h"
#include "Theme.h"
#include "TimeFormat.h"

ReferenceDetailView::ReferenceDetailView (ReferenceMaxAudioProcessor& p)
    : processor (p)
{
    setInterceptsMouseClicks (true, true);

    titleLabel.setText ("No reference", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Theme::greyLight);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    fileLabel.setText ("", juce::dontSendNotification);
    fileLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    fileLabel.setMinimumHorizontalScale (0.6f);
    fileLabel.setJustificationType (juce::Justification::centredLeft);
    fileLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (fileLabel);

    replaceButton.onClick = [this] { openFileChooser(); };
    replaceButton.setColour (juce::TextButton::buttonColourId, Theme::accentRed);
    replaceButton.setColour (juce::TextButton::textColourOnId, Theme::greyLight);
    replaceButton.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (replaceButton);

    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this]
    {
        processor.getSlot (currentLogicalIndex).setLoopEnabled (loopButton.getToggleState());
    };
    loopButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    loopButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    loopButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    loopButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    loopButton.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (loopButton);

    followButton.setClickingTogglesState (false);
    followButton.onClick = [this] { toggleFollow(); };
    followButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    followButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    followButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    followButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    followButton.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (followButton);

    matchButton.setClickingTogglesState (false);
    matchButton.onClick = [this] { onMatchButtonClicked(); };
    matchButton.onDoubleClick = [this]
    {
        if (! isLearning && ! isNoSlotMode())
        {
            learningAutoEngage = true;
            startLearning();
        }
    };
    matchButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    matchButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    matchButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    matchButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    matchButton.setTooltip (
        "Click to learn the DAW's loudness and match this reference to it.\n"
        "Click again to turn matching off.\n"
        "Double-click to re-learn the target level.");
    matchButton.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (matchButton);

    offsetButton.setClickingTogglesState (false);
    offsetButton.onClick = [this] { onOffsetButtonClicked(); };
    offsetButton.onDoubleClick = [this] { onOffsetButtonDoubleClicked(); };
    offsetButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    offsetButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    offsetButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    offsetButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    offsetButton.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (offsetButton);

    playheadTimeLabel.setText ("--:--", juce::dontSendNotification);
    playheadTimeLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    playheadTimeLabel.setColour (juce::Label::textColourId, Theme::greyLight);
    playheadTimeLabel.setJustificationType (juce::Justification::centredLeft);
    playheadTimeLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (playheadTimeLabel);

    timeSeparatorLabel.setText ("|", juce::dontSendNotification);
    timeSeparatorLabel.setFont (juce::FontOptions (16.0f));
    timeSeparatorLabel.setColour (juce::Label::textColourId, Theme::border);
    timeSeparatorLabel.setJustificationType (juce::Justification::centred);
    timeSeparatorLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (timeSeparatorLabel);

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centredLeft);
    gainLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    gainLabel.setColour (juce::Label::textColourId, Theme::greyLight);
    gainLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (gainLabel);

    gainValueLabel.setText ("+0.0 dB", juce::dontSendNotification);
    gainValueLabel.setJustificationType (juce::Justification::centredRight);
    gainValueLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    gainValueLabel.setColour (juce::Label::textColourId, Theme::greyLight);
    gainValueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (gainValueLabel);

    gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setRange (-24.0, 12.0, 0.1);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setInterceptsMouseClicks (true, true);
    gainSlider.setColour (juce::Slider::thumbColourId,        Theme::greyLight);
    gainSlider.setColour (juce::Slider::trackColourId,        Theme::accentRedBright);
    gainSlider.setColour (juce::Slider::backgroundColourId,   Theme::inset);
    gainSlider.onValueChange = [this]
    {
        pushGainToParameter (gainSlider.getValue());
    };
    addAndMakeVisible (gainSlider);

    statusTextColour = Theme::textPrimary;

    attachToSlot();

    startTimerHz (30);
}

ReferenceDetailView::~ReferenceDetailView()
{
    stopTimer();

    if (waveform != nullptr)
        removeChildComponent (waveform.get());

    // Remove ourselves from every slot's change broadcaster, not just the
    // current one. This is defensive: if we ever switch slots without
    // detaching cleanly (or if the current slot's listener was never
    // removed for any reason), iterating all slots guarantees no dangling
    // listener.
    for (int i = 0; i < ReferenceMaxAudioProcessor::maxReferenceSlots; ++i)
        processor.getSlot (i).removeChangeListener (this);
}

//==============================================================================

bool ReferenceDetailView::isNoSlotMode() const
{
    return processor.getNumVisibleSlots() <= 0;
}

void ReferenceDetailView::setSlot (int logicalIndex)
{
    const int visible = processor.getNumVisibleSlots();

    if (visible <= 0)
    {
        refresh();
        return;
    }

    if (logicalIndex < 0 || logicalIndex >= visible)
        logicalIndex = 0;

    if (logicalIndex == currentLogicalIndex && currentGainParam != nullptr)
        return;

    processor.getSlot (currentLogicalIndex).removeChangeListener (this);

    currentLogicalIndex = logicalIndex;

    cancelLearning();

    attachToSlot();
    refresh();
}

void ReferenceDetailView::attachToSlot()
{
    if (processor.getNumVisibleSlots() <= 0)
        return;

    auto& slot = processor.getSlot (currentLogicalIndex);

    slot.addChangeListener (this);

    waveform = std::make_unique<WaveformView> (slot);

    // Clicking on an empty waveform opens the file chooser.
    waveform->onEmptyClicked = [this] { openFileChooser(); };

    // Live preview of the loop range during a drag, so the "Loop In /
    // Loop Out" display updates as the user moves the mouse.
    waveform->onLoopRangePreviewChanged = [this] (float start, float end)
    {
        previewLoopStart = start;
        previewLoopEnd   = end;

        updateTimeDisplay();
        repaint (statusRect);
    };

    addAndMakeVisible (*waveform);
    resized();

    const int paramNum = processor.paramNumberFor (currentLogicalIndex);

    const juce::String gainParamID = "gain_" + juce::String (paramNum);
    currentGainParam = processor.apvts.getParameter (gainParamID);
    updateGainFromParameter();

    const juce::String followParamID = "follow_" + juce::String (paramNum);
    currentFollowParam = processor.apvts.getParameter (followParamID);

    updateFollowButton();

    const juce::String matchParamID = "match_" + juce::String (paramNum);
    currentMatchParam = processor.apvts.getParameter (matchParamID);

    const juce::String offsetParamID = "follow_offset_" + juce::String (paramNum);
    currentOffsetParam = processor.apvts.getParameter (offsetParamID);

    updateReplaceButtonLabel();
    updateMatchButtonText();
    updateOffsetButtonState();
    updateOffsetTooltip();

    titleLabel.setText ("REF " + juce::String (currentLogicalIndex + 1),
                        juce::dontSendNotification);
}

void ReferenceDetailView::updateReplaceButtonLabel()
{
    if (isNoSlotMode())
        return;

    auto& slot = processor.getSlot (currentLogicalIndex);

    if (slot.isLoading())
    {
        replaceButton.setButtonText ("Loading...");
        replaceButton.setEnabled (false);
    }
    else if (slot.isLoaded())
    {
        replaceButton.setButtonText ("Replace");
        replaceButton.setEnabled (true);
    }
    else
    {
        replaceButton.setButtonText ("Load");
        replaceButton.setEnabled (true);
    }
}

void ReferenceDetailView::updateGainFromParameter()
{
    if (currentGainParam == nullptr)
        return;

    const auto& range = currentGainParam->getNormalisableRange();
    const float valueDb = range.convertFrom0to1 (currentGainParam->getValue());

    gainSlider.setValue ((double) valueDb, juce::dontSendNotification);

    juce::String num = juce::String (valueDb, 1);
    if (! num.startsWithChar ('-') && ! num.startsWithChar ('+'))
        num = "+" + num;

    gainValueLabel.setText (num + " dB", juce::dontSendNotification);
}

void ReferenceDetailView::pushGainToParameter (double newValueDb)
{
    if (currentGainParam == nullptr)
        return;

    const auto& range = currentGainParam->getNormalisableRange();
    const float normalized = range.convertTo0to1 ((float) newValueDb);

    currentGainParam->beginChangeGesture();
    currentGainParam->setValueNotifyingHost (normalized);
    currentGainParam->endChangeGesture();

    juce::String num = juce::String (newValueDb, 1);
    if (! num.startsWithChar ('-') && ! num.startsWithChar ('+'))
        num = "+" + num;

    gainValueLabel.setText (num + " dB", juce::dontSendNotification);
}

void ReferenceDetailView::updateFollowButton()
{
    if (currentFollowParam == nullptr)
    {
        followButton.setToggleState (false, juce::dontSendNotification);
        followButton.setButtonText ("Follow");
        updateLoopButtonEnabledState (false);
        return;
    }

    const bool followOn = currentFollowParam->getValue() >= 0.5f;

    followButton.setToggleState (followOn, juce::dontSendNotification);
    followButton.setButtonText (followOn ? "Following" : "Follow");

    updateLoopButtonEnabledState (followOn);
}

void ReferenceDetailView::updateLoopButtonEnabledState (bool followIsOn)
{
    loopButton.setEnabled (! followIsOn);

    if (followIsOn)
        loopButton.setTooltip ("Loop is disabled while Follow is active.");
    else
        loopButton.setTooltip ("");
}

void ReferenceDetailView::toggleFollow()
{
    if (currentFollowParam == nullptr)
        return;

    const bool currentlyOn = currentFollowParam->getValue() >= 0.5f;
    const bool newValue = ! currentlyOn;

    currentFollowParam->beginChangeGesture();
    currentFollowParam->setValueNotifyingHost (newValue ? 1.0f : 0.0f);
    currentFollowParam->endChangeGesture();

    followButton.setToggleState (newValue, juce::dontSendNotification);
    followButton.setButtonText (newValue ? "Following" : "Follow");

    updateLoopButtonEnabledState (newValue);

    updateOffsetButtonState();
    updateOffsetTooltip();
    updateTimeDisplay();

    if (waveform != nullptr)
        waveform->repaint();
}

//==============================================================================

void ReferenceDetailView::onMatchButtonClicked()
{
    if (isNoSlotMode() || currentMatchParam == nullptr)
        return;

    if (isLearning)
    {
        cancelLearning();
        return;
    }

    const bool currentlyMatched = currentMatchParam->getValue() >= 0.5f;

    if (currentlyMatched)
    {
        setMatchEnabled (false);
        return;
    }

    const float currentTarget = processor.getMatchTargetLufs();
    const bool hasTarget = currentTarget > -69.5f;

    if (hasTarget)
    {
        setMatchEnabled (true);
    }
    else
    {
        learningAutoEngage = true;
        startLearning();
    }
}

void ReferenceDetailView::startLearning()
{
    isLearning = true;
    learningEndMs = juce::Time::getMillisecondCounterHiRes() + learningDurationMs;
    learningPulseStartMs = juce::Time::getMillisecondCounterHiRes();
    learningAccum = 0.0;
    learningSamples = 0;

    updateMatchButtonText();
    updateMatchButtonPulse();
    repaint();
}

void ReferenceDetailView::cancelLearning()
{
    if (! isLearning)
        return;

    isLearning = false;
    learningAccum = 0.0;
    learningSamples = 0;

    matchButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);

    updateMatchButtonText();
    repaint();
}

void ReferenceDetailView::finishLearningIfDone()
{
    if (! isLearning)
        return;

    const float dawLufs = processor.getDawLufsMeter().getShortTermLufs();

    if (dawLufs > -40.0f)
    {
        learningAccum += (double) dawLufs;
        learningSamples++;
    }

    if (juce::Time::getMillisecondCounterHiRes() < learningEndMs)
        return;

    isLearning = false;

    matchButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);

    if (learningSamples > 0)
    {
        const float avg = (float) (learningAccum / (double) learningSamples);
        processor.setMatchTargetLufs (avg);

        if (learningAutoEngage)
            setMatchEnabled (true);
    }
    else
    {
        updateMatchButtonText();
    }

    learningAccum = 0.0;
    learningSamples = 0;
    repaint();
}

void ReferenceDetailView::setMatchEnabled (bool shouldMatch)
{
    if (currentMatchParam == nullptr)
        return;

    currentMatchParam->beginChangeGesture();
    currentMatchParam->setValueNotifyingHost (shouldMatch ? 1.0f : 0.0f);
    currentMatchParam->endChangeGesture();

    updateMatchButtonText();
    repaint();
}

void ReferenceDetailView::updateMatchButtonText()
{
    if (isNoSlotMode() || currentMatchParam == nullptr)
    {
        matchButton.setButtonText ("Match");
        matchButton.setToggleState (false, juce::dontSendNotification);
        return;
    }

    if (isLearning)
    {
        matchButton.setButtonText ("Matching");
        matchButton.setToggleState (true, juce::dontSendNotification);
        return;
    }

    const bool matchOn = currentMatchParam->getValue() >= 0.5f;

    if (matchButton.getToggleState() != matchOn)
        matchButton.setToggleState (matchOn, juce::dontSendNotification);

    const float target = processor.getMatchTargetLufs();
    const bool hasTarget = target > -69.5f;

    if (! hasTarget)
    {
        matchButton.setButtonText ("Match");
        return;
    }

    if (! matchOn)
    {
        matchButton.setButtonText ("Match");
        return;
    }

    const float refLufs = processor.getSlot (currentLogicalIndex).getIntegratedLufs();

    if (refLufs <= -69.5f)
    {
        matchButton.setButtonText ("Match");
        return;
    }

    const float appliedGainDb = target - refLufs;

    juce::String num = juce::String (appliedGainDb, 1);
    if (! num.startsWithChar ('-') && ! num.startsWithChar ('+'))
        num = "+" + num;

    matchButton.setButtonText ("Match " + num + " dB");
}

void ReferenceDetailView::updateMatchButtonPulse()
{
    if (! isLearning)
        return;

    const double elapsedMs = juce::Time::getMillisecondCounterHiRes() - learningPulseStartMs;
    const double phase = elapsedMs / 1000.0 * juce::MathConstants<double>::twoPi * pulseHz;
    const float  t = (float) ((std::sin (phase) + 1.0) * 0.5);
    const float  brightness = 0.55f + 0.45f * t;

    const auto pulsed = Theme::accentRed.withMultipliedBrightness (brightness);
    matchButton.setColour (juce::TextButton::buttonOnColourId, pulsed);
}

//==============================================================================

static juce::String formatOffsetSeconds (float seconds)
{
    const float absV = std::abs (seconds);
    const juce::String sign = (seconds < 0.0f) ? "-" : "+";

    if (absV < 60.0f)
    {
        return sign + juce::String (absV, 1);
    }

    const int totalSeconds = (int) absV;
    const int minutes      = totalSeconds / 60;
    const int secs         = totalSeconds % 60;
    const int tenths       = (int) std::round ((absV - (float) totalSeconds) * 10.0f);

    int t = juce::jlimit (0, 9, tenths);

    return sign
         + juce::String (minutes)
         + "." + juce::String (secs).paddedLeft ('0', 2)
         + "." + juce::String (t);
}

void ReferenceDetailView::onOffsetButtonClicked()
{
    if (isNoSlotMode() || currentOffsetParam == nullptr || currentFollowParam == nullptr)
        return;

    if (currentFollowParam->getValue() < 0.5f)
        return;

    const double dawSeconds = processor.getLastKnownDawSeconds();

    if (dawSeconds < 0.0)
        return;

    const double clamped = juce::jlimit (-120.0, 120.0, dawSeconds);

    const auto& range = currentOffsetParam->getNormalisableRange();
    const float normalized = range.convertTo0to1 ((float) clamped);

    currentOffsetParam->beginChangeGesture();
    currentOffsetParam->setValueNotifyingHost (normalized);
    currentOffsetParam->endChangeGesture();

    updateOffsetButtonState();
    updateOffsetTooltip();
}

void ReferenceDetailView::onOffsetButtonDoubleClicked()
{
    if (isNoSlotMode() || currentOffsetParam == nullptr)
        return;

    const auto& range = currentOffsetParam->getNormalisableRange();
    const float normalized = range.convertTo0to1 (0.0f);

    currentOffsetParam->beginChangeGesture();
    currentOffsetParam->setValueNotifyingHost (normalized);
    currentOffsetParam->endChangeGesture();

    updateOffsetButtonState();
    updateOffsetTooltip();
}

void ReferenceDetailView::updateOffsetButtonState()
{
    if (isNoSlotMode() || currentFollowParam == nullptr || currentOffsetParam == nullptr)
    {
        offsetButton.setEnabled (false);
        offsetButton.setToggleState (false, juce::dontSendNotification);
        offsetButton.setButtonText ("Offset");
        return;
    }

    const bool followOn = currentFollowParam->getValue() >= 0.5f;
    const bool positionKnown = processor.getLastKnownDawSeconds() >= 0.0;

    offsetButton.setEnabled (followOn && positionKnown);

    const auto& range = currentOffsetParam->getNormalisableRange();
    const float offsetSec = range.convertFrom0to1 (currentOffsetParam->getValue());

    const bool engaged = std::abs (offsetSec) >= 0.05f;

    offsetButton.setToggleState (engaged, juce::dontSendNotification);

    if (! engaged)
    {
        offsetButton.setButtonText ("Offset");
        return;
    }

    offsetButton.setButtonText ("Offset " + formatOffsetSeconds (offsetSec) + " s");
}

void ReferenceDetailView::updateOffsetTooltip()
{
    if (isNoSlotMode() || currentFollowParam == nullptr || currentOffsetParam == nullptr)
    {
        offsetButton.setTooltip ("Enable Follow (Sync) first to set an offset.");
        return;
    }

    const bool followOn = currentFollowParam->getValue() >= 0.5f;

    if (! followOn)
    {
        offsetButton.setTooltip ("Enable Follow (Sync) first to set an offset.");
        return;
    }

    const auto& range = currentOffsetParam->getNormalisableRange();
    const float offsetSec = range.convertFrom0to1 (currentOffsetParam->getValue());

    offsetButton.setTooltip ("Offset set to: " + formatOffsetSeconds (offsetSec) + " s\n"
                             "Click to capture the current DAW transport position.\n"
                             "Double-click to reset to 0.");
}

//==============================================================================

void ReferenceDetailView::refresh()
{
    // Any committed change to the slot invalidates the in-progress preview.
    // During a drag, refresh() isn't called (the waveform only commits on
    // mouse-up), so this only fires after a commit has already landed.
    previewLoopStart = -1.0f;
    previewLoopEnd   = -1.0f;

    const bool noSlots = isNoSlotMode();

    titleLabel.setVisible (true);
    fileLabel.setVisible (true);
    replaceButton.setVisible (! noSlots);
    loopButton.setVisible (! noSlots);
    followButton.setVisible (! noSlots);
    matchButton.setVisible (! noSlots);
    offsetButton.setVisible (! noSlots);
    gainLabel.setVisible (! noSlots);
    gainValueLabel.setVisible (! noSlots);
    gainSlider.setVisible (! noSlots);
    playheadTimeLabel.setVisible (! noSlots);
    timeSeparatorLabel.setVisible (! noSlots);

    if (waveform != nullptr)
        waveform->setVisible (! noSlots);

    if (noSlots)
    {
        titleLabel.setText ("No reference - click + Add reference to start",
                            juce::dontSendNotification);
        fileLabel.setText ("", juce::dontSendNotification);
        statusText = "";
        resized();
        repaint();
        return;
    }

    auto& slot = processor.getSlot (currentLogicalIndex);

    titleLabel.setText ("REF " + juce::String (currentLogicalIndex + 1),
                        juce::dontSendNotification);
    fileLabel.setText (slot.getStatusText(), juce::dontSendNotification);

    updateReplaceButtonLabel();

    const bool isLooping = slot.isLoopEnabled();
    if (loopButton.getToggleState() != isLooping)
        loopButton.setToggleState (isLooping, juce::dontSendNotification);

    updateFollowButton();

    updateMatchButtonText();
    updateOffsetButtonState();
    updateOffsetTooltip();

    updateGainFromParameter();
    updateTimeDisplay();

    resized();
    repaint();
}

//==============================================================================

void ReferenceDetailView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (draggingOver ? Theme::panelHover : Theme::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (Theme::accentRed.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.5f);

    if (! isNoSlotMode() && statusText.isNotEmpty())
    {
        g.setColour (statusTextColour);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (statusText,
                    statusRect,
                    juce::Justification::centredLeft,
                    false);
    }
}

void ReferenceDetailView::resized()
{
    auto bounds = getLocalBounds().reduced (10, 6);

    auto header = bounds.removeFromTop (30);

    const bool noSlots = isNoSlotMode();

    if (noSlots)
    {
        titleLabel.setBounds (header);
        fileLabel.setBounds ({});
    }
    else
    {
        const int replaceButtonWidth = 78;
        replaceButton.setBounds (header.removeFromRight (replaceButtonWidth).reduced (0, 2));

        titleLabel.setBounds (header.removeFromLeft (56));
        header.removeFromLeft (6);

        fileLabel.setBounds (header);
    }

    bounds.removeFromTop (4);

    const int timeRowHeight = 28;
    auto timeRow = bounds.removeFromBottom (timeRowHeight);

    bounds.removeFromBottom (6);

    if (waveform != nullptr)
        waveform->setBounds (bounds);

    const int playheadWidth   = 58;
    const int loopButtonW     = 48;
    const int followButtonW   = 72;
    const int offsetButtonW   = 95;
    const int statusWidth     = 200;
    const int statusLeftPad   = 12;
    const int matchButtonW    = 108;
    const int gainLabelW      = 34;
    const int gainSliderW     = 110;
    const int gainValueW      = 62;
    const int tightGap        = 4;
    const int normalGap       = 6;
    const int gainLabelPad    = 4;

    const int rowY = timeRow.getY() + 2;
    const int rowH = timeRow.getHeight() - 4;

    int rx = timeRow.getRight();

    rx -= gainValueW;
    gainValueLabel.setBounds (rx, rowY, gainValueW, rowH);

    rx -= normalGap + gainSliderW;
    gainSlider.setBounds (rx, rowY, gainSliderW, rowH);

    rx -= normalGap + gainLabelW + gainLabelPad;
    gainLabel.setBounds (rx + gainLabelPad, rowY, gainLabelW, rowH);

    rx -= tightGap + matchButtonW;
    matchButton.setBounds (rx, rowY, matchButtonW, rowH);

    int lx = timeRow.getX();

    playheadTimeLabel.setBounds (lx + 2, rowY, playheadWidth - 4, rowH);
    lx += playheadWidth + normalGap;

    loopButton.setBounds (lx, rowY, loopButtonW, rowH);
    lx += loopButtonW + normalGap;

    followButton.setBounds (lx, rowY, followButtonW, rowH);
    lx += followButtonW + normalGap;

    offsetButton.setBounds (lx, rowY, offsetButtonW, rowH);
    lx += offsetButtonW;

    lx += statusLeftPad;
    statusRect = juce::Rectangle<int> (lx, rowY, statusWidth, rowH);
}

//==============================================================================

void ReferenceDetailView::timerCallback()
{
    if (! isNoSlotMode())
    {
        finishLearningIfDone();
        updateMatchButtonPulse();
        updateTimeDisplay();
        updateMatchButtonText();
        updateOffsetButtonState();
    }
}

void ReferenceDetailView::updateTimeDisplay()
{
    if (isNoSlotMode())
        return;

    auto& slot = processor.getSlot (currentLogicalIndex);

    if (! slot.isLoaded())
    {
        playheadTimeLabel.setText ("--:--", juce::dontSendNotification);
        statusText = "";
        statusTextColour = Theme::textPrimary;
        return;
    }

    const double length = slot.getLengthSeconds();
    if (length <= 0.0)
    {
        playheadTimeLabel.setText ("--:--", juce::dontSendNotification);
        statusText = "";
        statusTextColour = Theme::textPrimary;
        return;
    }

    const double currentTime = slot.getNormalizedPosition() * length;
    playheadTimeLabel.setText (TimeFormat::mmSs (currentTime),
                               juce::dontSendNotification);

    // Use the in-progress preview values if a drag is active, otherwise
    // the committed loop values.
    const bool previewing = (previewLoopStart >= 0.0f && previewLoopEnd >= 0.0f);

    if (previewing || slot.isLoopEnabled())
    {
        const float startNorm = previewing ? previewLoopStart : slot.getLoopStartNormalized();
        const float endNorm   = previewing ? previewLoopEnd   : slot.getLoopEndNormalized();

        const double loopStart = (double) startNorm * length;
        const double loopEnd   = (double) endNorm   * length;

        statusText = "Loop In: " + TimeFormat::mmSsTenths (loopStart)
                   + "   Loop Out: " + TimeFormat::mmSsTenths (loopEnd);
        statusTextColour = Theme::greyLight;
    }
    else
    {
        statusText = "Total: " + TimeFormat::mmSs (length);
        statusTextColour = Theme::textPrimary;
    }
}

//==============================================================================

void ReferenceDetailView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

//==============================================================================

bool ReferenceDetailView::isInterestedInFileDrag (const juce::StringArray& files)
{
    return ! isNoSlotMode() && AudioFileSupport::containsSupportedFile (files);
}

void ReferenceDetailView::fileDragEnter (const juce::StringArray&, int, int)
{
    draggingOver = true;
    repaint();
}

void ReferenceDetailView::fileDragExit (const juce::StringArray&)
{
    draggingOver = false;
    repaint();
}

void ReferenceDetailView::filesDropped (const juce::StringArray& files, int, int)
{
    draggingOver = false;
    repaint();

    if (isNoSlotMode())
        return;

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (AudioFileSupport::isSupportedFile (file))
        {
            processor.resetSlotForNewFile (currentLogicalIndex);
            processor.getSlot (currentLogicalIndex).loadFileAsync (file);
            break;
        }
    }
}

//==============================================================================

void ReferenceDetailView::openFileChooser()
{
    if (isNoSlotMode())
        return;

    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ReferenceDetailView> (this)]
    {
        if (safe == nullptr || safe->isNoSlotMode())
            return;

        safe->chooser = std::make_unique<juce::FileChooser> (
            "Load reference audio",
            juce::File(),
            AudioFileSupport::fileChooserWildcard());

        const auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

        const int slotIndex = safe->currentLogicalIndex;

        safe->chooser->launchAsync (flags, [safe, slotIndex] (const juce::FileChooser& fc)
        {
            if (safe == nullptr)
                return;

            const auto file = fc.getResult();

            if (AudioFileSupport::isSupportedFile (file))
            {
                safe->processor.resetSlotForNewFile (slotIndex);
                safe->processor.getSlot (slotIndex).loadFileAsync (file);
            }
        });
    });
}

void ReferenceDetailView::openChooserForCurrentSlot()
{
    openFileChooser();
}