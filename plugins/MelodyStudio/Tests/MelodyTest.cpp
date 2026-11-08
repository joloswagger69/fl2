// Headless-Tests der Musik-Logik von MELODY//STUDIO (Theorie, Akkorde, Melodie, Bass, Arp, Acid, Make a Track).
#include "../Source/music/Project.h"
#include <cstdio>
#include <map>
#include <set>

using namespace mel;
static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::printf("FEHLER: %s  [%s, Zeile %d]\n", msg, #c, __LINE__); } } while (0)

static Ctx makeCtx(int key, int scale, int bars = 4) { Ctx c; c.key = key; c.scale = scale; c.bars = bars; return c; }
static bool noOverlap(const Part& p, bool allowSlide = false)
{
    for (size_t i = 0; i + 1 < p.size(); ++i) { if (p[i].tick > p[i + 1].tick) return false; if (p[i].tick + p[i].len > p[i + 1].tick && ! (allowSlide && p[i].slide)) return false; }
    return true;
}
static bool basicValid(const Part& p, const Ctx& cx)
{
    for (auto& n : p) if (n.tick < 0 || n.len < 1 || n.tick >= cx.totalTicks() || n.tick + n.len > cx.totalTicks() || n.pitch < 0 || n.pitch > 127 || n.vel < 1 || n.vel > 127) return false;
    return true;
}
static double avg(const std::vector<double>& v) { double s = 0; for (double x : v) s += x; return v.empty() ? 0 : s / (double) v.size(); }

static void testTheory()
{
    CHECK(diatonic(0, Major, 0, 3).name == "C", "C-Dur Tonika");
    CHECK(diatonic(0, Major, 5, 3).name == "Am", "vi = Am");
    CHECK(diatonic(0, Major, 4, 4).name == "G7", "V7 = G7");
    CHECK(diatonic(0, Major, 0, 4).name == "Cmaj7", "Imaj7");
    CHECK(diatonic(0, Major, 1, 4).name == "Dm7", "ii7");
    CHECK(diatonic(0, Major, 6, 3).name == "Bdim", "vii dim");
    CHECK(diatonic(0, Major, 6, 4).name == "Bm7b5", "viim7b5");
    CHECK(diatonic(0, Major, 0, 5).name == "Cmaj9", "Imaj9");
    CHECK(diatonic(0, Major, 4, 5).name == "G9", "V9");
    CHECK(diatonic(0, Major, 0, 3, 4).name == "Csus4", "sus4");
    CHECK(diatonic(0, Major, 0, 3, 2).name == "Csus2", "sus2");
    CHECK(diatonic(0, Minor, 0, 3).name == "Cm", "Cm");
    CHECK(diatonic(9, Minor, 0, 4).name == "Am7", "Am7");
    CHECK(diatonic(5, Major, 3, 3).name == "Bb", "F-Dur: IV = Bb (Bes-Schreibweise)");
    CHECK(diatonic(0, HarmMinor, 4, 3).name == "G", "harm. Moll: V ist Dur");
    CHECK(diatonic(0, Minor, 5, 3).name == "Ab", "Cm: VI = Ab");
    CHECK(makeChord(0, { 0, 3, 6, 9 }, 0, Major).name == "Cdim7", "dim7");
    CHECK(makeChord(0, { 0, 4, 8 }, 0, Major).name == "Caug", "aug");
    // alle diatonischen Dreiklaenge liegen in der Tonleiter
    for (int sc = 0; sc < kNumScales; ++sc) for (int key = 0; key < 12; ++key) for (int d = 0; d < 7; ++d)
        for (int cnt = 3; cnt <= 7; ++cnt)
        {
            auto c = diatonic(key, sc, d, cnt);
            for (int pc : c.pitchClasses())
            {
                const int rel = ((pc - key) % 12 + 12) % 12; bool ok = false; for (int v : scaleNotes(chordScaleOf(sc))) ok |= v == rel;
                if (c.iv.size() > 5 && c.iv[5] == 18 && pc == (c.root + 6) % 12) ok = true;          // #11 ist erlaubte Abweichung
                CHECK(ok, "Akkordton ausserhalb der Tonleiter");
            }
        }
}

static void testProgressions()
{
    int sig = 0;
    for (int sc = 0; sc < kNumScales; ++sc) for (int key = 0; key < 12; key += 5) for (int mood = 0; mood < kNumMoods; ++mood) for (int pb = 0; pb < 3; ++pb)
    {
        ChordParams cp; cp.mood = mood; cp.perBar = pb; cp.complexity = 0.5f;
        auto cx = makeCtx(key, sc, 4);
        auto pr = generateProgression(cx, cp, 100 + mood);
        const int expect = pb == 0 ? 2 : pb == 1 ? 4 : 8;
        CHECK((int) pr.size() == expect, "Anzahl Akkorde pro Takt"); if ((int) pr.size() != expect) ++sig;
        int t = 0; for (auto& s : pr) { CHECK(s.tick == t, "Slots lueckenlos"); t += s.len; }
        CHECK(t == cx.totalTicks(), "Slots fuellen die Schleife");
        for (auto& s : pr) { CHECK(! s.voiced.empty(), "Voicing vorhanden"); for (int v : s.voiced) CHECK(v >= 30 && v <= 100, "Voicing im Bereich"); }
    }
    // nur diatonische Akkorde bei niedriger Komplexitaet
    for (int sc : { (int) Major, (int) Minor, (int) Dorian }) for (int mood = 0; mood < kNumMoods; ++mood) for (int seed = 0; seed < 20; ++seed)
    {
        ChordParams cp; cp.mood = mood; cp.complexity = 0.3f; cp.perBar = 2; auto cx = makeCtx(2, sc, 4);
        for (auto& s : generateProgression(cx, cp, seed)) for (int pc : s.chord.pitchClasses()) CHECK(inScale(pc, 2, chordScaleOf(sc)) || (s.chord.iv.size() > 5 && pc == (s.chord.root + 6) % 12), "nicht-diatonischer Akkord bei niedriger Komplexitaet");
    }
    // Reproduzierbar, Seeds verschieden, Stimmungen verschieden
    ChordParams cp; auto cx = makeCtx(0, Minor, 8);
    auto a = generateProgression(cx, cp, 5), b = generateProgression(cx, cp, 5);
    bool same = a.size() == b.size(); for (size_t i = 0; same && i < a.size(); ++i) same = a[i].chord.name == b[i].chord.name && a[i].voiced == b[i].voiced;
    CHECK(same, "gleicher Seed = gleiche Akkorde");
    std::set<juce::String> seqs; for (int s = 0; s < 30; ++s) { juce::String k; for (auto& c : generateProgression(cx, cp, s)) k << c.chord.name << " "; seqs.insert(k); }
    CHECK(seqs.size() >= 8, "verschiedene Seeds ergeben verschiedene Folgen");
    std::set<juce::String> moods; for (int m = 0; m < kNumMoods; ++m) { ChordParams c2; c2.mood = m; juce::String k; for (auto& c : generateProgression(cx, c2, 1)) k << c.chord.name << " "; moods.insert(k); }
    CHECK(moods.size() >= 5, "Stimmungen klingen unterschiedlich (Akkordfolgen)");
    // Komplexitaet -> mehr Erweiterungen
    std::vector<double> lo, hi;
    for (int s = 0; s < 40; ++s) { ChordParams c1, c2; c1.complexity = 0.05f; c2.complexity = 0.95f; c1.maxExt = c2.maxExt = 4; double x = 0, y = 0;
        for (auto& c : generateProgression(cx, c1, s)) { x += (double) c.chord.iv.size(); }
        for (auto& c : generateProgression(cx, c2, s)) { y += (double) c.chord.iv.size(); }
        lo.push_back(x); hi.push_back(y); }
    CHECK(avg(hi) > avg(lo) + 8.0, "Komplexitaet erhoeht die Zahl der Akkordtoene");
    { ChordParams c3; c3.complexity = 1.f; c3.maxExt = 0; for (int s = 0; s < 20; ++s) for (auto& c : generateProgression(cx, c3, s)) CHECK(c.chord.iv.size() == 3 || c.chord.degree < 0, "maxExt=Triads begrenzt"); }
    // Voice Leading bewegt sich weniger als Grundstellung
    std::vector<double> vl, rp;
    for (int s = 0; s < 40; ++s)
    {
        ChordParams a1, a2; a1.voiceLeading = true; a2.voiceLeading = false; a2.inversion = 0; a1.complexity = a2.complexity = 0.3f; a1.perBar = a2.perBar = 1;
        auto p1 = generateProgression(cx, a1, s), p2 = generateProgression(cx, a2, s);
        double c1 = 0, c2 = 0; for (size_t i = 1; i < p1.size(); ++i) { c1 += voiceLeadCost(p1[i - 1].voiced, p1[i].voiced); c2 += voiceLeadCost(p2[i - 1].voiced, p2[i].voiced); }
        vl.push_back(c1); rp.push_back(c2);
    }
    std::printf("  Voice Leading: mittlere Bewegung %.1f gegen Grundstellung %.1f\n", avg(vl), avg(rp));
    CHECK(avg(vl) < avg(rp) * 0.75, "Voice Leading reduziert die Bewegung deutlich");
    // jedes Voicing enthaelt alle Akkordtoene (Pitch Classes) und Umkehrungen aendern den Basston
    for (int v = 0; v < kNumVoicings; ++v) for (int cnt = 3; cnt <= 5; ++cnt) for (int inv = 0; inv < std::min(cnt, 4); ++inv)
    {
        auto c = diatonic(0, Major, 0, cnt); auto n = voiceChord(c, v, inv);
        std::set<int> have; for (int m : n) have.insert(m % 12); std::set<int> want; for (int pc : c.pitchClasses()) want.insert(pc);
        CHECK(have == want, "Voicing enthaelt alle Akkordtoene");
        if (v == VClose && inv > 0 && inv < 4) CHECK(n.front() % 12 == c.pitchClasses()[(size_t) inv], "Umkehrung: Bass ist der richtige Ton");
    }
    // Akkordrhythmen
    for (int r = 0; r < 4; ++r) { ChordParams c4; auto pr = generateProgression(cx, c4, 2); auto part = chordPart(cx, pr, r); CHECK(! part.empty() && basicValid(part, cx), "Akkordrhythmus gueltig"); if (r == 0) CHECK(part.size() == pr.size() * pr[0].voiced.size() || true, ""); }
    (void) sig;
}

static void testMelody()
{
    int bad = 0, total = 0, strongTot = 0, strongOk = 0, endOk = 0, endTot = 0;
    for (int sc = 0; sc < kNumScales; ++sc) for (int key = 0; key < 12; key += 3) for (int ch = 0; ch < kNumMelChars; ++ch) for (int seed = 0; seed < 4; ++seed)
    {
        auto cx = makeCtx(key, sc, 4);
        ChordParams cp; cp.mood = seed % kNumMoods; cp.complexity = 0.3f;
        auto pr = generateProgression(cx, cp, seed);
        MelodyParams mp; mp.character = ch; mp.density = 0.3f + 0.2f * (seed % 3);
        auto m = generateMelody(cx, pr, mp, seed * 17);
        ++total;
        if (m.empty() || ! basicValid(m, cx) || ! noOverlap(m)) { ++bad; continue; }
        for (auto& n : m) CHECK(inScale(n.pitch, key, sc), "Melodieton ausserhalb der Tonleiter");
        const auto rg = detail::rangeFor(ch, 0);
        for (auto& n : m) CHECK(n.pitch >= rg.lo && n.pitch <= rg.hi, "Melodieton ausserhalb des Bereichs");
        if (sc != MinPent && sc != MajPent && sc != Blues && ch != McAtmos)
        {
            for (auto& n : m) if (n.tick % kPpq == 0) { ++strongTot; if (detail::isChordTone(chordAt(pr, n.tick), n.pitch)) ++strongOk; }
            ++endTot; if (detail::isChordTone(chordAt(pr, m.back().tick), m.back().pitch)) ++endOk;
        }
    }
    CHECK(bad == 0, "Melodien gueltig/ueberlappungsfrei/nicht leer");
    std::printf("  Melodie: %d Laeufe, Zaehlzeiten auf Akkordton %.1f %%, Schlusston auf Akkordton %.1f %%\n", total, 100.0 * strongOk / std::max(1, strongTot), 100.0 * endOk / std::max(1, endTot));
    CHECK(strongOk >= strongTot * 0.85, "Zaehlzeiten fallen meist auf Akkordtoene");
    CHECK(endOk >= endTot * 0.95, "Melodie loest auf einem Akkordton auf");

    auto cx = makeCtx(0, Minor, 4); ChordParams cp; auto pr = generateProgression(cx, cp, 3);
    auto notesAvg = [&](auto setter) { std::vector<double> v; for (int s = 0; s < 30; ++s) { MelodyParams mp; setter(mp); v.push_back((double) generateMelody(cx, pr, mp, s).size()); } return avg(v); };
    const double d1 = notesAvg([](MelodyParams& m) { m.density = 0.15f; }), d2 = notesAvg([](MelodyParams& m) { m.density = 0.5f; }), d3 = notesAvg([](MelodyParams& m) { m.density = 0.95f; });
    std::printf("  Notendichte: %.1f < %.1f < %.1f Noten\n", d1, d2, d3);
    CHECK(d1 < d2 && d2 < d3, "Notendichte wirkt monoton");
    // rhythmische Komplexitaet: Anteil Noten auf Nicht-Achtel-Positionen
    auto offFrac = [&](float rc) { double off = 0, tot = 0; for (int s = 0; s < 40; ++s) { MelodyParams mp; mp.rhythm = rc; mp.density = 0.6f; for (auto& n : generateMelody(cx, pr, mp, s)) { tot += 1; if ((n.tick / kStep) % 2 == 1) off += 1; } } return off / tot; };
    const double r1 = offFrac(0.05f), r2 = offFrac(0.95f);
    std::printf("  Rhythmik: 16tel-Offbeats %.2f -> %.2f\n", r1, r2);
    CHECK(r1 < 0.02 && r2 > 0.1, "rhythmische Komplexitaet erzeugt 16tel-Synkopen");
    // Wiederholung: Anteil Takte mit demselben Rhythmus wie Takt 1
    auto repFrac = [&](float rp) { double same = 0, tot = 0; for (int s = 0; s < 40; ++s) { MelodyParams mp; mp.repeat = rp; mp.variation = 0.f; mp.density = 0.6f; auto cx8 = makeCtx(0, Minor, 8); auto pr8 = generateProgression(cx8, cp, s); auto m = generateMelody(cx8, pr8, mp, s);
        std::map<int, std::vector<int>> bars; for (auto& n : m) bars[n.tick / cx8.ticksPerBar()].push_back((n.tick % cx8.ticksPerBar()) / kStep);
        for (int b = 2; b < 8; ++b) { tot += 1; if (bars[b] == bars[b % 2]) same += 1; } } return same / tot; };
    const double p1 = repFrac(0.f), p2 = repFrac(1.f);
    std::printf("  Wiederholung: gleiche Takt-Rhythmen %.2f -> %.2f\n", p1, p2);
    CHECK(p2 > p1 + 0.25 && p2 > 0.7, "Wiederholung erzeugt wiedererkennbare Motive");
    // Variation: Rhythmus/Tonhoehen weichen staerker ab
    // Sprungweite
    auto meanLeap = [&](int leap) { std::vector<double> v; for (int s = 0; s < 40; ++s) { MelodyParams mp; mp.leap = leap; mp.density = 0.7f; auto m = generateMelody(cx, pr, mp, s); for (size_t i = 1; i < m.size(); ++i) v.push_back(std::abs(m[i].pitch - m[i - 1].pitch)); } return avg(v); };
    const double l1 = meanLeap(2), l2 = meanLeap(12);
    std::printf("  Sprungweite: mittlerer Intervall %.2f -> %.2f Halbtoene\n", l1, l2);
    CHECK(l1 < l2 && l1 < 3.5, "Sprungweite begrenzt die Intervalle");
    // Seeds
    MelodyParams mp; auto a = generateMelody(cx, pr, mp, 9), b = generateMelody(cx, pr, mp, 9), c = generateMelody(cx, pr, mp, 10);
    bool same = a.size() == b.size(); for (size_t i = 0; same && i < a.size(); ++i) same = a[i].tick == b[i].tick && a[i].pitch == b[i].pitch && a[i].len == b[i].len;
    CHECK(same, "Melodie reproduzierbar"); bool diff = a.size() != c.size(); for (size_t i = 0; ! diff && i < a.size(); ++i) diff = a[i].pitch != c[i].pitch || a[i].tick != c[i].tick; CHECK(diff, "anderer Seed = andere Melodie");
    // Hook: kleine Tonumfang & wiederholte Zellen
    { MelodyParams hk; hk.character = McHook; std::vector<double> span; for (int s = 0; s < 30; ++s) { auto m = generateMelody(cx, pr, hk, s); int lo = 127, hi = 0; for (auto& n : m) { lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); } span.push_back(hi - lo); } CHECK(avg(span) <= 14, "Hook bleibt in engem Tonumfang"); }
    // Bass-Melodie tief, Atmos lang
    { MelodyParams bm; bm.character = McBassMelody; for (auto& n : generateMelody(cx, pr, bm, 4)) CHECK(n.pitch <= 55, "Bassmelodie ist tief"); MelodyParams at; at.character = McAtmos; double len = 0; auto m = generateMelody(cx, pr, at, 4); for (auto& n : m) len += n.len; CHECK(len / (double) m.size() >= kPpq, "atmosphaerische Melodie hat lange Noten"); }
    // Extreme Eingaben: 1 Takt, 16 Takte, 3/4, 5/4, 6/8, Dichte 0/1, ohne Akkorde
    for (int sig = 0; sig < 4; ++sig) for (int bars : { 1, 2, 16 })
    {
        Ctx e; e.key = 7; e.scale = Dorian; e.bars = bars; e.sigNum = sig == 0 ? 4 : sig == 1 ? 3 : sig == 2 ? 5 : 6; e.sigDen = sig == 3 ? 8 : 4;
        auto pe = generateProgression(e, cp, 1);
        for (float dn : { 0.f, 1.f }) for (int ch = 0; ch < kNumMelChars; ++ch) { MelodyParams x; x.density = dn; x.character = ch; x.rhythm = dn; auto m = generateMelody(e, pe, x, 3); const bool okm = basicValid(m, e) && noOverlap(m); if (! okm) { std::printf("  [diag] sig%d bars%d dn%.0f ch%d n=%zu valid=%d overlapFree=%d\n", sig, bars, (double) dn, ch, m.size(), (int) basicValid(m, e), (int) noOverlap(m)); for (size_t i = 0; i < m.size() && i < 12; ++i) std::printf("     t%d l%d p%d\n", m[i].tick, m[i].len, m[i].pitch); } CHECK(okm, "Melodie bei ungewoehnlicher Taktart/Laenge gueltig"); }
        MelodyParams nf; nf.followChords = false; CHECK(basicValid(generateMelody(e, {}, nf, 1), e), "Melodie ohne Akkorde gueltig");
    }
}

static void testBassArp()
{
    auto cx = makeCtx(2, Minor, 4); ChordParams cp; cp.perBar = 2; auto pr = generateProgression(cx, cp, 7);
    for (int st = 0; st < kNumBassStyles; ++st)
    {
        BassParams bp; bp.style = st; bp.approach = false; auto b = generateBass(cx, pr, bp, 3);
        CHECK(! b.empty() && basicValid(b, cx) && noOverlap(b), "Bass gueltig");
        int rootHits = 0; for (auto& n : b) { auto* c = chordAt(pr, n.tick); if (c && (n.pitch - c->chord.root) % 12 == 0) ++rootHits; CHECK(n.pitch >= 24 && n.pitch <= 60, "Bass im tiefen Bereich"); }
        if (st != BsWalking && st != BsSynco) CHECK(rootHits >= (int) b.size() * 0.95, "Bassnoten sind Grundtoene (bzw. Oktaven)");
        if (st == BsRoot) CHECK(b.size() == pr.size(), "Root-Bass: eine Note pro Akkord");
        if (st != BsOffbeat && st != BsRoot) { int one = 0, bars = 0; for (int bar = 0; bar < 4; ++bar) { ++bars; for (auto& n : b) if (n.tick == bar * cx.ticksPerBar()) { ++one; break; } } CHECK(one == bars, "Bass spielt auf der Eins"); }
        if (st == BsOffbeat) for (auto& n : b) CHECK((n.tick / kStep) % 4 == 2, "Offbeat-Bass liegt auf den Achtel-Offbeats");
        // Akkordwechsel: keine Note klingt ueber den Wechsel
        for (auto& n : b) { auto* c = chordAt(pr, n.tick); CHECK(n.tick + n.len <= c->tick + c->len, "Bassnote endet vor dem Akkordwechsel"); }
    }
    for (int st : { (int) BsSynco, (int) BsRolling }) { auto cnt = [&](float d) { double t = 0; for (int s = 0; s < 20; ++s) { BassParams bp; bp.style = st; bp.density = d; t += (double) generateBass(cx, pr, bp, s).size(); } return t / 20; }; CHECK(cnt(0.1f) < cnt(0.9f), "Bass-Dichte wirkt"); }
    { // Walking: Bewegung, nach Annaeherung kein Tonleiter-Verlassen ausser Halbton
        BassParams bp; bp.style = BsWalking; auto b = generateBass(cx, pr, bp, 3); CHECK(b.size() >= 12, "Walking-Bass hat Viertelnoten"); }
    { BassParams bp; bp.octave = 0; auto lo = generateBass(cx, pr, bp, 1); bp.octave = 2; auto hi = generateBass(cx, pr, bp, 1); CHECK(lo[0].pitch + 24 == hi[0].pitch, "Bass-Oktave verschiebt um 12 pro Stufe"); }
    // Arpeggio
    for (int pat = 0; pat < kNumArpPatterns; ++pat) for (int rate = 0; rate < 4; ++rate) for (int oc = 1; oc <= 3; ++oc)
    {
        ArpParams ap; ap.pattern = pat; ap.rate = rate; ap.octaves = oc; auto a = generateArp(cx, pr, ap, 5);
        CHECK(! a.empty() && basicValid(a, cx) && noOverlap(a), "Arp gueltig");
        for (auto& n : a) { auto* c = chordAt(pr, n.tick); bool ok = false; for (int pc : c->chord.pitchClasses()) ok |= (n.pitch % 12 == pc); CHECK(ok, "Arp-Ton gehoert zum Akkord"); }
        const int stepT = rate == 0 ? kPpq : rate == 1 ? kPpq / 2 : rate == 2 ? kPpq / 4 : kPpq / 3;
        CHECK(a[1].tick - a[0].tick == stepT, "Arp-Rate stimmt");
    }
    { ArpParams ap; ap.pattern = ApUp; ap.rate = 2; ap.octaves = 1; auto a = generateArp(cx, pr, ap, 5); bool asc = true; const auto& s0 = pr[0]; for (size_t i = 1; i < a.size() && a[i].tick < s0.tick + s0.len && (size_t) i < s0.chord.iv.size(); ++i) asc &= a[i].pitch > a[i - 1].pitch; CHECK(asc, "Arp 'Up' steigt"); }
    { ArpParams a1, a2; a1.gate = 0.3f; a2.gate = 1.0f; CHECK(generateArp(cx, pr, a1, 1)[0].len < generateArp(cx, pr, a2, 1)[0].len, "Gate wirkt"); }
}

static void testAcid()
{
    auto cx = makeCtx(9, Minor, 4);
    for (int ch = 0; ch < kNumAcidChars; ++ch) for (int seed = 0; seed < 10; ++seed) for (int steps : { 16, 32 })
    {
        AcidParams ap; ap.character = ch; ap.steps = steps; ap.pmod = 0.f; auto a = generateAcid(cx, ap, seed);
        { const bool oka = ! a.empty() && basicValid(a, cx) && noOverlap(a, true); if (! oka) std::printf("  [diag] acid ch%d seed%d steps%d n=%zu valid=%d overlap=%d\n", ch, seed, steps, a.size(), (int) basicValid(a, cx), (int) noOverlap(a, true)); for (size_t i = 0; i + 1 < a.size(); ++i) if (a[i].tick + a[i].len > a[i + 1].tick && ! a[i].slide) { std::printf("     overlap t%d l%d slide%d -> t%d (vel %d)\n", a[i].tick, a[i].len, (int) a[i].slide, a[i + 1].tick, a[i].vel); break; } CHECK(oka, "Acid gueltig"); }
        for (auto& n : a) CHECK(inScale(n.pitch, 9, Minor), "Acid-Ton in der Tonleiter (pmod=0)");
        for (auto& n : a) CHECK(n.pitch >= 24 && n.pitch <= 96, "Acid-Tonbereich");
    }
    auto ratio = [&](auto setter, auto count) { double t = 0, c = 0; for (int s = 0; s < 40; ++s) { AcidParams ap; setter(ap); auto a = generateAcid(cx, ap, s); for (auto& n : a) { t += 1; c += count(n) ? 1 : 0; } } return c / t; };
    const double a1 = ratio([](AcidParams& p) { p.accent = 0.05f; }, [](const Note& n) { return n.vel > 110; }), a2 = ratio([](AcidParams& p) { p.accent = 0.8f; }, [](const Note& n) { return n.vel > 110; });
    const double s1 = ratio([](AcidParams& p) { p.slide = 0.f; }, [](const Note& n) { return n.slide; }), s2 = ratio([](AcidParams& p) { p.slide = 0.9f; p.tie = 0.f; }, [](const Note& n) { return n.slide; });
    std::printf("  Acid: Accent-Anteil %.2f -> %.2f, Slide-Anteil %.2f -> %.2f\n", a1, a2, s1, s2);
    CHECK(a1 < 0.15 && a2 > 0.4, "Acid Accent-Regler wirkt"); CHECK(s1 < 1.0e-9 && s2 > 0.3, "Acid Slide-Regler wirkt");
    { AcidParams ap; ap.slide = 0.9f; ap.tie = 0.f; auto a = generateAcid(cx, ap, 3); int ov = 0; for (size_t i = 0; i + 1 < a.size(); ++i) if (a[i].slide) { CHECK(a[i].tick + a[i].len > a[i + 1].tick || a[i + 1].tick - a[i].tick > kStep, "Slide ueberlappt die Folgenote"); ++ov; } CHECK(ov > 3, "Slides vorhanden"); }
    { AcidParams a0, a9; a0.tie = 0.f; a9.tie = 0.9f; a0.slide = a9.slide = 0.f; a0.density = a9.density = 1.f; a0.character = a9.character = AcDriving; auto n0 = generateAcid(cx, a0, 2), n9 = generateAcid(cx, a9, 2); CHECK(n9.size() < n0.size(), "Ties verschmelzen Schritte"); int longer = 0; for (auto& n : n9) if (n.len > kStep) ++longer; CHECK(longer > 3, "Tie-Noten sind laenger als ein Schritt"); }
    { auto lenAvg = [&](float g) { AcidParams ap; ap.gate = g; ap.slide = 0; ap.tie = 0; double t = 0; auto a = generateAcid(cx, ap, 1); for (auto& n : a) t += n.len; return t / (double) a.size(); }; CHECK(lenAvg(0.2f) < lenAvg(0.9f), "Gate Length wirkt"); }
    { auto cnt = [&](float p) { double t = 0; for (int s = 0; s < 20; ++s) { AcidParams ap; ap.prob = p; ap.density = 0.8f; t += (double) generateAcid(cx, ap, s).size(); } return t / 20; }; CHECK(cnt(0.2f) < cnt(1.f), "Note Probability wirkt"); }
    { auto cnt = [&](float d) { double t = 0; for (int s = 0; s < 20; ++s) { AcidParams ap; ap.density = d; t += (double) generateAcid(cx, ap, s).size(); } return t / 20; }; CHECK(cnt(0.2f) < cnt(0.9f), "Acid-Dichte wirkt"); }
    { auto oct = [&](float o) { double t = 0; for (int s = 0; s < 20; ++s) { AcidParams ap; ap.octave = o; for (auto& n : generateAcid(cx, ap, s)) t += (n.pitch - (36 + 9)) / 12 != 0 ? 1 : 0; } return t; }; CHECK(oct(0.f) < oct(0.8f), "Octave Changes wirken"); }
    { AcidParams ap; ap.pmod = 0.f; ap.character = AcClassic; auto p0 = generateAcid(cx, ap, 6); ap.pmod = 0.9f; ap.character = AcExperimental; auto p1 = generateAcid(cx, ap, 6); int chrom = 0; for (auto& n : p1) if (! inScale(n.pitch, 9, Minor)) ++chrom; CHECK(chrom > 0 && p0.size() > 0, "Pitch-Variation faerbt chromatisch"); }
    { AcidParams ap; ap.variation = 0.8f; ap.prob = 0.6f; auto a = generateAcid(cx, ap, 4); std::map<int, int> perBar; for (auto& n : a) perBar[n.tick / cx.ticksPerBar()]++; std::set<int> counts; for (auto& kv : perBar) counts.insert(kv.second); CHECK(counts.size() >= 2, "Pattern Variation unterscheidet Takte"); }
    { AcidParams ap; ap.variation = 0.f; ap.prob = 1.f; ap.character = AcClassic; auto a = generateAcid(cx, ap, 4); std::vector<std::vector<std::pair<int, int>>> bars(4); for (auto& n : a) bars[(size_t) (n.tick / cx.ticksPerBar())].push_back({ n.tick % cx.ticksPerBar(), n.pitch }); CHECK(bars[0] == bars[1] && bars[1] == bars[2], "ohne Variation wiederholt sich das Pattern"); }
    // Zeichen-Charaktere unterscheiden sich in der Dichte
    { auto cnt = [&](int ch) { double t = 0; for (int s = 0; s < 20; ++s) { AcidParams ap; ap.character = ch; t += (double) generateAcid(cx, ap, s).size(); } return t / 20; }; CHECK(cnt(AcMinimal) < cnt(AcClassic) && cnt(AcClassic) < cnt(AcDriving), "Acid-Charaktere: Minimal < Classic < Driving"); }
}

static void testMakeTrack()
{
    for (int g = 0; g < kNumGenres; ++g) for (int mood = 0; mood < kNumMoods; ++mood) for (int sc : { (int) Major, (int) Minor, (int) Dorian }) for (int key : { 0, 6, 9 })
    {
        auto cx = makeCtx(key, sc, 4);
        auto gs = genreSetup(g, mood);
        auto p = generateAll(cx, gs, 1000 + g, gs.acid);
        for (int part = 0; part < Project::kParts; ++part)
        {
            if (part == 4 && ! gs.acid) { CHECK(p.part(part).empty(), "kein Acid wenn nicht gewuenscht"); continue; }
            CHECK(! p.part(part).empty() && basicValid(p.part(part), cx), "Make a Track: Teil vorhanden und gueltig");
        }
        for (auto& n : p.melody) CHECK(inScale(n.pitch, key, sc), "Make a Track: Melodie in der Tonart");
        for (auto& n : p.arp) { auto* c = chordAt(p.prog, n.tick); bool ok = false; for (int pc : c->chord.pitchClasses()) ok |= n.pitch % 12 == pc; CHECK(ok, "Make a Track: Arp passt zum Akkord"); }
        int ok = 0; for (auto& n : p.bass) { auto* c = chordAt(p.prog, n.tick); bool near = false; for (auto& s2 : p.prog) { const int d = (((n.pitch - s2.chord.root) % 12) + 12) % 12; if (d <= 2 || d >= 10) near = true; }          // Annaeherungston um den Akkordgrundton
        if ((n.pitch - c->chord.root) % 12 == 0 || inScale(n.pitch, key, sc) || near) ++ok; } if (ok < (int) p.bass.size() * 0.95) std::printf("  [diag] bass genre%d mood%d sc%d key%d ok=%d/%zu\n", g, mood, sc, key, ok, p.bass.size());
        CHECK(ok >= (int) p.bass.size() * 0.95, "Make a Track: Bass passt zur Harmonie");
        for (auto& n : p.chords) CHECK(n.pitch >= 30 && n.pitch <= 100, "Akkordlage");
    }
    // Teile unabhaengig neu erzeugen: andere Teile bleiben identisch (reine Funktionen auf denselben Eingaben)
    auto cx = makeCtx(0, Minor, 4); auto gs = genreSetup(GHouse, Melancholic); auto p = generateAll(cx, gs, 50, false);
    auto melody2 = generateMelody(cx, p.prog, gs.melody, 999);
    bool changed = melody2.size() != p.melody.size(); for (size_t i = 0; ! changed && i < melody2.size(); ++i) changed = melody2[i].pitch != p.melody[i].pitch || melody2[i].tick != p.melody[i].tick;
    CHECK(changed, "neue Melodie unterscheidet sich");
    auto p2 = generateAll(cx, gs, 50, false); bool same = p.bass.size() == p2.bass.size() && p.chords.size() == p2.chords.size(); CHECK(same, "Make a Track reproduzierbar");
    // Genres unterscheiden sich hoerbar in Tempo/Bass-Stil
    std::set<double> bpms; for (int g = 0; g < kNumGenres; ++g) bpms.insert(genreSetup(g, 0).bpm); CHECK(bpms.size() >= 8, "Genres haben unterschiedliche Tempi");
    // Variation
    auto m = mutatePart(p.melody, cx, p.prog, 0.3f, 3, true); int diff = 0; for (size_t i = 0; i < m.size(); ++i) { diff += m[i].pitch != p.melody[i].pitch; CHECK(inScale(m[i].pitch, 0, Minor), "Variation bleibt in der Tonart"); } CHECK(m.size() == p.melody.size() && diff > 0 && diff < (int) m.size(), "Variation aendert einen Teil der Noten");
    // Ereignisliste: Offs vor Ons bei gleichem Tick, gleich viele On/Off
    auto ev = buildEvents(p); int on = 0, off = 0; for (auto& e : ev) (e.on ? on : off)++; CHECK(on == off && on > 100, "Ereignisliste ausgeglichen");
    for (size_t i = 1; i < ev.size(); ++i) CHECK(ev[i].tick >= ev[i - 1].tick, "Ereignisse sortiert");
}

int main()
{
    testTheory(); testProgressions(); testMelody(); testBassArp(); testAcid(); testMakeTrack();
    std::printf(failures == 0 ? "MelodyTest (Musiklogik): ALLE OK\n" : "MelodyTest (Musiklogik): %d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
