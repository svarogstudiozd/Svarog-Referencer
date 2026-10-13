#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <cstdint>

/**
    Lightweight session tracker for Svarog Referencer +.

    Counts how much time the user spends working in a DAW project. Time
    accrues only while there is audio or transport activity; it pauses
    automatically after a fixed idle window (5 minutes) and can be paused
    or resumed manually at any time.

    Manual pause is authoritative: audio or transport activity does *not*
    clear it. Only the user can resume.

    The auto-pause threshold is fixed at 5 minutes; there is no user-
    editable time field. Auto-pause itself can be toggled on or off by
    the user (see setAutoPauseEnabled / isAutoPauseEnabled).

    Persistence:
      - currentSessionTimeMs, totalCompletedMs, sessionStartTimeMs,
        everStarted, running, manualPause, autoPauseEnabled are written
        to a ValueTree by writeToTree() and restored by readFromTree().
      - The processor calls these from its own get/setStateInformation,
        so the timer rides along with the DAW project state.

    Threading:
      - The running counters are std::atomic<juce::int64>, read from the
        UI thread and written from the timer thread (1 Hz) and the audio
        thread (reportActivity), so they must be lock-free and safe.
      - reportAudioActivity() / reportTransportActivity() are called from
        processBlock and only store juce::Time::currentTimeMillis() into
        atomics; no allocation, no locks, no timers, no ValueTree access.
*/
class SessionTimer : private juce::Timer
{
public:
    SessionTimer();
    ~SessionTimer() override;

    //==============================================================================
    // Called from processBlock. Cheap: stores timestamps into atomics only.
    //
    //   Either signal being true counts as activity.

    void reportAudioActivity()      noexcept;
    void reportTransportActivity()  noexcept;

    //==============================================================================
    // Called from the editor on user action. Safe from the message thread.

    void startManually();     // Start / Continue
    void pauseManually();     // Pause
    void toggleManually();    // Cycles Start -> Pause -> Continue -> Pause ...
    void reset();             // Zero session, fold current into total

    //==============================================================================
    // Queries used by the UI. Safe to call from the message thread.

    juce::int64 getSessionMs() const noexcept { return currentSessionTimeMs.load(); }
    juce::int64 getTotalMs()   const noexcept
    {
        return totalCompletedMs.load() + currentSessionTimeMs.load();
    }

    bool isRunning()        const noexcept { return running.load(); }
    bool isManuallyPaused() const noexcept { return manualPause.load(); }
    bool isAutoPaused()     const noexcept { return autoPaused.load(); }
    bool hasEverStarted()   const noexcept { return everStarted.load(); }

    //==============================================================================
    // Auto-pause on/off. Threshold itself is fixed at 5 minutes (kAutoPauseMs).

    bool isAutoPauseEnabled() const noexcept { return autoPauseEnabled.load(); }

    void setAutoPauseEnabled (bool enabled) noexcept
    {
        autoPauseEnabled.store (enabled);

        // Turning auto-pause back on while we're sitting in the auto-paused
        // state should not immediately resume the clock: the next tick will
        // re-evaluate. Turning it off while auto-paused should resume.
        if (! enabled && autoPaused.load())
        {
            autoPaused = false;

            // Only resume if the user wasn't manually paused.
            if (! manualPause.load() && everStarted.load())
            {
                running = true;
                lastTickMs = juce::Time::currentTimeMillis();
                lastActivityMs.store (lastTickMs);
            }
        }
    }
    // Auto-pause threshold in minutes. Fixed range 1..60. The processor
    // pushes this in whenever the session_auto_pause_minutes parameter
    // changes. Default is 5 minutes.
    void setAutoPauseThresholdMinutes (int minutes) noexcept
    {
        const auto clamped = juce::jlimit (1, 60, minutes);
        autoPauseThresholdMs.store ((juce::int64) clamped * 60 * 1000);
    }
    //==============================================================================
    // Persistence. Both are called from the processor's own get/setStateInformation.

    void writeToTree (juce::ValueTree& parentTree) const;
    void readFromTree (const juce::ValueTree& parentTree);

private:
    void timerCallback() override;

    //==============================================================================
    // Tunables

    // Auto-pause threshold. Fixed at 5 minutes for the Referencer+ strip.
    static constexpr juce::int64 kAutoPauseMs      = 5 * 60 * 1000;

    // A signal is considered "recent" if it happened within this window.
    static constexpr juce::int64 kActivityWindowMs = 2000;

    //==============================================================================
    // Counters. Atomic because they are read from the UI and written from
    // the timer thread and (for the timestamp fields) the audio thread.

    std::atomic<juce::int64> currentSessionTimeMs { 0 };
    std::atomic<juce::int64> totalCompletedMs     { 0 };
    std::atomic<juce::int64> sessionStartTimeMs   { 0 };
    // Auto-pause threshold, in milliseconds. Default 5 minutes. Written
    // by setAutoPauseThresholdMinutes() from the message thread; read
    // from the timer thread in timerCallback().
    std::atomic<juce::int64> autoPauseThresholdMs { 5 * 60 * 1000 };

    //==============================================================================
    // State flags

    std::atomic<bool> running          { false };
    std::atomic<bool> manualPause      { false };
    std::atomic<bool> autoPaused       { false };
    std::atomic<bool> everStarted      { false };
    std::atomic<bool> autoPauseEnabled { true };   // default on


    // Guards against the DAW calling setStateInformation more than once
    // per load (autosave, undo, template re-instantiation).
    std::atomic<bool> hasLoaded        { false };

    //==============================================================================
    // Activity timestamps. Written from the audio thread, read from the
    // timer thread. Atomic so no data race is possible.

    std::atomic<juce::int64> lastAudioMs             { 0 };
    std::atomic<juce::int64> lastTransportActivityMs { 0 };
    std::atomic<juce::int64> lastActivityMs          { 0 };

    // Last wall-clock value at which timerCallback ran. Only accessed on
    // the timer thread, so not atomic.
    juce::int64 lastTickMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SessionTimer)
};