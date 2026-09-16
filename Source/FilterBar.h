#pragma once

#include "DraggableValueBox.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class FilterBar : public juce::Component,
                  public juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterBar (juce::AudioProcessorValueTreeState& apvts);
    ~FilterBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;

    juce::Label titleLabel;
    juce::TextButton lowButton  { "Low"  };
    juce::TextButton midButton  { "Mid"  };
    juce::TextButton highButton { "High" };

    DraggableValueBox lowXoverBox;
    DraggableValueBox highXoverBox;

    juce::TextButton monoButton { "Mono" };
    juce::TextButton swapButton { "Swap L/R" };

    juce::AudioProcessorValueTreeState::ButtonAttachment monoAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment swapAttachment;

    int currentSolo = 0;

    void updateButtonStates();
    // Toggle: if `choice` is already active, switch to Off (0). Otherwise
    // activate `choice`.
    void toggleSoloChoice (int choice);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterBar)
};