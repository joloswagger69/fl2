set(NEO_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp)

nwa_claim_plugin_identity(NeoWave Nwv1 "NEOWAVE")

juce_add_plugin(NeoWave
    COMPANY_NAME "NEOWAVE"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Neow
    PLUGIN_CODE Nwv1
    FORMATS VST3 Standalone
    PRODUCT_NAME "NEOWAVE"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

target_sources(NeoWave PRIVATE ${NEO_SOURCES})
target_compile_definitions(NeoWave PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(NeoWave
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)

# Offline-Test: rendert alle Presets, prueft NaN/Pegel/CPU, kalibriert Lautstaerken
juce_add_console_app(NeoWaveTest PRODUCT_NAME "NeoWaveTest")
target_sources(NeoWaveTest PRIVATE Tests/RenderTest.cpp ${NEO_SOURCES})
target_compile_definitions(NeoWaveTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(NeoWaveTest
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# GUI-Schnappschuss (Entwicklungswerkzeug): rendert jede Editor-Seite offscreen als PNG
juce_add_console_app(NeoWaveShot PRODUCT_NAME "NeoWaveShot")
target_sources(NeoWaveShot PRIVATE Tests/EditorShot.cpp ${NEO_SOURCES})
target_compile_definitions(NeoWaveShot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(NeoWaveShot
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# Automatische Tests (ctest)
add_test(NAME neowave_state  COMMAND NeoWaveTest --state)
add_test(NAME neowave_random COMMAND NeoWaveTest --random --repeat 20)
