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

    void paint (juce::Graphics&) override;
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

    // Expand toggle. Fires when the user clicks the expand button.
    // The editor is responsible for actually resizing/hiding layout;
    // this component only reports the intent and displays the state.
    std::function<void (bool)> onExpandToggled;

    // Used by the editor to sync the button state if it changes expanded
    // mode for any reason other than the button (e.g. state restore).
    void setExpanded (bool shouldBeExpanded);
    bool isExpanded() const noexcept { return expanded; }

private:

    enum class LegendEntry { none, daw, ref };

    LegendEntry legendEntryAt (juce::Point<int> position) const;

    juce::Rectangle<int> getDawLegendBounds() const;
    juce::Rectangle<int> getRefLegendBounds() const;

    bool dawVisible = true;
    bool refVisible = true;
    LegendEntry hoveredLegend = LegendEntry::none;

    
    enum class DragTarget { none, lowXover, highXover };

    void timerCallback() override;
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area) const;
       void drawCurve (juce::Graphics& g,
                    juce::Rectangle<float> area,
                    const SpectrumAnalyzer& analyzer,
                    juce::Colour colour,
                    bool slow,
                    bool strokeOnly = false) const;

    void drawDeltaFill (juce::Graphics& g,
                        juce::Rectangle<float> area) const;

    void drawLegend (juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawFilterDimming (juce::Graphics& g, juce::Rectangle<float> area) const;

    // Hover tooltip: frequency + DAW and Ref peak dB at the cursor.
    void drawHoverTooltip (juce::Graphics& g) const;

    float xForCrossoverHz (juce::Rectangle<float> plot, float hz) const;
    DragTarget crossoverAtX (float x, juce::Rectangle<float> plot) const;

    void setCursorForHover (DragTarget target);
    void writeCrossover (DragTarget target, float hz, bool sendGesture);

    juce::Rectangle<float> getPlotBounds() const;
    juce::Rectangle<int>   getLegendBounds() const;

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

    // Hover state for the tooltip.
    bool               hoverInPlot = false;
    juce::Point<float> hoverPos;

    static constexpr float grabRadiusPx = 8.0f;
    static constexpr float minRatio = 2.0f;

    // Display-mode buttons.
    juce::TextButton deltaButton;
    juce::TextButton expandButton;

    bool deltaMode = false;
    bool expanded  = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};