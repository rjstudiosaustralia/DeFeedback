cmake_minimum_required(VERSION 3.25)

project(DeFeedbackLive VERSION 0.2.0 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Apple Silicon only")
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0 CACHE STRING "Minimum supported macOS")

option(DEFEEDBACK_BUILD_TESTS "Build the configuration test suite" ON)
option(DEFEEDBACK_BUILD_CLEANROOM_PLUGIN "Build the RingGuard clean-room prototype" ON)

if(DEFEEDBACK_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(external/JUCE)

if(DEFEEDBACK_BUILD_CLEANROOM_PLUGIN)
    add_subdirectory(CleanRoom)
endif()

juce_add_gui_app(DeFeedbackLive
    VERSION ${PROJECT_VERSION}
    BUILD_VERSION ${PROJECT_VERSION}
    PRODUCT_NAME "DeFeedback Live"
    COMPANY_NAME "RJ Studios Australia"
    COMPANY_WEBSITE "https://github.com/rjstudiosaustralia"
    COMPANY_EMAIL "ryansomerfield@hotmail.com"
    BUNDLE_ID "au.com.rjstudiosaustralia.defeedback-live"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "DeFeedback Live needs access to audio interface inputs for live processing."
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(DeFeedbackLive PRIVATE
    Source/AppConfig.cpp
    Source/AudioEngine.cpp
    Source/LoginItem.mm
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MeterProcessor.cpp
    Source/PluginRegistration.mm
    Source/PluginWindow.cpp
    Source/SettingsStore.cpp)

target_compile_definitions(DeFeedbackLive PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DeFeedbackLive,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DeFeedbackLive,JUCE_VERSION>"
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST3=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_PLUGINHOST_ARA=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

target_link_libraries(DeFeedbackLive PRIVATE
    juce::juce_audio_utils
    juce::juce_gui_extra
    "-framework ServiceManagement"
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(DEFEEDBACK_BUILD_TESTS)
    juce_add_console_app(DeFeedbackTests
        PRODUCT_NAME "DeFeedback Tests")

    target_sources(DeFeedbackTests PRIVATE
        Source/AppConfig.cpp
        Tests/AppConfigTests.cpp)

    target_compile_definitions(DeFeedbackTests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)

    target_link_libraries(DeFeedbackTests PRIVATE
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    add_test(NAME DeFeedbackConfigTests COMMAND DeFeedbackTests)
endif()
