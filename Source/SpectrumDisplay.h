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

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    void setAnalyzers (SpectrumAnalyzer* dawAnalyzer,
                       SpectrumAnalyzer* refAnalyzer);

    // Connect to the APVTS so we can read the filter state, and write
    // the crossover frequencies when the user drags them.
    void setApvts (juce::AudioProcessorValueTreeState* apvts);

private:
    enum class DragTarget { none, lowXover, highXover };

    void timerCallback() override;
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawCurve (juce::Graphics& g,
                    juce::Rectangle<float> area,
                    const SpectrumAnalyzer& analyzer,
                    juce::Colour colour,
                    bool slow) const;
    void drawLegend (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawFilterDimming (juce::Graphics& g, juce::Rectangle<float> area) const;

    // The x-coordinate of a crossover line, in the plot rectangle's
    // coordinate space.
    float xForCrossoverHz (juce::Rectangle<float> plot, float hz) const;

    // Which line, if any, is under the given x position.
     DragTarget crossoverAtX (float x, juce::Rectangle<float> plot) const;

    void setCursorForHover (DragTarget target);

    // Push the crossover value into the APVTS parameter.
    void writeCrossover (DragTarget target, float hz, bool sendGesture);

    SpectrumAnalyzer* daw = nullptr;
    SpectrumAnalyzer* ref = nullptr;

    juce::AudioProcessorValueTreeState* apvtsRef = nullptr;

    // Cached filter state, refreshed each frame.
    int   filterSoloIndex = 0;
    float filterLowHz     = 250.0f;
    float filterHighHz    = 4000.0f;

    // Drag state.
    DragTarget dragging = DragTarget::none;
    DragTarget hovered  = DragTarget::none;

    // Grab radius for a crossover line, in pixels.
    static constexpr float grabRadiusPx = 8.0f;

    // Minimum ratio between crossovers (matches FilterProcessor and
    // FilterBar).
    static constexpr float minRatio = 2.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};