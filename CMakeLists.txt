cmake_minimum_required(VERSION 3.25)

# These must be established before project() enables the Apple compilers.
# Otherwise a fresh build can silently inherit the build machine's macOS
# version and produce an application that will not launch on older systems.
set(DEFEEDBACK_MINIMUM_MACOS_VERSION "13.0")
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${DEFEEDBACK_MINIMUM_MACOS_VERSION}"
        CACHE STRING "Minimum supported macOS" FORCE)
endif()
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Apple Silicon only" FORCE)
endif()

project(DeFeedbackLive VERSION 0.5.2 LANGUAGES C CXX OBJC OBJCXX)

if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS DEFEEDBACK_MINIMUM_MACOS_VERSION)
    message(FATAL_ERROR
        "DeFeedback Live requires macOS ${DEFEEDBACK_MINIMUM_MACOS_VERSION} or newer")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DEFEEDBACK_BUILD_TESTS "Build the configuration test suite" ON)
option(DEFEEDBACK_QA_BUILD "Use a separate bundle identity for side-by-side UI QA" OFF)

set(DEFEEDBACK_PRODUCT_NAME "DeFeedback Live")
set(DEFEEDBACK_BUNDLE_ID "au.com.rjstudiosaustralia.defeedback-live")
if(DEFEEDBACK_QA_BUILD)
    set(DEFEEDBACK_PRODUCT_NAME "DeFeedback Live QA")
    set(DEFEEDBACK_BUNDLE_ID "au.com.rjstudiosaustralia.defeedback-live.qa")
endif()

add_subdirectory(external/JUCE)

juce_add_gui_app(DeFeedbackLive
    VERSION ${PROJECT_VERSION}
    BUILD_VERSION ${PROJECT_VERSION}
    PRODUCT_NAME "${DEFEEDBACK_PRODUCT_NAME}"
    COMPANY_NAME "RJ Studios Australia"
    COMPANY_WEBSITE "https://github.com/rjstudiosaustralia"
    COMPANY_EMAIL "ryansomerfield@hotmail.com"
    BUNDLE_ID "${DEFEEDBACK_BUNDLE_ID}"
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
    Source/RemoteControlServer.cpp
    Source/SettingsStore.cpp)

target_compile_definitions(DeFeedbackLive PRIVATE
    DEFEEDBACK_UI_PREVIEW=$<BOOL:${DEFEEDBACK_QA_BUILD}>
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
    enable_testing()

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
