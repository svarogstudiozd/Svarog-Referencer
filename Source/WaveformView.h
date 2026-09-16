#pragma once

#include "ReferenceSlot.h"
#include <functional>

class WaveformView : public juce::Component,
                     public juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit WaveformView (ReferenceSlot& slotToFollow);
    ~WaveformView() override;

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;

    void changeListenerCallback (
        juce::ChangeBroadcaster* source) override;

    // Fires when the user clicks on a slot that has no audio loaded.
    // Owner (ReferenceDetailView) uses this to open the file chooser.
    std::function<void()> onEmptyClicked;

    // Fires while the user is adjusting the loop (shift-drag to create,
    // or dragging an existing edge). Args are the in-progress normalized
    // (start, end) values. Fired with (-1, -1) when a preview ends
    // without committing (e.g. a too-small new selection), so the owner
    // can clear any preview display.
    std::function<void(float, float)> onLoopRangePreviewChanged;

private:
    enum class LoopEdge { none, start, end };

    void timerCallback() override;
    void drawRuler (juce::Graphics& g, juce::Rectangle<float> bounds, float rulerHeight);

    // Waveform area (outer view bounds minus the ruler), in component
    // coordinates. Extracted so paint() and edgeAtPosition() agree.
    juce::Rectangle<float> getWaveformBounds() const;

    void seekToMouse (const juce::MouseEvent& event);

    void beginLoopSelection (const juce::MouseEvent& event);
    void updateLoopSelection (const juce::MouseEvent& event);
    void finishLoopSelection();

    // Loop-edge dragging (adjusting an existing loop range).
    LoopEdge edgeAtPosition (float x) const;
    void startDraggingEdge (LoopEdge edge);
    void moveLoopEdge (LoopEdge edge, float normalizedX);
    void finishDraggingEdge();

    // Re-evaluate the cursor based on the current mouse position. Called
    // from mouseMove and also explicitly after events that change the
    // loop geometry, because JUCE does not fire mouseMove when the
    // mouse is stationary (e.g. on release without movement).
    void refreshCursor();

    // Notify the owner of the current in-progress loop range.
    void fireLoopRangePreview();

    float xToNormalized (float x) const;

    ReferenceSlot& slot;

    // Creating a new loop from scratch (shift-drag on empty area).
    bool selectingLoop = false;

    // Adjusting an existing loop edge.
    LoopEdge draggingEdge = LoopEdge::none;
    LoopEdge hoveredEdge  = LoopEdge::none;

    // Shared in-progress loop range used by both selectingLoop and
    // draggingEdge. Committed to the slot on mouse-up.
    float loopSelectionStart = 0.0f;
    float loopSelectionEnd = 0.0f;

    // How close (in pixels) the pointer must be to a loop edge for it
    // to be treated as a grab.
    static constexpr float edgeGrabRadiusPx = 6.0f;

    // Minimum loop width (as a fraction of the file) enforced when a
    // loop is created from scratch. Generous, to avoid accidental
    // tiny loops.
    static constexpr float minLoopWidthNorm = 0.01f;

    // Minimum loop width enforced while dragging an existing edge.
    // Much smaller than the creation minimum, so the user can fine-tune
    // a loop down to a fraction of a second.
    static constexpr float minEdgeDragWidthNorm = 0.001f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};