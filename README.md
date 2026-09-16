# Reference Max

A JUCE mastering reference plugin (AU, VST3, Standalone) built with CMake.

Load up to four WAV/MP3 reference tracks, A/B them against the incoming DAW audio, and line up playback from an interactive waveform playhead.

## Features

- **Audio file loader** — drag-and-drop or Load for up to 4 references. Files are decoded on a background thread; `AudioTransportSource` uses a read-ahead `TimeSliceThread` so the audio callback never opens files.
- **Transport & playhead** — per-track `AudioThumbnail` waveform. Drag the playhead (0–100%) to seek. Solo selects which reference plays.
- **Gain & A/B** — per-track dB sliders (−24 dB to +12 dB) applied with `juce::Decibels::decibelsToGain`. **A DAW** passes host audio; **B Reference** plays the soloed file.

## Requirements

- CMake 3.22+
- A C++20 compiler (Xcode on macOS)
- Git (JUCE 9.0.1 is fetched on first configure)

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

On macOS, built plugins are copied into `~/Library/Audio/Plug-Ins/` after a successful build.

Standalone:

```bash
cmake --build build --target ReferenceMax_Standalone
```
