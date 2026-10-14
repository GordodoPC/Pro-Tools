cmake_minimum_required(VERSION 3.22)
project(NovaDAW VERSION 0.3.0 LANGUAGES C CXX)
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

add_library(NovaCore STATIC Source/Audio/AudioEngine.cpp Source/Audio/MixProcessor.cpp Source/Audio/PluginCatalog.cpp Source/Session/Session.cpp Source/Session/EditModel.cpp Source/Session/Preferences.cpp)
target_include_directories(NovaCore PUBLIC Source external/JUCE/modules)
target_compile_options(NovaCore PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_definitions(NovaCore PUBLIC
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_VST=0 JUCE_USE_VST3_SDK=1
    JUCE_MODULE_AVAILABLE_juce_dsp=1 JUCE_MODULE_AVAILABLE_juce_audio_processors=1)
target_compile_definitions(NovaCore PUBLIC JUCE_MODULE_AVAILABLE_juce_audio_utils=1 JUCE_MODULE_AVAILABLE_juce_audio_devices=1
    JUCE_MODULE_AVAILABLE_juce_audio_formats=1 JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_gui_extra=1 JUCE_MODULE_AVAILABLE_juce_gui_basics=1)
target_link_libraries(NovaCore PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_gui_app(NovaDAW PRODUCT_NAME "NOVA DAW" COMPANY_NAME "Projeto SOM"
    BUNDLE_ID "com.projetosom.novadaw" NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE)
target_sources(NovaDAW PRIVATE Source/App/Main.cpp Source/UI/MainComponent.cpp Source/UI/EditorComponents.cpp Source/UI/MixComponents.cpp)
target_compile_definitions(NovaDAW PRIVATE NOVA_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(NovaDAW PRIVATE NovaCore)
juce_add_console_app(NovaPluginScanner PRODUCT_NAME "NovaPluginScanner")
target_sources(NovaPluginScanner PRIVATE Source/App/PluginScanner.cpp)
target_link_libraries(NovaPluginScanner PRIVATE NovaCore)
add_dependencies(NovaDAW NovaPluginScanner)
add_custom_command(TARGET NovaDAW POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:NovaPluginScanner>" "$<TARGET_FILE_DIR:NovaDAW>/NovaPluginScanner.exe")
add_custom_target(NovaScannerRuntime ALL COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:NovaPluginScanner>" "$<TARGET_FILE_DIR:NovaDAW>/NovaPluginScanner.exe"
    DEPENDS NovaPluginScanner NovaDAW)

if(BUILD_TESTING)
    juce_add_plugin(NovaTestGain COMPANY_NAME "Projeto SOM" PLUGIN_MANUFACTURER_CODE NvDw
        BUNDLE_ID "com.projetosom.novatestgain" PLUGIN_CODE NvTg FORMATS VST3 PRODUCT_NAME "NOVA Test Gain" COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(NovaTestGain PRIVATE tests/TestGainPlugin.cpp)
    target_compile_definitions(NovaTestGain PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_compile_options(NovaTestGain PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_link_libraries(NovaTestGain PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
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
    juce_add_console_app(NovaMixerTests PRODUCT_NAME "NOVA mixer tests")
    target_sources(NovaMixerTests PRIVATE tests/MixerTests.cpp)
    target_link_libraries(NovaMixerTests PRIVATE NovaCore)
    add_test(NAME MixerRouting COMMAND NovaMixerTests)
    juce_add_console_app(NovaPhase3Tests PRODUCT_NAME "NOVA Phase 3 tests")
    target_sources(NovaPhase3Tests PRIVATE tests/Phase3Tests.cpp)
    target_link_libraries(NovaPhase3Tests PRIVATE NovaCore)
    add_dependencies(NovaPhase3Tests NovaTestGain_VST3 NovaPluginScanner)
    add_custom_command(TARGET NovaPhase3Tests POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:NovaPluginScanner>" "$<TARGET_FILE_DIR:NovaPhase3Tests>/NovaPluginScanner.exe")
    add_custom_target(NovaScannerTestRuntime ALL COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:NovaPluginScanner>" "$<TARGET_FILE_DIR:NovaPhase3Tests>/NovaPluginScanner.exe"
        DEPENDS NovaPluginScanner NovaPhase3Tests)
    add_test(NAME DSPVST3AutomationSessions COMMAND NovaPhase3Tests
        "${CMAKE_BINARY_DIR}/test-data" "$<TARGET_FILE:NovaTestGain_VST3>" "$<TARGET_FILE:NovaPluginScanner>")
endif()
