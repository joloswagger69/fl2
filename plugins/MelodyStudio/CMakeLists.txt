nwa_claim_plugin_identity(MelodyStudio Mds1 "MELODY-STUDIO")

juce_add_plugin(MelodyStudio
    COMPANY_NAME "NEOWAVE AUDIO"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Neow
    PLUGIN_CODE Mds1
    FORMATS VST3 Standalone
    PRODUCT_NAME "MELODY-STUDIO"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument)

target_sources(MelodyStudio PRIVATE Source/MelodyProcessor.cpp Source/gui/MelodyEditor.cpp)
target_include_directories(MelodyStudio PRIVATE Source)
target_compile_definitions(MelodyStudio PUBLIC MELODY_WITH_GUI=1)
target_link_libraries(MelodyStudio
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Musik-Logik-Tests (kein GUI, nur juce_core)
juce_add_console_app(MelodyTest PRODUCT_NAME "MelodyTest")
target_sources(MelodyTest PRIVATE Tests/MelodyTest.cpp)
target_include_directories(MelodyTest PRIVATE Source)
target_compile_definitions(MelodyTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MelodyTest PRIVATE juce::juce_core PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME melody_music COMMAND MelodyTest)

juce_add_console_app(MelodyProcessorTest PRODUCT_NAME "MelodyProcessorTest")
target_sources(MelodyProcessorTest PRIVATE Tests/ProcessorTest.cpp Source/MelodyProcessor.cpp)
target_include_directories(MelodyProcessorTest PRIVATE Source)
target_compile_definitions(MelodyProcessorTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0 MELODY_WITH_GUI=0)
target_link_libraries(MelodyProcessorTest PRIVATE nwa_core PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME melody_processor COMMAND MelodyProcessorTest)

# GUI-Schnappschuss + Bedienungs-Checks (Entwicklungswerkzeug, nur Linux/xvfb im CI)
juce_add_console_app(MelodyShot PRODUCT_NAME "MelodyShot")
target_sources(MelodyShot PRIVATE Tests/EditorShot.cpp Source/MelodyProcessor.cpp Source/gui/MelodyEditor.cpp)
target_include_directories(MelodyShot PRIVATE Source)
target_compile_definitions(MelodyShot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 MELODY_WITH_GUI=1)
target_link_libraries(MelodyShot PRIVATE nwa_core PUBLIC juce::juce_recommended_config_flags)
if(UNIX AND NOT APPLE)
    add_test(NAME melody_gui COMMAND MelodyShot ${CMAKE_BINARY_DIR}/melody_shots)
endif()
