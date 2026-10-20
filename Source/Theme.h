#pragma once

#include <juce_graphics/juce_graphics.h>

/**
    Central palette for Svarog Referencer.

    Svarog Studio identity:
        mid greys    - the base surface and panels
        deep black   - recessed wells and shadows
        rich red     - accents, active states, warnings
        off-white    - primary text, values, highlights
        medium greys - body text, secondary labels

    The theme is organised by ROLE rather than by appearance. Each
    colour has a job — "this is a raised panel", "this is a well",
    "this is a hover state" — so components pick the right colour for
    their structural role rather than guessing at a shade.

    Depth model:

        well         (darkest)   cut into panels
        base         (mid-dark)  the plugin background
        panel        (mid)       raised on the base
        card         (lighter)   raised on a panel
        hover        (lightest)  interactive feedback

    Legacy names are kept as aliases to the new role-based colours, so
    existing code continues to compile while components are migrated
    to the new naming over time.
*/
namespace Theme
{
    //==========================================================================
    // Surfaces — the horizontal layers of the UI, darkest to lightest.
    //==========================================================================

    inline const juce::Colour surfaceDeepest { 0xff080808 };
    inline const juce::Colour surfaceBase    { 0xff262626 };
    inline const juce::Colour surfaceRaised  { 0xff303030 };
    inline const juce::Colour surfaceCard    { 0xff383838 };
    inline const juce::Colour surfaceHover   { 0xff424242 };

    //==========================================================================
    // Borders and highlights
    //==========================================================================

    inline const juce::Colour borderStrong   { 0xff1a1a1a };
    inline const juce::Colour borderSubtle   { 0xff252525 };
    inline const juce::Colour topHighlight   { 0x40ffffff };
    inline const juce::Colour insetShadowTop { 0xb0000000 };
    inline const juce::Colour insetHighlightBottom { 0x1affffff };

    //==========================================================================
    // Accents (red ramp)
    //==========================================================================

    // Primary red — active states, toggles, warnings.
    inline const juce::Colour accentRed       { 0xff7a2d2d };

    // Brighter red — indicator bars, engaged controls, peak highlights.
    inline const juce::Colour accentRedBright { 0xffb34444 };

    // Deep red — gradient ends, pressed states.
    inline const juce::Colour accentRedDeep   { 0xff4a1f1f };

    // Warning amber — used for missing files and warnings that need
    // to stand out against red backgrounds.
    inline const juce::Colour warningAmber  { 0xffffb347 }; 

    //==========================================================================
    // Greys (structural — used for backgrounds, indicators, neutral fills)
    //==========================================================================

    inline const juce::Colour greyLight     { 0xffc8c8c8 };
    inline const juce::Colour greyMid       { 0xff909090 };
    inline const juce::Colour greyDim       { 0xff606060 };

    //==========================================================================
    // Text
    //==========================================================================

     inline const juce::Colour textPrimary   { 0xfff0f0f0 };
    inline const juce::Colour textSecondary { 0xffb8b8b8 };
    inline const juce::Colour textFaint     { 0xff8a8a8a };

    // Red text for warnings and active-state labels.
    inline const juce::Colour textAccent    = accentRedBright;

    //==========================================================================
    // Traces (spectrum analyser)
    //==========================================================================

    inline const juce::Colour dawTrace      { 0xffb0b0b0 };   // light grey
    inline const juce::Colour refTrace      { 0xffa83838 };   // dark red

    //==========================================================================
    // Legacy aliases
    //
    // These are the old colour names used throughout the codebase. They
    // map onto the role-based colours above. As components are migrated
    // to the new naming, these will gradually be removed. Until then
    // they let old and new code coexist.
    //==========================================================================

    inline const juce::Colour background    = surfaceBase;
    inline const juce::Colour panel         = surfaceCard;
    inline const juce::Colour panelHover    = surfaceHover;
    inline const juce::Colour panelDarker   = surfaceDeepest;
    inline const juce::Colour inset         = surfaceDeepest;
    inline const juce::Colour border        = borderStrong;
    // (borderSubtle is already declared above.)
}

/**
    Spacing scale.

    Every gap, margin, and padding in the UI should be one of these
    values. Consistency here is more important than pixel-perfect
    matching of any particular value.
*/
namespace Spacing
{
    static constexpr int hairline = 2;
    static constexpr int tight    = 4;
    static constexpr int normal   = 8;
    static constexpr int gap      = 12;
    static constexpr int inset    = 16;
    static constexpr int section  = 24;
}

/**
    Corner radii.

    Two values are used throughout: one for controls (buttons, small
    elements) and one for panels (filter bar, detail view, spectrum,
    tiles). Wells use a slightly smaller radius so they read as cut
    into the surface rather than sitting on top of it.
*/
namespace Radius
{
    static constexpr float control = 6.0f;
    static constexpr float panel   = 6.0f;
    static constexpr float well    = 6.0f;
}

/**
    Typography scale.

    Three sizes for the whole UI plus one for the plugin title. Anything
    that doesn't fit one of these should be reconsidered — the scale is
    what keeps the UI feeling coherent rather than ad-hoc.
*/
namespace Typography
{
    static constexpr float titleSize = 22.0f;   // the plugin name
    static constexpr float valueSize = 14.0f;   // numbers, readouts, primary buttons
    static constexpr float bodySize  = 12.5f;   // filenames, descriptive text
    static constexpr float labelSize = 11.0f;   // section labels, tags
}

/**
    Shadow parameters for the depth model.

    Two shadow depths are used throughout: one for panels (raised above
    the base surface) and one for cards (raised above a panel). No
    component should define its own shadow values.
*/
namespace Shadow
{
    // Panel shadow: raised panels (filter bar, detail view, spectrum frame).
    static constexpr int   panelOffsetY  = 3;
    static constexpr int   panelBlur     = 6;
    static constexpr float panelAlpha    = 0.75f;

    // Card shadow: controls inside panels (buttons).
    static constexpr int   cardOffsetY   = 3;
    static constexpr int   cardBlur      = 3;
    static constexpr float cardAlpha     = 0.30f;

    // Tile shadow: slot tiles. Darker than card shadow so the tiles
    // read as more substantial than buttons.
    static constexpr int   tileOffsetY   = 2;
    static constexpr int   tileBlur      = 3;
    static constexpr float tileAlpha     = 0.75f;
}

/**
    Shared UI timing constants.

    Everything in the plugin that runs on a timer is driven at the same
    frame rate, so time-based coefficients (ballistics, smoothing) can be
    expressed consistently in terms of this one value.
*/
namespace Display
{
    static constexpr float fps          = 60.0f;
    static constexpr float blockSeconds = 1.0f / fps;
}