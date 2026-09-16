#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

/**
    Custom LookAndFeel for Svarog Referencer.

    Gives buttons a subtle vertical gradient (soft bevel), a 1 px hairline
    border, slightly rounded corners, a hover brightening, and a pressed
    darkening. Text is drawn with a smaller bold font than JUCE's default
    for a subtler feel.

    The on-state base colour is read from the button's own
    buttonOnColourId, so callers can override it at runtime (e.g. the Match
    button's learning pulse). The off-state base is buttonColourId, and a
    button whose buttonColourId equals Theme::accentRed is treated as a
    primary action (Replace) and is always drawn red.

    Disabled buttons are drawn with a dimmed base and no hover response.

    Per-button font size:
        A button can override the default 15 pt text by setting a numeric
        "fontSize" property on itself, e.g.
            someButton.getProperties().set ("fontSize", 10.0f);
        Used for small controls where the default is too large.
*/
class ReferenceMaxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReferenceMaxLookAndFeel() = default;

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& /*backgroundColour*/,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        constexpr float cornerRadius = 6.0f;

        const bool isEnabled = button.isEnabled();
        const bool isOn      = button.getToggleState();
        const bool isDown    = shouldDrawButtonAsDown;

        // Read the caller-supplied base colours. Fall back to Theme values
        // if the button hasn't set them.
        auto offColour = button.findColour (juce::TextButton::buttonColourId);
        auto onColour  = button.findColour (juce::TextButton::buttonOnColourId);

        if (offColour.isTransparent())
            offColour = Theme::panel;

        if (onColour.isTransparent())
            onColour = Theme::accentRed;

        const bool isPrimary = ! isOn
                            && offColour.getARGB() == Theme::accentRed.getARGB();

        juce::Colour base = (isOn || isPrimary) ? onColour : offColour;

        if (! isEnabled)
        {
            base = base.withMultipliedBrightness (0.6f);
        }

        juce::Colour top, bottom;

        if (! isEnabled)
        {
            top    = base;
            bottom = base.darker (0.10f);
        }
        else if (isDown)
        {
            top    = base.darker (0.05f);
            bottom = base.darker (0.15f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            top    = base.brighter (0.10f);
            bottom = base.darker (0.05f);
        }
        else
        {
            top    = base.brighter (0.05f);
            bottom = base.darker (0.10f);
        }

        juce::ColourGradient grad (top,
                                   bounds.getX(), bounds.getY(),
                                   bottom,
                                   bounds.getX(), bounds.getBottom(),
                                   false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, cornerRadius);

        juce::Colour borderColour;

        if (! isEnabled)
            borderColour = Theme::borderSubtle;
        else if (isOn || isPrimary)
            borderColour = base.brighter (0.35f).withAlpha (0.7f);
        else
            borderColour = Theme::panel.brighter (0.15f);

        g.setColour (borderColour);
        g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/,
                         bool /*shouldDrawButtonAsDown*/) override
    {
        // A button may override the default 15 pt size via a numeric
        // "fontSize" property. Used for small controls (e.g. the LUFS
        // mode button) where the default is too large to fit.
        const auto fontSizeVar = button.getProperties()["fontSize"];
        const float fontSize = fontSizeVar.isVoid() ? 15.0f : (float) fontSizeVar;

        juce::Font font (juce::FontOptions (fontSize, juce::Font::bold));

        g.setFont (font);

        const auto normalColour = button.findColour (button.getToggleState()
                                                     ? juce::TextButton::textColourOnId
                                                     : juce::TextButton::textColourOffId);

        const auto textColour = button.isEnabled()
            ? normalColour
            : normalColour.withMultipliedAlpha (0.4f);

        g.setColour (textColour);

        const int hPad = 4;
        const int vPad = 2;

        const auto textArea = button.getLocalBounds().reduced (hPad, vPad);

        g.drawFittedText (button.getButtonText(),
                          textArea,
                          juce::Justification::centred,
                          1);
    }
};