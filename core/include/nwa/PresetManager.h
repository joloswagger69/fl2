#pragma once
// Benutzerpresets als Dateien. Nur im Message-Thread verwenden (Dateizugriffe!).
#include "StateIO.h"

namespace nwa
{
class PresetManager
{
public:
    enum class Status { ok, invalidName, ioError, notFound, corrupt, wrongPlugin };

    // rootOverride: nur fuer Tests; sonst Dokumente/NEOWAVE_AUDIO/<displayName>/Presets
    PresetManager(juce::AudioProcessorValueTreeState& apvts, juce::String pluginId, juce::String displayName,
                  int stateVersion, juce::File rootOverride = {});

    juce::File       directory() const { return dir; }
    juce::StringArray list() const;                       // Preset-Namen, alphabetisch
    Status           save(const juce::String& name, const juce::ValueTree& extra = {});
    Status           load(const juce::String& name, juce::ValueTree* extra = nullptr);
    bool             remove(const juce::String& name);

    // Gibt einen gueltigen Dateinamen (ohne Endung) zurueck oder einen leeren String, wenn nichts Brauchbares uebrig bleibt.
    static juce::String sanitizeName(const juce::String& raw);
    static constexpr const char* kExtension = ".nwapreset";

    juce::String currentName;

private:
    juce::File fileFor(const juce::String& cleanName) const { return dir.getChildFile(cleanName + kExtension); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String id;
    int version;
    juce::File dir;
};
} // namespace nwa
