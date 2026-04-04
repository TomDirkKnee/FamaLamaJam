cmake_minimum_required(VERSION 3.24)

project(FamaLamaJam VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Link the MSVC runtime statically so the VST3 runs on any Windows PC
# without needing a separate Visual C++ Redistributable install.
# MultiThreaded      = /MT  (Release)
# MultiThreadedDebug = /MTd (Debug)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(FAMALAMAJAM_ENABLE_TESTS "Build unit and integration tests." ON)

include(cmake/dependencies.cmake)

add_library(famalamajam_core STATIC)
target_sources(famalamajam_core
    PRIVATE
        src/app/session/SessionSettings.cpp
        src/app/session/ServerDiscoveryModel.cpp
        src/app/session/SessionSettingsController.cpp
        src/app/session/ConnectionLifecycle.cpp
        src/app/session/ConnectionLifecycleController.cpp
        src/app/session/RetryPolicy.cpp
        src/app/session/ConnectionFailureClassifier.cpp
        src/app/session/StemCaptureSettings.cpp
        src/infra/net/PublicServerDiscoveryClient.cpp
        src/audio/AudioBufferResampler.cpp
        src/audio/CodecStreamBridge.cpp
        src/audio/OggVorbisCodec.cpp
        src/audio/StemCaptureWriter.cpp
        src/net/FramedSocketTransport.cpp
        src/infra/state/SessionSettingsSerializer.cpp
        src/infra/state/RememberedServerStore.cpp)

target_include_directories(famalamajam_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(famalamajam_core
    PUBLIC
        JUCE_USE_OGGVORBIS=1)

target_link_libraries(famalamajam_core
    PUBLIC
        juce::juce_core
        juce::juce_data_structures
        juce::juce_audio_formats)

juce_add_plugin(famalamajam_plugin
    COMPANY_NAME "FamaLamaJam"
    BUNDLE_ID "com.famalamajam.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE FmLj
    PLUGIN_CODE Flj1
    FORMATS VST3 AU
    PRODUCT_NAME "FamaLamaJam")

target_sources(famalamajam_plugin
    PRIVATE
        src/plugin/FamaLamaJamAudioProcessor.h
        src/plugin/FamaLamaJamAudioProcessor.cpp
        src/plugin/FamaLamaJamAudioProcessorEditor.h
        src/plugin/FamaLamaJamAudioProcessorEditor.cpp)

target_compile_definitions(famalamajam_plugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_OGGVORBIS=1
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)

target_link_libraries(famalamajam_plugin
    PRIVATE
        famalamajam_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(famalamajam_core PRIVATE /W4 /WX)
    target_compile_options(famalamajam_plugin PRIVATE /W4 /WX)
endif()

juce_generate_juce_header(famalamajam_plugin)

if(FAMALAMAJAM_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
