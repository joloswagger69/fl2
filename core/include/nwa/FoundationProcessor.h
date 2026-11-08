#pragma once
// Plugin-Grundgeruest fuer das technische Fundament (Phase 1).
//
// Jedes Plugin der Familie startet als FoundationProcessor und wird in seiner Phase durch die echte
// Verarbeitung ersetzt. Das Grundgeruest hat bewusst nur Funktionen, die wirklich etwas tun:
//   * Effekt:     Durchleitung mit Gain-Regler (Mono/Stereo)
//   * Instrument: Test-Sinuston auf MIDI-Noten mit Gain-Regler
//   * Host-Tempo und Transport werden gelesen und im Editor angezeigt
//   * State-Speichern/-Laden und Benutzerpresets ueber die gemeinsamen Bausteine
#include "PresetManager.h"
#include "Util.h"

namespace nwa
{
struct PluginInfo
{
    juce::String id;       // stabile technische ID, z. B. "nwa.ravestation" (nie aendern!)
    juce::String name;     // Anzeigename, z. B. "RAVE//STATION"
    bool instrument = true;
    int phase = 0;         // Phase, in der die eigentlichen Funktionen entstehen
    juce::String summary;  // was geplant ist (nur Text, nicht implementiert)
    int stateVersion = 1;
};

class FoundationProcessor : public juce::AudioProcessor
{
public:
    explicit FoundationProcessor(const PluginInfo& infoIn);
    ~FoundationProcessor() override = default;

    // AudioProcessor
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return info.name; }
    bool acceptsMidi() const override { return info.instrument; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    const PluginInfo info;
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    // Anzeigedaten fuer den Editor (lock-free)
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<bool>   hostHasTempo { false }, hostPlaying { false };
    std::atomic<float>  outPeak { 0.f };
    std::atomic<int>    sanitizedBlocks { 0 };   // Blocks, in denen NaN/Inf ersetzt wurden

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout(bool instrument);
    static BusesProperties makeBuses(bool instrument);

private:
    void renderTone(juce::AudioBuffer<float>&, int start, int end) noexcept;
    void handleMidi(const juce::MidiMessage&) noexcept;

    std::atomic<float>* gainDb = nullptr;
    juce::SmoothedValue<float> gain;
    double sr = 44100.0;

    // Test-Ton (nur Instrumente)
    int note = -1;
    float phase = 0.f, inc = 0.f, env = 0.f, target = 0.f, vel = 0.f;
};
} // namespace nwa
