#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>
#include "ReferenceMaxLookAndFeel.h"

/**
    A two-segment toggle button.

    Displays two labels side-by-side. The currently-active half is filled
    with the accent red gradient; the inactive half is filled with the
    panel grey.

    Clicking anywhere on the button flips the active half to the other
    one. This means clicking the currently-active half will switch to the
    opposite source (not a no-op).

    When hovered, both halves brighten slightly to give a unified hover
    response.
*/
class ModeToggleButton : public juce::Button
{
public:
    ModeToggleButton()
        : juce::Button ("ModeToggle")
    {
        setClickingTogglesState (false);
    }

    void setLabels (const juce::String& leftLabel,
                    const juce::String& rightLabel)
    {
        leftText  = leftLabel;
        rightText = rightLabel;
        repaint();
    }

    // 0 = left (DAW), 1 = right (Reference).
    void setActiveIndex (int index)
    {
        index = juce::jlimit (0, 1, index);

        if (index != activeIndex)
        {
            activeIndex = index;
            repaint();
        }
    }

    int getActiveIndex() const noexcept { return activeIndex; }

    // Fires when the user clicks and the active half flips.
    std::function<void(int)> onSelectionChanged;

       void paintButton (juce::Graphics& g,
                      bool shouldDrawButtonAsHighlighted,
                      bool /*shouldDrawButtonAsDown*/) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        constexpr float cornerRadius = Radius::control;

        const int midX = getLocalBounds().getCentreX();

        auto leftBounds  = bounds.withRight ((float) midX);
        auto rightBounds = bounds.withLeft ((float) midX);

        const bool hovered = shouldDrawButtonAsHighlighted;

        // -----------------------------------------------------------------
        // Fill each half with the same gradient used by the raised buttons.
        // -----------------------------------------------------------------
               auto drawHalf = [&] (juce::Rectangle<float> half, bool isActive)
        {
            const juce::Colour base = isActive ? Theme::accentRed
                                               : Theme::surfaceCard;

            if (isActive)
            {
                // Radial glow for the active half.
                const auto centre = half.getCentre();
                const auto radius = juce::jmax (half.getWidth(),
                                                half.getHeight()) * 0.55f;

                const auto glowCentre = juce::Colour (0xffc84040);
                const auto glowEdge   = base.darker (0.30f);

                juce::ColourGradient glow (glowCentre,
                                           centre.x, centre.y,
                                           glowEdge,
                                           centre.x + radius, centre.y,
                                           true);
                g.setGradientFill (glow);
                g.fillRect (half);
            }
            else
            {
                // Standard vertical gradient for the inactive half.
                juce::Colour top, bottom;

                if (hovered)
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
                                           half.getX(), half.getY(),
                                           bottom,
                                           half.getX(), half.getBottom(),
                                           false);
                g.setGradientFill (grad);
                g.fillRect (half);
            }
        };

        {
            juce::Path clip;
            clip.addRoundedRectangle (bounds, cornerRadius);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (clip, {});

            drawHalf (leftBounds,  activeIndex == 0);
            drawHalf (rightBounds, activeIndex == 1);
        }

        // -----------------------------------------------------------------
        // Divider between halves.
        // -----------------------------------------------------------------
        {
            const float midF = (float) midX;
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine (midF, bounds.getY() + 2.0f,
                        midF, bounds.getBottom() - 2.0f,
                        1.0f);
        }

        // -----------------------------------------------------------------
        // Top highlight: curved line following the top edge of the whole
        // button (not each half separately), faded at both ends — same
        // treatment as the other raised controls.
        // -----------------------------------------------------------------
        {
            const auto r = cornerRadius;

            auto pathBounds = bounds.reduced (1.0f);
            const float pathR = juce::jmax (0.0f, r - 1.0f);

            juce::Path inner;
            inner.addRoundedRectangle (pathBounds, pathR);

            juce::Graphics::ScopedSaveState saveInner (g);

            juce::Rectangle<float> topBand (
                bounds.getX(),
                bounds.getY(),
                bounds.getWidth(),
                5.0f);

            juce::Path band;
            band.addRectangle (topBand);
            g.reduceClipRegion (band, {});

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
            fade.addColour (0.05, juce::Colours::white.withAlpha (0.30f));
            fade.addColour (0.50, juce::Colours::white.withAlpha (0.45f));
            fade.addColour (0.95, juce::Colours::white.withAlpha (0.30f));

            g.setGradientFill (fade);
            g.fillPath (outline);
        }

        // -----------------------------------------------------------------
        // Outer dark border, defining the control's edge.
        // -----------------------------------------------------------------
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

        // -----------------------------------------------------------------
        // Labels.
        // -----------------------------------------------------------------
        {
            g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (15.0f)));

            auto drawLabel = [&] (juce::Rectangle<float> half,
                                  const juce::String& label,
                                  bool isActive)
            {
                g.setColour (isActive ? Theme::textPrimary : Theme::greyLight);

                g.drawFittedText (label,
                                  half.toNearestInt().reduced (4, 2),
                                  juce::Justification::centred,
                                  1);
            };

            drawLabel (leftBounds,  leftText,  activeIndex == 0);
            drawLabel (rightBounds, rightText, activeIndex == 1);
        }
    }

    void clicked (const juce::ModifierKeys& /*mods*/) override
    {
        // Any click anywhere on the button flips to the other source.
        activeIndex = 1 - activeIndex;
        repaint();

        if (onSelectionChanged != nullptr)
            onSelectionChanged (activeIndex);
    }

private:
    juce::String leftText  { "DAW" };
    juce::String rightText { "Reference" };
    int activeIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModeToggleButton)
};