cmake_minimum_required(VERSION 3.22)
project(CockReverb VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "${CMAKE_SOURCE_DIR}/../JUCE")
endif()
add_subdirectory(${JUCE_PATH} JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(CockReverb
    PLUGIN_MANUFACTURER_CODE Ystu
    PLUGIN_CODE             Crvs
    FORMATS                 VST3
    PRODUCT_NAME            "CockReverb"
    COMPANY_NAME            "Cockamps Audio"
    BUNDLE_ID               "com.cockamps.CockReverb"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(CockReverb)

juce_add_binary_data(CockReverbData
    SOURCES Source/CockReverbBG.png
)
set_target_properties(CockReverbData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(CockReverb PRIVATE
    Source/PluginEntry.cpp
    Source/ReverbEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(CockReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

if(UNIX AND NOT APPLE)
    target_compile_options(CockReverb PRIVATE -fvisibility=hidden)
endif()

if(WIN32)
    target_compile_definitions(CockReverb PUBLIC NOMINMAX)
endif()

target_link_libraries(CockReverb PRIVATE
    CockReverbData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
