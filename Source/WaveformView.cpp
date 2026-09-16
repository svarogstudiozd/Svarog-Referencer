#include "WaveformView.h"
#include "Theme.h"

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
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    const float rulerHeight = 20.0f;
    auto waveformBounds = bounds;
    waveformBounds.removeFromBottom (rulerHeight);
    waveformBounds = waveformBounds.reduced (0, 2);

    return waveformBounds;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Theme::panelDarker);
    g.fillRoundedRectangle (bounds, 4.0f);

    auto& thumbnail = slot.getThumbnail();

    const float rulerHeight = 20.0f;
    auto waveformBounds = getWaveformBounds();

    const bool hasWaveform = (thumbnail.getNumChannels() > 0 &&
                              thumbnail.getTotalLength() > 0.0);

    //==============================================================
    // Waveform (bottom layer)
    //==============================================================

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
        // Placeholder text. When the slot is empty (not loading), invite
        // the user to click and load a file.
        const bool isEmpty = ! slot.isLoaded() && ! slot.isLoading();

        g.setColour (isEmpty ? Theme::textSecondary : Theme::textFaint);
        g.setFont (juce::FontOptions (14.0f));

        const juce::String placeholder =
            isEmpty ? juce::String ("Click to add a reference")
                    : slot.getStatusText();

        g.drawFittedText (placeholder,
                          waveformBounds.toNearestInt().reduced (8),
                          juce::Justification::centred,
                          2);
    }

    //==============================================================
    // Loop region (ON TOP of waveform)
    //
    //   - Hidden when Follow is active (loop is ignored in that mode).
    //   - While creating a new selection or dragging an existing edge:
    //     use the in-progress selection.
    //   - Otherwise: use the committed loop range.
    //==============================================================

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

    //==============================================================
    // Playhead
    //==============================================================

    const auto playheadX =
        waveformBounds.getX() +
        waveformBounds.getWidth() * slot.getNormalizedPosition();

    g.setColour (Theme::accentRedBright);
    g.drawLine (playheadX, waveformBounds.getY(), playheadX, waveformBounds.getBottom(), 2.0f);

    //==============================================================
    // Ruler
    //==============================================================

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
        g.setFont (12.0f);
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
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    auto formatTime = [] (double seconds) -> juce::String
    {
        if (seconds < 0.0) seconds = 0.0;
        const int totalSeconds = (int) seconds;
        const int minutes = totalSeconds / 60;
        const int secs    = totalSeconds % 60;

        if (minutes < 60)
            return juce::String::formatted ("%d:%02d", minutes, secs);

        const int hours = minutes / 60;
        const int mins  = minutes % 60;
        return juce::String::formatted ("%d:%02d:%02d", hours, mins, secs);
    };

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

    while (currentTime <= length)
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

        if (isMajorTick)
        {
            const juce::String label = formatTime (currentTime);

            g.setColour (Theme::textPrimary);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

            int edgeBehavior = 0;
            if (tickCount == 0)
                edgeBehavior = 1;
            else if (currentTime + intervalSeconds > length)
                edgeBehavior = 2;

            const auto labelRect = makeLabelRect (x, edgeBehavior);
            const auto just = justificationForEdge (edgeBehavior);

            g.drawFittedText (label, labelRect, just, 1);
        }

        currentTime += intervalSeconds;
        tickCount++;
    }

    if (tickCount > 0 && std::fmod (length, intervalSeconds) > 0.001)
    {
        const double x = rulerBounds.getX() + totalWidth;
        const juce::String label = formatTime (length);

        g.setColour (Theme::textPrimary);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

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