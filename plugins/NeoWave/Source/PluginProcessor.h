#pragma once
#include "Params.h"
#include "Presets.h"
#include "DSP/Voice.h"
#include "DSP/Effects.h"

class NeoProcessor : public juce::AudioProcessor
{
public:
    NeoProcessor();
    ~NeoProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "NEO//WAVE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return (int) presetLibrary().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int i) override { loadFactoryPreset(i); }
    const juce::String getProgramName(int i) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- Presets / Werkzeuge (Message-Thread) ----
    void loadFactoryPreset(int index);
    bool loadUserPreset(const juce::File& f);
    bool saveUserPreset(const juce::String& name, const juce::String& category);
    void setAllToDefault(bool keepGlobals);
    void randomize(bool smart);
    void panic();
    void toggleAB();
    juce::String presetName { "Init" }, presetCategory { "Init" };
    int abActive = 0;
    juce::ValueTree abSlot[2];

    // MIDI-Learn
    std::atomic<int> learnParam { -1 };
    std::array<std::atomic<int>, 128> ccMap;
    void clearMidiMap() { for (auto& c : ccMap) c = -1; }

    // Visualisierung (lock-free, nur lesend im GUI)
    static constexpr int kScope = 4096;
    std::array<float, kScope> scope {};
    std::atomic<int> scopePos { 0 };
    std::atomic<float> level { 0.f }, visLfo[3], visStep { 0.f };
    std::atomic<int> activeVoices { 0 };
    std::atomic<int> visStepIdx { 0 };
    std::atomic<int> nanEvents { 0 };   // Diagnose: wie oft wurde ein ungueltiges Sample abgefangen

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    juce::RangedAudioParameter* param(int i) const { return params[(size_t) i]; }
    void setParamReal(int i, float v);
    float getParamReal(int i) const;

private:
    void updateSnap();
    void handleMidi(const juce::MidiMessage&);
    void noteOn(int note, float vel);
    void noteOff(int note);
    void pedal(bool down);
    int  pickVoice();
    void applyValues(const std::vector<std::pair<int, float>>&, float level);

    std::array<std::atomic<float>*, P::kCount> raw {};
    std::array<juce::RangedAudioParameter*, P::kCount> params {};
    Snap snap;

    const WaveTables* tables = nullptr;
    std::array<nw::Voice, 16> voices;
    nw::FxChain fx;
    nw::Lfo glfo[3];
    nw::GlobalMod gm;
    float stepPh = 0.f, stepVal = 0.f;
    double sr = 44100.0;
    uint32_t stamp = 0;
    float lastNote = 60.f;
    bool sustain = false;
    std::atomic<bool> fxResetRequested { false };
    int held[32] {}, nHeld = 0;
    float dcX[2] {}, dcY[2] {};
    float peak = 0.f;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeoProcessor)
};
