#pragma once
#include "BassAcid.h"
#include <memory>

namespace mel
{
enum Genre { GHouse, GTechno, GTrance, GSynthwave, GPop, GHipHop, GAmbient, GDnB, GGarage, GRave, kNumGenres };
inline const char* kGenreNames = "House|Techno|Trance|Synthwave|Pop|Hip-Hop|Ambient|Drum & Bass|UK Garage|Rave";

// Alle Einstellungen eines Genres: so setzt "Make a Track" die Parameter der einzelnen Bereiche.
struct GenreSetup
{
    double bpm; int scaleMinor; /*Vorschlag, wird nur verwendet wenn t_scale-Automatik an ist*/ float swing;
    ChordParams chords; MelodyParams melody; BassParams bass; ArpParams arp; bool acid; AcidParams acidP;
};

inline GenreSetup genreSetup(int genre, int mood)
{
    GenreSetup g{}; g.chords.mood = mood; g.chords.voiceLeading = true;
    g.bpm = 120; g.swing = 0.f; g.acid = false;
    switch (genre)
    {
        case GHouse:     g.bpm = 124; g.swing = 0.12f; g.chords = { mood, 0.5f, 2, VClose, 0, true, 1, 1, false, false };
                         g.melody = { McHook, 0.45f, 0.45f, 0.85f, 0.25f, 7, 0, true, 0 }; g.bass = { BsOffbeat, 0.8f, 1, true, 0 }; g.arp = { ApUp, 2, 0.6f, 2, 0 }; break;
        case GTechno:    g.bpm = 132; g.swing = 0.05f; g.chords = { mood, 0.3f, 1, VClose, 0, true, 0, 1, false, false };
                         g.melody = { McAtmos, 0.3f, 0.5f, 0.5f, 0.4f, 9, 0, true, 0 }; g.bass = { BsRolling, 0.7f, 1, false, 0 }; g.arp = { ApUp, 2, 0.5f, 2, 0 }; g.acid = true; g.acidP.character = AcDriving; break;
        case GTrance:    g.bpm = 138; g.swing = 0.f; g.chords = { mood, 0.45f, 2, VOpen, 0, true, 1, 0, false, false };
                         g.melody = { McLead, 0.6f, 0.45f, 0.7f, 0.3f, 9, 0, true, 0 }; g.bass = { BsOffbeat, 0.9f, 1, false, 0 }; g.arp = { ApUpDown, 2, 0.8f, 2, 0 }; break;
        case GSynthwave: g.bpm = 100; g.swing = 0.f; g.chords = { mood, 0.4f, 1, VDrop2, 0, true, 1, 0, false, false };
                         g.melody = { McLead, 0.5f, 0.35f, 0.75f, 0.25f, 7, 0, true, 0 }; g.bass = { BsOctave, 0.9f, 1, false, 0 }; g.arp = { ApUpDown, 1, 0.8f, 2, 0 }; break;
        case GPop:       g.bpm = 118; g.swing = 0.f; g.chords = { mood, 0.3f, 1, VClose, 0, true, 1, 2, false, false };
                         g.melody = { McMelody, 0.5f, 0.35f, 0.7f, 0.3f, 7, 0, true, 0 }; g.bass = { BsEighths, 0.7f, 1, true, 0 }; g.arp = { ApUp, 1, 0.6f, 1, 0 }; break;
        case GHipHop:    g.bpm = 90; g.swing = 0.25f; g.chords = { mood, 0.55f, 2, VDrop2, 0, true, 1, 3, false, false };
                         g.melody = { McMelody, 0.35f, 0.5f, 0.6f, 0.4f, 5, 0, true, 0 }; g.bass = { BsSynco, 0.6f, 1, true, 0 }; g.arp = { ApSkip, 1, 0.5f, 1, 0 }; break;
        case GAmbient:   g.bpm = 80; g.swing = 0.f; g.chords = { mood, 0.7f, 3, VSpread, 0, true, 0, 0, true, false };
                         g.melody = { McAtmos, 0.25f, 0.2f, 0.4f, 0.5f, 10, 0, true, 0 }; g.bass = { BsRoot, 0.5f, 1, false, 0 }; g.arp = { ApUp, 1, 1.0f, 3, 0 }; break;
        case GDnB:       g.bpm = 174; g.swing = 0.f; g.chords = { mood, 0.5f, 2, VClose, 0, true, 0, 0, false, false };
                         g.melody = { McLead, 0.45f, 0.6f, 0.6f, 0.3f, 7, 0, true, 0 }; g.bass = { BsSynco, 0.8f, 1, true, 0 }; g.arp = { ApUp, 2, 0.5f, 2, 0 }; break;
        case GGarage:    g.bpm = 132; g.swing = 0.3f; g.chords = { mood, 0.6f, 2, VClose, 0, true, 1, 3, false, false };
                         g.melody = { McMelody, 0.4f, 0.6f, 0.6f, 0.35f, 7, 0, true, 0 }; g.bass = { BsSynco, 0.7f, 1, true, 0 }; g.arp = { ApSkip, 2, 0.5f, 1, 0 }; break;
        default:         g.bpm = 150; g.swing = 0.f; g.chords = { mood, 0.25f, 1, VClose, 0, true, 1, 1, false, false };   // Rave
                         g.melody = { McHook, 0.55f, 0.5f, 0.9f, 0.2f, 9, 0, true, 0 }; g.bass = { BsRolling, 0.6f, 1, false, 0 }; g.arp = { ApUp, 2, 0.5f, 2, 0 }; g.acid = true; g.acidP.character = AcSynco; break;
    }
    g.chords.mood = mood;
    return g;
}

struct Project
{
    Ctx ctx;
    Progression prog;
    Part chords, melody, bass, arp, acid;
    static constexpr int kParts = 5;
    Part& part(int i) { Part* p[] = { &chords, &melody, &bass, &arp, &acid }; return *p[std::clamp(i, 0, 4)]; }
    const Part& part(int i) const { const Part* p[] = { &chords, &melody, &bass, &arp, &acid }; return *p[std::clamp(i, 0, 4)]; }
};
inline const char* kPartNames[] = { "Chords", "Melody", "Bass", "Arp", "Acid" };

inline int swingTicksFor(float swing) { return (int) std::lround(std::clamp(swing, 0.f, 0.5f) * kStep); }

// Alles auf einmal ("Make a Track"): Akkorde, Melodie, Bass, Arpeggio (+ optional Acid) passen harmonisch zusammen.
inline Project generateAll(const Ctx& cx, GenreSetup g, int seed, bool includeAcid)
{
    Project p; p.ctx = cx;
    const int sw = swingTicksFor(g.swing);
    g.melody.swingTicks = sw; g.bass.swingTicks = sw; g.acidP.swingTicks = sw;
    p.prog = generateProgression(cx, g.chords, seed);
    p.chords = chordPart(cx, p.prog, g.chords.rhythm, sw);
    p.melody = generateMelody(cx, p.prog, g.melody, seed + 1);
    p.bass = generateBass(cx, p.prog, g.bass, seed + 2);
    p.arp = generateArp(cx, p.prog, g.arp, seed + 3);
    if (includeAcid) p.acid = generateAcid(cx, g.acidP, seed + 4);
    return p;
}

// Tick-Ereignisse fuer Vorhoeren: sortierte Liste aus Ein-/Ausschaltungen
struct Ev { int tick; int part; int pitch; int vel; bool on; bool slideIn; };
inline std::vector<Ev> buildEvents(const Project& p)
{
    std::vector<Ev> e;
    for (int part = 0; part < Project::kParts; ++part)
        for (auto& n : p.part(part))
        {
            e.push_back({ n.tick, part, n.pitch, n.vel, true, false });
            e.push_back({ std::min(n.tick + n.len, p.ctx.totalTicks()), part, n.pitch, 0, false, n.slide });
        }
    std::stable_sort(e.begin(), e.end(), [](const Ev& a, const Ev& b) { if (a.tick != b.tick) return a.tick < b.tick; return a.on < b.on; });   // Offs vor Ons
    return e;
}
} // namespace mel
