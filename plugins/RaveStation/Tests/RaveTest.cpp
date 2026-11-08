// Engine-Tests fuer RAVE//STATION (Stufe 2A): Stimmen, Sequencer-Timing, Swing, Ratchets, Wahrscheinlichkeit,
// Mute/Solo, Pattern-Wechsel, Polyphonie-Stresstest, Pattern-Serialisierung, Sample-Mailbox.
// Hoert nichts: geprueft werden Zahlen (Onset-Positionen, Endlichkeit, Pegel), keine Klangqualitaet.
#include "../Source/engine/Engine.h"
#include "../Source/engine/KitSynth.h"
#include "../Source/engine/Slicer.h"
#include "../Source/engine/Generator.h"
#include <juce_events/juce_events.h>
#include <cstdio>

using namespace rave;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static constexpr double SR = 44100.0;
static constexpr double STEP = 5512.5;   // 1/16 bei 120 BPM
static constexpr int LOOP = 88200;       // 16 Schritte

// Klick-Sample: 200 Samples konstant 1.0, damit Onsets eindeutig sind.
static SamplePtr clickSample(int len = 2000)
{
    juce::AudioBuffer<float> b(1, len);
    for (int i = 0; i < len; ++i) b.setSample(0, i, 1.f);
    return makeSample(b, SR, "click");
}

struct Rig
{
    PatternBank bank; RaveEngine eng { bank }; PadParams pads[kPads]; GlobalParams g;
    Rig(bool withClick = true)
    {
        eng.prepare(SR, 4096);
        g.hostSync = false; g.playing = true; g.bpm = 120.0; g.stepDiv = 1; g.master = 1.f;
        for (auto& p : pads) { p.gain = 0.5f; p.dec = 10.f; }
        if (withClick) for (int i = 0; i < kPads; ++i) eng.postSample(i, RaveEngine::kSource, clickSample());
        push();
    }
    void push() { eng.setPads(pads); eng.setGlobal(g); }
    // rendert total Samples in Bloecken der Groesse blk (mono linkes Signal)
    std::vector<float> render(int total, int blk, const std::vector<std::pair<int, int>>& noteOns = {})
    {
        std::vector<float> out((size_t) total);
        juce::AudioBuffer<float> buf(2, blk);
        HostInfo h;
        for (int pos = 0; pos < total; pos += blk)
        {
            const int n = juce::jmin(blk, total - pos);
            buf.setSize(2, n, false, false, true);
            juce::MidiBuffer m;
            for (auto& no : noteOns) if (no.first >= pos && no.first < pos + n) m.addEvent(juce::MidiMessage::noteOn(1, 36 + no.second, (juce::uint8) 127), no.first - pos);
            eng.process(buf, m, h);
            std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + n, out.begin() + pos);
            eng.drainTrash();
        }
        return out;
    }
};

// Onsets = Uebergang von "leise" (< 0.02) nach "laut" (>= 0.02); ein Plateau zaehlt nur einmal.
static std::vector<int> onsets(const std::vector<float>& x, int /*unused*/ = 0)
{
    std::vector<int> r; bool loud = false;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const bool l = std::abs(x[i]) > 0.02f;
        if (l && ! loud) r.push_back((int) i);
        loud = l;
    }
    return r;
}

static void testSimpleVoice()
{
    Rig r; r.bank.set(0, 0, 0, { 127 });
    auto x = r.render(20000, 512);
    auto o = onsets(x);
    CHECK(o.size() == 1, "n=%d", (int) o.size());
    if (! o.empty()) CHECK(o[0] < 40, "erster Onset bei %d (erwartet nahe 0)", o[0]);
    float pk = 0; for (float v : x) pk = juce::jmax(pk, std::abs(v));
    CHECK(pk > 0.3f && pk <= 1.0f, "peak=%f", pk);
}

static void testTimingBlockIndependent()
{
    // 120 BPM, 1/16: Schritt = 5512.5 Samples. Treffer auf Schritt 0, 4, 9
    const int steps[] = { 0, 4, 9 };
    for (int blk : { 64, 100, 512, 1000, 4096 })
    {
        Rig r; for (int s : steps) r.bank.set(0, 0, s, { 127 });
        auto o = onsets(r.render((int) (STEP * 12), blk));
        CHECK(o.size() == 3, "blk=%d n=%d", blk, (int) o.size());
        for (size_t i = 0; i < o.size() && i < 3; ++i)
            CHECK(std::abs(o[i] - (int) std::lround(steps[i] * STEP)) <= 24, "blk=%d onset %d bei %d erwartet %d", blk, (int) i, o[i], (int) std::lround(steps[i] * STEP));
    }
}

static void testPatternLoops()
{
    Rig r; r.bank.set(0, 0, 0, { 127 });         // 16 Schritte = 88200 Samples
    auto o = onsets(r.render(LOOP * 3 + 100, 777));
    CHECK(o.size() == 4, "n=%d", (int) o.size());
    for (size_t i = 0; i < o.size(); ++i) CHECK(std::abs(o[i] - (int) i * LOOP) <= 24, "loop %d bei %d", (int) i, o[i]);
}

static void testSwing()
{
    Rig r; r.g.swing = 1.0f; r.push();
    r.bank.set(0, 0, 0, { 127 }); r.bank.set(0, 0, 1, { 127 });
    auto o = onsets(r.render((int) (STEP * 4), 512));
    CHECK(o.size() == 2, "n=%d", (int) o.size());
    if (o.size() == 2)
    {
        const int expect = (int) std::lround(STEP + 0.5 * STEP);   // ungerader Schritt um swing*0.5*Schritt verzoegert
        CHECK(std::abs(o[1] - expect) <= 24, "swing onset %d erwartet %d", o[1], expect);
    }
}

static void testRatchet()
{
    Rig r; Step s{ 127 }; s.ratchet = 4; r.bank.set(0, 0, 0, s);
    r.pads[0].dec = 0.02f; r.push();                  // kurzer Decay, damit Teiltreffer getrennt sind
    auto o = onsets(r.render((int) STEP, 512), 100);
    CHECK(o.size() == 4, "ratchets=%d", (int) o.size());
    for (size_t i = 0; i < o.size(); ++i) CHECK(std::abs(o[i] - (int) (i * STEP / 4)) <= 24, "ratchet %d bei %d", (int) i, o[i]);
}

static void testProbability()
{
    {   Rig r; Step s{ 127 }; s.prob = 0; for (int i = 0; i < 16; ++i) r.bank.set(0, 0, i, s);
        CHECK(onsets(r.render(LOOP * 2, 512)).empty(), "prob 0 darf nichts spielen"); }
    Rig a, b;
    Step s{ 127 }; s.prob = 50;
    for (int i = 0; i < 16; ++i) { a.bank.set(0, 0, i, s); b.bank.set(0, 0, i, s); }
    auto xa = a.render(LOOP * 2, 512), xb = b.render(LOOP * 2, 333);
    auto oa = onsets(xa), ob = onsets(xb);
    bool same = oa.size() == ob.size(); for (size_t i = 0; same && i < oa.size(); ++i) same = std::abs(oa[i] - ob[i]) <= 2;
    CHECK(same, "Wahrscheinlichkeit nicht reproduzierbar / blockabhaengig (%d vs %d)", (int) oa.size(), (int) ob.size());
    CHECK(oa.size() > 6 && oa.size() < 26, "50%% ergab %d von 32", (int) oa.size());
    a.g.seed = 7; a.eng.reset(); a.push();
    auto oc = onsets(a.render(LOOP * 2, 512));
    CHECK(oc.size() != oa.size() || ! std::equal(oc.begin(), oc.end(), oa.begin(), [](int x, int y) { return std::abs(x - y) <= 2; }), "anderer Seed sollte anderes Muster liefern");
}

static void testMuteSolo()
{
    Rig r; r.bank.set(0, 0, 0, { 127 }); r.bank.set(0, 1, 0, { 127 });
    r.pads[0].mute = true; r.push();
    auto x = r.render(3000, 512);
    float pk = 0; for (float v : x) pk = juce::jmax(pk, std::abs(v));
    CHECK(pk > 0.2f && pk < 0.6f, "nur Pad 1 sollte klingen, peak=%f", pk);
    Rig q; q.bank.set(0, 0, 0, { 127 }); q.bank.set(0, 1, 0, { 127 }); q.pads[1].solo = true; q.push();
    auto y = q.render(3000, 512); pk = 0; for (float v : y) pk = juce::jmax(pk, std::abs(v));
    CHECK(pk > 0.2f && pk < 0.6f, "Solo: peak=%f", pk);
}

static void testPatternSwitch()
{
    Rig r; r.bank.set(0, 0, 0, { 127 }); r.bank.set(1, 1, 0, { 127 }); r.bank.setLength(1, 16);
    r.pads[0].gain = 0.25f; r.pads[1].gain = 0.5f; r.push();
    const int len = LOOP;
    (void) r.render(len / 2, 512);                         // mitten in Pattern 0
    r.g.slot = 1; r.push();
    auto x = r.render(len * 2, 512);                       // der Wechsel muss am Pattern-Ende (bei len) kommen
    auto o = onsets(x);
    CHECK(o.size() >= 1, "kein Treffer nach Wechsel");
    if (! o.empty()) { CHECK(std::abs(o[0] + len / 2 - len) <= 24, "Wechsel bei %d statt %d", o[0] + len / 2, len);
                       CHECK(std::abs(x[(size_t) o[0] + 60] - 0.5f) < 0.05f, "falsches Pad nach Wechsel (%f)", x[(size_t) o[0] + 60]); }
}

static void testMidiTrigger()
{
    Rig r; r.g.playing = false; r.push();
    auto o = onsets(r.render(20000, 300, { { 1000, 3 } }));
    CHECK(o.size() == 1 && std::abs(o[0] - 1000) <= 24, "MIDI-Onset %d", o.empty() ? -1 : o[0]);
}

static void testPolyphonyStress()
{
    Rig r(false);
    for (int i = 0; i < kPads; ++i) r.eng.postSample(i, RaveEngine::kSource, kit::makePad(i));
    r.g.bpm = 200; r.g.stepDiv = 2; r.g.swing = 0.3f; r.g.humTime = 0.5f; r.g.humVel = 0.5f; r.push();
    for (int i = 0; i < 16; ++i) { r.pads[i].drv = 0.5f; r.pads[i].lp = 4000; r.pads[i].res = 0.4f; r.pads[i].crush = 0.2f; r.pads[i].stutter = (i % 4); r.pads[i].dec = 0.8f; }
    r.push();
    r.bank.setLength(0, 32);
    for (int p = 0; p < kPads; ++p) for (int s = 0; s < 32; ++s) { Step st{ 110 }; st.ratchet = (uint8_t) (1 + (s + p) % 4); r.bank.set(0, p, s, st); }
    auto x = r.render((int) SR * 8, 256);
    bool finite = true; float pk = 0; for (float v : x) { if (! std::isfinite(v)) finite = false; pk = juce::jmax(pk, std::abs(v)); }
    CHECK(finite, "NaN/Inf im Ausgang");
    CHECK(pk <= 1.0f, "Pegel %f ueber 1.0", pk);
    CHECK(pk > 0.3f, "kaum Signal: %f", pk);
    CHECK(r.eng.voicesActive.load() <= RaveEngine::kVoices, "Stimmen %d", r.eng.voicesActive.load());
}

static void testMailboxAndStateString()
{
    Rig r; r.bank.set(0, 0, 0, { 127 });
    auto first = onsets(r.render(2000, 512));
    // Sample ersetzen: halber Pegel
    juce::AudioBuffer<float> b(1, 2000); for (int i = 0; i < 2000; ++i) b.setSample(0, i, 0.5f);
    r.eng.postSample(0, RaveEngine::kSource, makeSample(b, SR, "half"));
    (void) r.render(512, 512);
    r.eng.auditionPad(0, 1.f);
    auto y = r.render(1000, 512);
    CHECK(std::abs(y[100] - 0.25f) < 0.03f, "Ersatz-Sample nicht aktiv (%f)", y[100]);

    PatternBank a, c;
    Step s{ 99 }; s.prob = 60; s.ratchet = 3; s.flags = kGhost | kRoll; s.pitch = -5;
    a.set(2, 3, 7, s); a.set(2, 15, 63, { 5 }); a.setLength(2, 48);
    CHECK(c.fromString(5, a.toString(2)), "fromString");
    PlainPattern pa, pc; a.load(2, pa); c.load(5, pc);
    CHECK(pa == pc, "Pattern-Round-Trip");
    CHECK(! c.fromString(1, "Muell"), "Muell muss abgelehnt werden");
    CHECK(! c.fromString(1, "L=16;99,0,1,1,1,0,0"), "ungueltiges Pad muss abgelehnt werden");
}

static void testSlicerDetection()
{
    std::vector<int> truth; auto brk = kit::makeDemoBreak(&truth, 170.0);
    const int tol = (int) (0.012 * SR);   // 12 ms
    std::printf("Demo-Break: %d echte Onsets, %.2f s\n", (int) truth.size(), brk->seconds());
    float bestF = 0;
    for (float sens : { 0.3f, 0.5f, 0.6f, 0.75f, 0.9f })
    {
        auto det = slicer::detectOnsets(brk->data, brk->rate, sens);
        int hit = 0; std::vector<bool> used(det.size(), false);
        for (int t : truth) for (size_t i = 0; i < det.size(); ++i) if (! used[i] && std::abs(det[i] - t) <= tol) { used[i] = true; ++hit; break; }
        const double rec = hit / (double) truth.size(), prec = det.empty() ? 0.0 : hit / (double) det.size();
        std::printf("  Sensitivitaet %.2f: erkannt %3d  Treffer %3d  Recall %.2f  Praezision %.2f\n", sens, (int) det.size(), hit, rec, prec);
        if (sens > 0.55f && sens < 0.65f && (rec < 0.85 || prec < 0.85))
        {
            std::printf("    truth:"); for (int t : truth) std::printf(" %d", t);
            std::printf("\n    det:  "); for (int d : det) std::printf(" %d", d); std::printf("\n");
        }
        if (sens > 0.55f && sens < 0.65f) { CHECK(rec >= 0.85, "Recall bei 0.6 nur %.2f", rec); CHECK(prec >= 0.85, "Praezision bei 0.6 nur %.2f", prec); }
        bestF = juce::jmax(bestF, (float) (2 * rec * prec / juce::jmax(1e-9, rec + prec)));
    }
    CHECK(bestF > 0.9f, "bestes F-Mass %.2f", bestF);
    {   // Zeitliche Genauigkeit bei Sensitivitaet 0.6: Abstand jedes erkannten Onsets zum naechsten echten
        auto det = slicer::detectOnsets(brk->data, brk->rate, 0.6f);
        std::vector<double> errMs;
        for (int d : det) { int best = 1 << 30; for (int tr : truth) if (std::abs(d - tr) < std::abs(best)) best = d - tr; errMs.push_back(best * 1000.0 / SR); }
        std::sort(errMs.begin(), errMs.end(), [](double a, double b) { return std::abs(a) < std::abs(b); });
        const double med = std::abs(errMs[errMs.size() / 2]), p90 = std::abs(errMs[(errMs.size() * 9) / 10]), mx = std::abs(errMs.back());
        std::printf("  Zeitfehler: Median %.2f ms, 90%%-Perzentil %.2f ms, Maximum %.2f ms\n", med, p90, mx);
        CHECK(med < 2.0, "Median-Fehler %.2f ms", med);
        CHECK(p90 < 6.0, "90%%-Fehler %.2f ms", p90);
    }
    // Stille / Rauschen darf keine Slices erzeugen
    juce::AudioBuffer<float> silent(1, 44100); silent.clear();
    CHECK(slicer::detectOnsets(silent, SR, 0.9f).empty(), "Stille ergab Onsets");
    // Marker-Verwaltung
    slicer::SliceModel m; m.source = brk; m.detect(0.6f);
    CHECK(m.numSlices() > 8 && m.markers.front() == 0, "Marker-Start");
    const int n0 = m.numSlices();
    CHECK(m.addMarker(m.startOf(1) + 3000), "addMarker"); CHECK(m.numSlices() == n0 + 1, "Markeranzahl");
    CHECK(! m.addMarker(m.startOf(1) + 3010), "zu nah beieinander muss abgelehnt werden");
    CHECK(m.removeMarker(2) && m.numSlices() == n0, "removeMarker"); CHECK(! m.removeMarker(0), "Marker 0 darf nicht weg");
    CHECK(m.moveMarker(1, m.startOf(1) + 100), "moveMarker");
    int total = 0; for (int i = 0; i < m.numSlices(); ++i) total += m.endOf(i) - m.startOf(i);
    CHECK(total == brk->length(), "Slices muessen lueckenlos das Sample ergeben (%d vs %d)", total, brk->length());
    // Neuanordnung: Identitaet ergibt dieselbe Laenge, Umkehrung/Stutter/Stretch veraendern wie erwartet
    std::vector<slicer::SliceOp> id; for (int i = 0; i < m.numSlices(); ++i) { slicer::SliceOp o; o.slice = i; id.push_back(o); }
    auto same = slicer::renderArrangement(m, id, "id");
    CHECK(same && same->length() == brk->length(), "Identitaets-Arrangement Laenge %d", same ? same->length() : -1);
    auto sh1 = slicer::shuffledOps(m.numSlices(), 5), sh2 = slicer::shuffledOps(m.numSlices(), 5), sh3 = slicer::shuffledOps(m.numSlices(), 6);
    bool eq = true, neq = false; for (size_t i = 0; i < sh1.size(); ++i) { eq &= sh1[i].slice == sh2[i].slice; neq |= sh1[i].slice != sh3[i].slice; }
    CHECK(eq && neq, "Shuffle muss reproduzierbar und seed-abhaengig sein");
    CHECK(sh1[0].slice == 0, "erste Slice bleibt vorn");
    slicer::SliceOp rv; rv.slice = 0; rv.reverse = true;
    auto fwd = slicer::extractSlice(*brk, m.startOf(0), m.endOf(0)); auto rev = slicer::processSlice(*brk, m.startOf(0), m.endOf(0), rv);
    CHECK(rev.getNumSamples() == fwd.getNumSamples(), "reverse Laenge");
    slicer::SliceOp st; st.slice = 0; st.stretch = 1.5f; auto stch = slicer::processSlice(*brk, m.startOf(0), m.endOf(0), st);
    CHECK(std::abs(stch.getNumSamples() / (double) fwd.getNumSamples() - 1.5) < 0.02, "stretch Laenge %.3f", stch.getNumSamples() / (double) fwd.getNumSamples());
    slicer::SliceOp pt; pt.slice = 0; pt.semis = 12.f; auto pit = slicer::processSlice(*brk, m.startOf(0), m.endOf(0), pt);
    CHECK(std::abs(pit.getNumSamples() / (double) fwd.getNumSamples() - 0.5) < 0.02, "pitch +12 halbiert die Laenge (%.3f)", pit.getNumSamples() / (double) fwd.getNumSamples());
    slicer::SliceOp su; su.slice = 0; su.stutter = 4; auto stu = slicer::processSlice(*brk, m.startOf(0), m.endOf(0), su);
    CHECK(std::abs(stu.getNumSamples() - fwd.getNumSamples()) <= 4, "stutter behaelt Laenge");
    // Slices -> Pads / Pattern
    auto pads = slicer::slicesToPads(m, 0, "Brk");
    CHECK((int) pads.size() == juce::jmin(16, m.numSlices()), "Pad-Verteilung %d", (int) pads.size());
    auto pat = slicer::slicesToPattern(m, 0, 32, (int) (32 * 60.0 / 170.0 / 4.0 * SR));
    CHECK(pat.hits() >= juce::jmin(16, m.numSlices()) - 2, "Slice->Pattern Treffer %d", pat.hits());
}


// ------------------------------------------------------------------------------------------------ Generator / EVOLVE
static gen::RoleMap defaultRoles()
{
    int roles[kPads]; for (int i = 0; i < kPads; ++i) roles[i] = kit::kDefaultRoles[i];
    return gen::RoleMap::fromRoles(roles);
}
static bool hitAt(const PlainPattern& p, const std::vector<int>& pads, int st) { for (int pd : pads) if (p.s[pd][st].on()) return true; return false; }
static bool realAt(const PlainPattern& p, const std::vector<int>& pads, int st) { for (int pd : pads) if (p.s[pd][st].on() && ! (p.s[pd][st].flags & kGhost)) return true; return false; }
static int distance(const PlainPattern& a, const PlainPattern& b)
{
    int d = 0; for (int p = 0; p < kPads; ++p) for (int i = 0; i < kMaxSteps; ++i) d += (a.s[p][i].on() != b.s[p][i].on()) || (a.s[p][i].on() && ! (a.s[p][i] == b.s[p][i]));
    return d;
}
static int totalHits(const PlainPattern& p) { return p.hits(); }

static void testGenerator()
{
    const auto rm = defaultRoles();
    for (int genre = 0; genre < gen::kGenres; ++genre)
    {
        const bool exp = genre == gen::Experimental;
        for (int seed = 1; seed <= 20; ++seed)
        {
            gen::GenParams gp; gp.genre = genre; gp.seed = seed; gp.lengthSteps = 32;
            auto r = gen::generate(gp, rm);
            CHECK(r.pattern.length == 32 && r.bars == 2 && r.stepsPerBar == 16, "genre %d: Laenge %d", genre, r.pattern.length);
            CHECK(r.pattern.hits() >= 6, "genre %d seed %d: nur %d Treffer", genre, seed, r.pattern.hits());
            CHECK(r.pattern.hits() <= 32 * 7, "genre %d seed %d: zu viele Treffer %d", genre, seed, r.pattern.hits());
            for (int p = 0; p < kPads; ++p) for (int i = 0; i < kMaxSteps; ++i)
            {
                const auto s = r.pattern.s[p][i];
                if (i >= r.pattern.length) CHECK(! s.on(), "Treffer ausserhalb der Laenge");
                if (s.on()) { CHECK(s.vel >= 1 && s.vel <= 127 && s.prob >= 1 && s.prob <= 100 && s.ratchet >= 1 && s.ratchet <= 8 && std::abs(s.pitch) <= 12, "Wertebereich"); }
            }
            if (exp) continue;
            for (int bar = 0; bar < 2; ++bar)
            {
                CHECK(hitAt(r.pattern, rm.pads[gen::rKick], bar * 16), "genre %d seed %d: keine Kick auf der Eins (Takt %d)", genre, seed, bar);
                for (int bb : { 4, 12 })
                {
                    // im letzten Takt darf ein Fill die Zaehlzeit 4 uebernehmen; Anker auf 4 bleibt dennoch Pflicht
                    const bool ok = hitAt(r.pattern, rm.pads[gen::rSnare], bar * 16 + bb) || hitAt(r.pattern, rm.pads[gen::rClap], bar * 16 + bb);
                    CHECK(ok, "genre %d seed %d: kein Backbeat auf Schritt %d (Takt %d)", genre, seed, bb, bar);
                }
            }
            if (genre <= gen::Garage)
                for (int st = 0; st < 32; ++st)
                    if (st % 16 != 0) CHECK(! (hitAt(r.pattern, rm.pads[gen::rKick], st) && (realAt(r.pattern, rm.pads[gen::rSnare], st) || hitAt(r.pattern, rm.pads[gen::rClap], st))), "genre %d: Kick+Snare auf Schritt %d", genre, st);
        }
    }

    // Determinismus und Seed-Abhaengigkeit
    {
        gen::GenParams a; a.genre = gen::Jungle; a.seed = 4; a.randomness = 0.5f;
        auto p1 = gen::generate(a, rm).pattern, p2 = gen::generate(a, rm).pattern;
        CHECK(p1 == p2, "gleicher Seed muss gleiches Pattern liefern");
        int differing = 0; for (int s = 5; s < 25; ++s) { a.seed = s; differing += ! (gen::generate(a, rm).pattern == p1); }
        CHECK(differing >= 15, "Seeds ergeben zu oft dasselbe Pattern (%d/20 verschieden)", differing);
    }
    // Dichte: monoton
    for (int genre = 0; genre < gen::kGenres; ++genre)
        for (int seed = 1; seed <= 10; ++seed)
        {
            gen::GenParams gp; gp.genre = genre; gp.seed = seed; gp.variation = 0.f; gp.randomness = 0.f;
            int last = -1;
            for (float d : { 0.05f, 0.35f, 0.65f, 0.95f })
            {
                gp.density = d; const int h = totalHits(gen::generate(gp, rm).pattern);
                CHECK(h >= last, "genre %d seed %d: Dichte %.2f -> %d Treffer (vorher %d)", genre, seed, d, h, last);
                last = h;
            }
        }
    // Ghost Notes
    {
        double g0 = 0, g3 = 0, g10 = 0; int bad = 0;
        for (int seed = 1; seed <= 30; ++seed)
        {
            gen::GenParams gp; gp.genre = gen::Jungle; gp.seed = seed; gp.variation = 0.f;
            for (float gh : { 0.f, 0.3f, 1.f })
            {
                gp.ghost = gh; auto pat = gen::generate(gp, rm).pattern; int n = 0;
                for (int p = 0; p < kPads; ++p) for (int i = 0; i < 32; ++i)
                    if (pat.s[p][i].on() && (pat.s[p][i].flags & kGhost))
                    {
                        ++n;
                        if (pat.s[p][i].vel > 70 || hitAt(pat, rm.pads[gen::rKick], i) || (i % 16 == 4) || (i % 16 == 12)) ++bad;
                    }
                (gh < 0.01f ? g0 : gh < 0.5f ? g3 : g10) += n;
            }
        }
        std::printf("  Ghost Notes (30 Seeds, Jungle): ghost=0 -> %.0f, 0.3 -> %.0f, 1.0 -> %.0f\n", g0, g3, g10);
        CHECK(g0 < 0.5 && g3 > 0 && g10 > g3, "Ghost-Regler wirkt nicht monoton");
        CHECK(bad == 0, "%d Ghost-Notes auf Kick/Backbeat oder zu laut", bad);
    }
    // Variation
    {
        int same0 = 0, diff1 = 0;
        for (int seed = 1; seed <= 20; ++seed)
        {
            gen::GenParams gp; gp.genre = gen::Breakbeat; gp.seed = seed; gp.variation = 0.f; gp.randomness = 0.f;
            auto p0 = gen::generate(gp, rm).pattern; bool eq = true;
            for (int pd = 0; pd < kPads; ++pd) for (int i = 0; i < 16; ++i) eq &= p0.s[pd][i] == p0.s[pd][i + 16];
            same0 += eq;
            gp.variation = 1.f; gp.randomness = 0.3f;
            auto p1 = gen::generate(gp, rm).pattern; eq = true;
            for (int pd = 0; pd < kPads; ++pd) for (int i = 0; i < 16; ++i) eq &= p1.s[pd][i] == p1.s[pd][i + 16];
            diff1 += ! eq;
        }
        CHECK(same0 == 20, "variation=0: Takte muessen identisch sein (%d/20)", same0);
        CHECK(diff1 >= 18, "variation=1: Takte muessen sich unterscheiden (%d/20)", diff1);
    }
    // Intensitaet -> mittlere Velocity, Komplexitaet -> mehr Off-Grid-Treffer
    {
        double v0 = 0, v1 = 0, o0 = 0, o1 = 0; int n0 = 0, n1 = 0;
        for (int seed = 1; seed <= 30; ++seed)
        {
            gen::GenParams gp; gp.genre = gen::DnB; gp.seed = seed; gp.variation = 0.f;
            gp.intensity = 0.1f; { auto p = gen::generate(gp, rm).pattern; for (int pd = 0; pd < kPads; ++pd) for (int i = 0; i < 32; ++i) if (p.s[pd][i].on()) { v0 += p.s[pd][i].vel; ++n0; } }
            gp.intensity = 0.95f; { auto p = gen::generate(gp, rm).pattern; for (int pd = 0; pd < kPads; ++pd) for (int i = 0; i < 32; ++i) if (p.s[pd][i].on()) { v1 += p.s[pd][i].vel; ++n1; } }
            gp.intensity = 0.6f; gp.complexity = 0.f; { auto p = gen::generate(gp, rm).pattern; for (int pd = 0; pd < kPads; ++pd) for (int i = 0; i < 32; ++i) if (p.s[pd][i].on() && i % 2) ++o0; }
            gp.complexity = 1.f; { auto p = gen::generate(gp, rm).pattern; for (int pd = 0; pd < kPads; ++pd) for (int i = 0; i < 32; ++i) if (p.s[pd][i].on() && i % 2) ++o1; }
        }
        std::printf("  Intensitaet: mittlere Velocity %.1f -> %.1f;  Komplexitaet: ungerade Schritte %.0f -> %.0f\n", v0 / n0, v1 / n1, o0, o1);
        CHECK(v1 / n1 > v0 / n0 + 8, "Intensitaet erhoeht Velocity zu wenig");
        CHECK(o1 > o0 * 1.15, "Komplexitaet erhoeht Synkopen zu wenig");
    }
    // Genre-Charakter
    {
        int fourFloor = 0, dnbKicks = 0, total = 0;
        for (int seed = 1; seed <= 20; ++seed)
        {
            for (int g : { (int) gen::Techno, (int) gen::OldschoolRave, (int) gen::Hardcore })
            { gen::GenParams gp; gp.genre = g; gp.seed = seed; auto p = gen::generate(gp, rm).pattern; bool all = true; for (int st : { 0, 4, 8, 12 }) all &= hitAt(p, rm.pads[gen::rKick], st); fourFloor += all; ++total; }
            gen::GenParams gp; gp.genre = gen::DnB; gp.seed = seed; gp.density = 0.5f; auto p = gen::generate(gp, rm).pattern;
            int k = 0; for (int i = 0; i < 16; ++i) k += hitAt(p, rm.pads[gen::rKick], i); dnbKicks += k;
        }
        CHECK(fourFloor == total, "Techno/Rave/Hardcore: Four-on-the-floor in %d/%d", fourFloor, total);
        CHECK(dnbKicks / 20.0 <= 4.0, "DnB hat im Schnitt %.1f Kicks pro Takt", dnbKicks / 20.0);
        gen::GenParams a; a.genre = gen::DnB; CHECK(std::abs(gen::generate(a, rm).bpm - 174.0) < 1.0, "DnB-Tempo");
        a.genre = gen::Garage; CHECK(gen::generate(a, rm).swing > 0.4f, "Garage-Swing");
    }
    // Taktarten / Raster
    for (int sig = 0; sig < 8; ++sig)
        for (int div = 0; div < 3; ++div)
        {
            gen::GenParams gp; gp.genre = gen::Breakbeat; gp.sig = sig; gp.stepDiv = div; gp.lengthSteps = 64; gp.seed = 3;
            auto r = gen::generate(gp, rm);
            const int spb = gen::stepsPerBar(sig, div);
            CHECK(r.pattern.length == r.bars * spb && r.pattern.length <= 64 && r.bars >= 1, "sig %d div %d: Laenge %d (spb %d)", sig, div, r.pattern.length, spb);
            for (int b = 0; b < r.bars; ++b) CHECK(hitAt(r.pattern, rm.pads[gen::rKick], b * spb), "sig %d div %d: Kick fehlt auf Takteins %d", sig, div, b);
            CHECK(r.pattern.hits() >= 3, "sig %d div %d: fast leer", sig, div);
        }
    CHECK(gen::stepsPerBar(2, 1) == 16 && gen::stepsPerBar(1, 1) == 12 && gen::stepsPerBar(5, 1) == 14 && gen::stepsPerBar(4, 1) == 12 && gen::stepsPerBar(2, 2) == 32 && gen::stepsPerBar(2, 0) == 8, "Schritte pro Takt");
    // fehlende Rollen: kein Absturz, nichts auf ungueltigen Pads
    {
        int roles[kPads]; for (int& r : roles) r = gen::rNone; roles[0] = gen::rKick;
        auto sparse = gen::RoleMap::fromRoles(roles);
        gen::GenParams gp; gp.genre = gen::Jungle;
        auto r = gen::generate(gp, sparse);
        CHECK(r.pattern.hits(0) > 0 && r.pattern.hits() == r.pattern.hits(0), "nur Kick-Pad: Treffer %d / %d", r.pattern.hits(0), r.pattern.hits());
        int none[kPads]; for (int& r2 : none) r2 = gen::rNone;
        CHECK(gen::generate(gp, gen::RoleMap::fromRoles(none)).pattern.hits() == 0, "ohne Rollen darf nichts entstehen");
    }
}

static void testEvolve()
{
    const auto rm = defaultRoles();
    gen::GenParams gp; gp.genre = gen::Jungle; gp.seed = 2; gp.density = 0.6f;
    const auto base = gen::generate(gp, rm).pattern;
    const int baseHits = base.hits();
    auto copy = base;
    for (float amount : { 0.1f, 0.5f, 0.9f })
    {
        double dist = 0; int identical = 0, anchorLoss = 0;
        for (int seed = 1; seed <= 30; ++seed)
        {
            auto v = gen::evolve(base, amount, seed, rm, 16);
            CHECK(base == copy, "EVOLVE hat das Original veraendert!");
            const int d = distance(base, v);
            dist += d; identical += (d == 0);
            for (int bar = 0; bar < 2; ++bar)
            {
                if (! hitAt(base, rm.pads[gen::rKick], bar * 16)) continue;
                anchorLoss += ! hitAt(v, rm.pads[gen::rKick], bar * 16);
                for (int bb : { 4, 12 }) if (hitAt(base, rm.pads[gen::rSnare], bar * 16 + bb)) anchorLoss += ! (hitAt(v, rm.pads[gen::rSnare], bar * 16 + bb) || hitAt(v, rm.pads[gen::rClap], bar * 16 + bb));
            }
            CHECK(v.length == base.length, "Laenge");
        }
        std::printf("  EVOLVE amount %.1f: mittlere Aenderung %.1f von %d Treffern, identisch %d/30, verlorene Anker %d\n", amount, dist / 30, baseHits, identical, anchorLoss);
        CHECK(identical <= 3, "amount %.1f: zu oft identisch (%d/30)", amount, identical);
        CHECK(anchorLoss == 0, "amount %.1f: Anker verloren", amount);
        CHECK(dist / 30 < baseHits * 0.7, "amount %.1f: Aenderung zu gross (%.1f)", amount, dist / 30);
    }
    double d1 = 0, d9 = 0; for (int s = 1; s <= 30; ++s) { d1 += distance(base, gen::evolve(base, 0.1f, s, rm, 16)); d9 += distance(base, gen::evolve(base, 0.9f, s, rm, 16)); }
    CHECK(d9 > d1 * 1.5, "staerkeres EVOLVE muss mehr aendern (%.1f vs %.1f)", d1 / 30, d9 / 30);
    CHECK(gen::evolve(base, 0.5f, 7, rm, 16) == gen::evolve(base, 0.5f, 7, rm, 16), "EVOLVE nicht deterministisch");
    CHECK(! (gen::evolve(base, 0.5f, 7, rm, 16) == gen::evolve(base, 0.5f, 8, rm, 16)), "verschiedene Seeds ergeben dieselbe Variante");
    PlainPattern empty; empty.length = 16;
    CHECK(gen::evolve(empty, 0.9f, 1, rm, 16).length == 16, "leeres Pattern");
}

static int realMain();
int main()
{
    // Testobjekte (RaveProcessor ~400 KB) liegen hier auf dem Stack; Windows hat nur 1 MB -> grosser Thread-Stack
    struct Runner : juce::Thread { int rc = 1; Runner() : juce::Thread("test", 64u * 1024u * 1024u) {} void run() override { rc = realMain(); } } r;
    r.startThread(); r.waitForThreadToExit(-1); return r.rc;
}

static int realMain()
{
    juce::MessageManager::getInstance(); struct Guard { ~Guard() { juce::DeletedAtShutdown::deleteAll(); juce::MessageManager::deleteInstance(); } } guard;
    testSimpleVoice(); testTimingBlockIndependent(); testPatternLoops(); testSwing(); testRatchet();
    testProbability(); testMuteSolo(); testPatternSwitch(); testMidiTrigger(); testPolyphonyStress(); testMailboxAndStateString(); testSlicerDetection(); testGenerator(); testEvolve();
    std::printf(failures == 0 ? "RaveTest: ALLE OK\n" : "RaveTest: %d FEHLER\n", failures);
    return failures == 0 ? 0 : 1;
}
