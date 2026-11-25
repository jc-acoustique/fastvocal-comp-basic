cmake_minimum_required(VERSION 3.22)
project(FastVocalCompBasic VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded at configure time (it is not part of this repository).
# To use a local copy instead: cmake -S . -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(formats VST3)
if(APPLE)
    list(APPEND formats AU)
endif()

juce_add_plugin(FastVocalCompBasic
    VERSION 1.1.0
    COMPANY_NAME "TwinRoots Audio"
    BUNDLE_ID com.twinrootsaudio.fastvocalcompbasic
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Twra
    PLUGIN_CODE Fvcb
    FORMATS ${formats}
    PRODUCT_NAME "FastVocal Comp Basic"
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(FastVocalCompBasic PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.h
    source/OptoEngine.h
    source/StaticLaw.h
    source/MakeupLaw.h)

target_compile_definitions(FastVocalCompBasic PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=0)

target_link_libraries(FastVocalCompBasic
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
