#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    A juce::Slider subclass that animates a hover progress value from 0
    to 1 over ~150 ms on mouse enter / exit.

    The value is exposed to the LookAndFeel via the slider's property
    map, under the key "hoverAlpha". The look and feel reads it during
    drawLinearSlider to interpolate the fill's idle and hover colours.

    All other Slider behaviour is untouched.
*/
class GainSlider : public juce::Slider,
                   private juce::Timer
{
public:
    GainSlider()
    {
        // Publish the initial state so the LookAndFeel can read it.
        getProperties().set ("hoverAlpha", 0.0f);

        startTimerHz (60);
    }

    ~GainSlider() override
    {
        stopTimer();
    }

    void mouseEnter (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseEnter (e);
        target = 1.0f;
    }

    void mouseExit (const juce::MouseEvent& e) override
    {
        juce::Slider::mouseExit (e);
        target = 0.0f;
    }

private:
    private:
    void timerCallback() override
    {
        // If we're already at the target, do nothing — the animation
        // is settled and the timer will idle until the target changes.
        if (std::abs (current - target) < 0.0001f)
            return;

        // Step toward the target.
        constexpr float step = 1.0f / (0.250f * 60.0f);

        if (current < target)
            current = juce::jmin (target, current + step);
        else
            current = juce::jmax (target, current - step);

        // When we're within one step of the target, snap exactly so
        // there's no residual alpha hanging below 0.001.
        if (std::abs (current - target) < step)
            current = target;

        getProperties().set ("hoverAlpha", current);
        repaint();
    }
    float current = 0.0f;
    float target  = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GainSlider)
};