#include "DraggableValueBox.h"
#include "Theme.h"
#include "ReferenceMaxLookAndFeel.h"

DraggableValueBox::DraggableValueBox (juce::AudioProcessorValueTreeState& apvtsToUse,
                                      const juce::String& parameterIDToUse,
                                      const juce::String& prefixLabel,
                                      const juce::String& valueSuffix,
                                      double resetValue)
    : apvts (apvtsToUse),
      parameterID (parameterIDToUse),
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

    apvts.addParameterListener (parameterID, this);

    setValueFromParameter();
}

DraggableValueBox::~DraggableValueBox()
{
    apvts.removeParameterListener (parameterID, this);
}

void DraggableValueBox::setClampRange (double minVal, double maxVal)
{
    clampMin = minVal;
    clampMax = maxVal;
    hasClamp = (clampMin <= clampMax);

    if (hasClamp)
    {
        const double clamped = clampedValue (currentValue);

        if (std::abs (clamped - currentValue) > 1.0e-6)
            applyFromNormalized ((float) range.convertTo0to1 ((float) clamped));
    }
}

double DraggableValueBox::clampedValue (double value) const
{
    if (! hasClamp)
        return value;

    return juce::jlimit (clampMin, clampMax, value);
}

void DraggableValueBox::setValueFromParameter()
{
    if (param == nullptr)
        return;

    currentValue = range.convertFrom0to1 (param->getValue());
    repaint();
}

void DraggableValueBox::parameterChanged (const juce::String& id, float)
{
    if (id != parameterID)
        return;

    juce::MessageManager::callAsync (
        [safe = juce::Component::SafePointer<DraggableValueBox> (this)]
        {
            if (safe != nullptr)
                safe->setValueFromParameter();
        });
}

juce::String DraggableValueBox::formatDisplay() const
{
    const int rounded = juce::roundToInt (currentValue);
    return prefix + ": " + juce::String (rounded) + " " + suffix;
}

void DraggableValueBox::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const bool enabled = isEnabled();

    // The box is drawn as a recessed control — it reads as a well cut
    // into the panel, matching the disabled buttons and the spectrum
    // plot. When hovered and enabled, the fill lightens slightly.
    auto* lf = dynamic_cast<ReferenceMaxLookAndFeel*> (&getLookAndFeel());

 // Base fill: darker than the surrounding panel so the box reads as
    // a well cut into the surface. Hovering lightens it slightly;
    // disabled darkens it further.
    juce::Colour fill = Theme::surfaceRaised.darker (0.35f);

    if (! enabled)
        fill = fill.darker (0.35f);
    else if (hovered)
        fill = fill.brighter (0.05f);

    if (lf != nullptr)
    {
        lf->drawRecessedControl (g, bounds, fill);
    }
    else
    {
        // Fallback if the LookAndFeel isn't ours.
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, Radius::control);

        // Top inner shadow.
        {
            juce::Graphics::ScopedSaveState save (g);

            juce::Path clip;
            clip.addRoundedRectangle (bounds, Radius::control);
            g.reduceClipRegion (clip, {});

            juce::ColourGradient shadow (
                juce::Colours::black.withAlpha (0.6f),
                bounds.getX(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX(), bounds.getY() + 4.0f,
                false);
            g.setGradientFill (shadow);
            g.fillRect (bounds.withHeight (5.0f));
        }
    }

    // Text: brighter when enabled and hovered, faint when disabled.
    juce::Colour textColour;
    if (! enabled)
        textColour = Theme::textFaint;
    else if (hovered)
        textColour = Theme::textPrimary;
    else
        textColour = Theme::textSecondary;

    g.setColour (textColour);
    g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (13.5f)));

    // Subtle text shadow for a lifted feel on the recessed surface.
    if (enabled)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText (formatDisplay(),
                    getLocalBounds().reduced (8, 0).translated (0, 1),
                    juce::Justification::centred);
    }

    g.setColour (textColour);
    g.drawText (formatDisplay(),
                getLocalBounds().reduced (8, 0),
                juce::Justification::centred);
}

void DraggableValueBox::mouseDown (const juce::MouseEvent& e)
{
    dragStartValue = (float) currentValue;
    dragStartY = e.getMouseDownY();

    if (param != nullptr)
        param->beginChangeGesture();
}

void DraggableValueBox::mouseUp (const juce::MouseEvent&)
{
    if (param != nullptr)
        param->endChangeGesture();
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

    const float value = range.convertFrom0to1 (normNew);
    const float clamped = (float) clampedValue (value);
    const float normClamped = range.convertTo0to1 (clamped);

    param->setValueNotifyingHost (normClamped);

    currentValue = clamped;
    repaint();
}

void DraggableValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param == nullptr)
        return;

    const float norm = param->convertTo0to1 ((float) resetToValue);

    param->beginChangeGesture();
    param->setValueNotifyingHost (norm);
    param->endChangeGesture();

    currentValue = resetToValue;
    repaint();
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

void DraggableValueBox::enablementChanged()
{
    // Swap cursor based on enabled state: normal arrow when disabled,
    // resize cursor when enabled.
    setMouseCursor (isEnabled() ? juce::MouseCursor::UpDownResizeCursor
                                : juce::MouseCursor::NormalCursor);
    repaint();
}