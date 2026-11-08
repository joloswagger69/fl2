#pragma once
#include "Melody.h"

namespace mel
{
// --------------------------------------------------------------------------------------------------------- Bass
enum BassStyle { BsRoot, BsEighths, BsOffbeat, BsSynco, BsWalking, BsOctave, BsRolling, kNumBassStyles };
inline const char* kBassStyleNames = "Root notes|Root 8ths|Offbeat|Syncopated|Walking|Octave bounce|Rolling 16ths";
struct BassParams { int style = BsEighths; float density = 0.7f; int octave = 1; bool approach = true; int swingTicks = 0; };

inline Part generateBass(const Ctx& cx, const Progression& prog, const BassParams& bp, int seed)
{
    Rng rng((uint64_t) seed * 32452843u + 5u);
    Part out;
    if (prog.empty()) return out;
    const int low = 36 + (bp.octave - 1) * 12;              // tiefster erlaubter Ton, Grundton liegt in [low, low+11]
    auto rootPitch = [&](const ChordSlot& s) { return low + ((s.chord.root - low) % 12 + 12) % 12; };
    const int spb = cx.stepsPerBar(), tpb = cx.ticksPerBar();
    const float dens = std::clamp(bp.density, 0.f, 1.f);

    struct St { int step, len, off; bool core; };           // off = Halbtoene ueber Grundton (0, 12, ...)
    std::vector<St> pat;
    switch (bp.style)
    {
        case BsEighths: for (int s = 0; s < 16; s += 2) pat.push_back({ s, 2, 0, s % 4 == 0 }); break;
        case BsOffbeat: for (int s = 2; s < 16; s += 4) pat.push_back({ s, 2, 0, true }); break;
        case BsSynco:   pat = { { 0, 3, 0, true }, { 3, 3, 0, true }, { 6, 2, 12, false }, { 8, 2, 0, true }, { 10, 1, 0, false }, { 11, 2, 7, false }, { 13, 1, 0, false }, { 14, 2, 12, false } }; break;
        case BsOctave:  for (int s = 0; s < 16; s += 2) pat.push_back({ s, 2, (s / 2) % 2 ? 12 : 0, s % 4 == 0 }); break;
        case BsRolling: for (int s = 0; s < 16; ++s) pat.push_back({ s, 1, (s % 4 == 3) ? 12 : 0, s % 4 == 0 || s % 4 == 2 }); break;
        case BsWalking: for (int s = 0; s < 16; s += 4) pat.push_back({ s, 4, 0, true }); break;
        default: break;
    }
    const auto& scl = scaleNotes(cx.scale);
    for (int b = 0; b < cx.bars; ++b)
    {
        const int bt = b * tpb;
        if (bp.style == BsRoot)
        {
            for (auto& s : prog) if (s.tick >= bt && s.tick < bt + tpb) { Note n; n.tick = s.tick; n.len = std::max(30, (int) (s.len * 0.92)); n.pitch = rootPitch(s); n.vel = 100; out.push_back(n); }
            continue;
        }
        for (size_t i = 0; i < pat.size(); ++i)
        {
            const auto& p = pat[i];
            for (int base = 0; base < spb; base += 16)             // 12er-/20er-Takte: Muster wird fortgesetzt/gekuerzt
            {
                const int step = base + p.step;
                if (step >= spb) continue;
                const int tick = bt + step * kStep;
                const auto* cs = chordAt(prog, tick);
                if (! cs) continue;
                if (! p.core && ! rng.chance(0.25 + 0.75 * dens)) continue;               // Dichte: Zierschritte duennen aus
                if (p.core && dens < 0.35f && p.step % 4 != 0 && rng.chance(0.5 * (0.35 - dens) / 0.35)) continue;
                Note n; n.tick = tick; n.len = p.len * kStep; n.pitch = rootPitch(*cs) + (p.off == 7 ? (cs->chord.has(6) && ! cs->chord.has(7) ? 6 : (cs->chord.has(8) && ! cs->chord.has(7) ? 8 : 7)) : p.off); n.vel = (p.step % 4 == 0) ? 106 : p.core ? 94 : 80;
                if (bp.style == BsWalking)
                {
                    // Grundton - Terz/Quinte - Stufe - Annaeherung an den naechsten Akkord
                    const int beat = p.step / 4;
                    int off = 0;
                    if (beat == 1) off = cs->chord.has(3) ? 3 : 4;
                    else if (beat == 2) off = cs->chord.has(6) ? 6 : 7;
                    else if (beat == 3) off = scl[(size_t) (1 + rng.range((int) scl.size() - 1)) % scl.size()];
                    n.pitch = rootPitch(*cs) + off;
                    if (n.pitch > low + 14) n.pitch -= 12;
                    n.len = kPpq - 30;
                }
                n.len = std::max(30, n.len - (bp.style == BsRolling ? 10 : 20));
                if (p.step % 2 == 1) n.tick += bp.swingTicks;
                // nie ueber einen Akkordwechsel hinweg klingen
                const auto* ce = chordAt(prog, n.tick + std::max(1, n.len) - 1);
                if (ce && ce != cs) n.len = std::max(20, cs->tick + cs->len - n.tick - 10);
                out.push_back(n);
            }
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.tick < b.tick; });
    for (size_t i = 1; i < out.size();) { if (out[i].tick <= out[i - 1].tick) out.erase(out.begin() + (long) i); else ++i; }
    // Annaeherungstoene (chromatisch/diatonisch) an den naechsten Grundton vor einem Akkordwechsel
    if (bp.approach && bp.style != BsRoot)
        for (size_t i = 0; i + 1 < out.size(); ++i)
        {
            const auto* c0 = chordAt(prog, out[i].tick); const auto* c1 = chordAt(prog, out[i + 1].tick);
            if (c0 && c1 && c0 != c1 && out[i].len <= kPpq && rng.chance(0.55))
            {
                const int tgt = rootPitch(*c1);
                out[i].pitch = tgt + (rng.chance(0.5) ? -1 : 1) * (rng.chance(0.6) ? 1 : 2);
                if (! inScale(out[i].pitch, cx.key, cx.scale) && rng.chance(0.5)) out[i].pitch = tgt - 1;     // leittoniger Halbton bleibt erlaubt
            }
        }
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (i + 1 < out.size()) out[i].len = std::min(out[i].len, std::max(1, out[i + 1].tick - out[i].tick));
        out[i].len = std::min(out[i].len, cx.totalTicks() - out[i].tick);
    }
    out.erase(std::remove_if(out.begin(), out.end(), [&](const Note& n) { return n.len < 1 || n.tick >= cx.totalTicks(); }), out.end());
    return out;
}

// --------------------------------------------------------------------------------------------------------- Acid
enum AcidChar { AcClassic, AcDriving, AcSynco, AcExperimental, AcMinimal, kNumAcidChars };
inline const char* kAcidCharNames = "Classic|Driving|Syncopated|Experimental|Minimal";
struct AcidParams
{
    int character = AcClassic; float density = 0.7f, slide = 0.3f, accent = 0.3f, tie = 0.1f, octave = 0.2f, prob = 0.8f, gate = 0.5f, variation = 0.3f, pmod = 0.1f;
    int steps = 16; int octaveBase = 1; int swingTicks = 0;
};

inline Part generateAcid(const Ctx& cx, const AcidParams& ap, int seed)
{
    Rng rng((uint64_t) seed * 49979687u + 13u);
    Part out;
    const int N = (ap.steps >= 32) ? 32 : 16;
    const int totalSteps = cx.totalTicks() / kStep;
    const int low = 36 + (ap.octaveBase - 1) * 12;
    const int rootBase = low + ((cx.key - low) % 12 + 12) % 12;
    const auto& scl = scaleNotes(cx.scale);
    static const double kDegreeW[12] = { 0.45, 0.06, 0.05, 0.11, 0.11, 0.08, 0.03, 0.16, 0.04, 0.04, 0.11, 0.05 };   // Gewicht nach Intervall zum Grundton
    std::vector<double> w; for (int v : scl) w.push_back(kDegreeW[v % 12]);
    if (ap.character == AcMinimal) for (size_t i = 0; i < scl.size(); ++i) w[i] = (scl[i] == 0 ? 0.6 : scl[i] == 7 ? 0.3 : 0.02);
    if (ap.character == AcExperimental) for (auto& x : w) x = 0.1 + x;

    struct Step { bool on = false, accent = false, slide = false, tie = false; int semis = 0; double prob = 1.0; };
    std::vector<Step> master((size_t) N);
    const float d = std::clamp(ap.density, 0.f, 1.f);
    for (int i = 0; i < N; ++i)
    {
        double p;
        switch (ap.character)
        {
            case AcDriving: p = 0.55 + 0.45 * d; if (i % 4 == 0) p = 1.0; break;
            case AcSynco: p = d * (i % 4 == 0 ? 0.45 : i % 4 == 2 ? 0.85 : 0.9); break;
            case AcExperimental: p = d * (0.4 + 0.6 * rng.uni()); break;
            case AcMinimal: p = d * 0.6 * (i % 4 == 0 ? 1.0 : i % 4 == 2 ? 0.7 : 0.25); break;
            default: p = d * (i % 4 == 0 ? 1.0 : i % 2 == 0 ? 0.85 : 0.6); break;
        }
        auto& s = master[(size_t) i];
        s.on = rng.chance(p);
        if (i == 0 && d > 0.2f) s.on = true;
        s.prob = rng.chance(0.6) ? 1.0 : std::clamp((double) ap.prob, 0.0, 1.0);          // Kernschritte immer, Zierschritte nach "Probability"
        if (i == 0) s.prob = 1.0;
        int semis = scl[(size_t) rng.pickWeighted(w)];
        if (rng.chance(ap.pmod * (ap.character == AcExperimental ? 1.6 : 0.6))) semis += rng.chance(0.5) ? 1 : -1;          // chromatische Faerbung
        if (rng.chance(ap.octave)) semis += (ap.character == AcExperimental && rng.chance(0.25)) ? 24 : (rng.chance(0.7) ? 12 : -12);
        s.semis = semis;
        double accP = ap.accent * (i % 4 == 0 ? 0.8 : (i % 2 == 1 ? 1.4 : 1.1));
        s.accent = rng.chance(std::min(accP, 0.95));
    }
    for (int i = 0; i < N; ++i)
    {
        auto& s = master[(size_t) i]; auto& nx = master[(size_t) ((i + 1) % N)];
        if (! s.on || ! nx.on || i == N - 1) continue;     // Tie/Slide nie ueber das Musterende hinweg (sonst Ueberlappung beim Loop)
        if (rng.chance(ap.tie)) s.tie = true;
        else if (nx.semis != s.semis && rng.chance(ap.slide)) s.slide = true;
    }

    const int nBars = cx.bars;
    for (int bar = 0; bar < nBars; ++bar)
    {
        Rng br((uint64_t) seed * 6700417u + (uint64_t) bar * 131u + 17u);
        std::vector<Step> cur = master;
        const bool fill = bar > 0 && (bar % 4 == 3 || bar == nBars - 1) && ap.variation > 0.05f;
        for (int i = 0; i < N; ++i) if (bar > 0 && cur[(size_t) i].on && cur[(size_t) i].prob < 1.0 && ! br.chance(cur[(size_t) i].prob)) cur[(size_t) i].on = false;
        if (fill)
            for (int k = 0; k < 2 + (int) (ap.variation * 4); ++k)
            {
                auto& s = cur[(size_t) br.range(N)];
                if (br.chance(0.5)) s.on = true;
                s.semis = scl[(size_t) br.pickWeighted(w)] + (br.chance(ap.octave + 0.15) ? 12 : 0);
                if (br.chance(ap.accent)) s.accent = true;
            }
        else if (bar > 0 && ap.variation > 0.05f)
            for (int i = 0; i < N; ++i) if (br.chance(ap.variation * 0.15) && cur[(size_t) i].on && i % 4 != 0) cur[(size_t) i].semis = scl[(size_t) br.pickWeighted(w)];
        // Schritte der Takte in Noten umsetzen (Muster laeuft taktuebergreifend durch)
        for (int s = 0; s < cx.stepsPerBar(); ++s)
        {
            const int gstep = bar * cx.stepsPerBar() + s;
            if (gstep >= totalSteps) break;
            const int pi = gstep % N;
            if (! cur[(size_t) pi].on) continue;
            if (pi > 0 && cur[(size_t) ((pi + N - 1) % N)].on && cur[(size_t) ((pi + N - 1) % N)].tie) continue;   // von Vorgaengerton uebernommen (Tie)
            const auto& st = cur[(size_t) pi];
            Note n; n.tick = gstep * kStep; n.pitch = std::clamp(rootBase + st.semis, 24, 96); n.vel = st.accent ? 124 : 88; n.slide = st.slide;
            const int gate = std::max(12, (int) (kStep * std::clamp(ap.gate, 0.1f, 1.f)));
            if (st.tie) n.len = kStep + gate;
            else if (st.slide) n.len = kStep + 40;                      // ueberlappt den naechsten Ton -> Legato/Slide
            else n.len = gate;
            if (pi % 2 == 1) n.tick += ap.swingTicks;
            out.push_back(n);
        }
    }
    std::sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.tick < b.tick; });
    for (auto& n : out) n.len = std::min(n.len, cx.totalTicks() - n.tick);
    out.erase(std::remove_if(out.begin(), out.end(), [&](const Note& n) { return n.len < 1 || n.tick >= cx.totalTicks(); }), out.end());
    return out;
}

// --------------------------------------------------------------------------------------------------------- Variation bestehender Teile
// Veraendert einen Teil leicht (Seed bestimmt wie), bleibt in Tonart und auf den Zaehlzeiten bei Akkordtoenen.
inline Part mutatePart(const Part& in, const Ctx& cx, const Progression& prog, float amount, int seed, bool keepStrongChordTones)
{
    Rng rng((uint64_t) seed * 86028121u + 29u);
    Part out = in;
    const auto& scl = scaleNotes(cx.scale); (void) scl;
    for (auto& n : out)
    {
        if (! rng.chance(amount)) continue;
        const bool strong = (n.tick % kPpq) == 0;
        const int dir = rng.chance(0.5) ? 1 : -1;
        int np = n.pitch;
        for (int tries = 1; tries <= 4; ++tries)
        {
            const int cand = n.pitch + dir * tries;
            if (! inScale(cand, cx.key, cx.scale)) continue;
            if (strong && keepStrongChordTones && ! detail::isChordTone(chordAt(prog, n.tick), cand)) continue;
            np = cand; break;
        }
        n.pitch = np;
    }
    return out;
}
} // namespace mel
