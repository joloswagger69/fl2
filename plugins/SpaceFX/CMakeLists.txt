nwa_claim_plugin_identity(SpaceFX Spf1 "SPACE-FX")

juce_add_plugin(SpaceFX
    COMPANY_NAME "NEOWAVE AUDIO"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Neow
    PLUGIN_CODE Spf1
    FORMATS VST3 Standalone
    PRODUCT_NAME "SPACE-FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb)

target_sources(SpaceFX PRIVATE Source/Plugin.cpp)
target_link_libraries(SpaceFX
    PRIVATE nwa_core
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
