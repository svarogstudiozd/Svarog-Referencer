#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SessionTimer.h"
#include "DraggableValueBox.h"
#include "Theme.h"
#include <functional>

/**
    The slim session-tracking strip that sits at the bottom of the
    Svarog Referencer + editor.

    Layout (single row):

        [ Start / Pause / Continue ] | [ Auto pause ] [ After: 5 min ]
                                     | Session: HH:MM:SS
                                     | Total: HH:MM:SS | [ Reset ]

    All state comes from the shared SessionTimer owned by the processor.
    This component never runs the timing logic itself; it just displays
    the atomics and forwards user clicks.

    The auto-pause threshold is stored as an APVTS parameter
    ("session_auto_pause_minutes", 1..60 minutes, default 5), which the
    DraggableValueBox edits. That makes it persist with the DAW project
    automatically, via the existing get/setStateInformation path.

    The strip drives a 20 Hz timer:
      - to keep the HH:MM:SS displays fresh
      - to keep the toggle button's label in sync with the timer state
        (running / paused / not-yet-started)
      - to animate the reset button's "Confirm" pulse
*/
class SessionStrip : public juce::Component,
                     private juce::Timer
{
public:
    SessionStrip (SessionTimer& timerToUse,
                  juce::AudioProcessorValueTreeState& apvts);
    ~SessionStrip() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Height this component wants to occupy. Used by the editor's
    // computeHeightForSlots() so the window sizes correctly.
    static constexpr int preferredHeight = 42;

        // Iterate over the bounds of this strip's child controls, in
    // SessionStrip-local coordinates. Used by PluginEditor to draw
    // shadows for them.
    void forEachVisibleChildBounds (const std::function<void (juce::Rectangle<float>)>& fn) const;

private:
    void timerCallback() override;

    void updateButtonLabels();
    void updateTimeLabels();
    void armReset();
    void cancelResetArm();
    void performReset();

    static juce::String formatHMS (juce::int64 ms);

    SessionTimer& sessionTimer;

    juce::TextButton toggleButton;
    juce::TextButton autoPauseButton;
    juce::TextButton resetButton;

    // Field for the auto-pause threshold. Lives in the APVTS, so its
    // value persists with the DAW project automatically.
    DraggableValueBox autoPauseField;

    juce::Label sectionLabel; 
    juce::Label sessionLabel;
    juce::Label totalLabel;

    // Reset confirmation state (matches Project Timer's pattern).
    bool         resetArmed       = false;
    juce::int64  resetArmedAtMs   = 0;
    static constexpr juce::int64 resetConfirmWindowMs = 3000;

    // Used to keep the "Session:" time display from being re-laid-out
    // every tick when the value hasn't actually changed.
    juce::int64 lastDisplayedSessionSecs = -1;
    juce::int64 lastDisplayedTotalSecs   = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionStrip)
};