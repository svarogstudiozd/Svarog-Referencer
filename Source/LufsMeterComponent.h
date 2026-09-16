#pragma once

#include "LufsMeter.h"
#include <juce_gui_basics/juce_gui_basics.h>

/**
    Vertical loudness meter with a numeric readout above the bar and a
    cycling mode button below it (LUFS-S -> LUFS-M -> LUFS-I -> LUFS-S).
*/
class LufsMeterComponent : public juce::Component,
                           private juce::Timer
{
public:
    LufsMeterComponent();
    ~LufsMeterComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSource (LufsMeter* newSource) { source = newSource; }

private:
    enum class Mode { shortTerm, momentary, integrated };

    void timerCallback() override;
    void cycleMode();
    float currentLufs() const;
    juce::String currentLabel() const;

    LufsMeter* source = nullptr;

    Mode mode = Mode::shortTerm;
    juce::TextButton modeButton;

    // Smoothed displayed value (attack/release ballistics for the bar).
    float displayedLufs = -70.0f;

    static constexpr float minLufs = -40.0f;
    static constexpr float maxLufs =   0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LufsMeterComponent)
};