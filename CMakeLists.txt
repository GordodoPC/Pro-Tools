cmake_minimum_required(VERSION 3.22)
project(NovaDAW VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# CL emits UTF-8 after dev-shell selects code page 65001. Normalize the
# Portuguese prefix misdecoded by compiler discovery on an OEM console.
if(MSVC AND CMAKE_CXX_CL_SHOWINCLUDES_PREFIX MATCHES "^Observa")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "Observação: incluindo arquivo: ")
endif()
include(CTest)
add_subdirectory(external/JUCE)

add_library(NovaCore STATIC Source/Audio/AudioEngine.cpp Source/Session/Session.cpp)
target_include_directories(NovaCore PUBLIC Source external/JUCE/modules)
target_compile_definitions(NovaCore PUBLIC
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_MODULE_AVAILABLE_juce_audio_utils=1 JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1 JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1 JUCE_MODULE_AVAILABLE_juce_gui_basics=1)
target_link_libraries(NovaCore PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)

juce_add_gui_app(NovaDAW PRODUCT_NAME "NOVA DAW" COMPANY_NAME "Projeto SOM"
    BUNDLE_ID "com.projetosom.novadaw" NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE)
target_sources(NovaDAW PRIVATE Source/App/Main.cpp Source/UI/MainComponent.cpp)
target_link_libraries(NovaDAW PRIVATE NovaCore)

if(BUILD_TESTING)
    add_executable(Cxx20Smoke tests/Cxx20Smoke.cpp)
    add_test(NAME Cxx20AndWindowsSDK COMMAND Cxx20Smoke)
    juce_add_console_app(JuceSmoke PRODUCT_NAME "JUCE validation")
    target_sources(JuceSmoke PRIVATE tests/JuceSmoke.cpp)
    target_link_libraries(JuceSmoke PRIVATE NovaCore)
    add_test(NAME JUCEIntegration COMMAND JuceSmoke)
    juce_add_console_app(NovaDAWTests PRODUCT_NAME "NOVA DAW tests")
    target_sources(NovaDAWTests PRIVATE tests/AudioTests.cpp)
    target_link_libraries(NovaDAWTests PRIVATE NovaCore)
    add_test(NAME AudioAndSessions COMMAND NovaDAWTests "${CMAKE_BINARY_DIR}/test-data")
endif()
