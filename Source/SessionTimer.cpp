#include "SessionTimer.h"

//==============================================================================
SessionTimer::SessionTimer()
{
    const auto now = juce::Time::currentTimeMillis();

    sessionStartTimeMs.store (now);
    lastTickMs = now;

    lastAudioMs.store (0);
    lastTransportActivityMs.store (0);
    lastActivityMs.store (0);

    running          = false;
    manualPause      = false;
    autoPaused       = false;
    everStarted      = false;
    autoPauseEnabled = true;
    hasLoaded        = false;

    startTimer (1000);
}

SessionTimer::~SessionTimer()
{
    stopTimer();
}

//==============================================================================
// Activity reporting (audio thread). No locks, no allocation, no ValueTree.

void SessionTimer::reportAudioActivity() noexcept
{
    lastAudioMs.store (juce::Time::currentTimeMillis());
}

void SessionTimer::reportTransportActivity() noexcept
{
    lastTransportActivityMs.store (juce::Time::currentTimeMillis());
}

//==============================================================================
// 1 Hz tick.

void SessionTimer::timerCallback()
{
    const auto now   = juce::Time::currentTimeMillis();
    const auto delta = now - lastTickMs;
    lastTickMs = now;

    // Guard: if the machine slept or the host suspended us, delta could be
    // enormous. Ignore anything implausible so a single tick can't add
    // hours of "work" to the session.
    if (delta < 0 || delta > 5 * 1000)
        return;

    const bool audioRecent     = (now - lastAudioMs.load())             < kActivityWindowMs;
    const bool transportRecent = (now - lastTransportActivityMs.load()) < kActivityWindowMs;
    const bool anyActivity     = audioRecent || transportRecent;

    if (anyActivity)
        lastActivityMs.store (now);

    // Manual pause is authoritative.
    if (manualPause.load())
    {
        running = false;
        return;
    }

    // First-ever activity starts the clock.
    if (! everStarted.load() && anyActivity)
    {
        everStarted = true;
        sessionStartTimeMs.store (now);
    }

    // No activity has ever been seen: stay idle.
    if (! everStarted.load())
    {
        running = false;
        return;
    }

        // Auto-pause (if enabled) after the configured idle threshold.
    if (autoPauseEnabled.load())
    {
        const bool activeWithinWindow =
            (now - lastActivityMs.load()) < autoPauseThresholdMs.load();

        if (! activeWithinWindow)
        {
            running = false;
            autoPaused = true;
            return;
        }
    }

    running = true;
    autoPaused = false;
    currentSessionTimeMs.fetch_add (delta);
}

//==============================================================================
// Manual controls

void SessionTimer::startManually()
{
    const auto now = juce::Time::currentTimeMillis();

    everStarted = true;
    manualPause = false;
    autoPaused  = false;
    running     = true;

    lastTickMs = now;
    lastActivityMs.store (now);

    if (sessionStartTimeMs.load() <= 0)
        sessionStartTimeMs.store (now);
}

void SessionTimer::pauseManually()
{
    running     = false;
    manualPause = true;
    autoPaused  = false;
}

void SessionTimer::toggleManually()
{
    if (running.load())
        pauseManually();
    else
        startManually();
}

void SessionTimer::reset()
{
    const auto now = juce::Time::currentTimeMillis();

    // Reset wipes BOTH counters. The user pressed Reset, which means
    // "start over from zero" — not "close out this session and keep
    // the running total".
    currentSessionTimeMs.store (0);
    totalCompletedMs.store (0);

    sessionStartTimeMs.store (now);
    lastTickMs = now;

    running     = false;
    manualPause = false;
    autoPaused  = false;
    everStarted = false;

    lastAudioMs.store (0);
    lastTransportActivityMs.store (0);
    lastActivityMs.store (0);
}

//==============================================================================
// Persistence

void SessionTimer::writeToTree (juce::ValueTree& tree) const
{
    tree.setProperty ("session_current_ms",   currentSessionTimeMs.load(), nullptr);
    tree.setProperty ("session_total_ms",
                      totalCompletedMs.load() + currentSessionTimeMs.load(),
                      nullptr); 
    tree.setProperty ("session_start_ms",     sessionStartTimeMs.load(),   nullptr);
    tree.setProperty ("session_ever_started", everStarted.load(),          nullptr);
    tree.setProperty ("session_running",      running.load(),              nullptr);
    tree.setProperty ("session_manual_pause", manualPause.load(),          nullptr);
    tree.setProperty ("session_auto_pause_enabled", autoPauseEnabled.load(), nullptr);
}

void SessionTimer::readFromTree (const juce::ValueTree& tree)
{
    // Guard against autosave / template re-instantiation calling us twice.
    if (hasLoaded.load())
        return;
    hasLoaded = true;

    // The session timer always starts fresh on project load. Only the
    // running total persists across sessions.
    currentSessionTimeMs.store (0);

    totalCompletedMs.store     ((juce::int64) tree.getProperty ("session_total_ms",     0));
    autoPauseEnabled.store     ((bool)      tree.getProperty ("session_auto_pause_enabled", true));

    // The session starts in the "not yet started" state. It'll flip to
    // running on the first tick that sees activity, just like a fresh
    // plugin instance.
    everStarted = false;
    running     = false;
    manualPause = false;
    autoPaused  = false;

    const auto now = juce::Time::currentTimeMillis();
    sessionStartTimeMs.store (now);
    lastTickMs = now;

    lastActivityMs.store (now);
    lastAudioMs.store (0);
    lastTransportActivityMs.store (0);
}