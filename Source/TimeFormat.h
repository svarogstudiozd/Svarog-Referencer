#pragma once

#include <juce_core/juce_core.h>

/**
    Shared time-formatting helpers.

    Used anywhere the plugin displays a duration or position as
    minutes:seconds. The two functions differ only in whether a tenths
    digit is shown.

    Formats:
        mmSs       - "M:SS"      or "H:MM:SS"
        mmSsTenths - "M:SS.t"    or "H:MM:SS.t"

    Both functions accept negative inputs and clamp them to zero, so
    callers don't have to guard against small floating-point negatives
    from time arithmetic.
*/
namespace TimeFormat
{
    inline juce::String mmSs (double seconds)
    {
        if (seconds < 0.0)
            seconds = 0.0;

        const int totalSeconds = (int) seconds;
        const int minutes = totalSeconds / 60;
        const int secs    = totalSeconds % 60;

        if (minutes < 60)
            return juce::String::formatted ("%d:%02d", minutes, secs);

        const int hours = minutes / 60;
        const int mins  = minutes % 60;
        return juce::String::formatted ("%d:%02d:%02d", hours, mins, secs);
    }

    inline juce::String mmSsTenths (double seconds)
    {
        if (seconds < 0.0)
            seconds = 0.0;

        const int totalSeconds = (int) seconds;
        const double frac = seconds - (double) totalSeconds;

        int tenths = (int) std::round (frac * 10.0);
        int secs   = totalSeconds % 60;
        int minutes = totalSeconds / 60;

        // Rounding tenths can push us over to the next second.
        if (tenths >= 10)
        {
            tenths = 0;
            secs += 1;

            if (secs >= 60)
            {
                secs = 0;
                minutes += 1;
            }
        }

        if (minutes < 60)
            return juce::String::formatted ("%d:%02d.%d", minutes, secs, tenths);

        const int hours = minutes / 60;
        const int mins  = minutes % 60;
        return juce::String::formatted ("%d:%02d:%02d.%d", hours, mins, secs, tenths);
    }
}