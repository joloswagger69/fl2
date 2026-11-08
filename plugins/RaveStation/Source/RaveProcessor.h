#pragma once
#include <nwa/PresetManager.h>
#include <nwa/Util.h>
#include "PluginInfo.h"
#include "engine/Engine.h"
#include "engine/Morph.h"
#include "engine/Slicer.h"
#include "engine/Generator.h"
#include "engine/Export.h"
#include <deque>

namespace rave
{
// Reihenfolge der Pad-Parameter (Index in der Tabelle)
enum PadParam { pVol, pPan, pPitch, pAtk, pDec, pStart, pEnd, pRev, pMute, pSolo, pLp, pHp, pRes, pDrv, pCrush, pChoke, pKeep, pStretch, pLayer, pStutter, pGlitch, pRole, kPadParamCount };
inline const char* const kRoleNames = "Kick|Snare|Clap|Closed Hat|Open Hat|Perc|Tom|FX|None";
inline const char* const kTimeSigNames = "2/4|3/4|4/4|5/4|6/8|7/8|9/8|12/8";
inline const char* const kGenreNames = "Jungle|Drum & Bass|Breakbeat|UK Garage|Oldschool Rave|Hardcore|Techno|Experimental";
inline const char* const kStepDivNames = "1/8|1/16|1/32";

juce::String padParamId(int pad, int p);
juce::String padParamName(int p);

class RaveProcessor : public juce::AudioProcessor
{
public:
    RaveProcessor();
    ~RaveProcessor() override;

    // AudioProcessor
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "RAVE//STATION"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // ---- Zugriff fuer Editor / Tests (Message-Thread) ----
    PadParams readPad(int pad) const;
    GlobalParams readGlobal() const;
    float getRaw(const juce::String& id) const { auto* p = apvts.getRawParameterValue(id); return p ? p->load() : 0.f; }
    void setParam(const juce::String& id, float plainValue);                     // setzt mit Host-Gesten

    SamplePtr padSample(int pad) const { return pad >= 0 && pad < kPads ? source[(size_t) pad] : nullptr; }
    SamplePtr padLayer(int pad) const  { return pad >= 0 && pad < kPads ? layerS[(size_t) pad] : nullptr; }
    bool setPadSample(int pad, SamplePtr s);   // false, wenn der Audio-Thread die Nachricht nicht annehmen konnte
    bool setPadLayer(int pad, SamplePtr s);
    void clearPad(int pad) { setPadSample(pad, nullptr); }
    void loadDefaultKit();

    juce::ValueTree makeExtra() const;
    void applyExtra(const juce::ValueTree& extra);

    // ---- Slicer (Message-Thread; modelLock schuetzt Modell gegen gleichzeitiges Speichern durch den Host) ----
    slicer::SliceModel sliceModel;
    std::vector<slicer::SliceOp> arrangement;
    int sliceLoopSamples = 0;                  // Laenge des Loops im Break (0 = ganzes Sample)
    float sliceSensitivity = 0.6f;
    juce::CriticalSection modelLock;
    bool loadBreakFile(const juce::File& f, juce::String* error = nullptr);
    void setBreak(SamplePtr s, bool detect = true);
    void detectSlices(float sensitivity);
    int  distributeSlicesToPads(int firstPad);                       // Anzahl verteilter Slices
    void slicesToPatternSlot(int slot, int firstPad, int steps);      // Break als Slice-Pattern nachspielen
    SamplePtr renderArranged(const juce::String& name);               // Arrangement -> neues Sample
    void previewSample(SamplePtr s);                                  // Vorhoeren ohne Pad (Audio-Thread mischt es dazu)
    void stopPreview();
    void previewSlice(int idx);

    // ---- Generator / EVOLVE / Pattern-Werkzeuge (Message-Thread) ----
    gen::GenParams readGenParams(int lengthSteps) const;
    gen::RoleMap roleMap() const;
    gen::GenResult generatePattern(int slot, bool applyTempoAndSwing);            // schreibt in einen Slot (ueberschreibt ihn!)
    void evolveFrom(int sourceSlot, float amount);                                // fuellt Varianten-Slots 16..19, Quelle bleibt unveraendert
    bool acceptVariant(int variantIndex, int destSlot);                           // Variante -> Nutzer-Slot (erst dann wird etwas ueberschrieben)
    void discardVariants();
    int  variantSource() const { return evolveSource.load(); }
    // Rueckgaengig fuer Pattern-Aenderungen (Message-Thread, max. 40 Schritte)
    void pushUndo(int slot) { PlainPattern p; bank.load(slot, p); undoStack.emplace_back(slot, p); if (undoStack.size() > 40) undoStack.erase(undoStack.begin()); }
    bool popUndo() { if (undoStack.empty()) return false; bank.store(undoStack.back().first, undoStack.back().second); undoStack.pop_back(); ++editRevision; return true; }
    bool canUndo() const { return ! undoStack.empty(); }
    void copyPattern(int from, int to);
    void clearPattern(int slot);
    int  stepsPerBarNow() const;

    // ---- Export (Message-Thread) ----
    bool exportMidi(int slot, const juce::File& f, int loops, bool rollProbability, juce::String* error = nullptr) const;
    bool exportPatternWav(int slot, const juce::File& f, int loops, bool wrapTail, double sampleRate, juce::String* error = nullptr);
    int  exportStems(int slot, const juce::File& dir, int loops, bool wrapTail, double sampleRate);   // eine Datei pro Pad, Anzahl geschrieben
    int  exportSlices(const juce::File& dir, const juce::String& baseName, juce::String* error = nullptr);
    bool exportPadWav(int pad, const juce::File& f, bool processed, juce::String* error = nullptr);       // processed: mit Pad-Einstellungen
    bool resamplePad(int pad);                                                                            // Pad-Klang "backen" und Einstellungen neutralisieren
    SamplePtr bounce(int pad) const;

    // ---- Kits ----
    juce::File kitsDirectory() const;
    juce::StringArray listKits() const;
    bool saveKit(const juce::String& name, juce::String* error = nullptr);
    bool loadKit(const juce::String& name, juce::String* error = nullptr);

    PatternBank bank;
    RaveEngine engine { bank };
    juce::AudioProcessorValueTreeState apvts;
    nwa::PresetManager presets;
    std::atomic<int> editRevision { 0 };
    std::atomic<int> evolveSource { -1 };
    std::atomic<int> evolveCounter { 0 };      // zaehlt Aenderungen an Samples/Patterns (Editor zeichnet neu)

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    class Worker;
    void postProcessedClear(int pad);

    std::array<SamplePtr, kPads> source, layerS;       // Message-Thread
    std::array<std::atomic<int>, kPads> generation;
    juce::CriticalSection sampleLock;
    std::vector<std::pair<int, PlainPattern>> undoStack;
    std::unique_ptr<Worker> worker;

    std::atomic<float>* pp[kPads][kPadParamCount] {};
    std::atomic<float>* gBpm = nullptr; std::atomic<float>* gSync = nullptr; std::atomic<float>* gPlay = nullptr; std::atomic<float>* gSwing = nullptr;
    std::atomic<float>* gHumT = nullptr; std::atomic<float>* gHumV = nullptr; std::atomic<float>* gDiv = nullptr; std::atomic<float>* gPat = nullptr;
    std::atomic<float>* gMaster = nullptr; std::atomic<float>* gSeed = nullptr;
    double sr = 44100.0;
    // Vorhoeren: Message-Thread legt ab, Audio-Thread uebernimmt per Try-Lock (kein Warten, keine Allokation)
    juce::SpinLock previewLock;
    SamplePtr previewPending; bool previewNew = false;
    std::deque<SamplePtr> previewHolder;      // haelt Referenzen, damit der Audio-Thread nie der letzte Besitzer ist
    SamplePtr previewActive; double previewPos = 0.0;
    friend class Worker;
};
} // namespace rave
