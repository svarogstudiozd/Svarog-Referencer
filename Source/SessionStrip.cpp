#include "SessionStrip.h"
#include <cmath>
#include "ReferenceMaxLookAndFeel.h"

//==============================================================================
SessionStrip::SessionStrip (SessionTimer& timerToUse,
                            juce::AudioProcessorValueTreeState& apvts)
    : sessionTimer (timerToUse),
      autoPauseField (apvts,
                      "session_auto_pause_minutes",
                      "After",
                      "min",
                      5.0)          // double-click resets to 5 min
{
    // ---- Toggle button (Start / Pause / Continue) ----
    addAndMakeVisible (toggleButton);
    toggleButton.getProperties().set ("fontSize", 12.0f);
    toggleButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    toggleButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    toggleButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    toggleButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
        toggleButton.setTooltip ("Start, stop, or continue the session timer.\n"
                             "When auto-paused, the timer resumes by itself "
                             "as soon as audio or transport activity returns.");

    toggleButton.onClick = [this]
    {
        sessionTimer.toggleManually();
        updateButtonLabels();
        updateTimeLabels();
        repaint();
    };

    // ---- Auto-pause toggle ----
    addAndMakeVisible (autoPauseButton);
    autoPauseButton.getProperties().set ("fontSize", 12.0f);
    autoPauseButton.setButtonText ("Auto pause");
    autoPauseButton.setClickingTogglesState (true);
    autoPauseButton.setToggleState (sessionTimer.isAutoPauseEnabled(),
                                    juce::dontSendNotification);
    autoPauseButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    autoPauseButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
    autoPauseButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    autoPauseButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    autoPauseButton.setTooltip ("Pause the timer automatically after the "
                                "configured idle time. Turn off to keep "
                                "counting through inactivity.");

    autoPauseButton.onClick = [this]
    {
        const bool on = autoPauseButton.getToggleState();
        sessionTimer.setAutoPauseEnabled (on);
        autoPauseField.setEnabled (on);
         repaint(); 
    };

        // ---- Section label ----
    // Visually groups this strip as something separate from the audio-
    // processing controls above it.
       addAndMakeVisible (sectionLabel);
    sectionLabel.setText ("Project Tracking", juce::dontSendNotification);
    sectionLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    sectionLabel.setColour (juce::Label::textColourId, Theme::textPrimary);
    sectionLabel.setJustificationType (juce::Justification::centredLeft);

    // ---- Auto-pause threshold field ----
    // The DraggableValueBox listens to the APVTS parameter directly.
    // The processor forwards changes to the SessionTimer via its own
    // parameter listener, so this component doesn't have to do anything
    // when the value changes.
    addAndMakeVisible (autoPauseField);
    autoPauseField.setEnabled (sessionTimer.isAutoPauseEnabled());

    

    // ---- Session time label ----
    addAndMakeVisible (sessionLabel);
    sessionLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    sessionLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    sessionLabel.setJustificationType (juce::Justification::centredLeft);

    // ---- Total time label ----
    addAndMakeVisible (totalLabel);
    totalLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    totalLabel.setColour (juce::Label::textColourId, Theme::textPrimary);
    totalLabel.setJustificationType (juce::Justification::centredLeft);

    // ---- Reset button ----
    addAndMakeVisible (resetButton);
    resetButton.getProperties().set ("fontSize", 12.0f);
    resetButton.setButtonText ("Reset");
    resetButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    resetButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRedBright);
    resetButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    resetButton.setColour (juce::TextButton::textColourOnId,   Theme::greyLight);
    resetButton.setTooltip ("Clears the current session and total time. "
                            "Click twice to confirm.");

    resetButton.onClick = [this]
    {
        if (resetArmed)
            performReset();
        else
            armReset();
    };

    updateButtonLabels();
    updateTimeLabels();

    // 20 Hz is plenty for a pulsing button and a once-per-second clock.
    startTimerHz (20);
}

SessionStrip::~SessionStrip()
{
    stopTimer();
}

//==============================================================================
void SessionStrip::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Recessed well — same treatment as the spectrum, crossover boxes,
    // and waveform. Lighter fill so the top inner shadow has something
    // to darken against.
    const float wellRadius = Radius::panel;

    // Base fill.
    g.setColour (Theme::surfaceRaised.darker (0.35f));
    g.fillRoundedRectangle (bounds, wellRadius);

    // Inner shadow / highlight and side edges, clipped to the well's
    // rounded corners.
    {
        juce::Graphics::ScopedSaveState save (g);

        juce::Path clip;
        clip.addRoundedRectangle (bounds, wellRadius);
        g.reduceClipRegion (clip, {});

        // Top inner shadow — multi-stop gradient.
        const float shadowHeight = 10.0f;

        juce::ColourGradient topShadow (
            juce::Colours::black.withAlpha (0.0f),
            bounds.getX(), bounds.getY(),
            juce::Colours::black.withAlpha (0.0f),
            bounds.getX(), bounds.getY() + shadowHeight,
            false);

        topShadow.addColour (0.15, juce::Colours::black.withAlpha (0.55f));
        topShadow.addColour (0.40, juce::Colours::black.withAlpha (0.30f));
        topShadow.addColour (0.70, juce::Colours::black.withAlpha (0.12f));

        g.setGradientFill (topShadow);
        g.fillRect (bounds.withHeight (shadowHeight + 1.0f));

        // Bottom inner highlight — curved line along the bottom edge
        // and lower corners.
        {
            juce::Path inner;
            inner.addRoundedRectangle (bounds.reduced (1.0f),
                                       juce::jmax (0.0f, wellRadius - 1.0f));

            juce::Path outline;
            juce::PathStrokeType (1.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, inner);

            juce::Graphics::ScopedSaveState saveInner (g);

            juce::Rectangle<float> bottomBand (
                bounds.getX(),
                bounds.getBottom() - wellRadius - 0.0f,
                bounds.getWidth(),
                wellRadius + 0.0f);

            juce::Path band;
            band.addRectangle (bottomBand);
            g.reduceClipRegion (band, {});

            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillPath (outline);
        }

               // Left and right side highlights — faded from nothing at the
        // top to visible at the bottom, matching the spectrum well.
        {
            juce::Path inner;
            inner.addRoundedRectangle (bounds.reduced (1.5f),
                                       juce::jmax (0.0f, wellRadius - 1.0f));

            juce::Path outline;
            juce::PathStrokeType (1.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, inner);

            juce::ColourGradient fade (
                juce::Colours::white.withAlpha (0.03f),
                bounds.getX(), bounds.getY(),
                juce::Colours::white.withAlpha (0.25f),
                bounds.getX(), bounds.getBottom(),
                false);

            fade.addColour (0.21, juce::Colours::white.withAlpha (0.10f));

            // Left side band.
            {
                juce::Graphics::ScopedSaveState saveLeft (g);

                juce::Rectangle<float> leftBand (
                    bounds.getX(),
                    bounds.getY() + wellRadius,
                    2.0f,
                    bounds.getHeight() - wellRadius * 2.0f);

                juce::Path band;
                band.addRectangle (leftBand);
                g.reduceClipRegion (band, {});

                g.setGradientFill (fade);
                g.fillPath (outline);
            }

            // Right side band.
            {
                juce::Graphics::ScopedSaveState saveRight (g);

                juce::Rectangle<float> rightBand (
                    bounds.getRight() - 2.0f,
                    bounds.getY() + wellRadius,
                    2.0f,
                    bounds.getHeight() - wellRadius * 2.0f);

                juce::Path band;
                band.addRectangle (rightBand);
                g.reduceClipRegion (band, {});

                g.setGradientFill (fade);
                g.fillPath (outline);
            }
        }
    }

    // Dark line along the top edge, fading at the ends.
    {
        juce::Graphics::ScopedSaveState save (g);

        juce::Path clip;
        clip.addRoundedRectangle (bounds, wellRadius);
        g.reduceClipRegion (clip, {});

        juce::Path topLine;
        topLine.addRoundedRectangle (bounds.reduced (0.5f),
                                     juce::jmax (0.0f, wellRadius - 0.5f));

        juce::Path outline;
        juce::PathStrokeType (1.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt)
            .createStrokedPath (outline, topLine);

        juce::Graphics::ScopedSaveState saveInner (g);

        juce::Rectangle<float> topBand (
            bounds.getX(),
            bounds.getY(),
            bounds.getWidth(),
            3.0f);

        juce::Path band;
        band.addRectangle (topBand);
        g.reduceClipRegion (band, {});

        juce::ColourGradient fade (
            juce::Colours::black.withAlpha (0.0f),
            bounds.getX(), 0.0f,
            juce::Colours::black.withAlpha (0.0f),
            bounds.getRight(), 0.0f,
            false);
        fade.addColour (0.02, juce::Colours::black.withAlpha (0.7f));
        fade.addColour (0.50, juce::Colours::black.withAlpha (0.7f));
        fade.addColour (0.98, juce::Colours::black.withAlpha (0.7f));

        g.setGradientFill (fade);
        g.fillPath (outline);
    }

        // Draw shadows for our child buttons. Buttons cannot draw outside
    // their own bounds — JUCE clips them — so we draw the shadows here
    // in the parent, right after the well fill and before the buttons
    // paint themselves.
        if (auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel()))
    {
        const auto drawShadow = [&] (juce::Component& c)
        {
            if (! c.isEnabled())
                return;

            if (auto* button = dynamic_cast<juce::Button*> (&c))
            {
                if (button->getToggleState())
                {
                    lf->drawActiveControlShadow (g, c.getBounds().toFloat());
                    return;
                }
            }

            lf->drawControlShadow (g, c.getBounds().toFloat());
        };

        drawShadow (toggleButton);
        drawShadow (autoPauseButton);
        drawShadow (resetButton);
    }
}

void SessionStrip::paintOverChildren (juce::Graphics& g)
{
    // We only draw the custom gradient when the button is in the "Waiting" state
    if (toggleButton.getButtonText() == "Waiting")
    {
        auto bounds = toggleButton.getBounds().toFloat().reduced (0.5f);
        const float radius = Radius::control;

        // 1. Draw the base button shape so it's solid
        g.setColour (Theme::surfaceRaised.darker (0.35f));
        g.fillRoundedRectangle (bounds, radius);

        // 2. Create the radial gradient (Dark Red center fading to Grey)
        const auto centre = bounds.getCentre();
        const auto gradRadius = juce::jmax (bounds.getWidth(), bounds.getHeight()) * 0.6f;

        // Center color: lighter, richer red.
        const auto centerColor = Theme::accentRed; 
        
        // Edge color: fades into the dark grey of the button
        const auto edgeColor = Theme::surfaceRaised.darker (0.35f);

        juce::ColourGradient glowGrad (
            centerColor, centre.x, centre.y,
            edgeColor, centre.x + gradRadius, centre.y,
            true // Radial gradient
        );

        g.setGradientFill (glowGrad);
        g.fillRoundedRectangle (bounds, radius);

        // 3. Draw the dark border matching your LookAndFeel
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, radius, 1.0f);

        // 4. Draw the text on top
        // We must save the state and translate the drawing origin to the button's 
        // local position so the text appears in the right spot.
        juce::Graphics::ScopedSaveState save (g);
        g.setOrigin (toggleButton.getPosition()); 
        
        if (auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel()))
        {
            lf->drawButtonText (g, toggleButton, false, false);
        }
    }
}

void SessionStrip::forEachVisibleChildBounds (const std::function<void (juce::Rectangle<float>)>& fn) const
{
    const auto emit = [&] (const juce::Component& c)
    {
        if (c.isVisible())
            fn (c.getBounds().toFloat());
    };

    emit (toggleButton);
    emit (autoPauseButton);
    emit (autoPauseField);
    emit (resetButton);
}

void SessionStrip::resized()
{
    auto area = getLocalBounds().reduced (10, 8);

    // ---- Section label ----
    auto sectionBlock = area.removeFromLeft (130);
    sectionLabel.setBounds (sectionBlock.reduced (2, 0));

    area.removeFromLeft (4);

    // ---- Left: toggle button ----
    auto toggleBlock = area.removeFromLeft (100);
    toggleButton.setBounds (toggleBlock.reduced (1));

    area.removeFromLeft (12);   // room for divider

     // ---- Auto pause button ----
    auto autoPauseBlock = area.removeFromLeft (100);
    autoPauseButton.setBounds (autoPauseBlock.reduced (1));

    area.removeFromLeft (12);

    // ---- Auto pause threshold field ----
    auto fieldBlock = area.removeFromLeft (100);
    autoPauseField.setBounds (fieldBlock.reduced (1));

    area.removeFromLeft (12);   // room for divider

    // ---- Right: reset button ----
    auto resetBlock = area.removeFromRight (72);
    resetButton.setBounds (resetBlock.reduced (1));

    area.removeFromRight (12);  // room for divider

    // ---- Middle: two time labels ----
    const int halfWidth = area.getWidth() / 2;

    auto sessionArea = area.removeFromLeft (halfWidth);
    sessionLabel.setBounds (sessionArea.reduced (4, 0));

    totalLabel.setBounds (area.reduced (4, 0));
}

//==============================================================================
void SessionStrip::timerCallback()
{
    // Reset-confirm pulse: smooth sinusoidal brightness, matching the
    // Match button's pulse in ReferenceDetailView (pulseHz = 1.2).
    // The button's toggle state stays ON for the whole armed window
    // and we modulate buttonOnColourId, so the LookAndFeel draws it as
    // "on" with the oscillating red.
    if (resetArmed)
    {
        const auto now = juce::Time::currentTimeMillis();
        const auto elapsed = (double) (now - resetArmedAtMs);

        constexpr double pulseHz = 1.2;
        const double phase = elapsed / 1000.0
                           * juce::MathConstants<double>::twoPi
                           * pulseHz;
        const float t = (float) ((std::sin (phase) + 1.0) * 0.5);
        const float brightness = 0.55f + 0.45f * t;

        const auto pulsed = Theme::accentRed.withMultipliedBrightness (brightness);
        resetButton.setColour (juce::TextButton::buttonOnColourId, pulsed);

        if (! resetButton.getToggleState())
            resetButton.setToggleState (true, juce::dontSendNotification);

        if (elapsed > (double) resetConfirmWindowMs)
            cancelResetArm();
    }

    updateButtonLabels();
    updateTimeLabels();
}

//==============================================================================
void SessionStrip::updateButtonLabels()
{
    juce::String text;
    bool primary = false;

    if (! sessionTimer.hasEverStarted())
    {
        // Nothing has started yet — the user needs to press this.
        text = "Start";
        primary = true;
    }
    else if (sessionTimer.isRunning())
    {
        // Clock is counting. Pressing will stop it.
        text = "Stop";
        primary = false;
    }
    else if (sessionTimer.isAutoPaused())
    {
        // Idle for longer than the auto-pause threshold. The clock will
        // resume on its own when activity returns; pressing the button
        // forces an immediate resume. Neutral styling signals that no
        // user action is required.
        text = "Waiting";
        primary = false;
    }
    else
    {
        // Manually paused. Only the user can resume, so this is the
        // primary action.
        text = "Continue";
        primary = true;
    }

    // Three-state appearance:
    //   - Running (Stop)         → neutral panel, off state
    //   - Auto-paused (Waiting)  → engaged state with muted red base,
    //                              so it gets the radial centre-bright
    //                              gradient but in a softer red
    //   - Start / Continue       → engaged state with accent red
    //
    // We set toggleState = true for Waiting so drawButtonBackground
    // passes isGlowing = true, triggering the radial gradient.
     if (text == "Waiting")
    {
        // Make the button's own background transparent so our 
        // paintOverChildren gradient is the only thing visible.
        toggleButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        
        // Hide the native text because paintOverChildren will draw it perfectly centered
        toggleButton.setButtonText (""); 
        
        toggleButton.setToggleState (false, juce::dontSendNotification);
    }
    else
    {
        // Reset the off-state colour for non-Waiting states.
        toggleButton.setColour (juce::TextButton::buttonColourId, Theme::panel);

        if (primary)
            toggleButton.setToggleState (true, juce::dontSendNotification);
        else
            toggleButton.setToggleState (false, juce::dontSendNotification);
    }

    if (toggleButton.getButtonText() != text)
        toggleButton.setButtonText (text);

    repaint();
}

void SessionStrip::updateTimeLabels()
{
    const juce::int64 sessionSecs = sessionTimer.getSessionMs() / 1000;
    const juce::int64 totalSecs   = sessionTimer.getTotalMs()   / 1000;

    if (sessionSecs != lastDisplayedSessionSecs)
    {
        lastDisplayedSessionSecs = sessionSecs;
        sessionLabel.setText ("Session: " + formatHMS (sessionTimer.getSessionMs()),
                              juce::dontSendNotification);
    }

    if (totalSecs != lastDisplayedTotalSecs)
    {
        lastDisplayedTotalSecs = totalSecs;
        totalLabel.setText ("Total: " + formatHMS (sessionTimer.getTotalMs()),
                            juce::dontSendNotification);
    }
}

//==============================================================================
void SessionStrip::armReset()
{
    resetArmed     = true;
    resetArmedAtMs = juce::Time::currentTimeMillis();
    resetButton.setButtonText ("Confirm");
    resetButton.setToggleState (true, juce::dontSendNotification);
    resetButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRed);
}

void SessionStrip::cancelResetArm()
{
    resetArmed = false;
    resetArmedAtMs = 0;
    resetButton.setButtonText ("Reset");
    resetButton.setToggleState (false, juce::dontSendNotification);
    resetButton.setColour (juce::TextButton::buttonOnColourId, Theme::accentRedBright);
}

void SessionStrip::performReset()
{
    sessionTimer.reset();
    cancelResetArm();
    updateButtonLabels();
    updateTimeLabels();
}

//==============================================================================
juce::String SessionStrip::formatHMS (juce::int64 ms)
{
    if (ms < 0)
        ms = 0;

    const int seconds = (int) ((ms / 1000) % 60);
    const int minutes = (int) ((ms / 60000) % 60);
    const int hours   = (int)  (ms / 3600000);

    return juce::String (hours).paddedLeft ('0', 2) + ":"
         + juce::String (minutes).paddedLeft ('0', 2) + ":"
         + juce::String (seconds).paddedLeft ('0', 2);
}