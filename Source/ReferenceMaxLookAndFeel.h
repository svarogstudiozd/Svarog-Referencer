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

        const bool isEnabled = button.isEnabled();
        const bool isOn      = button.getToggleState();
        const bool isDown    = shouldDrawButtonAsDown;

        // Read the caller-supplied base colours. Fall back to theme
        // values if the button hasn't set them.
        auto offColour = button.findColour (juce::TextButton::buttonColourId);
        auto onColour  = button.findColour (juce::TextButton::buttonOnColourId);

        if (offColour.isTransparent())
            offColour = Theme::surfaceCard;

        if (onColour.isTransparent())
            onColour = Theme::accentRed;

        // A button whose off-state base matches the accent red is treated
        // as a primary action and always drawn in its on-colour.
        const bool isPrimary = ! isOn
                            && offColour.getARGB() == Theme::accentRed.getARGB();

        juce::Colour base = (isOn || isPrimary) ? onColour : offColour;

        if (! isEnabled)
            base = base.withMultipliedBrightness (0.55f);

        // Use the shared depth helper for the button's face. This gives
        // every button in the plugin the same gradient, top highlight,
        // and soft shadow as the tiles and panels.
        if (isEnabled)
        {
            drawRaisedControl (g, bounds, base,
                               shouldDrawButtonAsHighlighted,
                               isDown,
                               isOn);
        }
        else
        {
            // Disabled: recessed, so it reads as inert and pushed in.
            drawRecessedControl (g, bounds, base.darker (0.15f));
        }


    }

    /** Draws a shadow for a control at the given position. Called by
        container components in their paint() to place shadows under
        their button children, since a button cannot draw outside its
        own bounds.
    */

        /** Draws a shadow under an active (engaged) control, with a soft red
        glow around it. Falls through to the standard shadow underneath.
    */
    void drawActiveControlShadow (juce::Graphics& g,
                                  juce::Rectangle<float> bounds) const
    {
        // Red glow: a blurred version of the control's shape, drawn in
        // a bright red and offset outward. We use JUCE's DropShadow with
        // a red colour for this.
        {
            juce::Path glowPath;
            glowPath.addRoundedRectangle (bounds, Radius::control);

            juce::DropShadow glow (Theme::accentRedBright.withAlpha (0.55f),
                                   10,
                                   { 0, 0 });
            glow.drawForPath (g, glowPath);
        }

        // Regular shadow underneath.
        drawControlShadow (g, bounds);
    }

    void drawControlShadow (juce::Graphics& g,
                            juce::Rectangle<float> bounds) const
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle (bounds.translated (0.0f,
                                                           (float) Shadow::cardOffsetY),
                                        Radius::control);

        juce::DropShadow drop (juce::Colours::black.withAlpha (Shadow::cardAlpha),
                               Shadow::cardBlur,
                               { 0, 0 });
        drop.drawForPath (g, shadowPath);
    }

        /** Draws a panel-sized shadow at the given bounds. Called by
        container components in their paint() to place shadows under
        their child panels, since a panel cannot draw outside its own
        bounds.
    */

        /** Draws a shadow for a slot tile. Same as drawControlShadow but
        uses the tile-specific alpha and offset. */

    /** Draws only the red glow around a control (no regular shadow).
        Used by the mode toggle to glow just its active half.
    */
    void drawControlGlow (juce::Graphics& g,
                          juce::Rectangle<float> bounds) const
    {
        juce::Path glowPath;
        glowPath.addRoundedRectangle (bounds, Radius::control);

        juce::DropShadow glow (Theme::accentRedBright.withAlpha (0.55f),
                               10,
                               { 0, 0 });
        glow.drawForPath (g, glowPath);
    }


    void drawTileShadow (juce::Graphics& g,
                         juce::Rectangle<float> bounds) const
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle (bounds.translated (0.0f,
                                                           (float) Shadow::tileOffsetY),
                                        Radius::control);

        juce::DropShadow drop (juce::Colours::black.withAlpha (Shadow::tileAlpha),
                               Shadow::tileBlur,
                               { 0, 0 });
        drop.drawForPath (g, shadowPath);
    }

    void drawPanelShadow (juce::Graphics& g,
                          juce::Rectangle<float> bounds) const
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle (bounds.translated (0.0f,
                                                           (float) Shadow::panelOffsetY),
                                        Radius::panel);

        juce::DropShadow drop (juce::Colours::black.withAlpha (Shadow::panelAlpha),
                               Shadow::panelBlur,
                               { 0, 0 });
        drop.drawForPath (g, shadowPath);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/,
                         bool /*shouldDrawButtonAsDown*/) override
    {
        const auto fontSizeVar = button.getProperties()["fontSize"];
        const float fontSize = fontSizeVar.isVoid() ? 15.0f : (float) fontSizeVar;

        juce::Font font (juce::FontOptions (fontSize, juce::Font::bold));

        g.setFont (font);

        // If the button is toggled ON, we force it to use the brightest 
        // text color (textPrimary) for maximum contrast against the red glow.
        // Otherwise, we use whatever Off color was assigned.
        const auto normalColour = button.getToggleState()
                                ? Theme::textPrimary
                                : button.findColour (juce::TextButton::textColourOffId);
                                
        const auto textColour = button.isEnabled()
            ? normalColour
            : normalColour.withMultipliedAlpha (0.4f);

        const int hPad = 4;
        const int vPad = 0;

        const auto textArea = button.getLocalBounds().reduced (hPad, vPad);

        // Subtle text shadow for a lifted feel. Only for enabled,
        // non-engaged buttons — for those, the text should sit crisply
        // on the background without extra depth.
        if (button.isEnabled() && ! button.getToggleState())
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawFittedText (button.getButtonText(),
                              textArea.translated (0, 1),
                              juce::Justification::centred,
                              1);
        }

        // Draw the actual text on top.
        g.setColour (textColour);
        g.drawText (button.getButtonText(),
            textArea,
            juce::Justification::centred,
            false);   // <-- false = don't shrink to fit
    }

    //==========================================================================
    // Depth helpers
    //
    // Consistent drawing routines for the three structural elements in
    // the UI: raised panels, recessed wells, and raised controls.
    //
    // Using these everywhere is what makes the depth model coherent. No
    // component should draw its own shadows or highlights — it should
    // call one of these with the appropriate bounds and fill colour.
    //==========================================================================

    /** Draws a raised panel: a shadow underneath, a filled rounded rect,
        and a 1 px highlight line along the top inside edge.

        Used for: filter bar, detail view, session strip, spectrum frame,
        slot tiles.
    */
    void drawPanel (juce::Graphics& g,
                    juce::Rectangle<float> bounds,
                    juce::Colour fill) const
    {
        // Fill only — the shadow is drawn by the parent.
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, Radius::panel);

        // Top highlight: a curved line that follows the panel's top edge
        // and corners, fading at both ends. Same technique as the buttons.
        {
            const auto r = Radius::panel;

            auto pathBounds = bounds.reduced (1.0f);
            const float pathR = juce::jmax (0.0f, r - 1.0f);

            juce::Path inner;
            inner.addRoundedRectangle (pathBounds, pathR);

            // Clip to a shallow band along the top.
            juce::Graphics::ScopedSaveState saveInner (g);

            juce::Rectangle<float> topBand (
                bounds.getX(),
                bounds.getY(),
                bounds.getWidth(),
                6.0f);

            juce::Path band;
            band.addRectangle (topBand);
            g.reduceClipRegion (band, {});

            // Convert the stroke to a filled path so it takes a gradient.
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
            fade.addColour (0.01, juce::Colours::white.withAlpha (0.40f));
            fade.addColour (0.50, juce::Colours::white.withAlpha (0.45f));
            fade.addColour (0.99, juce::Colours::white.withAlpha (0.40f));

            g.setGradientFill (fade);
            g.fillPath (outline);
        }

        // Outer dark border.
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), Radius::panel, 1.0f);
    }
     void drawWell (juce::Graphics& g,
                   juce::Rectangle<float> bounds,
                   juce::Colour fill) const
    {
        const float wellRadius = juce::jmax (Radius::well, 6.0f);

        // 1. Fill.
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, wellRadius);

               // Everything else is clipped to the well's interior.
        {
            juce::Graphics::ScopedSaveState save (g);

            juce::Path clip;
            clip.addRoundedRectangle (bounds, wellRadius);
            g.reduceClipRegion (clip, {});

            // Inner shadow, all around, with a slight vertical bias
            // toward the bottom so the well reads as "lit from above".
            //
            // Four gradient bands. The top is the deepest; the sides
            // and bottom are lighter so the shadow feels directional.
            const float topFade    = 14.0f;
            const float bottomFade = 12.0f;
            const float sideFade   = 10.0f;

            // Top inner shadow
            {
                juce::ColourGradient grad (
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getX(), bounds.getY(),
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getX(), bounds.getY() + topFade,
                    false);

                grad.addColour (0.15, juce::Colours::black.withAlpha (0.60f));
                grad.addColour (0.40, juce::Colours::black.withAlpha (0.35f));
                grad.addColour (0.70, juce::Colours::black.withAlpha (0.20f));

                g.setGradientFill (grad);
                g.fillRect (bounds.withHeight (topFade + 1.0f));
            }

            // Bottom inner shadow — lighter than the top, so the
            // shadow reads as coming from above.
            {
                juce::ColourGradient grad (
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getX(), bounds.getBottom(),
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getX(), bounds.getBottom() - bottomFade,
                    false);

                grad.addColour (0.3, juce::Colours::black.withAlpha (0.30f));
                grad.addColour (0.7, juce::Colours::black.withAlpha (0.08f));

                g.setGradientFill (grad);
                g.fillRect (juce::Rectangle<float> (
                    bounds.getX(),
                    bounds.getBottom() - bottomFade,
                    bounds.getWidth(),
                    bottomFade));
            }

            // Left inner shadow
            {
                juce::ColourGradient grad (
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getX(), bounds.getY(),
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getX() + sideFade, bounds.getY(),
                    false);

                grad.addColour (0.3, juce::Colours::black.withAlpha (0.25f));
                grad.addColour (0.7, juce::Colours::black.withAlpha (0.10f));

                g.setGradientFill (grad);
                g.fillRect (juce::Rectangle<float> (
                    bounds.getX(),
                    bounds.getY(),
                    sideFade,
                    bounds.getHeight()));
            }

            // Right inner shadow
            {
                juce::ColourGradient grad (
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getRight(), bounds.getY(),
                    juce::Colours::black.withAlpha (0.0f),
                    bounds.getRight() - sideFade, bounds.getY(),
                    false);

                grad.addColour (0.3, juce::Colours::black.withAlpha (0.25f));
                grad.addColour (0.7, juce::Colours::black.withAlpha (0.10f));

                g.setGradientFill (grad);
                g.fillRect (juce::Rectangle<float> (
                    bounds.getRight() - sideFade,
                    bounds.getY(),
                    sideFade,
                    bounds.getHeight()));
            }
        }

        // 3. Bottom highlight — 2 px curved line, faded at both ends.
        // Drawn OUTSIDE the interior clip so it can sit exactly on the
        // well's bottom edge.
        {
            juce::Graphics::ScopedSaveState save (g);

            // Clip to the bottom band only.
            juce::Rectangle<float> bottomBand (
                bounds.getX(),
                bounds.getBottom() - wellRadius - 2.0f,
                bounds.getWidth(),
                wellRadius + 2.0f);

            juce::Path band;
            band.addRectangle (bottomBand);
            g.reduceClipRegion (band, {});

            // Inner rounded path, one pixel inside the well.
            juce::Path inner;
            inner.addRoundedRectangle (bounds.reduced (0.5f),
                                       juce::jmax (0.0f, wellRadius - 1.0f));

            juce::Path outline;
            juce::PathStrokeType (0.5f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, inner);

            // Horizontal fade: transparent at the ends, bright in
            // the middle.
            juce::ColourGradient fade (
                juce::Colours::white.withAlpha (0.0f),
                bounds.getX(), 0.0f,
                juce::Colours::white.withAlpha (0.0f),
                bounds.getRight(), 0.0f,
                false);

            fade.addColour (0.03, juce::Colours::white.withAlpha (0.10f));
            fade.addColour (0.50, juce::Colours::white.withAlpha (0.05f));
            fade.addColour (0.97, juce::Colours::white.withAlpha (0.10f));

            g.setGradientFill (fade);
            g.fillPath (outline);
        }

        // 4. Gradient border — dark at the top, lighter at the bottom.
        // Drawn last so it defines the well's outer shape.
        {
            juce::Path borderPath;
            borderPath.addRoundedRectangle (bounds.reduced (1.0f),
                                            juce::jmax (0.0f, wellRadius - 1.0f));

            juce::Path outline;
            juce::PathStrokeType (2.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, borderPath);

            juce::ColourGradient borderGrad (
                juce::Colours::black.withAlpha (0.10f),
                bounds.getX(), bounds.getY(),
                juce::Colours::white.withAlpha (0.20f),
                bounds.getX(), bounds.getBottom(),
                false);

            borderGrad.addColour (0.0, juce::Colours::white.withAlpha (0.10f));

            g.setGradientFill (borderGrad);
            g.fillPath (outline);
        }
    }
    /** Draws a raised control: a soft drop shadow, a vertical gradient
        (brighter at the top, darker at the bottom), and a 1 px highlight
        along the top inside edge.

        Used for: buttons, tiles (when not selected), the DAW/Reference
        toggle halves.

        `isHighlighted` brightens the gradient slightly for hover states.
        `isDown` darkens it for pressed states.
    */
        void drawRaisedControl (juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            juce::Colour base,
                            bool isHighlighted = false,
                            bool isDown = false,
                            bool isGlowing = false) const
    {


         // Gradient. Active (glowing) buttons use a radial centre-bright
        // gradient; others use the standard top-bright / bottom-dark
        // gradient.
        if (isGlowing)
        {
            // Radial glow: bright red in the centre, darker red toward
            // the edges. Uses the button's centre as the gradient origin
            // and the diagonal as the radius so the edges are darkest.
            const auto centre = bounds.getCentre();
            const auto radius = juce::jmax (bounds.getWidth(),
                                            bounds.getHeight()) * 0.6f;

                       // A vivid saturated red at the centre — not a brighter, whiter
            // version of the base (which desaturates). The edges stay
            // darker so the button reads as glowing from within.
            const auto glowCentre = juce::Colour (0xffc84040);
            const auto glowEdge   = base.darker (0.30f);

            juce::ColourGradient glow (glowCentre,
                                       centre.x, centre.y,
                                       glowEdge,
                                       centre.x + radius, centre.y,
                                       true);   // radial
            g.setGradientFill (glow);
            g.fillRoundedRectangle (bounds, Radius::control);
        }
        else
        {
            juce::Colour top, bottom;

            if (isDown)
            {
                top    = base.darker (0.08f);
                bottom = base.darker (0.18f);
            }
            else if (isHighlighted)
            {
                top    = base.brighter (0.20f);
                bottom = base.darker (0.12f);
            }
            else
            {
                top    = base.brighter (0.14f);
                bottom = base.darker (0.18f);
            }

            juce::ColourGradient grad (top,
                                       bounds.getX(), bounds.getY(),
                                       bottom,
                                       bounds.getX(), bounds.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, Radius::control);
        }

        // Top highlight: a curved line that follows the button's top
        // edge and top corners, fading at both ends. Built as a stroked
        // path so it curves with the rounded shape and doesn't leave
        // artefacts at the corners.
        {
            const auto r = Radius::control;

            // Build an inner rounded path one pixel inside the button.
            auto pathBounds = bounds.reduced (1.0f);
            const float pathR = juce::jmax (0.0f, r - 1.0f);

            juce::Path inner;
            inner.addRoundedRectangle (pathBounds, pathR);

            // Clip to a shallow band along the top so only the top
            // edge and the start of the corner arcs are drawn.
            juce::Graphics::ScopedSaveState saveInner (g);

            juce::Rectangle<float> topBand (
                bounds.getX(),
                bounds.getY(),
                bounds.getWidth(),
                5.0f);

            juce::Path band;
            band.addRectangle (topBand);
            g.reduceClipRegion (band, {});

            // Convert the stroke into a filled path so it can take a
            // gradient (strokes only take solid colours).
            juce::Path outline;
            juce::PathStrokeType (1.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, inner);

            // Fade the highlight at both ends so it doesn't terminate
            // abruptly at the corners.
            juce::ColourGradient fade (
                juce::Colours::white.withAlpha (0.0f),
                bounds.getX(), 0.0f,
                juce::Colours::white.withAlpha (0.0f),
                bounds.getRight(), 0.0f,
                false);
            fade.addColour (0.05, juce::Colours::white.withAlpha (0.30f));
            fade.addColour (0.50, juce::Colours::white.withAlpha (0.45f));
            fade.addColour (0.95, juce::Colours::white.withAlpha (0.30f));

            g.setGradientFill (fade);
            g.fillPath (outline);
        }

                // Outer dark border, defining the control's edge.
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), Radius::control, 1.0f);
    }

    /** Draws a recessed control: darker fill with a strong inner shadow at
        the top and a faint inner highlight at the bottom, no drop shadow.

        Used for: disabled buttons, draggable value fields, anything that
        should read as physically set into the panel.
    */
      void drawRecessedControl (juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour fill) const
    {
        // Fill.
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, Radius::control);

        // Inner shadow / highlight.
        {
            juce::Graphics::ScopedSaveState save (g);

            juce::Path clip;
            clip.addRoundedRectangle (bounds, Radius::control);
            g.reduceClipRegion (clip, {});

            // Inner shadow: all around the well's interior, biased
            // toward the bottom. Built by stroking a downward-offset
            // rounded path with a wide, dark stroke.
            {
                                    {
                juce::Graphics::ScopedSaveState saveInner (g);

                juce::Path clip;
                clip.addRoundedRectangle (bounds, Radius::control);
                g.reduceClipRegion (clip, {});

                // Fade distances, in pixels. Bigger = softer shadow.
                constexpr float topFade    = 9.0f;
                constexpr float bottomFade = 6.0f;
                constexpr float sideFade   = 6.0f;

                // Alphas. Higher = darker shadow.
                constexpr float topAlpha    = 0.55f;
                constexpr float bottomAlpha = 0.30f;
                constexpr float sideAlpha   = 0.30f;

                // Top
                {
                    juce::ColourGradient grad (
                        juce::Colours::black.withAlpha (topAlpha),
                        bounds.getX(), bounds.getY(),
                        juce::Colours::black.withAlpha (0.0f),
                        bounds.getX(), bounds.getY() + topFade,
                        false);

                    g.setGradientFill (grad);
                    g.fillRect (bounds.withHeight (topFade + 1.0f));
                }

                // Bottom
                {
                    juce::ColourGradient grad (
                        juce::Colours::black.withAlpha (bottomAlpha),
                        bounds.getX(), bounds.getBottom(),
                        juce::Colours::black.withAlpha (0.0f),
                        bounds.getX(), bounds.getBottom() - bottomFade,
                        false);

                    g.setGradientFill (grad);
                    g.fillRect (juce::Rectangle<float> (
                        bounds.getX(),
                        bounds.getBottom() - bottomFade,
                        bounds.getWidth(),
                        bottomFade));
                }

                // Left
                {
                    juce::ColourGradient grad (
                        juce::Colours::black.withAlpha (sideAlpha),
                        bounds.getX(), bounds.getY(),
                        juce::Colours::black.withAlpha (0.0f),
                        bounds.getX() + sideFade, bounds.getY(),
                        false);

                    g.setGradientFill (grad);
                    g.fillRect (juce::Rectangle<float> (
                        bounds.getX(),
                        bounds.getY(),
                        sideFade,
                        bounds.getHeight()));
                }

                // Right
                {
                    juce::ColourGradient grad (
                        juce::Colours::black.withAlpha (sideAlpha),
                        bounds.getRight(), bounds.getY(),
                        juce::Colours::black.withAlpha (0.0f),
                        bounds.getRight() - sideFade, bounds.getY(),
                        false);

                    g.setGradientFill (grad);
                    g.fillRect (juce::Rectangle<float> (
                        bounds.getRight() - sideFade,
                        bounds.getY(),
                        sideFade,
                        bounds.getHeight()));
                }
            
            }
            }

                      // Bottom inner highlight: a soft bright line along the inside
            // of the bottom edge that fades out toward the two ends, so
            // it reads as light catching the middle of the lower lip
            // rather than a hard line.
            {
                const auto r = Radius::control;

                // Stroke a path that sits one pixel inside the box.
                auto pathBounds = bounds.reduced (0.5f);
                const float pathR = juce::jmax (0.0f, r - 1.5f);

                juce::Path inner;
                inner.addRoundedRectangle (pathBounds, pathR);

                // Restrict drawing to a band along the bottom.
                juce::Graphics::ScopedSaveState saveInner (g);

                 juce::Rectangle<float> bottomBand (
                    bounds.getX(),
                    bounds.getBottom() - 4.0f,
                    bounds.getWidth(),
                    4.0f);

                juce::Path band;
                band.addRectangle (bottomBand);
                g.reduceClipRegion (band, {});

                // Fade the stroke at both ends with a horizontal gradient
                // used as the stroke colour. Bright in the middle,
                // transparent at 0 and 100 % across the width.
                juce::ColourGradient fade (
                    juce::Colours::white.withAlpha (0.0f),
                    bounds.getX(), 0.0f,
                    juce::Colours::white.withAlpha (0.0f),
                    bounds.getRight(), 0.0f,
                    false);

                // INCREASED ALPHA here for a brighter lip
                fade.addColour (0.02, juce::Colours::white.withAlpha (0.30f));
                fade.addColour (0.50, juce::Colours::white.withAlpha (0.30f));
                fade.addColour (0.98, juce::Colours::white.withAlpha (0.30f));

                // Fill the stroked path by expanding it into a filled
                // shape, then fill with the gradient.
                juce::Path outline;
                // INCREASED THICKNESS to 1.5f so it doesn't get lost
                // under the outer dark border drawn below.
                juce::PathStrokeType (1.5f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::butt)
                    .createStrokedPath (outline, inner);

                g.setGradientFill (fade);
                g.fillPath (outline);
            }
        }

         // Outer border: dark at the top, fading to lighter at the
        // bottom. The corner curves at the bottom therefore show the
        // light accent rather than a dark outline.
        {
            // Build the stroked outline of the well's border.
            juce::Path borderPath;
            borderPath.addRoundedRectangle (bounds.reduced (0.3f),
                                            juce::jmax (0.3f, Radius::control - 0.5f));

            juce::Path outline;
            juce::PathStrokeType (1.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::butt)
                .createStrokedPath (outline, borderPath);

            // Vertical gradient: dark at the top, transparent in the
            // middle, lighter at the bottom.
            juce::ColourGradient borderGrad (
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX(), bounds.getY(),
                juce::Colours::white.withAlpha (0.12f),
                bounds.getX(), bounds.getBottom(),
                false);

            borderGrad.addColour (0.1, juce::Colours::black.withAlpha (0.00f));

            g.setGradientFill (borderGrad);
            g.fillPath (outline);
        }

        

      }  

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float /*minSliderPos*/,
                           float /*maxSliderPos*/,
                           juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearHorizontal)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                    sliderPos,
                                                    0.0f, 0.0f,
                                                    style, slider);
            return;
        }

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                                    (float) width, (float) height)
                                .reduced (0.5f, 3.0f);

                const float cornerRadius = Radius::control;

        const float minPos = bounds.getX();
        const float maxPos = bounds.getRight();
        const float fillRight = juce::jlimit (minPos, maxPos, sliderPos);

        // --- 1. Recessed well background ----------------------------------
        //
        // Same visual treatment as the crossover value boxes: dark fill
        // with a soft inner shadow at the top and a faint inner highlight
        // at the bottom. Reads as a well cut into the panel.
        {
            // Base fill — matches the DraggableValueBox.
            const auto fill = Theme::surfaceRaised.darker (0.35f);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, cornerRadius);

            // Top inner shadow and bottom inner highlight, clipped to the
            // rounded shape.
            juce::Graphics::ScopedSaveState save (g);

            juce::Path clip;
            clip.addRoundedRectangle (bounds, cornerRadius);
            g.reduceClipRegion (clip, {});

            // Top inner shadow — multi-stop gradient, same as
            // drawRecessedControl.
            const float shadowHeight = 8.0f;

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

           
        }

        // --- 2. Fill: horizontal gradient across the FULL slider width,
        //       then clipped to the filled region. This means the colour
        //       at any x position is fixed regardless of the current
        //       value — moving the value right simply reveals more of
        //       the gradient, exactly like a meter. -----------------
         if (fillRight > minPos + 0.5f)
        {
            juce::Graphics::ScopedSaveState save (g);

            // Inset the red fill by 1.5 pixels on all sides.
            // This is aggressive enough to fully contain it inside the well's border.
            auto fillBounds = bounds.reduced (1.5f, 1.5f);
            
            // Recalculate the width relative to the inset bounds
            float fillWidth = fillRight - fillBounds.getX();
            if (fillWidth < 1.0f)
                fillWidth = 1.0f;

            // Use a slightly smaller radius so the fill's curves nest perfectly inside the well's curves.
            const float innerRadius = juce::jmax (0.0f, cornerRadius - 1.5f);

            juce::Path fillClip;
            // Force the fill to be fully rounded on ALL four corners at all times.
            // This means it never has a flat edge that could bleed outside the well.
            fillClip.addRoundedRectangle (fillBounds.getX(),
                                          fillBounds.getY(),
                                          fillWidth,
                                          fillBounds.getHeight(),
                                          innerRadius,
                                          innerRadius,
                                          true,  // round top-left
                                          true,  // round top-right
                                          true,  // round bottom-left
                                          true); // round bottom-right
            g.reduceClipRegion (fillClip, {});

            // Read the animated hover progress published by GainSlider.
            const float hoverAlpha = juce::jlimit (0.0f, 1.0f,
                (float) slider.getProperties().getWithDefault ("hoverAlpha", 0.0f));

            // Idle: darker red. Hover: brighter red, like an engaged
            // button whose glow intensifies.
            const auto idleRed    = juce::Colour (0xff8a3535);   // accentRed
            const auto hoverRed   = juce::Colour (0xffa03838);   // brighter red

            const auto baseRed = idleRed.interpolatedWith (hoverRed, hoverAlpha);

            // Fill the region with a red gradient. Slightly brighter at
            // the top edge, darker at the bottom, matching the button
            // treatment.
            const auto topRed    = baseRed.brighter (0.15f);
            const auto bottomRed = baseRed.darker (0.20f);

            juce::ColourGradient grad (topRed,
                                       bounds.getX(), bounds.getY(),
                                       bottomRed,
                                       bounds.getX(), bounds.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillRect (bounds);

            // Subtle inner glow at the top edge of the fill, brighter
            // the more the slider is hovered.
            if (hoverAlpha > 0.0f)
            {
                const float glowAlpha = hoverAlpha * 0.35f;

                juce::ColourGradient glow (
                    juce::Colours::white.withAlpha (glowAlpha),
                    bounds.getX(), bounds.getY(),
                    juce::Colours::white.withAlpha (0.0f),
                    bounds.getX(), bounds.getY() + 4.0f,
                    false);
                g.setGradientFill (glow);
                g.fillRect (bounds.withHeight (5.0f));
            }
        }

        // --- 3. Divider at the current value -----------------------------
        {
            g.setColour (Theme::background.withAlpha (0.7f));
            g.drawLine (fillRight, bounds.getY() + 2.0f,
                        fillRight, bounds.getBottom() - 2.0f,
                        1.0f);
        }

         // --- 4. Border ---------------------------------------------------
        {
            // Dark top half — drawn all around.
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), cornerRadius, 1.0f);

            // Bottom highlight — curved, following the rounded bottom
            // edge, faded at both ends. Drawn after the fill so it
            // appears on both the red fill and the empty well, making
            // the fill read as sitting inside the well.
            {
                juce::Graphics::ScopedSaveState save (g);

                juce::Rectangle<float> bottomBand (
                    bounds.getX(),
                    bounds.getBottom() - cornerRadius - 0.0f,
                    bounds.getWidth(),
                    cornerRadius + 0.0f);

                juce::Path band;
                band.addRectangle (bottomBand);
                g.reduceClipRegion (band, {});

                juce::Path inner;
                inner.addRoundedRectangle (bounds.reduced (1.0f),
                                           juce::jmax (0.0f, cornerRadius - 1.0f));

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
                fade.addColour (0.03, juce::Colours::white.withAlpha (0.40f));
                fade.addColour (0.50, juce::Colours::white.withAlpha (0.50f));
                fade.addColour (0.97, juce::Colours::white.withAlpha (0.40f));

                g.setGradientFill (fade);
                g.fillPath (outline);
            }
        }

                // Publish the well's bounds so the parent can draw a glow around
        // the fill using the same geometry.
        slider.getProperties().set ("wellX", bounds.getX());
        slider.getProperties().set ("wellY", bounds.getY());
        slider.getProperties().set ("wellW", bounds.getWidth());
        slider.getProperties().set ("wellH", bounds.getHeight());
        slider.getProperties().set ("fillRightX", fillRight);
    }
};