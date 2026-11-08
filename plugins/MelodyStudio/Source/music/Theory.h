#pragma once
// Musiktheorie-Grundlagen: Tonleitern, Akkorde (diatonisch, Erweiterungen, Sus/Dim/Aug), Benennung, Voicings, Voice Leading.
// Reine Funktionen ohne JUCE-Abhaengigkeit ausser juce::String fuer Namen.
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mel
{
constexpr int kPpq = 480;     // MIDI-Ticks pro Viertel
constexpr int kStep = 120;    // Ticks pro 16tel

// Deterministischer Zufall (splitmix64) - plattformunabhaengig, damit Seeds ueberall dasselbe ergeben.
struct Rng
{
    uint64_t s;
    explicit Rng(uint64_t seed = 1) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    uint64_t next() { uint64_t z = (s += 0x9E3779B97F4A7C15ull); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
    double uni() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }
    bool chance(double p) { return uni() < p; }
    int range(int n) { return n <= 0 ? 0 : (int) (next() % (uint64_t) n); }          // 0..n-1
    int between(int a, int b) { return a + range(b - a + 1); }                       // a..b
    template <typename V> int pickWeighted(const V& w) { double t = 0; for (auto x : w) t += (double) x; if (t <= 0) return 0; double r = uni() * t; for (size_t i = 0; i < w.size(); ++i) { r -= (double) w[i]; if (r < 0) return (int) i; } return (int) w.size() - 1; }
};

enum Scale { Major, Minor, HarmMinor, MelMinor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, MinPent, MajPent, Blues, kNumScales };
inline const char* kScaleNames = "Major|Minor|Harmonic Minor|Melodic Minor|Dorian|Phrygian|Lydian|Mixolydian|Locrian|Minor Pentatonic|Major Pentatonic|Blues";
inline const char* kKeyNames = "C|C#|D|D#|E|F|F#|G|G#|A|A#|B";

inline const std::vector<int>& scaleNotes(int s)
{
    static const std::vector<int> t[kNumScales] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 11 },
        { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 4, 6, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 1, 3, 5, 6, 8, 10 },
        { 0, 3, 5, 7, 10 }, { 0, 2, 4, 7, 9 }, { 0, 3, 5, 6, 7, 10 } };
    return t[std::clamp(s, 0, (int) kNumScales - 1)];
}
inline int chordScaleOf(int s) { return (s == MinPent || s == Blues) ? (int) Minor : (s == MajPent ? (int) Major : std::clamp(s, 0, (int) kNumScales - 1)); }
inline bool isMinorish(int s) { return scaleNotes(chordScaleOf(s))[2] == 3; }
inline bool inScale(int midi, int key, int scale) { const int pc = ((midi - key) % 12 + 12) % 12; for (int v : scaleNotes(scale)) if (v == pc) return true; return false; }

// Tonart-Schreibweise (Kreuze/Bes) nach der parallelen Dur-Tonart
inline bool useFlats(int key, int scale)
{
    const auto& sc = scaleNotes(chordScaleOf(scale));
    const int majorRoot = ((key + (sc[2] == 3 ? 3 : 0) - (scale == Dorian ? 2 : 0) - (scale == Phrygian ? 4 : 0) - (scale == Lydian ? -5 : 0) - (scale == Mixolydian ? 7 : 0) - (scale == Locrian ? 11 : 0)) % 12 + 12) % 12;
    return majorRoot == 5 || majorRoot == 10 || majorRoot == 3 || majorRoot == 8 || majorRoot == 1 || majorRoot == 6;
}
inline juce::String noteName(int pc, bool flats)
{
    static const char* sh[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* fl[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return (flats ? fl : sh)[((pc % 12) + 12) % 12];
}

struct Chord
{
    int root = 0;                 // Tonhoehenklasse 0..11
    std::vector<int> iv { 0, 4, 7 };   // Halbtoene ueber dem Grundton, aufsteigend (Erweiterungen > 12)
    juce::String name = "C";
    int degree = -1;              // Stufe in der Tonleiter (0-basiert), -1 = chromatisch (z. B. Zwischendominante)
    bool has(int semis) const { for (int v : iv) if (v % 12 == semis % 12) return true; return false; }
    std::vector<int> pitchClasses() const { std::vector<int> r; for (int v : iv) r.push_back((root + v) % 12); return r; }
};

// Benennt einen Akkord aus seinen Intervallen (Dur/Moll, Sus, Dim, Aug, 7/maj7, 9, 11, 13).
inline juce::String chordName(int root, const std::vector<int>& iv, bool flats)
{
    auto has = [&](int s) { for (int v : iv) if (v == s) return true; return false; };
    auto hasPc = [&](int s) { for (int v : iv) if (v % 12 == s) return true; return false; };
    const bool maj3 = has(4), min3 = has(3), sus2 = has(2) && ! maj3 && ! min3, sus4 = has(5) && ! maj3 && ! min3;
    const bool dim5 = has(6), aug5 = has(8) && ! has(7);
    const bool b7 = has(10), M7 = has(11), d7 = has(9) && min3 && dim5;
    const bool n9 = has(14) || has(13) || has(15), n11 = has(17) || has(18), n13 = has(21) || has(20);
    juce::String q;
    int ext = 0;                                   // 7, 9, 11, 13
    if (n13) ext = 13; else if (n11) ext = 11; else if (n9) ext = 9; else if (b7 || M7 || d7) ext = 7;
    juce::String n = noteName(root, flats);
    (void) hasPc;
    if (d7) return n + "dim7";
    if (min3 && dim5) return n + (b7 ? "m7b5" : "dim");
    if (maj3 && aug5) return n + "aug" + (ext ? juce::String(ext) : juce::String());
    juce::String sus = sus2 ? "sus2" : sus4 ? "sus4" : "";
    if (sus.isNotEmpty()) return n + (ext ? juce::String(ext) : juce::String()) + sus;
    if (min3) { n += "m"; if (M7) return n + (ext > 7 ? "Maj" : "Maj") + juce::String(ext ? ext : 7); return n + (ext ? juce::String(ext) : juce::String()); }
    if (M7) return n + "maj" + juce::String(ext ? ext : 7);
    if (ext == 0) return n;
    return n + juce::String(ext);
}

// Diatonischer Akkord auf einer Stufe. count = Anzahl Akkordtoene (3 Dreiklang, 4 Septakkord, 5 None, 6 Undezime, 7 Tredezime).
inline Chord diatonic(int key, int scale, int degree, int count, int sus = 0)
{
    const auto& sc = scaleNotes(chordScaleOf(scale));
    degree = ((degree % 7) + 7) % 7;
    auto note = [&](int k) { const int idx = degree + k; return sc[(size_t) (idx % 7)] + 12 * (idx / 7); };
    Chord c; const int r = note(0);
    c.root = (key + r) % 12; c.degree = degree; c.iv.clear();
    count = std::clamp(count, 3, 7);
    for (int j = 0; j < count; ++j) c.iv.push_back(note(2 * j) - r);
    if (c.has(4) && c.iv.size() > 5 && c.iv[5] == 17) c.iv[5] = 18;      // natuerliche 11 auf Dur-Terz klingt reibend -> #11
    if (sus == 2 || sus == 4) { if (c.has(3) || c.has(4)) c.iv[1] = (sus == 2 ? 2 : 5); if (c.iv[2] == 6) c.iv[2] = 7; std::sort(c.iv.begin(), c.iv.end()); }
    c.name = chordName(c.root, c.iv, useFlats(key, scale));
    return c;
}
inline Chord makeChord(int root, std::vector<int> iv, int key, int scale, int degree = -1)
{
    Chord c; c.root = ((root % 12) + 12) % 12; c.iv = std::move(iv); c.degree = degree; c.name = chordName(c.root, c.iv, useFlats(key, scale)); return c;
}

enum Voicing { VClose, VDrop2, VOpen, VSpread, kNumVoicings };
inline const char* kVoicingNames = "Close|Drop 2|Open|Spread";

// Konkrete MIDI-Toene eines Akkords. inversion 0..n-1 (wird begrenzt), shift in Halbtonen (Vielfache von 12).
inline std::vector<int> voiceChord(const Chord& c, int voicing, int inversion, int shift = 0, int baseMidi = 48)
{
    const int r0 = baseMidi + ((c.root - baseMidi) % 12 + 12) % 12;
    std::vector<int> n;
    const int cnt = (int) c.iv.size();
    if (voicing == VOpen || voicing == VSpread)
    {
        n.push_back(r0 - 12);
        int fifth = -1; for (size_t j = 1; j < c.iv.size(); ++j) if (c.iv[j] % 12 == 7 || c.iv[j] % 12 == 6 || c.iv[j] % 12 == 8) { fifth = (int) j; break; }
        if (voicing == VSpread && fifth >= 0) n.push_back(r0 - 12 + c.iv[(size_t) fifth]);
        for (int j = 1; j < cnt; ++j)
        {
            if (voicing == VSpread && j == fifth) continue;
            n.push_back(r0 + c.iv[(size_t) j] + (c.iv[(size_t) j] < 12 ? 12 : 0));
        }
        std::sort(n.begin(), n.end());
        int inv = std::clamp(inversion, 0, std::max(0, (int) n.size() - 1));
        for (int i = 0; i < inv; ++i) { n[0] += 12; std::sort(n.begin(), n.end()); }
    }
    else
    {
        for (int j = 0; j < cnt; ++j) n.push_back(r0 + c.iv[(size_t) j]);
        std::sort(n.begin(), n.end());
        int inv = std::clamp(inversion, 0, std::min(cnt - 1, 3));     // Erweiterungen (9, 11, 13) liegen nie im Bass
        for (int i = 0; i < inv; ++i) { n[0] += 12; std::sort(n.begin(), n.end()); }
        if (voicing == VDrop2 && n.size() >= 4) { n[n.size() - 2] -= 12; std::sort(n.begin(), n.end()); }
    }
    for (auto& v : n) v += shift;
    return n;
}

// Bewegungskosten zwischen zwei Voicings (kleiner = sanfter). Beruecksichtigt Gemeinsamtoene und Lage.
inline double voiceLeadCost(const std::vector<int>& prev, const std::vector<int>& next)
{
    if (prev.empty()) return 0.0;
    double cost = 0.0;
    for (int v : next) { int best = 99; for (int p : prev) best = std::min(best, std::abs(v - p)); cost += best; }
    for (int p : prev) { int best = 99; for (int v : next) best = std::min(best, std::abs(v - p)); cost += 0.5 * best; }
    return cost;
}
inline double rangePenalty(const std::vector<int>& v)
{
    double pen = 0.0, mean = 0.0;
    for (int x : v) { mean += x; if (x < 40) pen += (40 - x) * 3.0; if (x > 84) pen += (x - 84) * 3.0; }
    mean /= std::max<size_t>(1, v.size());
    return pen + std::abs(mean - 60.0) * 0.4;
}
// Bestes Voicing mit Voice Leading (alle Umkehrungen x Oktavlagen).
inline std::vector<int> voiceLead(const Chord& c, int voicing, const std::vector<int>& prev)
{
    std::vector<int> best; double bestCost = 1e18;
    const int cnt = (int) c.iv.size();
    for (int inv = 0; inv < std::min(cnt, 4); ++inv)
        for (int sh : { -12, 0, 12 })
        {
            auto v = voiceChord(c, voicing, inv, sh);
            const double cost = voiceLeadCost(prev, v) + rangePenalty(v) + (prev.empty() ? std::abs(inv) * 0.2 : 0.0);
            if (cost < bestCost) { bestCost = cost; best = std::move(v); }
        }
    return best;
}
} // namespace mel
