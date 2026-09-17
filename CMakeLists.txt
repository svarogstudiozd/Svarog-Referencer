cmake_minimum_required(VERSION 3.22)

project(SvarogReferencer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(SvarogReferencer
    COMPANY_NAME "Svarog Studio"
    BUNDLE_ID com.svarogstudio.referencer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE SvrG
    PLUGIN_CODE Sref
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Svarog Referencer"
)

target_sources(SvarogReferencer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/ModeToggleButton.h
        Source/ReferenceMaxLookAndFeel.h
        Source/ReferenceSlot.cpp
        Source/ReferenceSlot.h
        Source/ReferenceDetailView.cpp
        Source/ReferenceDetailView.h
        Source/SlotTile.cpp
        Source/SlotTile.h
        Source/SlotGrid.cpp
        Source/SlotGrid.h
        Source/WaveformView.cpp
        Source/WaveformView.h
        Source/AudioFileSupport.h
        Source/FilterProcessor.h
        Source/FilterBar.cpp
        Source/FilterBar.h
        Source/SpectrumAnalyzer.cpp
        Source/SpectrumAnalyzer.h
        Source/SpectrumDisplay.cpp
        Source/SpectrumDisplay.h
        Source/LevelMeterSource.h
        Source/LevelMeterComponent.cpp
        Source/LevelMeterComponent.h
        Source/LufsMeter.cpp
        Source/LufsMeter.h
        Source/LufsMeterComponent.cpp
        Source/LufsMeterComponent.h
        Source/DraggableValueBox.cpp
        Source/DraggableValueBox.h
        Source/Theme.h
)

target_compile_definitions(SvarogReferencer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_USE_MP3AUDIOFORMAT=1
)

target_link_libraries(SvarogReferencer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)