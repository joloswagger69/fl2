nwa_claim_plugin_identity(RaveStation Rvs1 "RAVE-STATION")

juce_add_plugin(RaveStation
    COMPANY_NAME "NEOWAVE AUDIO"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Neow
    PLUGIN_CODE Rvs1
    FORMATS VST3 Standalone
    PRODUCT_NAME "RAVE-STATION"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Sampler)

target_sources(RaveStation PRIVATE Source/RaveProcessor.cpp Source/engine/Engine.cpp Source/gui/RaveEditor.cpp)
target_compile_definitions(RaveStation PUBLIC RAVE_WITH_GUI=1)
target_include_directories(RaveStation PRIVATE Source)
target_link_libraries(RaveStation
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Engine-Tests (laufen ohne Host, ohne GUI)
juce_add_console_app(RaveTest PRODUCT_NAME "RaveTest")
target_sources(RaveTest PRIVATE Tests/RaveTest.cpp Source/engine/Engine.cpp)
target_compile_definitions(RaveTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(RaveTest
    PRIVATE juce::juce_audio_basics juce::juce_audio_formats juce::juce_core juce::juce_events juce::juce_data_structures
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME rave_engine COMMAND RaveTest)

juce_add_console_app(RaveProcessorTest PRODUCT_NAME "RaveProcessorTest")
target_sources(RaveProcessorTest PRIVATE Tests/ProcessorTest.cpp Source/RaveProcessor.cpp Source/engine/Engine.cpp)
target_include_directories(RaveProcessorTest PRIVATE Source)
target_compile_definitions(RaveProcessorTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(RaveProcessorTest
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME rave_processor COMMAND RaveProcessorTest)

# GUI-Schnappschuss (Entwicklungswerkzeug): rendert jede Editor-Seite offscreen als PNG
juce_add_console_app(RaveShot PRODUCT_NAME "RaveShot")
target_sources(RaveShot PRIVATE Tests/EditorShot.cpp Source/RaveProcessor.cpp Source/engine/Engine.cpp Source/gui/RaveEditor.cpp)
target_include_directories(RaveShot PRIVATE Source)
target_compile_definitions(RaveShot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 RAVE_WITH_GUI=1)
target_link_libraries(RaveShot
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags)

# GUI-Checks laufen nur unter Linux (xvfb) im CI; unter Windows wird RaveShot nicht gebaut/ausgefuehrt
if(UNIX AND NOT APPLE)
    add_test(NAME rave_gui COMMAND RaveShot ${CMAKE_BINARY_DIR}/rave_shots)
endif()
