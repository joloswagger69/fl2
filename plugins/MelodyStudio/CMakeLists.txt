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

target_sources(MelodyStudio PRIVATE Source/Plugin.cpp)
target_link_libraries(MelodyStudio
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
