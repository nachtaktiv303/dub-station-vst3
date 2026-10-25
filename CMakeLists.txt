cmake_minimum_required(VERSION 3.22)

project(DubStation VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED ENV{JUCE_DIR} AND NOT JUCE_DIR)
    set(JUCE_DIR $ENV{JUCE_DIR})
endif()
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(DubStation
    COMPANY_NAME "nachtaktiv"
    COMPANY_WEBSITE "https://github.com/nachtaktiv303"
    BUNDLE_ID ch.nachtaktiv.dubstation
    PLUGIN_MANUFACTURER_CODE Nakt
    PLUGIN_CODE DbSt
    PRODUCT_NAME "Dub Station"
    DESCRIPTION "Dub techno delay, reverb and tape effect"
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(DubStationAssets SOURCES
    assets/moog_knob_voice.png
    assets/panic_on.png
    assets/panic_off.png
    assets/grunge.png
    assets/grime_panel.png
    assets/TitilliumWeb-Regular.ttf
    assets/TitilliumWeb-SemiBold.ttf)

target_sources(DubStation PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    dsp/engine.cpp)

target_include_directories(DubStation PRIVATE dsp source)

target_compile_definitions(DubStation PUBLIC
    HAS_LFO_BPM=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    # the vendored DSP is plain C-style C++; keep MSVC quiet about its float conversions
    set_source_files_properties(dsp/engine.cpp PROPERTIES COMPILE_OPTIONS "/wd4244;/wd4305;/wd4267;/wd4996")
    target_compile_definitions(DubStation PRIVATE _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()

target_link_libraries(DubStation
    PRIVATE
        DubStationAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Offline checks (not shipped): cmake -DDUB_BUILD_TESTS=ON, then run DubStationTests [png-output-dir]
option(DUB_BUILD_TESTS "Build the offline test program" OFF)
if(DUB_BUILD_TESTS)
    juce_add_console_app(DubStationTests PRODUCT_NAME "DubStationTests")
    target_sources(DubStationTests PRIVATE tests/Tests.cpp source/PluginProcessor.cpp source/PluginEditor.cpp
        dsp/engine.cpp)
    target_include_directories(DubStationTests PRIVATE source dsp)
    target_compile_definitions(DubStationTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Dub Station" HAS_LFO_BPM=1)
    target_link_libraries(DubStationTests PRIVATE DubStationAssets juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
