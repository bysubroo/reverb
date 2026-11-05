cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel), minimum macOS 10.15
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(SubRooRooms VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(SUBROO_BUILD_CLAP "Build the CLAP plugin" ON)
set(SUBROO_JUCE_PATH "" CACHE PATH "Local JUCE checkout (empty = download)")
set(SUBROO_CLAP_EXT_PATH "" CACHE PATH "Local clap-juce-extensions checkout (empty = download)")

include(FetchContent)

# ---------------------------------------------------------------- JUCE
if(SUBROO_JUCE_PATH)
    add_subdirectory(${SUBROO_JUCE_PATH} juce EXCLUDE_FROM_ALL)
else()
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# ---------------------------------------------------------------- CLAP
if(SUBROO_BUILD_CLAP)
    if(SUBROO_CLAP_EXT_PATH)
        add_subdirectory(${SUBROO_CLAP_EXT_PATH} clap-juce-extensions EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(clap_juce_extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG main
            GIT_SHALLOW TRUE
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(clap_juce_extensions)
    endif()
endif()

# ---------------------------------------------------------------- pffft (BSD-style)
add_library(subroo_pffft STATIC
    third_party/pffft/pffft.c
    third_party/pffft/pffft_common.c)
target_include_directories(subroo_pffft PUBLIC third_party/pffft)
target_compile_definitions(subroo_pffft PUBLIC PFFFT_STATIC_DEFINE _USE_MATH_DEFINES)
set_target_properties(subroo_pffft PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(subroo_pffft PRIVATE /O2 /fp:fast)
else()
    target_compile_options(subroo_pffft PRIVATE -O3 -ffast-math)
endif()

# ---------------------------------------------------------------- Plugin
set(SUBROO_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SUBROO_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND SUBROO_FORMATS LV2)
endif()

juce_add_plugin(SubRooRooms
    PRODUCT_NAME "SubRoo Rooms"
    PLUGIN_NAME "SubRoo Rooms"
    COMPANY_NAME "SubRoo"
    COMPANY_WEBSITE "https://subroo.xyz"
    COMPANY_EMAIL "contact@subroo.xyz"
    BUNDLE_ID com.subroo.rooms
    PLUGIN_MANUFACTURER_CODE Sbro
    PLUGIN_CODE Srrm
    VERSION ${PROJECT_VERSION}
    FORMATS ${SUBROO_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Reverb Spatial
    AU_MAIN_TYPE kAudioUnitType_Effect
    LV2URI "https://subroo.xyz/plugins/rooms"
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(SubRooRooms)

file(GLOB SUBROO_LOCATION_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/locations/*.json)
juce_add_binary_data(SubRooRoomsData
    NAMESPACE SubRooData
    SOURCES ${SUBROO_LOCATION_FILES})
set_target_properties(SubRooRoomsData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(SubRooRooms PRIVATE
    Source/Acoustics/Materials.cpp
    Source/Acoustics/Location.cpp
    Source/Acoustics/LocationLibrary.cpp
    Source/Acoustics/Scene.cpp
    Source/Acoustics/MicRig.cpp
    Source/Acoustics/Synth.cpp
    Source/Acoustics/MeasuredPack.cpp
    Source/Acoustics/RenderEngine.cpp
    Source/DSP/MatrixConvolver.cpp
    Source/Plugin/Parameters.cpp
    Source/Plugin/Presets.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/RoomView.cpp
    Source/UI/AngleDial.cpp
    Source/UI/PluginEditor.cpp)

target_include_directories(SubRooRooms PRIVATE Source)

target_compile_definitions(SubRooRooms PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(SubRooRooms
    PRIVATE
        SubRooRoomsData
        subroo_pffft
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(SUBROO_BUILD_CLAP)
    clap_juce_extensions_plugin(TARGET SubRooRooms
        CLAP_ID "com.subroo.rooms"
        CLAP_FEATURES audio-effect reverb surround stereo
        CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64)
endif()

# ---------------------------------------------------------------- tests (optional)
option(SUBROO_BUILD_TESTS "Build the offline engine tests" OFF)
if(SUBROO_BUILD_TESTS)
    juce_add_console_app(SubRooRoomsTests PRODUCT_NAME "SubRooRoomsTests")
    target_sources(SubRooRoomsTests PRIVATE
        tests/EngineTests.cpp
        Source/Acoustics/Materials.cpp
        Source/Acoustics/Location.cpp
        Source/Acoustics/LocationLibrary.cpp
        Source/Acoustics/Scene.cpp
        Source/Acoustics/MicRig.cpp
        Source/Acoustics/Synth.cpp
        Source/Acoustics/MeasuredPack.cpp
        Source/Acoustics/RenderEngine.cpp
        Source/DSP/MatrixConvolver.cpp)
    target_include_directories(SubRooRoomsTests PRIVATE Source)
    target_compile_definitions(SubRooRoomsTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(SubRooRoomsTests PRIVATE
        SubRooRoomsData subroo_pffft
        juce::juce_core juce::juce_audio_basics juce::juce_audio_formats juce::juce_graphics juce::juce_events
        juce::juce_recommended_config_flags)
endif()

if(SUBROO_BUILD_TESTS)
    # Instantiates the processor + editor directly: processing checks and a GUI snapshot
    add_executable(SubRooRoomsGuiCheck tests/GuiCheck.cpp)
    target_link_libraries(SubRooRoomsGuiCheck PRIVATE SubRooRooms)
    target_include_directories(SubRooRoomsGuiCheck PRIVATE Source $<TARGET_PROPERTY:SubRooRooms,INCLUDE_DIRECTORIES>)
    target_compile_definitions(SubRooRoomsGuiCheck PRIVATE $<TARGET_PROPERTY:SubRooRooms,COMPILE_DEFINITIONS>)

    # Loads the built VST3 through JUCE's hosting code, like a DAW would
    juce_add_console_app(SubRooRoomsHostCheck PRODUCT_NAME "SubRooRoomsHostCheck")
    target_sources(SubRooRoomsHostCheck PRIVATE tests/HostCheck.cpp)
    target_compile_definitions(SubRooRoomsHostCheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SubRooRoomsHostCheck PRIVATE
        juce::juce_audio_processors juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
