#pragma once
#ifndef MELODY_WITH_GUI
#define MELODY_WITH_GUI 0
#endif
#include <nwa/PresetManager.h>
#include <nwa/Util.h>
#include "PluginInfo.h"
#include "music/Project.h"
#include "music/MidiExport.h"
#include "music/Synth.h"
#include <atomic>
#include <deque>
#include <memory>

namespace mel
{
// Unveraenderlicher Zustand der erzeugten Musik. Der Audio-Thread liest nur ueber einen Rohzeiger; die letzten Staende bleiben am Leben.
struct Snapshot { Project proj; std::vector<Ev> ev; };

class MelodyProcessor : public juce::AudioProcessor
{
public:
    MelodyProcessor();
    ~MelodyProcessor() override = default;

    const juce::String getName() const override { return "MELODY//STUDIO"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    bool hasEditor() const override { return MELODY_WITH_GUI != 0; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- Parameter-Zugriff (Message-Thread)
    float getRaw(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    int   getInt(const juce::String& id) const { return (int) std::lround(getRaw(id)); }
    void  setParam(const juce::String& id, float plain);

    // ---- Einstellungen aus den Parametern
    Ctx ctx() const;
    ChordParams chordParams() const;
    MelodyParams melodyParams() const;
    ArpParams arpParams() const;
    BassParams bassParams() const;
    AcidParams acidParams() const;
    int swingTicks() const { return swingTicksFor(getRaw("swing")); }

    // ---- Erzeugen (Message-Thread). part: 0 Chords, 1 Melody, 2 Bass, 3 Arp, 4 Acid, -1 alles.
    // newSeed: wuerfelt einen neuen Seed fuer den Teil. Aendert sich Tonart/Skala/Laenge/Taktart, wird alles neu erzeugt (steht in der Rueckgabe).
    bool regenerate(int part, bool newSeed);
    void variation(int part, float amount);          // leichte Veraenderung des vorhandenen Teils
    void makeTrack(bool newIdea);                   // Genre-Voreinstellungen in die Regler schreiben, dann alles erzeugen
    void applyGenreToParams(int genre, int mood, bool setTempo);
    void clearPart(int part);
    bool undo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool isStale() const;                           // Tonart/Skala/Laenge/Taktart weichen vom erzeugten Stand ab

    std::shared_ptr<const Snapshot> current() const { return currentPtr; }     // nur Message-Thread
    std::atomic<int> revision { 0 };                // zaehlt Aenderungen der erzeugten Musik (fuer die GUI)

    // ---- Export (Message-Thread)
    bool exportPart(int part, const juce::File& file, juce::String* err = nullptr) const;
    int  exportSeparate(const juce::File& dir, const juce::String& baseName, juce::String* err = nullptr) const;   // Anzahl Dateien
    bool exportCombined(const juce::File& file, juce::String* err = nullptr) const;
    juce::File exportDirectory() const;

    // ---- Wiedergabe-Anzeige (Audio-Thread schreibt, GUI liest)
    std::atomic<double> playTicks { -1.0 };

    juce::AudioProcessorValueTreeState apvts;
    nwa::PresetManager presets;
    juce::ValueTree makeExtra() const;
    void applyExtra(const juce::ValueTree& extra);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void install(std::shared_ptr<Snapshot> s, bool pushUndo);
    std::shared_ptr<Snapshot> make(Project p) const;
    int seedFor(const char* id) const { return getInt(id); }

    std::shared_ptr<Snapshot> currentPtr;
    std::deque<std::shared_ptr<Snapshot>> history;          // haelt die letzten Staende fuer den Audio-Thread am Leben
    std::vector<std::shared_ptr<Snapshot>> undoStack;
    std::atomic<const Snapshot*> live { nullptr };
    Ctx generatedCtx;

    // Audio
    PreviewSynth synth;
    double sr = 44100.0, internalTicks = 0.0, expectedTicks = -1.0; bool wasRunning = false;
    std::array<std::atomic<float>*, 5> pMute {};
    std::atomic<float> *pBpm = nullptr, *pSync = nullptr, *pPlay = nullptr, *pVol = nullptr;
    std::array<bool, 5> wasMuted {};
    std::vector<float> scratchL, scratchR;
};
} // namespace mel
