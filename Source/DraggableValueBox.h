#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
    A compact numeric field you change by dragging up/down.

    Backed by an AudioProcessorValueTreeState parameter. Displays a label
    (e.g. "Low/Mid") on the left and the value + suffix on the right.
    Double-click resets to a provided default.
*/
class DraggableValueBox : public juce::Component
{
public:
    DraggableValueBox (juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& parameterID,
                       const juce::String& prefixLabel,
                       const juce::String& valueSuffix,
                       double resetValue);

    ~DraggableValueBox() override = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    void setValueFromParameter();

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* param = nullptr;
    juce::NormalisableRange<float> range;

    juce::String prefix;
    juce::String suffix;
    double resetToValue = 0.0;

    float currentValue = 0.0f;
    bool   hovered = false;

    // Drag state
    float dragStartValue = 0.0f;
    int   dragStartY = 0;

    juce::String formatDisplay() const;
    void applyFromNormalized (float normalized);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggableValueBox)
};