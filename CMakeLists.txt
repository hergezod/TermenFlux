cmake_minimum_required(VERSION 3.22)

project(TermenFlux VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if no build type specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "No build type specified — defaulting to Release")
endif()

set(JUCE_VST3_CAN_REPLACE_VST2 OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
)
FetchContent_MakeAvailable(juce)

juce_add_binary_data(TermenFluxFonts
    SOURCES
        fonts/Cantarell-Regular.otf
        assets/theremin.png
        assets/ok.svg
        assets/hand.svg
        assets/knob.svg)

juce_add_plugin(TermenFlux
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "hergezod"
    BUNDLE_ID com.hergezod.termenflux
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Thr1
    PLUGIN_CODE Thrm
    FORMATS VST3 LV2
    LV2URI "http://hergezod.com/plugins/TermenFlux"
    VST3_CATEGORIES "Instrument|Synth"
    PRODUCT_NAME "TermenFlux")

target_sources(TermenFlux
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h)

target_compile_definitions(TermenFlux
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_XINERAMA=0
		JUCE_USE_XRANDR=0
		JUCE_USE_XRENDER=0
		JUCE_USE_XCURSOR=0
		JUCE_PLUGINHOST_LADSPA=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_JACK=0)

target_link_libraries(TermenFlux
    PRIVATE
        juce::juce_audio_plugin_client
        TermenFluxFonts
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
