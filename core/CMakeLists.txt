include(cmake/NwaPlugin.cmake)

# Gemeinsames Fundament als INTERFACE-Bibliothek: Die Quellen werden in jedes Plugin einkompiliert.
# (Eine statische Bibliothek wuerde JUCE-Module doppelt enthalten.)
add_library(nwa_core INTERFACE)
target_sources(nwa_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/StateIO.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FoundationProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FoundationEditor.cpp)
target_include_directories(nwa_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(nwa_core INTERFACE
    NWA_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(nwa_core INTERFACE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags)
