cmake_minimum_required(VERSION 3.22)

project(PinkTron VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/JUCE/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Run: git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git libs/JUCE")
endif()

add_subdirectory(libs/JUCE)

juce_add_plugin(PinkTron
    COMPANY_NAME "jumang4423"
    BUNDLE_ID "com.jumang4423.pinktron"
    PLUGIN_MANUFACTURER_CODE Jmng
    PLUGIN_CODE Ptpt
    PRODUCT_NAME "Pink Tron"
    DESCRIPTION "Pink Trombone vocal tract resonator driven by the incoming audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS VST3 AU
)

target_sources(PinkTron PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/Tract.cpp
)

target_include_directories(PinkTron PRIVATE Source)

target_compile_definitions(PinkTron PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PinkTron
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- offline tools (plain C++, no JUCE) --------------------------------------
add_executable(pinktron-render tests/offline_render.cpp Source/dsp/Tract.cpp)
target_include_directories(pinktron-render PRIVATE Source/dsp)

add_executable(pinktron-faithfulness tests/faithfulness/port.cpp Source/dsp/Tract.cpp)
target_include_directories(pinktron-faithfulness PRIVATE Source/dsp)

# --- plugin host used for end-to-end verification ----------------------------
juce_add_console_app(pinktron-host PRODUCT_NAME "PinkTronHost")

target_sources(pinktron-host PRIVATE tests/host_render.cpp)

target_compile_definitions(pinktron-host PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(pinktron-host
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
)

# --- UI snapshot tool --------------------------------------------------------
juce_add_gui_app(pinktron-ui PRODUCT_NAME "PinkTronUI")

target_sources(pinktron-ui PRIVATE
    tests/ui_snapshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/Tract.cpp
)

target_include_directories(pinktron-ui PRIVATE Source)

target_compile_definitions(pinktron-ui PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(pinktron-ui
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
)
