cmake_minimum_required(VERSION 3.22)

project(EchoSpace VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Downloads JUCE 8.0.15 automatically on the first configure (about 100 MB).
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(EchoSpace
    COMPANY_NAME             "DIY Audio"         # vendor name shown in Ableton's browser
    PRODUCT_NAME             "Echo Space"        # the plugin's name in Ableton
    PLUGIN_MANUFACTURER_CODE Diya                # 4 letters, at least one capital
    PLUGIN_CODE              Ecsp                # 4 letters, unique per plugin
    BUNDLE_ID                com.diyaudio.echospace
    FORMATS                  VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx Delay Reverb
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(EchoSpace PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(EchoSpace PRIVATE Source)

target_compile_definitions(EchoSpace PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EchoSpace
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
