#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
    A compact numeric field you change by dragging up/down.

    Backed by an AudioProcessorValueTreeState parameter. Displays a label
    (e.g. "Low/Mid") on the left and the value + suffix on the right.
    Double-click resets to a provided default.

    Optionally clamped to a range that is narrower than the parameter's
    own range. Used so that the two filter crossovers can never cross
    each other. The clamps are supplied as absolute values (in the same
    units as the parameter) and can be updated at runtime — the box
    re-clamps its displayed value if it is outside.
*/
class DraggableValueBox : public juce::Component,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    DraggableValueBox (juce::AudioProcessorValueTreeState& apvts,
                       const juce::String& parameterID,
                       const juce::String& prefixLabel,
                       const juce::String& valueSuffix,
                       double resetValue);

    ~DraggableValueBox() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    void setValueFromParameter();

    // Absolute-value clamps applied on top of the parameter's own range.
    // `minVal > maxVal` disables clamping on that side.
    void setClampRange (double minVal, double maxVal);

    juce::String getParameterID() const { return parameterID; }
    double getCurrentValue() const noexcept { return currentValue; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String parameterID;
    juce::RangedAudioParameter* param = nullptr;
    juce::NormalisableRange<float> range;

    juce::String prefix;
    juce::String suffix;
    double resetToValue = 0.0;

    double currentValue = 0.0;
    double clampMin = 0.0;
    double clampMax = 0.0;
    bool   hasClamp = false;

    bool hovered = false;

    // Drag state
    float dragStartValue = 0.0f;
    int   dragStartY = 0;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::String formatDisplay() const;
    void applyFromNormalized (float normalized);
    double clampedValue (double value) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DraggableValueBox)
};