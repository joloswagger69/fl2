#pragma once
#include "Params.h"
#include <string>
#include <vector>

struct PresetDef
{
    std::string category, name;
    std::vector<std::pair<int, float>> v; // nur abweichende Parameter (Rest = Default)
    float level = 0.7f;                   // kalibrierter Master-Pegel
};

// Factory-Bibliothek (Presets.cpp)
const std::vector<PresetDef>& presetLibrary();

// Benutzer-Presets & Favoriten (Dateien in Dokumente/NEOWAVE)
struct PresetStore
{
    static juce::File root()
    {
        auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEOWAVE");
        d.getChildFile("Presets").createDirectory();
        return d;
    }
    static juce::File userDir() { return root().getChildFile("Presets"); }

    static juce::StringArray favorites()
    {
        juce::StringArray a;
        a.addLines(root().getChildFile("favorites.txt").loadFileAsString());
        a.removeEmptyStrings();
        return a;
    }
    static void setFavorites(const juce::StringArray& a)
    {
        root().getChildFile("favorites.txt").replaceWithText(a.joinIntoString("\n"));
    }
    static juce::String key(const juce::String& cat, const juce::String& name) { return cat + "/" + name; }
};
