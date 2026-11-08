#pragma once
#include "Theory.h"

namespace mel
{
struct Note { int tick = 0, len = 120, pitch = 60, vel = 90; bool slide = false; };
using Part = std::vector<Note>;

struct Ctx   // musikalischer Rahmen
{
    int key = 0, scale = Minor, bars = 4, sigNum = 4, sigDen = 4;
    int ticksPerBar() const { return sigNum * kPpq * 4 / sigDen; }
    int stepsPerBar() const { return ticksPerBar() / kStep; }
    int totalTicks() const { return bars * ticksPerBar(); }
};

struct ChordSlot { int tick = 0, len = 0; Chord chord; std::vector<int> voiced; };
using Progression = std::vector<ChordSlot>;

enum Mood { Happy, Uplifting, Energetic, Dreamy, Melancholic, Dark, Tense, kNumMoods };
inline const char* kMoodNames = "Happy|Uplifting|Energetic|Dreamy|Melancholic|Dark|Tense";
inline const char* kChordRhythmNames = "Sustained|Offbeat stabs|Pulse 8ths|Syncopated";
inline const char* kPerBarNames = "1 chord / 2 bars|1 chord / bar|2 chords / bar";
inline const char* kExtNames = "Triads|7ths|9ths|11ths|13ths";

struct ChordParams
{
    int mood = Melancholic; float complexity = 0.4f; int maxExt = 2; int voicing = VClose; int inversion = 0;
    bool voiceLeading = true; int perBar = 1; int rhythm = 0; bool sus = false, dimAug = false;
};

inline const ChordSlot* chordAt(const Progression& p, int tick)
{
    for (auto& s : p) if (tick >= s.tick && tick < s.tick + s.len) return &s;
    return p.empty() ? nullptr : &p.back();
}

namespace detail
{
// Progressions-Vorlagen als Stufen (0-basiert). [ton: 0 dur-artig, 1 moll-artig][stimmung][vorlage][4]
inline const int kTpl[2][kNumMoods][4][4] = {
  { // dur-artig
    /*Happy*/       { { 0, 4, 5, 3 }, { 0, 3, 4, 0 }, { 0, 5, 3, 4 }, { 0, 4, 0, 3 } },
    /*Uplifting*/   { { 0, 4, 5, 3 }, { 3, 0, 4, 5 }, { 0, 2, 5, 4 }, { 5, 3, 0, 4 } },
    /*Energetic*/   { { 0, 3, 0, 4 }, { 0, 4, 3, 4 }, { 0, 3, 4, 4 }, { 0, 0, 3, 4 } },
    /*Dreamy*/      { { 0, 2, 5, 3 }, { 3, 0, 2, 5 }, { 0, 3, 0, 5 }, { 0, 5, 2, 3 } },
    /*Melancholic*/ { { 5, 3, 0, 4 }, { 0, 5, 1, 4 }, { 5, 4, 3, 4 }, { 0, 2, 5, 1 } },
    /*Dark*/        { { 5, 2, 3, 4 }, { 5, 3, 4, 5 }, { 5, 4, 3, 4 }, { 0, 5, 4, 5 } },
    /*Tense*/       { { 0, 5, 6, 4 }, { 1, 4, 0, 6 }, { 0, 1, 6, 4 }, { 5, 6, 0, 4 } } },
  { // moll-artig
    /*Happy*/       { { 2, 6, 0, 3 }, { 0, 5, 2, 6 }, { 2, 3, 0, 6 }, { 0, 2, 6, 0 } },
    /*Uplifting*/   { { 0, 5, 2, 6 }, { 5, 6, 0, 2 }, { 2, 6, 5, 6 }, { 0, 6, 5, 6 } },
    /*Energetic*/   { { 0, 6, 5, 6 }, { 0, 0, 6, 5 }, { 0, 3, 6, 0 }, { 0, 5, 6, 6 } },
    /*Dreamy*/      { { 0, 5, 2, 6 }, { 5, 2, 6, 0 }, { 0, 3, 5, 6 }, { 2, 5, 0, 6 } },
    /*Melancholic*/ { { 0, 3, 6, 2 }, { 0, 5, 3, 4 }, { 0, 2, 3, 5 }, { 0, 5, 2, 6 } },
    /*Dark*/        { { 0, 5, 6, 0 }, { 0, 5, 4, 0 }, { 0, 3, 4, 0 }, { 0, 6, 5, 4 } },
    /*Tense*/       { { 0, 1, 4, 0 }, { 0, 5, 1, 4 }, { 0, 4, 5, 4 }, { 0, 1, 5, 4 } } } };

inline std::vector<ChordSlot> layout(const Ctx& cx, int perBar)
{
    const int total = cx.totalTicks();
    int slots = perBar == 0 ? std::max(1, cx.bars / 2) : perBar == 1 ? cx.bars : cx.bars * 2;
    slots = std::max(1, slots);
    std::vector<ChordSlot> v((size_t) slots);
    for (int i = 0; i < slots; ++i) { v[(size_t) i].tick = total * i / slots; v[(size_t) i].len = total * (i + 1) / slots - v[(size_t) i].tick; }
    return v;
}
} // namespace detail

inline Progression generateProgression(const Ctx& cx, const ChordParams& cp, int seed)
{
    Rng rng((uint64_t) seed * 7919u + 11u);
    const int tonal = isMinorish(cx.scale) ? 1 : 0;
    const int mood = std::clamp(cp.mood, 0, (int) kNumMoods - 1);
    auto slots = detail::layout(cx, cp.perBar);
    const int n = (int) slots.size();
    const int t1 = rng.range(4); int t2 = (t1 + 1 + rng.range(3)) % 4;
    std::vector<int> deg((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const int* t = detail::kTpl[tonal][mood][(i / 4) % 2 == 0 ? t1 : t2];
        deg[(size_t) i] = t[i % 4];
    }
    if (n == 1) deg[0] = 0;
    else if (n == 2) deg[1] = detail::kTpl[tonal][mood][t1][3];
    const int maxSize = 3 + std::clamp(cp.maxExt, 0, 4);
    const float cplx = std::clamp(cp.complexity, 0.f, 1.f);
    const bool flats = useFlats(cx.key, cx.scale);
    (void) flats;
    Progression out;
    for (int i = 0; i < n; ++i)
    {
        int size = 3;
        if (cplx > 0.12f && maxSize > 3)
        {
            const double t = cplx * (maxSize - 3) + (rng.uni() - 0.5) * 0.9;
            size = std::clamp(3 + (int) std::lround(t), 3, maxSize);
            if (deg[(size_t) i] == 4 && cplx > 0.3f) size = std::max(size, 4);       // Dominante meist mit Septime
        }
        int sus = 0;
        if (cp.sus && size <= 4 && deg[(size_t) i] != 6 && deg[(size_t) i] != 1 && rng.chance(0.2 + (mood == Dreamy ? 0.2 : 0.0))) sus = rng.chance(0.5) ? 2 : 4;
        ChordSlot s = slots[(size_t) i];
        s.chord = diatonic(cx.key, cx.scale, deg[(size_t) i], size, sus);
        out.push_back(std::move(s));
    }
    // Chromatische Faerbungen bei hoher Komplexitaet: Zwischendominanten, erweiterte Akkorde
    if (cplx > 0.6f && cp.maxExt >= 1)
        for (int i = 1; i + 1 < n; ++i)
        {
            const auto& nxt = out[(size_t) i + 1].chord;
            if (out[(size_t) i].chord.degree >= 0 && nxt.degree >= 0 && nxt.has(3) != nxt.has(4) && rng.chance(0.25) && nxt.iv.size() >= 3 && ! (nxt.has(3) && nxt.has(6) && ! nxt.has(7)))
            {
                ChordSlot s = out[(size_t) i]; s.chord = makeChord(nxt.root + 7, { 0, 4, 7, 10 }, cx.key, cx.scale, -1); out[(size_t) i] = s;
            }
        }
    if (cp.dimAug && cplx > 0.35f)
        for (int i = 0; i + 1 < n; ++i)
        {
            auto& c = out[(size_t) i].chord;
            if (c.degree == 4 && rng.chance(0.35) && c.iv.size() <= 4 && c.has(4) && out[(size_t) i + 1].chord.degree == 0) c = makeChord(c.root, { 0, 4, 8 }, cx.key, cx.scale, 4);                 // V+ -> I
            else if (n >= 4 && rng.chance(0.15 * cplx) && out[(size_t) i + 1].chord.degree >= 0) c = makeChord(out[(size_t) i + 1].chord.root - 1, { 0, 3, 6, 9 }, cx.key, cx.scale, -1);   // verminderter Durchgangsakkord
        }
    // Voicing + Voice Leading
    std::vector<int> prev;
    for (auto& s : out)
    {
        if (cp.voiceLeading) s.voiced = voiceLead(s.chord, cp.voicing, prev);
        else s.voiced = voiceChord(s.chord, cp.voicing, cp.inversion);
        prev = s.voiced;
    }
    return out;
}

// Rhythmus der Akkorde -> Noten
inline Part chordPart(const Ctx& cx, const Progression& prog, int rhythm, int swingTicks = 0)
{
    Part out;
    const int spb = cx.stepsPerBar();
    for (auto& s : prog)
    {
        auto add = [&](int tick, int len, int vel)
        {
            if (len < 1) return;
            const int end = std::min(tick + len, s.tick + s.len);
            for (int p : s.voiced) { Note n; n.tick = tick; n.len = end - tick; n.pitch = p; n.vel = vel; out.push_back(n); }
        };
        if (rhythm == 0) { add(s.tick, s.len, 82); continue; }
        std::vector<std::pair<int, int>> hits;   // (stepImTakt, laengeInSteps)
        if (rhythm == 1) for (int st = 2; st < spb; st += 4) hits.push_back({ st, 2 });
        else if (rhythm == 2) for (int st = 0; st < spb; st += 2) hits.push_back({ st, 2 });
        else { const int pat[] = { 0, 3, 6, 10, 12 }; const int ln[] = { 3, 3, 4, 2, 4 }; for (int k = 0; k < 5; ++k) if (pat[k] < spb) hits.push_back({ pat[k], std::min(ln[k], spb - pat[k]) }); }
        const int b0 = s.tick / cx.ticksPerBar(), b1 = (s.tick + s.len - 1) / cx.ticksPerBar();
        for (int b = b0; b <= b1; ++b)
            for (auto& h : hits)
            {
                int t = b * cx.ticksPerBar() + h.first * kStep;
                if (t < s.tick || t >= s.tick + s.len) continue;
                const bool odd = (h.first % 2) == 1;
                add(t + (odd ? swingTicks : 0), h.second * kStep - (rhythm == 2 ? 20 : 10), (h.first % 4 == 0) ? 92 : 80);
            }
    }
    return out;
}
} // namespace mel
