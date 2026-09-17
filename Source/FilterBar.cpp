#include "FilterBar.h"
#include "Theme.h"

FilterBar::FilterBar (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts),
      lowXoverBox  (apvts, "filter_low_xover",  "Low/Mid",  "Hz", 250.0),
      highXoverBox (apvts, "filter_high_xover", "Mid/High", "Hz", 4000.0),
      monoAttachment (apvts, "mono",    monoButton),
      swapAttachment (apvts, "swap_lr", swapButton)
{
    titleLabel.setText ("Filter", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    addAndMakeVisible (titleLabel);

    auto setupSoloButton = [this] (juce::TextButton& b, int choice)
    {
        b.setClickingTogglesState (false);
        b.onClick = [this, choice] { toggleSoloChoice (choice); };
        addAndMakeVisible (b);
    };

    setupSoloButton (lowButton,  1);
    setupSoloButton (midButton,  2);
    setupSoloButton (highButton, 3);

    addAndMakeVisible (lowXoverBox);
    addAndMakeVisible (highXoverBox);

    auto setupUtilityButton = [this] (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        addAndMakeVisible (b);
    };

    setupUtilityButton (monoButton);
    setupUtilityButton (swapButton);

    apvtsRef.addParameterListener ("filter_solo", this);

    if (auto* p = apvtsRef.getParameter ("filter_solo"))
        currentSolo = juce::roundToInt (p->convertFrom0to1 (p->getValue()));

    updateButtonStates();
}

FilterBar::~FilterBar()
{
    apvtsRef.removeParameterListener ("filter_solo", this);
}

void FilterBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Theme::borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);
}

void FilterBar::resized()
{
    auto bounds = getLocalBounds().reduced (10, 6);

    titleLabel.setBounds (bounds.removeFromLeft (44));
    bounds.removeFromLeft (6);

    const int buttonWidth = 52;
    const int buttonGap   = 4;

    lowButton .setBounds (bounds.removeFromLeft (buttonWidth));
    bounds.removeFromLeft (buttonGap);
    midButton .setBounds (bounds.removeFromLeft (buttonWidth));
    bounds.removeFromLeft (buttonGap);
    highButton.setBounds (bounds.removeFromLeft (buttonWidth));

    bounds.removeFromLeft (16);

    const int boxWidth = 140;
    const int boxGap   = 8;

    lowXoverBox .setBounds (bounds.removeFromLeft (boxWidth));
    bounds.removeFromLeft (boxGap);
    highXoverBox.setBounds (bounds.removeFromLeft (boxWidth));

    const int utilWidth = 70;
    const int utilGap   = 6;

    auto rightArea = bounds.removeFromRight (utilWidth * 2 + utilGap);
    swapButton.setBounds (rightArea.removeFromRight (utilWidth));
    rightArea.removeFromRight (utilGap);
    monoButton.setBounds (rightArea.removeFromRight (utilWidth));
}

void FilterBar::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "filter_solo")
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<FilterBar> (this)]
            {
                if (safe == nullptr)
                    return;

                if (auto* p = safe->apvtsRef.getParameter ("filter_solo"))
                    safe->currentSolo = juce::roundToInt (
                        p->convertFrom0to1 (p->getValue()));

                safe->updateButtonStates();
            });
    }
}

void FilterBar::updateButtonStates()
{
    lowButton .setToggleState (currentSolo == 1, juce::dontSendNotification);
    midButton .setToggleState (currentSolo == 2, juce::dontSendNotification);
    highButton.setToggleState (currentSolo == 3, juce::dontSendNotification);
}

void FilterBar::toggleSoloChoice (int choice)
{
    const int newValue = (currentSolo == choice) ? 0 : choice;

    if (auto* p = apvtsRef.getParameter ("filter_solo"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) newValue));
        p->endChangeGesture();
    }
}