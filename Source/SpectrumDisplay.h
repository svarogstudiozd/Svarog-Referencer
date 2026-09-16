#pragma once

#include "SpectrumAnalyzer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class SpectrumDisplay : public juce::Component,
                        private juce::Timer
{
public:
    SpectrumDisplay();
    ~SpectrumDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setAnalyzers (SpectrumAnalyzer* dawAnalyzer,
                       SpectrumAnalyzer* refAnalyzer);

    // Connect to the APVTS so we can read the filter state.
    void setApvts (juce::AudioProcessorValueTreeState* apvts);

private:
    void timerCallback() override;
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawCurve (juce::Graphics& g,
                    juce::Rectangle<float> area,
                    const SpectrumAnalyzer& analyzer,
                    juce::Colour colour,
                    bool slow) const;
    void drawLegend (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawFilterDimming (juce::Graphics& g, juce::Rectangle<float> area) const;

    SpectrumAnalyzer* daw = nullptr;
    SpectrumAnalyzer* ref = nullptr;

    juce::AudioProcessorValueTreeState* apvtsRef = nullptr;

    // Cached filter state, refreshed each frame.
    int filterSoloIndex = 0;
    float filterLowHz  = 250.0f;
    float filterHighHz = 4000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};