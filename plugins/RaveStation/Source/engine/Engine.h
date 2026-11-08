#pragma once
// Sample-Engine mit Pads, Stimmen und Step-Sequencer. Unabhaengig von JUCE-Plugin-Klassen,
// damit sie sowohl im Audio-Callback als auch fuer Offline-Rendering (Export, Resample, Tests) laeuft.
//
// Threading:
//  * process() laeuft im Audio-Thread: keine Allokation, keine Sperren, keine Dateizugriffe.
//  * Samples kommen ueber postSample() (Message-/Worker-Thread) per FIFO in den Audio-Thread.
//    Freigegebene Samples gehen ueber einen zweiten FIFO zurueck und werden in drainTrash() zerstoert.
//  * Patterns liegen in der PatternBank (Atomics), die GUI schreibt, der Audio-Thread liest.
#include "Pattern.h"
#include "Sample.h"
#include "Dsp.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace rave
{
struct PadParams
{
    float gain = 1.f;        // linear
    float pan = 0.f;         // -1..1
    float pitch = 0.f;       // Halbtoene
    float atk = 0.f;         // Sekunden
    float dec = 10.f;        // Sekunden; >= 9.99 = aus (natuerliche Laenge)
    float start = 0.f, end = 1.f;
    bool rev = false, mute = false, solo = false;
    float lp = 20000.f, hp = 20.f, res = 0.f;
    float drv = 0.f, crush = 0.f;
    int choke = 0;           // 0 = keine, 1..8
    bool keep = false;       // Pitch ohne Laengenaenderung (Time-Stretch-Pitch)
    float stretch = 1.f;     // Zeitfaktor (Ausgabelaenge / Eingabelaenge)
    float layer = 0.f;       // Pegel von Layer B
    int stutter = 0;         // 0 aus, 1..: Teilung (siehe stutterBeats)
    float glitch = 0.f;      // 0..1 Wahrscheinlichkeit
    int role = 8;            // fuer den Generator
};

inline double stutterBeats(int idx) noexcept
{
    static const double v[] = { 0.0, 0.5, 0.25, 0.125, 0.0625, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };
    return v[juce::jlimit(0, 7, idx)];
}
inline const char* const kStutterNames = "Off|1/8|1/16|1/32|1/64|1/8T|1/16T|1/32T";

// Schluessel, der beschreibt, welcher verarbeitete Sample-Zustand (Stretch / Pitch ohne Laengenaenderung) gebraucht wird.
inline uint32_t processedKey(const PadParams& p) noexcept
{
    const int st = (int) std::lround(p.stretch * 1000.f);
    const int pi = p.keep ? (int) std::lround(p.pitch * 100.f) : 0;
    return (uint32_t) (st * 73856093) ^ (uint32_t) (pi * 19349663) ^ (p.keep ? 0x5bd1e995u : 0u);
}
inline bool needsProcessing(const PadParams& p) noexcept { return std::abs(p.stretch - 1.f) > 0.001f || (p.keep && std::abs(p.pitch) > 0.005f); }

struct GlobalParams
{
    double bpm = 140.0;
    bool hostSync = true;
    bool playing = false;        // nur Intern-Modus
    float swing = 0.f;           // 0..1 (1.0 = 75:25)
    float humTime = 0.f, humVel = 0.f;
    int stepDiv = 1;             // 0 = 1/8, 1 = 1/16, 2 = 1/32
    int slot = 0;                // gewuenschtes Pattern (0..kPatternSlots-1); Wechsel am Pattern-Ende
    float master = 1.f;
    int seed = 1;
};

struct HostInfo
{
    bool valid = false, playing = false, hasPpq = false, hasBpm = false;
    double ppq = 0.0, bpm = 120.0;
};

class RaveEngine
{
public:
    static constexpr int kVoices = 64;
    static constexpr int kMaxEvents = 2048;
    enum SlotType { kSource = 0, kProcessed = 1, kLayer = 2 };

    explicit RaveEngine(PatternBank& bank);
    void prepare(double sampleRate, int maxBlock);
    void reset();                                  // Audio-Thread-sicher: beendet alle Stimmen

    // Message/Worker-Thread
    bool postSample(int pad, SlotType type, SamplePtr s, uint32_t key = 0);
    void drainTrash();
    void auditionPad(int pad, float vel);          // von der GUI, wird im naechsten Block gespielt
    void setPads(const PadParams (&p)[kPads]) { for (int i = 0; i < kPads; ++i) padsPending[i] = p[i]; }
    void setGlobal(const GlobalParams& g) { global = g; }

    // Audio-Thread
    void process(juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const HostInfo& host);

    // Anzeige (lock-free)
    std::atomic<int> uiStep { -1 };                // Position im aktiven Pattern, -1 = steht
    std::atomic<int> uiSlot { 0 };
    std::atomic<int> padHits[kPads];
    std::atomic<int> voicesActive { 0 };
    std::atomic<float> peak { 0.f };

private:
    struct Msg { int pad = 0; int type = 0; uint32_t key = 0; SamplePtr s; };
    struct Event { int offset = 0; int order = 0; int pad = 0; float vel = 1.f; int pitch = 0; bool ghost = false; int ratchetCount = 1; double lifeBeats = 0.0; int sub = 0; };

    struct PadSlots { SamplePtr src, proc, layer; uint32_t procKey = 0; };

    struct Voice
    {
        bool active = false, dying = false;
        int pad = 0; uint64_t age = 0;
        SamplePtr a, b;
        double posA = 0, posB = 0, incA = 0, incB = 0;
        double startA = 0, endA = 0, startB = 0, endB = 0;
        bool rev = false;
        float gl = 1.f, gr = 1.f, layerGain = 0.f;
        float atkGain = 0.f, atkInc = 1.f, decMul = 1.f, decOn = 0.f, amp = 1.f;
        float dieGain = 1.f, dieDec = 0.f;
        float fadeIn = 0.f, fadeInInc = 0.f;
        long long life = -1;                    // verbleibende Samples bis zum Ausblenden (-1 = unbegrenzt)
        double loopSrcA = 0, loopSrcB = 0;      // Stutter-Schleifenlaenge in Quellsamples (0 = aus)
        float loopFade = 1.f;
        float gLP = 0.f, gHP = 0.f;
        float drive = 0.f, crush = 0.f, res = 0.f;
        bool fx = false;
        Svf lpL, lpR, hpL, hpR;
        Crusher cL, cR;
    };

    void trigger(const Event& e);
    void killVoice(Voice& v);
    void releaseSample(SamplePtr&& s);
    void renderSegment(float* L, float* R, int start, int end);
    void scheduleSequencer(const HostInfo& host, int n, int& count);
    void pushEvent(int& count, const Event& e);
    bool padAudible(int pad) const;

    PatternBank& bank;
    double sr = 44100.0;
    int maxBlock = 512;
    PadParams pads[kPads], padsPending[kPads];
    GlobalParams global;
    PadSlots slots[kPads];
    std::array<Voice, kVoices> voices;
    uint64_t ageCounter = 0;
    std::array<Event, kMaxEvents> events;
    bool anySolo = false;

    // Mailbox
    juce::AbstractFifo inFifo { 512 }, trashFifo { 2048 }, audFifo { 256 };
    std::array<Msg, 512> inBuf;
    std::array<Msg, 2048> trashBuf;
    struct Aud { int pad; float vel; };
    std::array<Aud, 256> audBuf;
    juce::CriticalSection postLock, audLock, trashLock;

    // Transport
    bool wasRunning = false;
    double internalBeat = 0.0, lastEndBeat = 0.0;
    struct Cur { int slot = 0; long long startStep = 0; };
    Cur cur, prev;
    long long switchStep = 0;
    long long lastStepSeen = -1000000;
    float masterSmoothed = 1.f;
    double bpmNow = 120.0, stepBeatsNow = 0.25;
};
} // namespace rave
