#pragma once
#include "Chords.h"

namespace mel
{
enum MelChar { McMelody, McLead, McHook, McBassMelody, McAtmos, kNumMelChars };
inline const char* kMelCharNames = "Melody|Lead|Hook|Bass melody|Atmospheric";

struct MelodyParams
{
    int character = McMelody; float density = 0.5f, rhythm = 0.4f, repeat = 0.6f, variation = 0.3f;
    int leap = 7;          // maximale Sprungweite in Halbtonschritten (2..12)
    int octave = 0;        // -1, 0, +1
    bool followChords = true; int swingTicks = 0;
};

namespace detail
{
struct Range { int lo, hi; };
inline Range rangeFor(int c, int octave)
{
    Range r{ 60, 81 };
    switch (c) { case McLead: r = { 67, 91 }; break; case McHook: r = { 62, 79 }; break; case McBassMelody: r = { 36, 55 }; break; case McAtmos: r = { 55, 88 }; break; default: break; }
    r.lo += 12 * octave; r.hi += 12 * octave;
    r.lo = std::max(r.lo, 24); r.hi = std::min(r.hi, 108);
    return r;
}
struct Pool
{
    std::vector<int> p;
    Pool(int key, int scale, Range r) { for (int m = r.lo; m <= r.hi; ++m) if (inScale(m, key, scale)) p.push_back(m); }
    int size() const { return (int) p.size(); }
    int nearest(int midi) const { int b = 0; for (int i = 1; i < size(); ++i) if (std::abs(p[(size_t) i] - midi) < std::abs(p[(size_t) b] - midi)) b = i; return b; }
};
inline bool isChordTone(const ChordSlot* s, int midi) { if (! s) return true; for (int v : s->chord.iv) if (((midi - s->chord.root - v) % 12 + 12) % 12 == 0) return true; return false; }
inline bool isCoreTone(const ChordSlot* s, int midi) { if (! s) return true; for (size_t j = 0; j < s->chord.iv.size() && j < 4; ++j) if (((midi - s->chord.root - s->chord.iv[j]) % 12 + 12) % 12 == 0) return true; return false; }
// naechster Akkordton in der Pool-Liste (Abstand in Stufen <= maxD), sonst Ausgangsindex
inline int snapToChord(const Pool& pool, int idx, const ChordSlot* s, bool core, int maxD = 3)
{
    idx = std::clamp(idx, 0, pool.size() - 1);
    auto ok = [&](int i) { return core ? isCoreTone(s, pool.p[(size_t) i]) : isChordTone(s, pool.p[(size_t) i]); };
    if (ok(idx)) return idx;
    for (int d = 1; d <= maxD; ++d) { if (idx - d >= 0 && ok(idx - d)) return idx - d; if (idx + d < pool.size() && ok(idx + d)) return idx + d; }
    return idx;
}

// naechster Grundton des Akkords (Pool-Index), sonst Ausgangsindex
inline int rootNear(const Pool& pool, int idx, const ChordSlot* cs)
{
    if (! cs) return idx;
    for (int d = 0; d < 6; ++d) for (int sgn : { -1, 1 }) { const int t = idx + sgn * d; if (t >= 0 && t < pool.size() && ((pool.p[(size_t) t] - cs->chord.root) % 12 + 12) % 12 == 0) return t; }
    return idx;
}

struct OnsetT { int step, len; };
inline std::vector<OnsetT> barRhythm(Rng& rng, int steps, const MelodyParams& mp)
{
    const int c = mp.character;
    const float dens = std::clamp(mp.density, 0.f, 1.f), rc = std::clamp(mp.rhythm, 0.f, 1.f);
    int maxNotes = (int) std::lround(steps * (c == McLead ? 0.8 : c == McAtmos ? 0.2 : c == McBassMelody ? 0.45 : 0.6));
    int target = std::clamp((int) std::lround(dens * maxNotes), 1, std::max(1, maxNotes));
    if (c == McAtmos) target = std::clamp(target, 1, 3);
    std::vector<std::pair<double, int>> sc;
    for (int s = 0; s < steps; ++s)
    {
        double w = (s % 4 == 0) ? 1.0 : (s % 2 == 0) ? 0.55 : 0.18 + 0.5 * rc;
        if (rc < 0.25f && (s % 2 == 1)) continue;                             // wenig Rhythmik: nur Achtel-Raster
        if (c == McAtmos && s % 4 != 0) w *= 0.3;
        if (s % 4 != 0 && s % 2 == 0) w *= (1.0 + rc * 0.6);                     // Synkopen
        sc.push_back({ w * (0.55 + 0.9 * rng.uni()), s });
    }
    std::sort(sc.begin(), sc.end(), [](auto& a, auto& b) { return a.first > b.first; });
    std::vector<int> on;
    if (c != McAtmos || rng.chance(0.7)) on.push_back(0);
    for (auto& p : sc) { if ((int) on.size() >= target) break; if (std::find(on.begin(), on.end(), p.second) == on.end()) on.push_back(p.second); }
    std::sort(on.begin(), on.end());
    std::vector<OnsetT> out;
    for (size_t i = 0; i < on.size(); ++i)
    {
        const int next = i + 1 < on.size() ? on[i + 1] : steps;
        int gap = next - on[i];
        int len = gap;
        if (c == McAtmos) len = gap;
        else if (c == McHook || c == McMelody) { if (gap > 4 && rng.chance(0.5)) len = std::min(gap, 4); }
        else if (c == McBassMelody) len = std::max(1, std::min(gap, 3));
        else len = std::max(1, std::min(gap, gap > 2 && rng.chance(0.4) ? gap - 1 : gap));
        out.push_back({ on[i], std::max(1, len) });
    }
    return out;
}
} // namespace detail

inline Part generateMelody(const Ctx& cx, const Progression& prog, const MelodyParams& mp, int seed)
{
    using namespace detail;
    Rng rng((uint64_t) seed * 104729u + 7u);
    const Range rg = rangeFor(mp.character, mp.octave);
    Pool pool(cx.key, cx.scale, rg);
    Part out;
    if (pool.size() < 5) return out;
    const int spb = cx.stepsPerBar();
    Progression fake;
    if (! mp.followChords || prog.empty())
    {
        ChordSlot s; s.tick = 0; s.len = cx.totalTicks(); s.chord = diatonic(cx.key, cx.scale, 0, 3); fake.push_back(s);
    }
    const Progression& P = (mp.followChords && ! prog.empty()) ? prog : fake;

    const float repeatP = mp.character == McHook ? std::max(0.8f, mp.repeat) : mp.repeat;
    const int maxLeapSteps = std::max(1, (int) std::lround(mp.leap / 1.8));

    struct Tpl { int step, len, rel, vel; };
    std::vector<std::vector<Tpl>> bars((size_t) cx.bars);
    std::vector<int> barStartIdx((size_t) cx.bars, 0);
    int idx = pool.nearest(mp.character == McBassMelody ? rg.lo + 7 : (rg.lo + rg.hi) / 2);
    int lastDir = 0, lastDelta = 0;

    auto velFor = [&](int step, bool phraseEnd) {
        int v = (step % 4 == 0) ? 98 : (step % 2 == 0) ? 88 : 78;
        if (mp.character == McAtmos) v = 62 + rng.range(10);
        if (phraseEnd) v += 6;
        return std::clamp(v + rng.range(7) - 3, 1, 127);
    };

    for (int b = 0; b < cx.bars; ++b)
    {
        const int barTick = b * cx.ticksPerBar();
        const bool lastBar = (b == cx.bars - 1);
        const bool midEnd = (cx.bars >= 4 && b == cx.bars / 2 - 1);
        std::vector<Tpl> tpl;
        int srcBar = -1;
        if (b >= 1 && rng.chance(repeatP)) srcBar = (b == 1) ? 0 : (b % 2 == 0 ? b - 2 : b - 2);
        if (srcBar >= 0 && ! bars[(size_t) srcBar].empty())
        {
            // Motiv uebernehmen: Rhythmus und Kontur bleiben, Hoehe wird an den neuen Akkord angepasst
            const auto& src = bars[(size_t) srcBar];
            const auto* cs = chordAt(P, barTick);
            int start = snapToChord(pool, barStartIdx[(size_t) srcBar], cs, true, 4);
            int shift = start - barStartIdx[(size_t) srcBar];
            for (auto t : src)
            {
                Tpl n = t;
                if (rng.chance(mp.variation * 0.6f))
                {
                    const int kind = rng.range(4);
                    if (kind == 0) n.rel += rng.chance(0.5) ? 1 : -1;
                    else if (kind == 1 && n.step % 4 != 0 && n.step + 1 < spb) n.step += 1;
                    else if (kind == 2) n.len = std::max(1, n.len + (rng.chance(0.5) ? 1 : -1));
                    else if (kind == 3 && n.step % 4 != 0) continue;                    // Note weglassen (nie auf einer Zaehlzeit)
                }
                n.rel += shift;
                tpl.push_back(n);
            }
            barStartIdx[(size_t) b] = start;
            if (! tpl.empty()) idx = std::clamp(tpl.back().rel, 0, pool.size() - 1);
        }
        else
        {
            auto rh = barRhythm(rng, spb, mp);
            if (mp.character == McHook && spb >= 8)           // Hook: kurze Zelle, Zelle wird wiederholt
            {
                const int cell = spb >= 16 ? 8 : spb / 2;
                auto cellRh = barRhythm(rng, cell, mp);
                rh.clear();
                for (int off = 0; off + cell <= spb; off += cell) for (auto& o : cellRh) rh.push_back({ off + o.step, o.len });
            }
            barStartIdx[(size_t) b] = idx;
            for (size_t i = 0; i < rh.size(); ++i)
            {
                const int step = rh[i].step;
                const auto* cs = chordAt(P, barTick + step * kStep);
                int nidx = idx;
                if (! (b == 0 && i == 0))
                {
                    std::vector<double> w;
                    for (int d = -maxLeapSteps; d <= maxLeapSteps; ++d)
                    {
                        double wt;
                        const int a = std::abs(d);
                        if (a == 0) wt = (i == 0 && b == 0) ? 0.0 : 0.22;
                        else wt = (a == 1 ? 1.0 : a == 2 ? 0.5 : a == 3 ? 0.22 : a == 4 ? 0.1 : 0.05) * (a >= 2 ? (1.0 + (mp.leap - 2) / 10.0) : 1.0);
                        if (mp.character == McAtmos && a >= 2) wt *= 1.8;
                        const int dir = d > 0 ? 1 : d < 0 ? -1 : 0;
                        if (dir != 0 && dir == lastDir) wt *= 1.4;
                        if (std::abs(lastDelta) >= 3 && dir != 0) wt *= (dir == -lastDir ? 3.0 : 0.3);        // Sprung wird durch Gegenbewegung ausgeglichen
                        const int t = idx + d;
                        if (t < 0 || t >= pool.size()) wt = 0.0;
                        else if (t <= 1 && d < 0) wt *= 0.3; else if (t >= pool.size() - 2 && d > 0) wt *= 0.3;
                        w.push_back(wt);
                    }
                    nidx = idx + (rng.pickWeighted(w) - maxLeapSteps);
                }
                nidx = std::clamp(nidx, 0, pool.size() - 1);
                if (step % 4 == 0 || (mp.character == McBassMelody && step % 2 == 0)) nidx = snapToChord(pool, nidx, cs, true, 2);
                if (mp.character == McBassMelody && step == 0 && rng.chance(0.7))         // Bass-Melodie: Grundton auf der Eins
                {
                    nidx = rootNear(pool, nidx, cs);
                }
                const int delta = nidx - idx; lastDir = delta > 0 ? 1 : delta < 0 ? -1 : 0; lastDelta = delta; idx = nidx;
                tpl.push_back({ rh[i].step, rh[i].len, nidx, 0 });
            }
            // fuer Hook: die zweite Zellhaelfte als "Antwort" leicht veraendern -> bleibt wiedererkennbar
        }
        bars[(size_t) b] = tpl;

        // Realisieren
        for (size_t i = 0; i < tpl.size(); ++i)
        {
            Tpl t = tpl[i];
            t.rel = std::clamp(t.rel, 0, pool.size() - 1);
            const int tick = barTick + t.step * kStep;
            const auto* cs = chordAt(P, tick);
            int pi = t.rel;
            if (t.step % 4 == 0) pi = snapToChord(pool, pi, cs, true, 2);
            int len = t.len * kStep;
            const bool endOfBar = (i + 1 == tpl.size());
            const bool phraseEnd = endOfBar && (lastBar || midEnd);
            if (phraseEnd)
            {
                // Aufloesung: letzte Note der Phrase auf einen Akkordton, lang gehalten
                const auto* ce = chordAt(P, barTick + (spb - 1) * kStep);
                int target = snapToChord(pool, pi, ce, true, 5);
                if (lastBar && ce) {   // Schlusston: Grundton oder Terz des letzten Akkords
                    target = rootNear(pool, pi, ce);
                }
                pi = target; tpl[i].rel = pi; bars[(size_t) b] = tpl;
                len = std::max(len, std::min(cx.ticksPerBar() - t.step * kStep, 2 * kPpq));
            }
            Note n; n.tick = tick; n.pitch = pool.p[(size_t) pi]; n.vel = velFor(t.step, phraseEnd);
            if (mp.character == McMelody || mp.character == McHook) len = std::max(kStep / 2, (int) (len * 0.92));
            else if (mp.character == McLead) len = std::max(kStep / 2, (int) (len * 0.97));
            n.len = std::max(30, len);
            if (t.step % 2 == 1) n.tick += mp.swingTicks;
            out.push_back(n);
        }
    }
    // Ueberlappungen vermeiden, Ende an Schleifenende kappen
    std::stable_sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.tick < b.tick; });
    for (size_t i = 1; i < out.size();) { if (out[i].tick <= out[i - 1].tick) out.erase(out.begin() + (long) i); else ++i; }          // zwei Noten auf demselben Tick: nur die erste
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (i + 1 < out.size()) out[i].len = std::min(out[i].len, std::max(1, out[i + 1].tick - out[i].tick));
        out[i].len = std::min(out[i].len, cx.totalTicks() - out[i].tick);
    }
    out.erase(std::remove_if(out.begin(), out.end(), [&](const Note& n) { return n.len < 1 || n.tick >= cx.totalTicks(); }), out.end());
    return out;
}

// --------------------------------------------------------------------------------------------------------- Arpeggio
enum ArpPattern { ApUp, ApDown, ApUpDown, ApRandom, ApSkip, kNumArpPatterns };
inline const char* kArpPatternNames = "Up|Down|Up-Down|Random|Skip (1-3-2-4)";
inline const char* kArpRateNames = "1/4|1/8|1/16|1/8 triplet";
struct ArpParams { int pattern = ApUp; int rate = 2; float gate = 0.7f; int octaves = 2; int octave = 0; };

inline Part generateArp(const Ctx& cx, const Progression& prog, const ArpParams& ap, int seed)
{
    Rng rng((uint64_t) seed * 15485863u + 3u);
    Part out;
    const int stepTicks = ap.rate == 0 ? kPpq : ap.rate == 1 ? kPpq / 2 : ap.rate == 2 ? kPpq / 4 : kPpq / 3;
    for (auto& s : prog)
    {
        std::vector<int> base;                               // Akkordtoene als Tonhoehen, 1..3 Oktaven
        for (int o = 0; o < std::clamp(ap.octaves, 1, 3); ++o)
            for (int v : s.chord.iv)
            {
                int m = 60 + ap.octave * 12 + ((s.chord.root - 60) % 12 + 12) % 12 + v % 12 + (v >= 12 ? 0 : 0) + 12 * o;
                if (m > 100) continue;
                base.push_back(m);
            }
        std::sort(base.begin(), base.end()); base.erase(std::unique(base.begin(), base.end()), base.end());
        if (base.empty()) continue;
        const int n = (int) base.size();
        std::vector<int> order;
        switch (ap.pattern)
        {
            case ApUp: for (int i = 0; i < n; ++i) order.push_back(i); break;
            case ApDown: for (int i = n - 1; i >= 0; --i) order.push_back(i); break;
            case ApUpDown: for (int i = 0; i < n; ++i) order.push_back(i); for (int i = n - 2; i >= 1; --i) order.push_back(i); break;
            case ApSkip: for (int i = 0; i + 1 < n; i += 2) { order.push_back(i); order.push_back(i + 2 < n ? i + 2 : i + 1); order.push_back(i + 1); } if (order.empty()) order.push_back(0); break;
            default: { std::vector<int> tmp; for (int i = 0; i < n; ++i) tmp.push_back(i); for (int i = n - 1; i > 0; --i) std::swap(tmp[(size_t) i], tmp[(size_t) rng.range(i + 1)]); order = tmp; } break;
        }
        int k = 0;
        for (int t = s.tick; t < s.tick + s.len; t += stepTicks, ++k)
        {
            Note nt; nt.tick = t; nt.pitch = base[(size_t) order[(size_t) k % order.size()]];
            nt.len = std::max(20, std::min((int) (stepTicks * std::clamp(ap.gate, 0.1f, 1.f)), s.tick + s.len - t));
            nt.vel = (k % 4 == 0) ? 96 : (k % 2 == 0) ? 86 : 76;
            nt.vel = std::clamp(nt.vel + rng.range(5) - 2, 1, 127);
            out.push_back(nt);
        }
    }
    (void) cx;
    return out;
}
} // namespace mel
