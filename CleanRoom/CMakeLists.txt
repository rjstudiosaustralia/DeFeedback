add_library(RingGuardCore STATIC
    Source/Core/RingGuardCore.cpp
    Source/Core/RingGuardCore.h)

set_target_properties(RingGuardCore PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_include_directories(RingGuardCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Core")

target_compile_features(RingGuardCore PUBLIC cxx_std_20)
target_link_libraries(RingGuardCore PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

juce_add_plugin(RingGuardPrototype
    VERSION 0.1.0
    COMPANY_NAME "RJ Studios Australia"
    COMPANY_WEBSITE "https://github.com/rjstudiosaustralia"
    COMPANY_EMAIL "ryansomerfield@hotmail.com"
    COMPANY_COPYRIGHT "Copyright 2026 Ryan Somerfield / RJ Studios Australia"
    BUNDLE_ID "au.com.rjstudiosaustralia.ringguardprototype"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Rjsa
    PLUGIN_CODE Rgdp
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "RingGuard Prototype"
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(RingGuardPrototype PRIVATE
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginProcessor.h)

target_compile_definitions(RingGuardPrototype PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RingGuardPrototype PRIVATE
    RingGuardCore
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(DEFEEDBACK_BUILD_TESTS)
    add_executable(RingGuardCoreTests
        Tests/RingGuardCoreTests.cpp)
    target_link_libraries(RingGuardCoreTests PRIVATE RingGuardCore)
    add_test(NAME RingGuardCoreTests COMMAND RingGuardCoreTests)

    add_executable(RingGuardBenchmark
        Benchmarks/RingGuardBenchmark.cpp)
    target_link_libraries(RingGuardBenchmark PRIVATE RingGuardCore)
endif()
