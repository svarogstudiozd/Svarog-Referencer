#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

/**
    A compact drag-to-change numeric box for the follow-transport offset.

    Displays the value as a signed seconds string with millisecond
    resolution, e.g. "+2.043 s" or "-0.150 s".

    Drag behaviour:
        - normal drag       : ~200 px for the full range
        - shift held        : 10x finer
        - cmd/ctrl held     : 100x finer

    Double-click resets to 0.
*/
class OffsetBox : public juce::Component
{
public:
    OffsetBox (juce::AudioProcessorValueTreeState& apvts,
               const juce::String& parameterID);

    ~OffsetBox() override = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit  (const juce::MouseEvent& e) override;

    void setValueFromParameter();

private:
    juce::String formatDisplay() const;
    void applyFromNormalized (float normalized);

    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter* param = nullptr;
    juce::NormalisableRange<float> range;

    float currentValue = 0.0f;
    bool  hovered = false;

    // Drag state
    float dragStartValue = 0.0f;
    int   dragStartY = 0;
    float dragScale = 1.0f;   // set on mouseDown from modifier keys

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OffsetBox)
};