cmake_minimum_required(VERSION 3.22)

project(HatsOff VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime so the plugin doesn't need extra Visual C++ DLLs
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HatsOff
    COMPANY_NAME            "Ireground"
    PLUGIN_MANUFACTURER_CODE Irgd
    PLUGIN_CODE             Htof
    FORMATS                 VST3
    PRODUCT_NAME            "HatsOff"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Instrument Sampler)

juce_generate_juce_header(HatsOff)

target_sources(HatsOff PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ReaperBridge.cpp)

# REAPER extension interface (Steinberg headers shipped with JUCE)
target_include_directories(HatsOff PRIVATE
    ${juce_SOURCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK)

target_compile_definitions(HatsOff PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(HatsOff PRIVATE /bigobj)
endif()

target_link_libraries(HatsOff
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
