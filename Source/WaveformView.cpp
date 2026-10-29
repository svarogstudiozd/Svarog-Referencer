#include "WaveformView.h"
#include "Theme.h"
#include "TimeFormat.h"
#include "ReferenceMaxLookAndFeel.h"

WaveformView::WaveformView (ReferenceSlot& slotToFollow)
    : slot (slotToFollow)
{
    slot.addChangeListener (this);
    slot.getThumbnail().addChangeListener (this);

    startTimerHz (30);
}

WaveformView::~WaveformView()
{
    stopTimer();

    slot.getThumbnail().removeChangeListener (this);
    slot.removeChangeListener (this);
}

//==============================================================================

juce::Rectangle<float> WaveformView::getWaveformBounds() const
{
    // Outer inset gives a small margin between the well's edge and the
    // waveform area. A few pixels here means the playhead at position
    // zero sits visibly inside the well, not flush with its border.
    auto bounds = getLocalBounds().toFloat().reduced (4.0f, 1.0f);

    const float rulerHeight = 20.0f;
    auto waveformBounds = bounds;
    waveformBounds.removeFromBottom (rulerHeight);
    waveformBounds = waveformBounds.reduced (0, 2);

    return waveformBounds;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float wellRadius = Radius::panel;

    g.setColour (Theme::surfaceRaised.darker (0.35f));
    g.fillRoundedRectangle (bounds, wellRadius);

    juce::Graphics::ScopedSaveState saveWaveform (g);
    juce::Path clipPath;
    clipPath.addRoundedRectangle (bounds.expanded (1.0f, 1.0f), wellRadius);
    g.reduceClipRegion (clipPath, {});


    // Unified gradient border
    {
        juce::Path borderPath;
        borderPath.addRoundedRectangle (bounds.reduced (0.4f),
                                        juce::jmax (0.0f, wellRadius - 0.5f));

        juce::Path outline;
        juce::PathStrokeType (1.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::butt)
            .createStrokedPath (outline, borderPath);

        juce::ColourGradient borderGrad (
            juce::Colours::black.withAlpha (0.9f),
            bounds.getX(), bounds.getY(),
            juce::Colours::white.withAlpha (0.5f),
            bounds.getX(), bounds.getBottom(),
            false);
        borderGrad.addColour (0.0, juce::Colours::grey.withAlpha (0.35f));

        g.setGradientFill (borderGrad);
        g.fillPath (outline);
    }


    auto& thumbnail = slot.getThumbnail();
    const float rulerHeight = 20.0f;
    auto waveformBounds = getWaveformBounds();

    const bool hasWaveform = (thumbnail.getNumChannels() > 0 &&
                              thumbnail.getTotalLength() > 0.0);

    if (hasWaveform)
    {
        g.setColour (Theme::greyLight);
        thumbnail.drawChannel (g,
                               waveformBounds.toNearestInt().reduced (4, 4),
                               0.0,
                               thumbnail.getTotalLength(),
                               0,
                               1.0f);
    }
    else
    {
        const bool isEmpty = ! slot.isLoaded() && ! slot.isLoading();
        g.setColour (isEmpty ? Theme::textSecondary : Theme::textFaint);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (14.0f)));

        const juce::String placeholder =
            isEmpty ? juce::String ("Click to add a reference")
                    : slot.getStatusText();

        g.drawFittedText (placeholder,
                          waveformBounds.toNearestInt().reduced (8),
                          juce::Justification::centred,
                          2);
    }

    const bool followActive = slot.isFollowMode();
    const bool inProgress = selectingLoop || draggingEdge != LoopEdge::none;

    float regionStart = 0.0f;
    float regionEnd   = 0.0f;
    bool  drawRegion  = false;

    if (! followActive)
    {
        if (inProgress)
        {
            regionStart = juce::jmin (loopSelectionStart, loopSelectionEnd);
            regionEnd   = juce::jmax (loopSelectionStart, loopSelectionEnd);
            drawRegion  = (regionEnd > regionStart);
        }
        else if (slot.isLoopEnabled() && hasWaveform)
        {
            regionStart = slot.getLoopStartNormalized();
            regionEnd   = slot.getLoopEndNormalized();
            drawRegion  = (regionEnd > regionStart);
        }
    }

    if (drawRegion)
    {
        const auto startX = waveformBounds.getX() + waveformBounds.getWidth() * regionStart;
        const auto endX   = waveformBounds.getX() + waveformBounds.getWidth() * regionEnd;

        g.setColour (Theme::accentRed.withAlpha (0.4f));
        g.fillRect (juce::Rectangle<float> (startX,
                                            waveformBounds.getY(),
                                            endX - startX,
                                            waveformBounds.getHeight()));

        g.setColour (Theme::accentRedBright);
        g.drawLine (startX, waveformBounds.getY(), startX, waveformBounds.getBottom(), 2.0f);
        g.drawLine (endX,   waveformBounds.getY(), endX,   waveformBounds.getBottom(), 2.0f);
    }

    const auto playheadX =
        waveformBounds.getX() +
        waveformBounds.getWidth() * slot.getNormalizedPosition();

    g.setColour (Theme::accentRedBright);
    g.drawLine (playheadX, waveformBounds.getY(), playheadX, waveformBounds.getBottom(), 2.0f);

    // Second top shadow (over the waveform, loop, and playhead)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, wellRadius);
        g.reduceClipRegion (clip, {});

        const float shadowHeight = 10.0f;
        juce::ColourGradient topShadow (
            juce::Colours::black.withAlpha (0.0f),
            bounds.getX(), bounds.getY(),
            juce::Colours::black.withAlpha (0.0f),
            bounds.getX(), bounds.getY() + shadowHeight,
            false);
        topShadow.addColour (0.15, juce::Colours::black.withAlpha (0.55f));
        topShadow.addColour (0.40, juce::Colours::black.withAlpha (0.30f));
        topShadow.addColour (0.70, juce::Colours::black.withAlpha (0.12f));
        g.setGradientFill (topShadow);
        g.fillRect (bounds.withHeight (shadowHeight + 1.0f));
    }

        // Inner shadow — all four sides, offset downward so the top reads
    // as the deepest edge, matching the analyser's recessed well.
    {
        juce::Graphics::ScopedSaveState save (g);

        juce::Path shadowClip;
        shadowClip.addRoundedRectangle (bounds, wellRadius);
        g.reduceClipRegion (shadowClip, {});

        const float topFade    = 14.0f;
        const float bottomFade = 10.0f;
        const float sideFade   = 12.0f;

        // Top: strongest shadow, darkens from the top edge downward.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.55f),
                bounds.getX(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX(), bounds.getY() + topFade,
                false);
            grad.addColour (0.25, juce::Colours::black.withAlpha (0.30f));
            grad.addColour (0.60, juce::Colours::black.withAlpha (0.12f));
            g.setGradientFill (grad);
            g.fillRect (bounds.withHeight (topFade + 1.0f));
        }

        // Bottom: lighter shadow, fades upward. Slight darkening here
        // completes the rounded well feel.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.30f),
                bounds.getX(), bounds.getBottom(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX(), bounds.getBottom() - bottomFade,
                false);
            grad.addColour (0.40, juce::Colours::black.withAlpha (0.08f));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (
                bounds.getX(),
                bounds.getBottom() - bottomFade,
                bounds.getWidth(),
                bottomFade));
        }

        // Left side: shadow hugging the left edge, fading right.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.35f),
                bounds.getX(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getX() + sideFade, bounds.getY(),
                false);
            grad.addColour (0.35, juce::Colours::black.withAlpha (0.15f));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (
                bounds.getX(),
                bounds.getY(),
                sideFade,
                bounds.getHeight()));
        }

        // Right side: shadow hugging the right edge, fading left.
        {
            juce::ColourGradient grad (
                juce::Colours::black.withAlpha (0.35f),
                bounds.getRight(), bounds.getY(),
                juce::Colours::black.withAlpha (0.0f),
                bounds.getRight() - sideFade, bounds.getY(),
                false);
            grad.addColour (0.35, juce::Colours::black.withAlpha (0.15f));
            g.setGradientFill (grad);
            g.fillRect (juce::Rectangle<float> (
                bounds.getRight() - sideFade,
                bounds.getY(),
                sideFade,
                bounds.getHeight()));
        }
    }

    drawRuler (g, bounds, rulerHeight);


}
    


//==============================================================================

void WaveformView::drawRuler (juce::Graphics& g, juce::Rectangle<float> bounds, float rulerHeight)
{
    auto rulerBounds = bounds;
    rulerBounds.removeFromTop (bounds.getHeight() - rulerHeight);
    rulerBounds = rulerBounds.reduced (4, 2);

    const double length = slot.getLengthSeconds();

    if (length <= 0.0)
    {
        g.setColour (Theme::textFaint);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterRegular()).withHeight (12.0f)));
        g.drawFittedText ("--:--", rulerBounds.toNearestInt(), juce::Justification::centred, 1);
        return;
    }

    const double totalWidth = rulerBounds.getWidth();
    const double pixelsPerSecond = totalWidth / length;

    // Pick the smallest interval whose spacing exceeds ~40 px. If even
    // the largest interval is too dense, fall back to the largest.
    const double intervals[] = { 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0, 120.0 };
    constexpr int numIntervals = (int) (sizeof (intervals) / sizeof (intervals[0]));

    double intervalSeconds = intervals[numIntervals - 1];

    for (int i = 0; i < numIntervals; ++i)
    {
        if (pixelsPerSecond * intervals[i] > 40.0)
        {
            intervalSeconds = intervals[i];
            break;
        }
    }

    g.setColour (Theme::textSecondary);
    g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (11.0f)));

    const int labelBoxWidth = 60;
    const int labelBoxHeight = (int) (rulerBounds.getHeight() - 8.0f + 2);

    auto makeLabelRect = [&] (double tickX, int edgeBehavior) -> juce::Rectangle<int>
    {
        const int rulerLeft  = (int) rulerBounds.getX();
        const int rulerRight = (int) rulerBounds.getRight();

        int rectX;
        const int rectW = labelBoxWidth;

        if (edgeBehavior == 1)
            rectX = rulerLeft;
        else if (edgeBehavior == 2)
            rectX = rulerRight - labelBoxWidth;
        else
            rectX = (int) tickX - labelBoxWidth / 2;

        if (rectX < rulerLeft)
            rectX = rulerLeft;
        if (rectX + rectW > rulerRight)
            rectX = rulerRight - rectW;

        return juce::Rectangle<int> (rectX,
                                     (int) rulerBounds.getY() - 1,
                                     rectW,
                                     labelBoxHeight);
    };

    auto justificationForEdge = [] (int edgeBehavior) -> juce::Justification
    {
        switch (edgeBehavior)
        {
            case 1:  return juce::Justification::centredLeft;
            case 2:  return juce::Justification::centredRight;
            default: return juce::Justification::centred;
        }
    };

    double currentTime = 0.0;
    int tickCount = 0;

    while (currentTime < length - 0.001)
    {
        const double x = rulerBounds.getX() + (currentTime / length) * totalWidth;

        const bool isMajorTick = (tickCount % 5 == 0);
        const float tickHeight = isMajorTick ? 8.0f : 4.0f;
        const float tickAlpha = isMajorTick ? 0.7f : 0.4f;

        g.setColour (Theme::textSecondary.withMultipliedAlpha (tickAlpha));
        g.drawLine ((float) x,
                    rulerBounds.getBottom() - tickHeight,
                    (float) x,
                    rulerBounds.getBottom(),
                    1.5f);

        // Only draw a text label if this tick is far enough from
        // the right edge that it won't collide with the end label.
        const bool nearRightEdge = (length - currentTime) < (intervalSeconds * 1.5);

        if (isMajorTick && ! nearRightEdge)
        {
            const juce::String label = TimeFormat::mmSs (currentTime);

            g.setColour (Theme::textPrimary);
            g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (12.0f)));

            int edgeBehavior = 0;
            if (tickCount == 0)
                edgeBehavior = 1;

            const auto labelRect = makeLabelRect (x, edgeBehavior);
            const auto just = justificationForEdge (edgeBehavior);

            g.drawFittedText (label, labelRect, just, 1);
        }

        currentTime += intervalSeconds;
        tickCount++;
    }

    // Single end-of-track label, always drawn at the right edge.
    {
        const double x = rulerBounds.getX() + totalWidth;
        const juce::String label = TimeFormat::mmSs (length);

        g.setColour (Theme::textPrimary);
        g.setFont (juce::Font (juce::FontOptions (ReferenceMaxLookAndFeel::getInterBold()).withHeight (12.0f)));

        const auto labelRect = makeLabelRect (x, 2);
        g.drawFittedText (label, labelRect, juce::Justification::centredRight, 1);
    }
}

//==============================================================================

float WaveformView::xToNormalized (float x) const
{
    const auto width = (float) juce::jmax (1, getWidth());
    return juce::jlimit (0.0f, 1.0f, x / width);
}

WaveformView::LoopEdge WaveformView::edgeAtPosition (float x) const
{
    if (! slot.isLoaded() || ! slot.isLoopEnabled() || slot.isFollowMode())
        return LoopEdge::none;

    const auto bounds = getWaveformBounds();

    const float startX = bounds.getX() + bounds.getWidth() * slot.getLoopStartNormalized();
    const float endX   = bounds.getX() + bounds.getWidth() * slot.getLoopEndNormalized();

    const float distToStart = std::abs (x - startX);
    const float distToEnd   = std::abs (x - endX);

    const bool onStart = distToStart <= edgeGrabRadiusPx;
    const bool onEnd   = distToEnd   <= edgeGrabRadiusPx;

    // Nearest edge wins when both are within reach. This lets the user
    // still grab either edge of a very narrow loop, instead of being
    // locked out of it entirely.
    if (onStart && onEnd)
        return distToStart <= distToEnd ? LoopEdge::start : LoopEdge::end;

    if (onStart) return LoopEdge::start;
    if (onEnd)   return LoopEdge::end;

    return LoopEdge::none;
}

void WaveformView::startDraggingEdge (LoopEdge edge)
{
    draggingEdge = edge;
    loopSelectionStart = slot.getLoopStartNormalized();
    loopSelectionEnd   = slot.getLoopEndNormalized();

    fireLoopRangePreview();
}

void WaveformView::moveLoopEdge (LoopEdge edge, float normalizedX)
{
    normalizedX = juce::jlimit (0.0f, 1.0f, normalizedX);

    if (edge == LoopEdge::start)
    {
        const float s = juce::jmin (normalizedX, loopSelectionEnd - minEdgeDragWidthNorm);
        loopSelectionStart = juce::jmax (0.0f, s);
    }
    else if (edge == LoopEdge::end)
    {
        const float e = juce::jmax (normalizedX, loopSelectionStart + minEdgeDragWidthNorm);
        loopSelectionEnd = juce::jmin (1.0f, e);
    }

    fireLoopRangePreview();
}

void WaveformView::finishDraggingEdge()
{
    draggingEdge = LoopEdge::none;

    const float start = juce::jmin (loopSelectionStart, loopSelectionEnd);
    const float end   = juce::jmax (loopSelectionStart, loopSelectionEnd);

    // The clamps in moveLoopEdge guarantee end - start >= minEdgeDragWidthNorm.
    slot.setLoopRangeNormalized (start, end);

    // Re-evaluate the cursor. Without this, if the mouse is stationary
    // on release, no mouseMove fires and the cursor can stay stuck in
    // the resize shape.
    refreshCursor();

    repaint();
}

//==============================================================================

void WaveformView::refreshCursor()
{
    if (draggingEdge != LoopEdge::none)
        return;

    const auto pos = getMouseXYRelative();
    const auto edge = edgeAtPosition ((float) pos.x);

    if (edge != hoveredEdge)
    {
        hoveredEdge = edge;
        setMouseCursor (edge == LoopEdge::none
                            ? juce::MouseCursor::NormalCursor
                            : juce::MouseCursor::LeftRightResizeCursor);
    }
}

void WaveformView::fireLoopRangePreview()
{
    if (onLoopRangePreviewChanged != nullptr)
        onLoopRangePreviewChanged (loopSelectionStart, loopSelectionEnd);
}

//==============================================================================

void WaveformView::mouseDown (const juce::MouseEvent& event)
{
    // Empty slot: any click opens the file chooser.
    if (! slot.isLoaded())
    {
        if (onEmptyClicked != nullptr)
            onEmptyClicked();

        return;
    }

    // Loop interaction is disabled while Follow is active.
    if (slot.isFollowMode())
    {
        seekToMouse (event);
        return;
    }

    // Loop edge drag takes priority over seek, but only when the loop
    // is visible and the click landed on one of its edges.
    const auto edge = edgeAtPosition (event.position.x);
    if (edge != LoopEdge::none && slot.isLoopEnabled())
    {
        startDraggingEdge (edge);
        return;
    }

    if (event.mods.isShiftDown())
        beginLoopSelection (event);
    else
        seekToMouse (event);
}

void WaveformView::mouseMove (const juce::MouseEvent&)
{
    refreshCursor();
}

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    if (draggingEdge != LoopEdge::none)
        return;

    hoveredEdge = LoopEdge::none;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseDrag (const juce::MouseEvent& event)
{
    if (draggingEdge != LoopEdge::none)
    {
        moveLoopEdge (draggingEdge, xToNormalized (event.position.x));
        repaint();
        return;
    }

    if (selectingLoop)
        updateLoopSelection (event);
    else
        seekToMouse (event);
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (draggingEdge != LoopEdge::none)
    {
        finishDraggingEdge();
        return;
    }

    if (selectingLoop)
        finishLoopSelection();
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! slot.isLoaded())
        return;

    // Loop clearing is disabled while Follow is active.
    if (slot.isFollowMode())
        return;

    if (slot.isLoopEnabled())
    {
        slot.setLoopEnabled (false);

        // Loop just disappeared, so any hovered-edge state is stale.
        refreshCursor();

        repaint();
    }
}

void WaveformView::mouseWheelMove (const juce::MouseEvent&,
                                   const juce::MouseWheelDetails& wheel)
{
    if (slot.isLoaded() && wheel.deltaY != 0.0f)
    {
        const float newPos = juce::jlimit (0.0f, 1.0f,
            slot.getNormalizedPosition() + wheel.deltaY * 0.01f);
        slot.setNormalizedPosition (newPos);
        repaint();
    }
}

//==============================================================================

void WaveformView::seekToMouse (const juce::MouseEvent& event)
{
    if (! slot.isLoaded())
        return;

    const auto proportion = xToNormalized (event.position.x);
    slot.setNormalizedPosition (proportion);
    repaint();
}

void WaveformView::beginLoopSelection (const juce::MouseEvent& event)
{
    selectingLoop = true;
    loopSelectionStart = xToNormalized (event.position.x);
    loopSelectionEnd = loopSelectionStart;

    fireLoopRangePreview();

    repaint();
}

void WaveformView::updateLoopSelection (const juce::MouseEvent& event)
{
    loopSelectionEnd = xToNormalized (event.position.x);

    fireLoopRangePreview();

    repaint();
}

void WaveformView::finishLoopSelection()
{
    selectingLoop = false;

    auto start = juce::jmin (loopSelectionStart, loopSelectionEnd);
    auto end   = juce::jmax (loopSelectionStart, loopSelectionEnd);

    if (end - start >= minLoopWidthNorm)
    {
        slot.setLoopRangeNormalized (start, end);
        slot.setLoopEnabled (true);
    }
    else
    {
        // Too small to commit. Signal the owner to clear its preview.
        if (onLoopRangePreviewChanged != nullptr)
            onLoopRangePreviewChanged (-1.0f, -1.0f);
    }

    // The loop geometry just changed, so re-evaluate the cursor.
    refreshCursor();

    repaint();
}

//==============================================================================

void WaveformView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void WaveformView::timerCallback()
{
    if (slot.isLoaded() && slot.isPlaying())
        repaint();
    else if (! slot.isLoading())
        repaint();
}