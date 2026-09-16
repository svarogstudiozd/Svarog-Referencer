#include "OffsetBox.h"
#include "Theme.h"

OffsetBox::OffsetBox (juce::AudioProcessorValueTreeState& apvtsToUse,
                      const juce::String& parameterID)
    : apvts (apvtsToUse)
{
    param = apvts.getParameter (parameterID);
    jassert (param != nullptr);

    if (param != nullptr)
        range = param->getNormalisableRange();

    setInterceptsMouseClicks (true, false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    setValueFromParameter();
}

void OffsetBox::setValueFromParameter()
{
    if (param == nullptr)
        return;

    currentValue = range.convertFrom0to1 (param->getValue());
    repaint();
}

juce::String OffsetBox::formatDisplay() const
{
    juce::String sign;

    if (currentValue > 0.0005f)
        sign = "+";
    else if (currentValue < -0.0005f)
        sign = "-";

    const float absV = std::abs (currentValue);

    juce::String num = juce::String (absV, 3);

    while (num.length() > 0 && num.endsWithChar ('0'))
        num = num.dropLastCharacters (1);

    if (num.endsWithChar ('.'))
        num += "0";

    return "Offset: " + sign + num + " s";
}

void OffsetBox::paint (juce::Graphics& g)
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

void OffsetBox::mouseDown (const juce::MouseEvent& e)
{
    dragStartValue = currentValue;
    dragStartY = e.getMouseDownY();

    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        dragScale = 0.01f;
    else if (e.mods.isShiftDown())
        dragScale = 0.1f;
    else
        dragScale = 1.0f;
}

void OffsetBox::mouseDrag (const juce::MouseEvent& e)
{
    if (param == nullptr)
        return;

    constexpr float pixelsForFullRange = 200.0f;

    const int deltaY = dragStartY - e.getPosition().y;

    const float normStart = param->convertTo0to1 (dragStartValue);
    const float normDelta = ((float) deltaY / pixelsForFullRange) * dragScale;
    const float normNew   = juce::jlimit (0.0f, 1.0f, normStart + normDelta);

    applyFromNormalized (normNew);
}

void OffsetBox::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param == nullptr)
        return;

    applyFromNormalized (param->convertTo0to1 (0.0f));
}

void OffsetBox::mouseEnter (const juce::MouseEvent&)
{
    hovered = true;
    repaint();
}

void OffsetBox::mouseExit (const juce::MouseEvent&)
{
    hovered = false;
    repaint();
}

void OffsetBox::applyFromNormalized (float normalized)
{
    if (param == nullptr)
        return;

    param->beginChangeGesture();
    param->setValueNotifyingHost (normalized);
    param->endChangeGesture();

    currentValue = range.convertFrom0to1 (normalized);
    repaint();
}