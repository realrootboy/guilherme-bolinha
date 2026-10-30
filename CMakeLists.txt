cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(GuilhermeBolinha VERSION 1.0.1)

# JUCE: use a local checkout when present (JUCE_DIR), otherwise download it.
set(JUCE_DIR "$ENV{HOME}/Developer/JUCE" CACHE PATH "Path to a JUCE checkout")
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(GB_FORMATS AU VST3 Standalone)
elseif(UNIX)
    set(GB_FORMATS VST3 LV2 Standalone)
else()
    set(GB_FORMATS VST3 Standalone)
endif()

juce_add_plugin(GuilhermeBolinha
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Guilherme Bolinha"
    BUNDLE_ID "com.guilhermebolinha.vst"
    PRODUCT_NAME "Guilherme Bolinha"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Gblh
    PLUGIN_CODE Gbvs
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    LV2URI "https://github.com/realrootboy/guilherme-bolinha"
    FORMATS ${GB_FORMATS}
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(BolinhaAssets SOURCES assets/logo-transparent.png)

target_sources(GuilhermeBolinha PRIVATE
    Source/ContentImport.cpp
    Source/CureData.cpp
    Source/Engine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(GuilhermeBolinha PRIVATE cxx_std_17)
target_compile_definitions(GuilhermeBolinha PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GuilhermeBolinha PRIVATE
    BolinhaAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

option(GB_BUILD_TOOLS "Build the offline renderer / host test tools" ON)
if(GB_BUILD_TOOLS)
    # Offline renderer used to verify the engine (renders patches to WAV).
    juce_add_console_app(BolinhaRender PRODUCT_NAME "BolinhaRender")
    target_sources(BolinhaRender PRIVATE Tools/Render.cpp Source/ContentImport.cpp Source/CureData.cpp Source/Engine.cpp)
    target_compile_features(BolinhaRender PRIVATE cxx_std_17)
    target_compile_definitions(BolinhaRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BolinhaRender PRIVATE juce::juce_audio_formats juce::juce_dsp juce::juce_data_structures juce::juce_recommended_config_flags)

    # Automated arpeggiator checks (patterns, rates, octaves, gate, latch, sync).
    juce_add_console_app(BolinhaArpTest PRODUCT_NAME "BolinhaArpTest")
    target_sources(BolinhaArpTest PRIVATE Tools/ArpTest.cpp Source/ContentImport.cpp Source/CureData.cpp Source/Engine.cpp)
    target_compile_features(BolinhaArpTest PRIVATE cxx_std_17)
    target_compile_definitions(BolinhaArpTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BolinhaArpTest PRIVATE juce::juce_audio_basics juce::juce_dsp juce::juce_data_structures juce::juce_recommended_config_flags)

    # Render the real editor directly (native host window wrappers cannot be snapshotted).
    juce_add_console_app(BolinhaUiTest PRODUCT_NAME "BolinhaUiTest")
    target_sources(BolinhaUiTest PRIVATE Tools/UiTest.cpp)
    target_compile_features(BolinhaUiTest PRIVATE cxx_std_17)
    target_include_directories(BolinhaUiTest PRIVATE $<TARGET_PROPERTY:GuilhermeBolinha,INCLUDE_DIRECTORIES>)
    target_compile_definitions(BolinhaUiTest PRIVATE $<TARGET_PROPERTY:GuilhermeBolinha,COMPILE_DEFINITIONS>)
    target_link_libraries(BolinhaUiTest PRIVATE GuilhermeBolinha)

    # Loads the built plugin through JUCE's hosting layer to confirm it produces sound.
    juce_add_console_app(BolinhaHostTest PRODUCT_NAME "BolinhaHostTest")
    target_sources(BolinhaHostTest PRIVATE Tools/HostTest.cpp)
    target_compile_features(BolinhaHostTest PRIVATE cxx_std_17)
    target_compile_definitions(BolinhaHostTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1
        $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>)
    target_link_libraries(BolinhaHostTest PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
    if(APPLE)
        target_link_libraries(BolinhaHostTest PRIVATE "-framework CoreAudioKit")
    endif()
    # CI packages every platform format and compiles the host smoke test.
    add_custom_target(BolinhaRelease DEPENDS BolinhaHostTest)
    foreach(format IN LISTS GB_FORMATS)
        add_dependencies(BolinhaRelease GuilhermeBolinha_${format})
    endforeach()
endif()
