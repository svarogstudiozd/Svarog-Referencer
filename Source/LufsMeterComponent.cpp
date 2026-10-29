#include "LufsMeterComponent.h"
#include "Theme.h"
#include "ReferenceMaxLookAndFeel.h"

namespace
{
    float normFromLufs (float lufs, float minLufs, float maxLufs)
    {
        return juce::jlimit (0.0f, 1.0f, (lufs - minLufs) / (maxLufs - minLufs));
    }
}

LufsMeterComponent::LufsMeterComponent()
{
    modeButton.setClickingTogglesState (false);
    modeButton.onClick = [this] { cycleMode(); };

    // The meter button uses the panel colour for both states (not the
    // LookAndFeel default accent red) so it stays visually neutral.
    modeButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    modeButton.setColour (juce::TextButton::buttonOnColourId, Theme::panel);
    modeButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    modeButton.setColour (juce::TextButton::textColourOnId,   Theme::textPrimary);
    modeButton.setTooltip ("Click to switch between short-term, momentary and integrated LUFS.");

    // Small text so the six-character label fits comfortably inside the
    // narrow meter width. Read by ReferenceMaxLookAndFeel::drawButtonText.
    modeButton.getProperties().set ("fontSize", 10.0f);
    modeButton.getProperties().set ("cornerRadius", 4.0f);

    modeButton.setButtonText (currentLabel());
    addAndMakeVisible (modeButton);

    startTimerHz (10);
}

LufsMeterComponent::~LufsMeterComponent()
{
    stopTimer();
}

void LufsMeterComponent::resized()
{
    auto inner = getLocalBounds().toFloat().reduced (4.0f).toNearestInt();

    auto valueStrip = inner.removeFromTop (20);
    inner.removeFromTop (4);

    auto labelStrip = inner.removeFromBottom (18);
    inner.removeFromBottom (4);

    // The button fills the label strip.
    modeButton.setBounds (labelStrip);
}

void LufsMeterComponent::timerCallback()
{
    if (source == nullptr)
        return;

    const float target = currentLufs();

    const float coeff = (target > displayedLufs) ? 0.6f : 0.15f;

    displayedLufs = target + coeff * (displayedLufs - target);

    repaint();
}

void LufsMeterComponent::cycleMode()
{
    switch (mode)
    {
        case Mode::shortTerm:   mode = Mode::momentary;  break;
        case Mode::momentary:   mode = Mode::integrated; break;
        case Mode::integrated:  mode = Mode::shortTerm;  break;
    }

    modeButton.setButtonText (currentLabel());

    // Snap the display to the new mode's value immediately instead of
    // smoothly interpolating from the previous mode's number.
    displayedLufs = currentLufs();
    repaint();
}

float LufsMeterComponent::currentLufs() const
{
    if (source == nullptr)
        return -70.0f;

    switch (mode)
    {
        case Mode::shortTerm:   return source->getShortTermLufs();
        case Mode::momentary:   return source->getMomentaryLufs();
        case Mode::integrated:  return source->getIntegratedLufs();
    }

    return -70.0f;
}

juce::String LufsMeterComponent::currentLabel() const
{
    switch (mode)
    {
        case Mode::shortTerm:   return "LUFS-S";
        case Mode::momentary:   return "LUFS-M";
        case Mode::integrated:  return "LUFS-I";
    }

    return "LUFS";
}

void LufsMeterComponent::paint (juce::Graphics& g)
{
       auto bounds = getLocalBounds().toFloat();

    const float wellRadius = Radius::well;

  // Dark fill.
    g.setColour (juce::Colour (0xff4a1f1f));
    g.fillRoundedRectangle (bounds, wellRadius);

    // Inner shadow all around the edges, plus the bottom highlight.
    // All drawn inside a clip so the shadow doesn't spill outside the
    // rounded shape.
    {
        juce::Graphics::ScopedSaveState save (g);

        juce::Path clip;
        clip.addRoundedRectangle (bounds, wellRadius);
        g.reduceClipRegion (clip, {});

        // --- Inner shadow, four soft bands ---
        const float shadowDepth = 22.0f;

        // Top band — dark at the top edge, fading downward.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.55f),
                bounds.getX(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX(), bounds.getY() + shadowDepth,
                false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (bounds.getX(),
                                                bounds.getY(),
                                                bounds.getWidth(),
                                                shadowDepth));
        }

        // Bottom band — dark at the bottom edge, fading upward.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.35f),
                bounds.getX(), bounds.getBottom(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX(), bounds.getBottom() - shadowDepth,
                false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (bounds.getX(),
                                                bounds.getBottom() - shadowDepth,
                                                bounds.getWidth(),
                                                shadowDepth));
        }

        // Left band.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.45f),
                bounds.getX(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX() + shadowDepth, bounds.getY(),
                false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (bounds.getX(),
                                                bounds.getY(),
                                                shadowDepth,
                                                bounds.getHeight()));
        }

        // Right band.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.45f),
                bounds.getRight(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getRight() - shadowDepth, bounds.getY(),
                false);
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (bounds.getRight() - shadowDepth,
                                                bounds.getY(),
                                                shadowDepth,
                                                bounds.getHeight()));
        }

        // --- Bottom highlight (on top of the shadow) ---
        {
            juce::Graphics::ScopedSaveState saveInner (g);

            juce::Rectangle<float> bottomBand (
                bounds.getX(),
                bounds.getBottom() - wellRadius - 0.0f,
                bounds.getWidth(),
                wellRadius + 0.0f);

            juce::Path band;
            band.addRectangle (bottomBand);
            g.reduceClipRegion (band, {});

            juce::Path inner;
            inner.addRoundedRectangle (bounds.reduced (1.0f),
                                       juce::jmax (0.0f, wellRadius - 1.0f));

            juce::Path outline;
            juce::PathStrokeType (1.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, inner);

            juce::ColourGradient fade (
                juce::Colours::white.withAlpha (0.0f),
                bounds.getX(), 0.0f,
                juce::Colours::white.withAlpha (0.0f),
                bounds.getRight(), 0.0f,
                false);
            fade.addColour (0.04, juce::Colours::white.withAlpha (0.35f));
            fade.addColour (0.50, juce::Colours::white.withAlpha (0.45f));
            fade.addColour (0.96, juce::Colours::white.withAlpha (0.35f));

            g.setGradientFill (fade);
            g.fillPath (outline);
        }
    }
    auto inner = bounds.reduced (4.0f, 4.0f);

    auto valueStrip = inner.removeFromTop (20.0f);
    inner.removeFromTop (4.0f);

    auto labelStrip = inner.removeFromBottom (18.0f);
    inner.removeFromBottom (4.0f);

    auto barArea = inner;
    auto scaleArea = barArea.removeFromRight (18.0f);
    barArea.removeFromRight (2.0f);

    auto bar = barArea;

    g.setColour (Theme::inset);
    g.fillRoundedRectangle (bar, 2.0f);

        const bool isSilent = displayedLufs <= -39.5f;
    const float norm = normFromLufs (displayedLufs, minLufs, maxLufs);

    if (norm > 0.0001f)
    {
        const float fillHeight = bar.getHeight() * norm;
        auto fillArea = bar.withHeight (fillHeight)
                           .withY (bar.getBottom() - fillHeight);

        // Same gradient as the RMS meter: dark red at the bottom,
        // saturated light red at the top.
        juce::ColourGradient grad (
            juce::Colour (0xff6b2020),   // dark red at bottom
            bar.getX(), fillArea.getBottom(),
            juce::Colour (0xffd0a8a8),   // light red at top
            bar.getX(), fillArea.getY(),
            false);

        g.setGradientFill (grad);
        g.fillRect (fillArea);

        // Thin bright line at the top of the fill.
        g.setColour (juce::Colour (0xfffff0f0));
        g.fillRect (bar.getX(), fillArea.getY(), bar.getWidth(), 1.5f);
    }
    // Reference lines at -14 and -9
    {
        const float refs[] = { -14.0f, -9.0f };

        for (float r : refs)
        {
            const float n = normFromLufs (r, minLufs, maxLufs);
            const float y = bar.getBottom() - n * bar.getHeight();

            g.setColour (r > -10.0f
                            ? Theme::accentRedBright.withAlpha (0.7f)
                            : Theme::greyDim.withAlpha (0.6f));
            g.drawLine (bar.getX(), y, bar.getRight(), y, 1.0f);
        }
    }

    // Scale labels
    {
        g.setColour (Theme::textSecondary);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (9.0f)));

        const float ticks[] = { 0.0f, -9.0f, -14.0f, -24.0f, -40.0f };

        for (float t : ticks)
        {
            const float n = normFromLufs (t, minLufs, maxLufs);
            const float y = bar.getBottom() - n * bar.getHeight();

            g.drawFittedText (juce::String ((int) t),
                              juce::Rectangle<int> ((int) scaleArea.getX(),
                                                    (int) y - 6,
                                                    (int) scaleArea.getWidth(),
                                                    12),
                              juce::Justification::centredLeft,
                              1);
        }
    }

    // Top numeric value
    {
        g.setColour (isSilent ? Theme::textFaint : Theme::greyLight);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (11.0f)));

        const juce::String txt = isSilent
            ? juce::String ("--")
            : juce::String (displayedLufs, 1);

        g.drawFittedText (txt,
                          valueStrip.toNearestInt(),
                          juce::Justification::centred,
                          1);
    }


}