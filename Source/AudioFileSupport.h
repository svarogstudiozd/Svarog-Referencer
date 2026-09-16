#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace AudioFileSupport
{
inline juce::StringArray extensions()
{
    return { ".wav", ".mp3", ".aiff", ".aif", ".flac" };
}

inline juce::String fileChooserWildcard()
{
    return "*.wav;*.mp3;*.aiff;*.aif;*.flac";
}

inline bool isSupportedFile (const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return extensions().contains (ext);
}

inline bool containsSupportedFile (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isSupportedFile (juce::File (path)))
            return true;

    return false;
}

inline void registerFormats (juce::AudioFormatManager& formats)
{
    formats.registerBasicFormats();

#if JUCE_USE_MP3AUDIOFORMAT
    if (formats.findFormatForFileExtension ("mp3") == nullptr)
        formats.registerFormat (new juce::MP3AudioFormat(), false);
#endif
}
} // namespace AudioFileSupport
