#pragma once
// Versioniertes, robustes Zustandsformat fuer Host-State und Presets.
//
//   <NWA_STATE format="1" plugin="nwa.ravestation" version="1">
//       <NWA_PARAMS> ...APVTS-Zustand... </NWA_PARAMS>
//       <EXTRA> ...optionale Plugin-Daten... </EXTRA>
//   </NWA_STATE>
//
// Ein fehlerhafter oder fremder Zustand wird abgelehnt, ohne den aktuellen Zustand zu veraendern.
#include <juce_audio_processors/juce_audio_processors.h>

namespace nwa::state
{
inline constexpr int kFormat = 1;

enum class Result { ok, empty, corrupt, wrongPlugin, newerVersion };

// Baut das XML. `extra` darf ungueltig sein (dann entfaellt das EXTRA-Element).
std::unique_ptr<juce::XmlElement> toXml(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId,
                                        int version, const juce::ValueTree& extra = {});

// Wendet das XML an. `newerVersion` bedeutet: wurde geladen, stammt aber von einer neueren Plugin-Version.
Result fromXml(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId, int version,
               const juce::XmlElement& xml, juce::ValueTree* extraOut = nullptr);

void   save(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId, int version,
            juce::MemoryBlock& dest, const juce::ValueTree& extra = {});
Result load(juce::AudioProcessorValueTreeState& apvts, const juce::String& pluginId, int version,
            const void* data, int sizeInBytes, juce::ValueTree* extraOut = nullptr);
} // namespace nwa::state
