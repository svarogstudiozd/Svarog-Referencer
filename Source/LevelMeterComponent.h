#pragma once

#include "LevelMeterSource.h"
#include <juce_gui_basics/juce_gui_basics.h>

class LevelMeterComponent : public juce::Component,
                            private juce::Timer
{
public:
    LevelMeterComponent();
    ~LevelMeterComponent() override;

    void paint (juce::Graphics& g) override;

    void setSource (LevelMeterSource* newSource) { source = newSource; }

private:
    void timerCallback() override;

    LevelMeterSource* source = nullptr;

    // Ballistics (per channel)
    float smoothedRmsDb[LevelMeterSource::numChannels]  { -100.0f, -100.0f };
    float peakHoldDb[LevelMeterSource::numChannels]     { -100.0f, -100.0f };
    int   peakHoldCountdown[LevelMeterSource::numChannels] { 0, 0 };

    // Held peak value shown in the numeric readout (max across channels).
    float heldPeakDb = -100.0f;
    int   heldPeakCountdown = 0;

    static constexpr float minDb = -60.0f;
    static constexpr float maxDb =   6.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeterComponent)
};