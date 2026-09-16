#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <functional>

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
        constexpr float cornerRadius = 6.0f;

        const int midX = getLocalBounds().getCentreX();

        auto leftBounds  = bounds.withRight ((float) midX);
        auto rightBounds = bounds.withLeft ((float) midX);

        // Hover brightening applies to both halves for a unified response.
        const bool hovered = shouldDrawButtonAsHighlighted;

        auto drawHalf = [&] (juce::Rectangle<float> half, bool isActive)
        {
            const juce::Colour base = isActive ? Theme::accentRed : Theme::panel;

            juce::Colour top, bottom;

            if (hovered)
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
                                       half.getX(), half.getY(),
                                       bottom,
                                       half.getX(), half.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillRect (half);
        };

        {
            juce::Path clip;
            clip.addRoundedRectangle (bounds, cornerRadius);
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (clip, {});

            drawHalf (leftBounds,  activeIndex == 0);
            drawHalf (rightBounds, activeIndex == 1);
        }

        // Divider between halves.
        {
            const float midF = (float) midX;
            g.setColour (Theme::background.withAlpha (0.7f));
            g.drawLine (midF, bounds.getY() + 2.0f,
                        midF, bounds.getBottom() - 2.0f,
                        1.0f);
        }

        // Outer border.
        {
            g.setColour (Theme::panel.brighter (0.15f));
            g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
        }

        // Labels.
        {
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));

            auto drawLabel = [&] (juce::Rectangle<float> half,
                                  const juce::String& label,
                                  bool isActive)
            {
                g.setColour (isActive ? Theme::greyLight : Theme::textPrimary);

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