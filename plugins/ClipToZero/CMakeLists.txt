cmake_minimum_required(VERSION 3.22)

project(ClipToZero VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Min macOS")

# Universal binary on Apple Silicon: build both arm64 and x86_64 so the
# resulting .vst3/.component loads in any DAW running natively or via Rosetta.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build archs")
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ClipToZero
    COMPANY_NAME              "stephengeller"
    BUNDLE_ID                 com.stephengeller.cliptozero
    PLUGIN_MANUFACTURER_CODE  Sgel
    PLUGIN_CODE               Cz01
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "ClipToZero"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_DESCRIPTION        "Clip-to-zero workflow utility: peak/RMS metering, auto-gain, clipper, oscilloscope"
    VST3_CATEGORIES           Fx Distortion Analyzer
    AU_MAIN_TYPE              kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD   TRUE
)

target_sources(ClipToZero PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/DSP/LevelMeter.cpp
    Source/DSP/LevelMeter.h
    Source/DSP/Clipper.cpp
    Source/DSP/Clipper.h
    Source/DSP/AutoGainAnalyzer.cpp
    Source/DSP/AutoGainAnalyzer.h
    Source/DSP/LUFSMeter.cpp
    Source/DSP/LUFSMeter.h
    Source/DSP/GRHistory.cpp
    Source/DSP/GRHistory.h
    Source/DSP/SpectrumAnalyzer.cpp
    Source/DSP/SpectrumAnalyzer.h
    Source/DSP/TruePeakMeter.cpp
    Source/DSP/TruePeakMeter.h
    Source/UI/Theme.h
    Source/UI/LookAndFeel_F.cpp
    Source/UI/LookAndFeel_F.h
    Source/UI/StageLane.cpp
    Source/UI/StageLane.h
    Source/UI/HorizontalMeter.cpp
    Source/UI/HorizontalMeter.h
    Source/UI/TransferCurveComponent.cpp
    Source/UI/TransferCurveComponent.h
    Source/UI/LufsBox.cpp
    Source/UI/LufsBox.h
    Source/UI/Knob.cpp
    Source/UI/Knob.h
    Source/UI/MeterComponent.cpp
    Source/UI/MeterComponent.h
    Source/UI/GRMeterComponent.cpp
    Source/UI/GRMeterComponent.h
    Source/UI/OscilloscopeComponent.cpp
    Source/UI/OscilloscopeComponent.h
)

target_compile_definitions(ClipToZero PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ClipToZero PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
