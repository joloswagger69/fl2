#pragma once
// Regelbasierter Breakbeat-Generator und EVOLVE-Mutation.
//
// Kein reiner Zufall: Pro Genre gibt es Gewichts-Schablonen (wie wichtig ist ein Schritt fuer Kick, Snare, Hats ...).
// Anker (Kick auf der Eins, Snare/Clap auf dem Backbeat) sind immer vorhanden. Regler verschieben, wie viele und wie
// ungewoehnliche Schritte dazukommen. Alles ist ueber einen Seed reproduzierbar (hash01), unabhaengig von Plattform und Compiler.
#include "Pattern.h"
#include "Dsp.h"
#include <string>
#include <vector>

namespace rave::gen
{
enum Genre { Jungle, DnB, Breakbeat, Garage, OldschoolRave, Hardcore, Techno, Experimental, kGenres };
enum Role { rKick, rSnare, rClap, rClosedHat, rOpenHat, rPerc, rTom, rFx, rNone, kRoles };

struct GenParams
{
    int genre = Jungle;
    float intensity = 0.6f, complexity = 0.5f, density = 0.5f, ghost = 0.4f, variation = 0.4f, randomness = 0.3f;
    int seed = 1;
    int stepDiv = 1;        // 0 = 1/8, 1 = 1/16, 2 = 1/32 (Schrittraster des Sequencers)
    int sig = 2;            // Index in kTimeSigNames: 2/4 3/4 4/4 5/4 6/8 7/8 9/8 12/8
    int lengthSteps = 32;   // gewuenschte Laenge; gerundet auf ganze Takte, max. 64
};

struct RoleMap
{
    std::vector<int> pads[kRoles];
    static RoleMap fromRoles(const int (&roles)[kPads])
    {
        RoleMap m;
        for (int i = 0; i < kPads; ++i) if (roles[i] >= 0 && roles[i] < kRoles) m.pads[roles[i]].push_back(i);
        return m;
    }
    int primary(int role) const { return pads[role].empty() ? -1 : pads[role][0]; }
    int alt(int role) const { return pads[role].size() > 1 ? pads[role][1] : primary(role); }
};

struct GenResult { PlainPattern pattern; double bpm = 140.0; float swing = 0.f; int stepsPerBar = 16; int bars = 2; };

inline double quartersPerBar(int sig) { static const double q[] = { 2, 3, 4, 5, 3, 3.5, 4.5, 6 }; return q[juce::jlimit(0, 7, sig)]; }
inline int stepsPerQuarter(int stepDiv) { return stepDiv == 0 ? 2 : (stepDiv == 1 ? 4 : 8); }
inline int stepsPerBar(int sig, int stepDiv) { return juce::jmax(1, (int) std::lround(quartersPerBar(sig) * stepsPerQuarter(stepDiv))); }

namespace detail
{
    // 16 Ziffern = Gewichte 0..9 fuer die Schritte eines 4/4-Taktes im 16tel-Raster (Index = Zaehlzeit*4 + 16tel)
    struct Tpl { const char *kick, *snare, *clap, *ch, *oh, *perc, *tom, *fx, *ghost; double bpm; float swing; };
    inline const Tpl& tpl(int g)
    {
        static const Tpl t[kGenres] = {
            /* Jungle  */ { "9060001000900200", "0000900000009000", "0000000000000000", "8383838383838383", "0000000000000030", "0010000101000010", "0000000000000000", "0000000000000000", "0002000403000034", 170.0, 0.10f },
            /* DnB     */ { "9000002000900000", "0000900000009000", "0000000000000000", "8282828282828282", "0000003000000030", "0001000000100000", "0000000000000000", "0000000000000000", "0000000303000003", 174.0, 0.00f },
            /* Breaks  */ { "9000005000700300", "0000900000009000", "0000200000002000", "8282828282828282", "0000000000000040", "0010000000100100", "0000000000000000", "0000000000000000", "0003000203000033", 130.0, 0.10f },
            /* Garage  */ { "9000000000900200", "0000900000009000", "0000500000005000", "6565656565656565", "0000000000000040", "0010000100100001", "0000000000000000", "0000000000000000", "0002000402000034", 132.0, 0.55f },
            /* Rave    */ { "9000900090009000", "0000300000003000", "0000900000009000", "6363636363636363", "0090009000900090", "0010001000100010", "0000000000000020", "0000000000000000", "0000000000000003", 140.0, 0.00f },
            /* Hardcore*/ { "9000900090009020", "0000900000009000", "0000500000005000", "7777777777777777", "0040004000400040", "0001000100010001", "0000000000000000", "0000000000000001", "0001000100010004", 170.0, 0.00f },
            /* Techno  */ { "9000900090009000", "0000200000002000", "0000900000009000", "5353535353535353", "0090009000900090", "0010001000100013", "0000000000000020", "0000000000000000", "0000000000000003", 132.0, 0.05f },
            /* Exp.    */ { "6222322242223222", "2223222422222323", "1111211112111112", "4444444444444444", "1111111111111111", "3333333333333333", "1111111111111111", "1000000000000000", "2222222222222222", 140.0, 0.00f },
        };
        return t[juce::jlimit(0, kGenres - 1, g)];
    }

    inline float w(const char* s, int idx) { return (float) (s[juce::jlimit(0, 15, idx)] - '0') / 9.f; }

    // Backbeat-Zaehlzeiten (0-basiert) je Taktart
    inline bool isBackbeat(int sig, int beat)
    {
        switch (juce::jlimit(0, 7, sig))
        {
            case 0: return beat == 1;
            case 1: return beat == 1;
            case 2: return beat == 1 || beat == 3;
            case 3: return beat == 1 || beat == 3;
            case 4: return beat == 1;
            case 5: return beat == 1;
            case 6: return beat == 1 || beat == 3;
            default: return beat == 1 || beat == 3 || beat == 5;
        }
    }

    struct Pos { int beat, sub, tIdx; bool downbeat, onBeat, offGrid; };
    inline Pos posOf(int stepInBar, int spq)
    {
        Pos p; p.beat = stepInBar / spq; p.sub = stepInBar % spq;
        const int tsub = juce::jlimit(0, 3, p.sub * 4 / spq);
        p.tIdx = (p.beat % 4) * 4 + tsub;
        p.downbeat = stepInBar == 0; p.onBeat = p.sub == 0;
        p.offGrid = (spq >= 4 && (p.sub * 4 / spq) % 2 == 1) || (spq == 8 && (p.sub & 1));
        return p;
    }
    inline uint64_t S(int seed) { return (uint64_t) (unsigned) seed * 0x9E3779B1ull + 12345ull; }
}

// ---------------------------------------------------------------------------------------------------------- Generator
inline GenResult generate(const GenParams& gp, const RoleMap& rm)
{
    using namespace detail;
    GenResult res;
    const auto& T = tpl(gp.genre);
    const int spq = stepsPerQuarter(gp.stepDiv);
    const int spb = stepsPerBar(gp.sig, gp.stepDiv);
    res.stepsPerBar = spb;
    res.bars = juce::jmax(1, juce::jmin(64 / spb, juce::jmax(1, gp.lengthSteps / spb)));
    res.pattern.length = res.bars * spb;
    res.bpm = T.bpm; res.swing = T.swing;
    const float I = juce::jlimit(0.f, 1.f, gp.intensity), C = juce::jlimit(0.f, 1.f, gp.complexity), D = juce::jlimit(0.f, 1.f, gp.density);
    const float G = juce::jlimit(0.f, 1.f, gp.ghost), V = juce::jlimit(0.f, 1.f, gp.variation), R = juce::jlimit(0.f, 1.f, gp.randomness);
    const uint64_t seed = S(gp.seed);
    const float gamma = 2.0f - 1.4f * D;
    const bool isExp = gp.genre == Experimental;

    auto setHit = [&](int pad, int step, int vel, int flags = 0, int ratchet = 1, int pitch = 0, int prob = 100)
    {
        if (pad < 0 || step < 0 || step >= res.pattern.length) return;
        Step s; s.vel = (uint8_t) juce::jlimit(1, 127, vel); s.flags = (uint8_t) flags; s.ratchet = (uint8_t) juce::jlimit(1, 8, ratchet); s.pitch = (int8_t) pitch; s.prob = (uint8_t) prob;
        res.pattern.s[pad][step] = s;
    };
    auto occupied = [&](int role, int step) { for (int p : rm.pads[role]) if (res.pattern.s[p][step].on()) return true; return false; };
    auto jitter = [&](int a, int b, int c) { return (hash01((uint64_t) a, (uint64_t) b, seed + (uint64_t) c) - 0.5f) * 2.f; };

    for (int bar = 0; bar < res.bars; ++bar)
    {
        for (int sb = 0; sb < spb; ++sb)
        {
            const int step = bar * spb + sb;
            const auto pos = posOf(sb, spq);
            // Variation: in Folgetakten wird ein Schritt nur mit Wahrscheinlichkeit V neu gewuerfelt (Salz = Taktnummer)
            const int salt = (bar > 0 && hash01((uint64_t) step, 77, seed) < V) ? bar : 0;
            auto roll = [&](int role, int extra = 0) { return hash01((uint64_t) (sb * 16 + role + extra), (uint64_t) salt, seed); };
            const float offBoost = pos.offGrid ? (0.45f + 1.1f * C) : 1.f;
            const bool backbeat = pos.onBeat && isBackbeat(gp.sig, pos.beat);

            auto weightFor = [&](const char* tplStr, int role) -> float
            {
                float wv = w(tplStr, pos.tIdx);
                if (spq == 8 && (pos.sub & 1)) wv *= 0.3f;                 // 1/32-Raster: ungerade 32tel selten
                if (gp.sig >= 3 && ! isExp)                                   // ungerade Taktarten: Anker aus der Zaehlzeit ableiten
                {
                    if (role == rKick) wv = (pos.downbeat ? 1.f : (pos.onBeat && gp.genre >= OldschoolRave ? 0.9f : wv * 0.5f));
                    if (role == rSnare || role == rClap) wv = backbeat ? w(tplStr, 4) : (wv >= 0.9f ? 0.f : wv);
                }
                const bool core = role == rKick || role == rSnare || role == rClap;
                if (R > 0.f && ! (core && wv >= 0.95f)) wv = juce::jlimit(0.f, 1.f, wv + jitter(sb, role, 5) * R * 0.25f);   // Anker nie verwackeln
                return wv * (core ? 1.f : offBoost);
            };

            // --- Kick / Snare / Clap: Anker immer, Rest per Gewicht
            const char* strs[kRoles] = { T.kick, T.snare, T.clap, T.ch, T.oh, T.perc, T.tom, T.fx, "" };
            for (int role : { (int) rKick, (int) rSnare, (int) rClap, (int) rClosedHat, (int) rOpenHat, (int) rPerc, (int) rTom, (int) rFx })
            {
                const int pad = rm.primary(role);
                if (pad < 0) continue;
                const float wv = weightFor(strs[role], role);
                const bool anchor = ! isExp && wv >= 0.95f && (role == rKick || role == rSnare || role == rClap);
                const float keep = std::pow(juce::jlimit(0.f, 1.f, wv), gamma);
                if (! anchor && roll(role) >= keep * (role >= rPerc ? 0.8f : 1.f)) continue;
                if (wv <= 0.f) continue;

                float vel;
                switch (role)
                {
                    case rKick:      vel = 108.f + 16.f * I; if (! pos.onBeat) vel -= 14.f; break;
                    case rSnare:     vel = 104.f + 20.f * I; break;
                    case rClap:      vel = 96.f + 24.f * I;  break;
                    case rClosedHat: vel = (pos.onBeat ? 92.f : (pos.offGrid ? 52.f : 72.f)) * (0.72f + 0.35f * I); break;
                    case rOpenHat:   vel = 84.f + 24.f * I;  break;
                    default:         vel = 70.f + 30.f * I;  break;
                }
                vel += jitter(step, role, 9) * R * 14.f;
                int ratchet = 1, flags = 0, pitch = 0;
                if ((role == rClosedHat) && I * C > 0.35f && pos.offGrid && roll(role, 40) < 0.10f + 0.25f * I * C) ratchet = roll(role, 41) < 0.5f ? 2 : 3;
                if ((role == rPerc || role == rTom) && C > 0.6f) pitch = (int) std::lround(jitter(step, role, 11) * 3.f);
                setHit(role == rSnare && rm.pads[rSnare].size() > 1 && ! pos.onBeat ? rm.alt(rSnare) : pad, step, (int) vel, flags, ratchet, pitch);
            }

            // --- Open Hat ersetzt Closed Hat auf demselben Schritt (nie beide gleichzeitig)
            if (occupied(rOpenHat, step) && rm.primary(rClosedHat) >= 0) res.pattern.s[rm.primary(rClosedHat)][step] = Step();

            // --- Geisternoten (Ghost Notes) auf schwachen Schritten, nie auf Kick/Snare-Schritten
            const int snarePad = rm.primary(rSnare);
            if (snarePad >= 0 && G > 0.f && ! occupied(rKick, step) && ! occupied(rSnare, step) && ! occupied(rClap, step))
            {
                const float gw = w(T.ghost, pos.tIdx) * (spq == 8 && (pos.sub & 1) ? 0.3f : 1.f);
                if (gw > 0.f && roll(rSnare, 100) < juce::jlimit(0.f, 1.f, G * gw * 2.2f))
                    setHit(rm.pads[rSnare].size() > 1 ? rm.alt(rSnare) : snarePad, step, (int) (26.f + 20.f * I + 8.f * jitter(step, 3, 13) * R), kGhost);
            }
        }

        // --- Fill im letzten Takt: Snare-Wirbel / Toms in der letzten Zaehlzeit
        const bool lastBar = bar == res.bars - 1;
        const float fillChance = res.bars > 1 ? V : V * 0.6f;
        if (lastBar && fillChance > 0.25f && hash01(999, (uint64_t) bar, seed) < fillChance * (0.4f + 0.6f * I) && gp.genre != Techno)
        {
            const int s0 = bar * spb + spb - spq;           // letzte Zaehlzeit
            const int snare = rm.primary(rSnare), tom = rm.primary(rTom);
            for (int k = 0; k < spq; ++k)
            {
                const int st = s0 + k;
                if (st < 0 || st >= res.pattern.length) continue;
                const int pad = (C > 0.5f && tom >= 0 && k >= spq / 2) ? tom : snare;
                if (pad < 0) continue;
                for (int p : rm.pads[rKick]) res.pattern.s[p][st] = res.pattern.s[p][st].vel >= 100 && k == 0 ? res.pattern.s[p][st] : Step();
                Step s; s.vel = (uint8_t) juce::jlimit(1, 127, 70 + k * 50 / juce::jmax(1, spq - 1)); s.flags = kRoll; s.ratchet = (k == spq - 1 && C > 0.4f) ? 2 : 1;
                res.pattern.s[pad][st] = s;
            }
        }
    }
    // Kick und Snare/Clap nicht auf demselben Schritt (ausser Experimental)
    if (! isExp)
        for (int st = 0; st < res.pattern.length; ++st)
        {
            bool k = false, s = false;
            for (int p : rm.pads[rKick]) k |= res.pattern.s[p][st].on();
            for (int p : rm.pads[rSnare]) s |= res.pattern.s[p][st].on() && ! (res.pattern.s[p][st].flags & kGhost);
            for (int p : rm.pads[rClap]) s |= res.pattern.s[p][st].on();
            if (k && s && gp.genre != OldschoolRave && gp.genre != Hardcore && gp.genre != Techno) for (int p : rm.pads[rKick]) if (st % res.stepsPerBar != 0) res.pattern.s[p][st] = Step();
        }
    return res;
}

// ---------------------------------------------------------------------------------------------------------- EVOLVE
// Erzeugt aus `src` eine Variante. `src` wird nie veraendert. amount 0..1 bestimmt, wie viele Mutationen es gibt.
inline PlainPattern evolve(const PlainPattern& src, float amount, int seed, const RoleMap& rm, int spb)
{
    PlainPattern out = src;
    const uint64_t sd = detail::S(seed) * 7919ull;
    const int L = juce::jmax(1, src.length);
    spb = juce::jlimit(1, L, spb);
    amount = juce::jlimit(0.f, 1.f, amount);
    const int nMut = 2 + (int) std::lround(amount * 9.f) + (src.hits() > 40 ? 2 : 0);
    int counter = 0;
    auto rnd = [&]() { return hash01((uint64_t) ++counter, 31, sd); };
    auto rndInt = [&](int n) { return juce::jmin(n - 1, (int) (rnd() * n)); };

    auto isRole = [&](int pad, int role) { return std::find(rm.pads[role].begin(), rm.pads[role].end(), pad) != rm.pads[role].end(); };
    auto protectedHit = [&](int pad, int st)
    {
        const auto& s = src.s[pad][st];
        if (! s.on()) return false;
        if (isRole(pad, rKick) && st % spb == 0) return true;                                 // Kick auf der Eins
        if ((isRole(pad, rSnare) || isRole(pad, rClap)) && s.vel >= 90 && ! (s.flags & kGhost)) return true;  // Backbeat
        return false;
    };
    auto freeStep = [&](int st) { for (int p = 0; p < kPads; ++p) if (out.s[p][st].on() && (isRole(p, rKick) || isRole(p, rSnare) || isRole(p, rClap))) return false; return true; };

    int applied = 0;
    for (int attempt = 0; applied < nMut && attempt < 400; ++attempt)
    {
        const PlainPattern before = out;
        const int op = rndInt(9);
        const int st = rndInt(L);
        switch (op)
        {
            case 0: case 1: {   // Geisternote auf freiem, schwachem Schritt
                const int pad = rm.alt(rSnare) >= 0 ? rm.alt(rSnare) : rm.primary(rSnare);
                if (pad < 0 || ! freeStep(st) || out.s[pad][st].on() || st % spb == 0) break;
                Step s; s.vel = (uint8_t) (24 + rndInt(24)); s.flags = kGhost; out.s[pad][st] = s; break; }
            case 2: {           // Hi-Hat ausduennen
                const int pad = rm.primary(rClosedHat);
                if (pad >= 0 && out.s[pad][st].on() && st % 4 != 0 && ! protectedHit(pad, st)) out.s[pad][st] = Step();
                break; }
            case 3: {           // Hi-Hat ergaenzen
                const int pad = rm.primary(rClosedHat);
                const int open = rm.primary(rOpenHat);
                if (pad >= 0 && ! out.s[pad][st].on() && ! (open >= 0 && out.s[open][st].on())) { Step s; s.vel = (uint8_t) (46 + rndInt(30)); out.s[pad][st] = s; }
                break; }
            case 4: {           // Kick um einen Schritt verschieben (Synkope)
                const int pad = rm.primary(rKick);
                if (pad < 0 || ! out.s[pad][st].on() || protectedHit(pad, st)) break;
                const int to = st + (rnd() < 0.5f ? 1 : -1);
                if (to < 0 || to >= L || out.s[pad][to].on() || ! freeStep(to) || to % spb == 0) break;
                out.s[pad][to] = out.s[pad][st]; out.s[pad][st] = Step();
                break; }
            case 5: {           // Ratschen auf Hat oder Ghost
                const int pad = rm.primary(rClosedHat);
                const int sp = rm.alt(rSnare);
                if (pad >= 0 && out.s[pad][st].on() && st % 4 != 0) out.s[pad][st].ratchet = (uint8_t) (2 + rndInt(2));
                else if (sp >= 0 && out.s[sp][st].on() && (out.s[sp][st].flags & kGhost)) out.s[sp][st].ratchet = 2;
                break; }
            case 6: {           // Akzente verschieben
                for (int p = 0; p < kPads; ++p)
                    if (out.s[p][st].on() && ! protectedHit(p, st)) { const int v = out.s[p][st].vel + (int) std::lround((rnd() - 0.5f) * 36.f); out.s[p][st].vel = (uint8_t) juce::jlimit(12, 127, v); }
                break; }
            case 7: {           // Wahrscheinlichkeit lockern (lebendiger)
                for (int p = 0; p < kPads; ++p)
                    if (out.s[p][st].on() && ! protectedHit(p, st) && st % spb != 0) { out.s[p][st].prob = (uint8_t) (60 + rndInt(30)); break; }
                break; }
            default: {          // Pad innerhalb der Rolle tauschen (z. B. Snare <-> Snare Tight, Perc A <-> Perc B)
                bool swapped = false;
                for (int role : { (int) rSnare, (int) rPerc, (int) rKick, (int) rTom })
                {
                    if (swapped || rm.pads[role].size() < 2) continue;
                    for (int p : rm.pads[role])
                    {
                        if (! out.s[p][st].on() || protectedHit(p, st)) continue;
                        const int to = p == rm.pads[role][0] ? rm.pads[role][1] : rm.pads[role][0];
                        if (! out.s[to][st].on()) { out.s[to][st] = out.s[p][st]; out.s[p][st] = Step(); swapped = true; }
                        break;
                    }
                }
                break; }
        }
        if (! (out == before)) ++applied;
    }
    // Ab mittlerer Staerke: Fill in der letzten Zaehlzeit des letzten Taktes (nur auf freien Schritten, Anker bleiben)
    if (amount > 0.45f && rnd() < amount)
    {
        const int pad = rm.primary(rSnare), tom = rm.primary(rTom);
        const int spq = juce::jmax(1, spb / 4);
        for (int k = 0; k < spq; ++k)
        {
            const int st = L - spq + k;
            if (st < 0 || pad < 0) continue;
            bool prot = false; for (int p = 0; p < kPads; ++p) prot |= protectedHit(p, st);
            if (prot) continue;
            const int p2 = (tom >= 0 && k >= spq / 2) ? tom : pad;
            Step s; s.vel = (uint8_t) (60 + k * 60 / juce::jmax(1, spq - 1)); s.flags = kRoll; out.s[p2][st] = s;
        }
    }
    return out;
}
} // namespace rave::gen
