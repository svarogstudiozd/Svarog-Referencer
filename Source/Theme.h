#pragma once

#include <juce_graphics/juce_graphics.h>

/**
    Central colour palette for Svarog Referencer.

    Svarog Studio identity:
        dark grey    - main surface
        black        - insets, backgrounds
        light grey   - text, DAW trace, meter mid
        dark red     - reference accent, active toggles, peaks
*/
namespace Theme
{
    // Surfaces
    inline const juce::Colour background    { 0xff1a1a1a };
    inline const juce::Colour panel         { 0xff222222 };
    inline const juce::Colour panelHover    { 0xff2a2a2a };
    inline const juce::Colour panelDarker   { 0xff101010 };
    inline const juce::Colour inset         { 0xff0f0f0f };

    // Accent (dark red)
    inline const juce::Colour accentRed       { 0xff6b2a2a };
    inline const juce::Colour accentRedBright { 0xffa83838 };
    inline const juce::Colour accentRedDeep   { 0xff4a1f1f };

    // Greys
    inline const juce::Colour greyLight     { 0xffc8c8c8 };
    inline const juce::Colour greyMid       { 0xff909090 };
    inline const juce::Colour greyDim       { 0xff606060 };

    // Borders
    inline const juce::Colour border        { 0xff383838 };
    inline const juce::Colour borderSubtle  { 0xff2e2e2e };

    // Semantic
    inline const juce::Colour textPrimary   { 0xffd8d8d8 };
    inline const juce::Colour textSecondary { 0xff909090 };
    inline const juce::Colour textFaint     { 0xff606060 };

    inline const juce::Colour dawTrace      { 0xffb0b0b0 };   // light grey
    inline const juce::Colour refTrace      { 0xffa83838 };   // dark red
}

/**
    Shared UI timing constants.

    Everything in the plugin that runs on a timer is driven at the same
    frame rate, so time-based coefficients (ballistics, smoothing) can be
    expressed consistently in terms of this one value.
*/
namespace Display
{
    static constexpr float fps          = 30.0f;
    static constexpr float blockSeconds = 1.0f / fps;
}