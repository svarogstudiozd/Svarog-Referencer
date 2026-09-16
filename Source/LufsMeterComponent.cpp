#include "LufsMeterComponent.h"
#include "Theme.h"

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
    modeButton.setColour (juce::TextButton::buttonColourId,   Theme::panel);
    modeButton.setColour (juce::TextButton::buttonOnColourId, Theme::panel);
    modeButton.setColour (juce::TextButton::textColourOffId,  Theme::textPrimary);
    modeButton.setColour (juce::TextButton::textColourOnId,   Theme::textPrimary);
    modeButton.setTooltip ("Click to switch between short-term, momentary and integrated LUFS.");

    // Small text so the six-character label fits comfortably inside the
    // narrow meter width. Read by ReferenceMaxLookAndFeel::drawButtonText.
    modeButton.getProperties().set ("fontSize", 10.0f);

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

    g.setColour (Theme::panelDarker);
    g.fillRoundedRectangle (bounds, 6.0f);

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

        juce::ColourGradient grad (Theme::greyDim,
                                   bar.getX(), bar.getBottom(),
                                   Theme::accentRedBright,
                                   bar.getX(), bar.getY(),
                                   false);
        grad.addColour (0.7, Theme::greyLight);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fillArea, 2.0f);
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
        g.setFont (juce::FontOptions (9.0f));

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
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

        const juce::String txt = isSilent
            ? juce::String ("--")
            : juce::String (displayedLufs, 1);

        g.drawFittedText (txt,
                          valueStrip.toNearestInt(),
                          juce::Justification::centred,
                          1);
    }

    // The bottom label is now the modeButton, drawn by the LookAndFeel.

    g.setColour (Theme::borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}