#include "FilterBar.h"
#include "Theme.h"
#include "ReferenceMaxLookAndFeel.h"

FilterBar::FilterBar (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts),
      lowXoverBox  (apvts, "filter_low_xover",  "Low/Mid",  "Hz", 250.0),
      highXoverBox (apvts, "filter_high_xover", "Mid/High", "Hz", 4000.0),
      monoAttachment (apvts, "mono",    monoButton),
      swapAttachment (apvts, "swap_lr", swapButton)
{
    // Clamp the two crossovers so they cannot cross. The minimum ratio
    // is the same factor the processor enforces: 4x (two octaves)
    // between the low and high crossovers. These limits are updated
    // dynamically when either crossover changes, so neither can push
    // past the other.
    updateCrossoverClamps();

    titleLabel.setText ("Filter", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (Typography::titleSize)));
    titleLabel.setMinimumHorizontalScale (1.0f);
    titleLabel.setColour (juce::Label::textColourId, Theme::textSecondary);
    addAndMakeVisible (titleLabel);

    auto setupSoloButton = [this] (juce::TextButton& b, int choice)
    {
        b.setClickingTogglesState (false);
        b.onClick = [this, choice] { toggleSoloChoice (choice); };
        b.getProperties().set ("fontSize", 12.0f);
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
        b.onClick = [this] { repaint(); };
        b.getProperties().set ("fontSize", 12.0f);
        addAndMakeVisible (b);
    };

    setupUtilityButton (monoButton);
    setupUtilityButton (swapButton);

    apvtsRef.addParameterListener ("filter_solo", this);
    apvtsRef.addParameterListener ("filter_low_xover", this);
    apvtsRef.addParameterListener ("filter_high_xover", this);

    if (auto* p = apvtsRef.getParameter ("filter_solo"))
        currentSolo = juce::roundToInt (p->convertFrom0to1 (p->getValue()));

    updateButtonStates();
}

FilterBar::~FilterBar()
{
    apvtsRef.removeParameterListener ("filter_solo", this);
    apvtsRef.removeParameterListener ("filter_low_xover", this);
    apvtsRef.removeParameterListener ("filter_high_xover", this);
}

void FilterBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Raised panel: drop shadow underneath, filled rounded rect, and a
    // subtle highlight line along the top edge.
    if (auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel()))
        lf->drawPanel (g, bounds, Theme::surfaceRaised);
    else
    {
        // Fallback if the LookAndFeel isn't ours (shouldn't happen).
        g.setColour (Theme::surfaceRaised);
        g.fillRoundedRectangle (bounds, Radius::panel);
    }

        // Shadows for child buttons, drawn here because the buttons
    // themselves can't draw outside their own bounds.
       if (auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel()))
    {
        const auto shadow = [&] (juce::Component& c)
        {
            if (! c.isEnabled())
                return;

            if (auto* button = dynamic_cast<juce::Button*> (&c))
            {
                if (button->getToggleState())
                {
                    lf->drawActiveControlShadow (g, c.getBounds().toFloat());
                    return;
                }
            }

            lf->drawControlShadow (g, c.getBounds().toFloat());
        };

        shadow (lowButton);
        shadow (midButton);
        shadow (highButton);
        shadow (monoButton);
        shadow (swapButton);
    }


}

void FilterBar::resized()
{
    auto bounds = getLocalBounds().reduced (10, 6);

    titleLabel.setBounds (bounds.removeFromLeft (56));
    bounds.removeFromLeft (6);

    const int buttonWidth = 78;
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

    const int utilWidth = 78;
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
    else if (parameterID == "filter_low_xover" || parameterID == "filter_high_xover")
    {
        juce::MessageManager::callAsync (
            [safe = juce::Component::SafePointer<FilterBar> (this)]
            {
                if (safe == nullptr)
                    return;

                safe->updateCrossoverClamps();
            });
    }
}

void FilterBar::updateCrossoverClamps()
{
    constexpr double minRatio = 2.0;

    const double lowValue  = lowXoverBox.getCurrentValue();
    const double highValue = highXoverBox.getCurrentValue();

    // Low/Mid cannot go above (Mid/High) / minRatio.
    lowXoverBox.setClampRange (20.0,
                               juce::jmax (20.0, highValue / minRatio));

    // Mid/High cannot go below (Low/Mid) * minRatio.
    highXoverBox.setClampRange (juce::jmin (20000.0, lowValue * minRatio),
                                20000.0);
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
    repaint(); 
}