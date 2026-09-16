#include "DraggableValueBox.h"
#include "Theme.h"

DraggableValueBox::DraggableValueBox (juce::AudioProcessorValueTreeState& apvtsToUse,
                                      const juce::String& parameterID,
                                      const juce::String& prefixLabel,
                                      const juce::String& valueSuffix,
                                      double resetValue)
    : apvts (apvtsToUse),
      prefix (prefixLabel),
      suffix (valueSuffix),
      resetToValue (resetValue)
{
    param = apvts.getParameter (parameterID);
    jassert (param != nullptr);

    if (param != nullptr)
        range = param->getNormalisableRange();

    setInterceptsMouseClicks (true, false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    setValueFromParameter();
}

void DraggableValueBox::setValueFromParameter()
{
    if (param == nullptr)
        return;

    currentValue = range.convertFrom0to1 (param->getValue());
    repaint();
}

juce::String DraggableValueBox::formatDisplay() const
{
    const int rounded = juce::roundToInt (currentValue);
    return prefix + ": " + juce::String (rounded) + " " + suffix;
}

void DraggableValueBox::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (hovered ? Theme::panelHover : Theme::panel);
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (hovered ? Theme::border : Theme::borderSubtle);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    g.setColour (Theme::textPrimary);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    g.drawText (formatDisplay(),
                getLocalBounds().reduced (8, 0),
                juce::Justification::centred);
}

void DraggableValueBox::mouseDown (const juce::MouseEvent& e)
{
    dragStartValue = currentValue;
    dragStartY = e.getMouseDownY();
}

void DraggableValueBox::mouseDrag (const juce::MouseEvent& e)
{
    if (param == nullptr)
        return;

    constexpr float pixelsForFullRange = 200.0f;

    const int deltaY = dragStartY - e.getPosition().y;
    const float normStart = param->convertTo0to1 (dragStartValue);
    const float normDelta = (float) deltaY / pixelsForFullRange;
    const float normNew   = juce::jlimit (0.0f, 1.0f, normStart + normDelta);

    applyFromNormalized (normNew);
}

void DraggableValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param == nullptr)
        return;

    applyFromNormalized (param->convertTo0to1 ((float) resetToValue));
}

void DraggableValueBox::mouseEnter (const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void DraggableValueBox::mouseExit (const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

void DraggableValueBox::applyFromNormalized (float normalized)
{
    if (param == nullptr)
        return;

    param->beginChangeGesture();
    param->setValueNotifyingHost (normalized);
    param->endChangeGesture();

    currentValue = range.convertFrom0to1 (normalized);
    repaint();
}