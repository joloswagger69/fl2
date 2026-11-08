// Prozessor-Tests fuer MELODY//STUDIO: erste Idee, unabhaengige Neuerzeugung, Undo, Make a Track, MIDI-Export (Round Trip ueber juce::MidiFile),
// State-Round-Trip inkl. Noten, beschaedigter State, Audio-Ausgabe (endlich, begrenzt, nicht still), Parameter-Fuzz. Kein Klangurteil.
#include "../Source/MelodyProcessor.h"
#include <juce_events/juce_events.h>
#include <cstdio>

using namespace mel;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void prep(MelodyProcessor& p, double sr = 44100.0, int blk = 512) { p.setPlayConfigDetails(0, 2, sr, blk); p.prepareToPlay(sr, blk); }

struct Out { std::vector<float> l, r; };
static Out run(MelodyProcessor& p, int total, int blk = 512)
{
    Out o; juce::AudioBuffer<float> buf(2, blk);
    for (int pos = 0; pos < total; pos += blk)
    {
        const int n = juce::jmin(blk, total - pos); buf.setSize(2, n, false, false, true); juce::MidiBuffer m;
        p.processBlock(buf, m);
        o.l.insert(o.l.end(), buf.getReadPointer(0), buf.getReadPointer(0) + n); o.r.insert(o.r.end(), buf.getReadPointer(1), buf.getReadPointer(1) + n);
    }
    return o;
}
static float peak(const Out& o) { float m = 0; for (size_t i = 0; i < o.l.size(); ++i) m = std::max({ m, std::abs(o.l[i]), std::abs(o.r[i]) }); return m; }
static bool finite(const Out& o) { for (size_t i = 0; i < o.l.size(); ++i) if (! std::isfinite(o.l[i]) || ! std::isfinite(o.r[i])) return false; return true; }
static double rms(const Out& o) { double s = 0; for (float v : o.l) s += (double) v * v; return std::sqrt(s / std::max<size_t>(1, o.l.size())); }
static bool same(const Part& a, const Part& b) { if (a.size() != b.size()) return false; for (size_t i = 0; i < a.size(); ++i) if (a[i].tick != b[i].tick || a[i].pitch != b[i].pitch || a[i].len != b[i].len || a[i].vel != b[i].vel) return false; return true; }

static int realMain()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto up = std::make_unique<MelodyProcessor>(); auto& p = *up; prep(p);

    // --- erste Idee ohne Zutun
    {
        auto s = p.current();
        CHECK(s != nullptr, "keine erste Idee");
        CHECK(s && ! s->proj.chords.empty() && ! s->proj.melody.empty() && ! s->proj.bass.empty() && ! s->proj.arp.empty(), "Teile leer");
        CHECK(s && s->proj.acid.empty(), "Acid soll standardmaessig leer sein");
        CHECK(! p.canUndo(), "Undo nach Start");
        CHECK(! p.isStale(), "stale nach Start");
    }
    // --- unabhaengige Neuerzeugung
    {
        auto before = p.current(); const auto bc = before->proj.chords, bb = before->proj.bass, ba = before->proj.arp;
        p.regenerate(1, true);
        auto after = p.current();
        CHECK(! same(before->proj.melody, after->proj.melody), "Melody blieb gleich trotz neuem Seed");
        CHECK(same(bc, after->proj.chords) && same(bb, after->proj.bass) && same(ba, after->proj.arp), "andere Teile veraendert");
        CHECK(p.canUndo(), "Undo fehlt");
        CHECK(p.undo(), "undo fehlgeschlagen");
        CHECK(same(p.current()->proj.melody, before->proj.melody), "Undo stellt Melody nicht her");
        // Acid erzeugen
        p.regenerate(4, true);
        CHECK(! p.current()->proj.acid.empty(), "Acid leer");
        CHECK(same(p.current()->proj.chords, bc), "Chords nach Acid geaendert");
        // Variation
        const auto m0 = p.current()->proj.melody; p.variation(1, 0.4f);
        CHECK(! same(m0, p.current()->proj.melody), "Variation ohne Wirkung");
        p.clearPart(3); CHECK(p.current()->proj.arp.empty(), "clearPart");
    }
    // --- Tonart geaendert -> stale, Teil-Erzeugung erzeugt alles neu passend
    {
        p.setParam("key", 2.f); CHECK(p.isStale(), "stale fehlt");
        p.regenerate(2, true);
        CHECK(! p.isStale(), "noch stale");
        CHECK(p.current()->proj.ctx.key == 2, "ctx key");
        CHECK(! p.current()->proj.melody.empty() && ! p.current()->proj.chords.empty(), "alles neu erwartet");
    }
    // --- Make a Track: alle Genres und Moods, Laenge/Taktarten
    {
        for (int g = 0; g < 10; ++g) for (int m = 0; m < 7; ++m)
        {
            p.setParam("genre", (float) g); p.setParam("mood", (float) m); p.setParam("t_acid", g % 2 ? 1.f : 0.f); p.makeTrack(true);
            auto s = p.current();
            CHECK(! s->proj.chords.empty() && ! s->proj.melody.empty() && ! s->proj.bass.empty(), "Track g=%d m=%d leer", g, m);
            if (g % 2) CHECK(! s->proj.acid.empty(), "Acid gewuenscht g=%d", g);
            for (int q = 0; q < 5; ++q) for (auto& n : s->proj.part(q)) { CHECK(n.tick >= 0 && n.tick < s->proj.ctx.totalTicks() && n.len >= 1 && n.tick + n.len <= s->proj.ctx.totalTicks() && n.pitch >= 0 && n.pitch < 128 && n.vel >= 1 && n.vel < 128, "Note ungueltig"); }
        }
        for (int sg = 0; sg < 4; ++sg) for (int b = 0; b < 5; ++b)
        { p.setParam("sig", (float) sg); p.setParam("bars", (float) b); p.makeTrack(true); CHECK(! p.current()->proj.melody.empty(), "sig=%d bars=%d", sg, b); }
        p.setParam("sig", 0.f); p.setParam("bars", 2.f); p.setParam("genre", 0.f); p.setParam("mood", 4.f); p.setParam("t_acid", 1.f); p.setParam("key", 9.f); p.setParam("scale", 1.f); p.makeTrack(false);
    }
    // --- MIDI-Export Round Trip
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mel_test_" + juce::String(juce::Time::currentTimeMillis()));
        dir.createDirectory(); juce::String err;
        p.setParam("bpm", 128.f);
        auto s = p.current();
        const int n = p.exportSeparate(dir, "Idea", &err);
        int expected = 0; for (int q = 0; q < 5; ++q) if (! s->proj.part(q).empty()) ++expected;
        CHECK(n == expected && n >= 4, "Dateien %d erwartet %d (%s)", n, expected, err.toRawUTF8());
        for (int q = 0; q < 5; ++q)
        {
            auto f = dir.getChildFile(juce::String("Idea_") + kPartNames[q] + ".mid"); if (s->proj.part(q).empty()) { CHECK(! f.existsAsFile(), "unerwartete Datei"); continue; }
            juce::FileInputStream in(f); juce::MidiFile mf; CHECK(in.openedOk() && mf.readFrom(in), "lesen %s", f.getFileName().toRawUTF8());
            CHECK(mf.getTimeFormat() == 480, "ppq %d", mf.getTimeFormat());
            int ons = 0, offs = 0; bool tempo = false, sig = false; double lastOff = 0;
            std::vector<std::array<int, 4>> got;
            for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* e : *mf.getTrack(t))
            {
                auto& m = e->message;
                if (m.isTempoMetaEvent()) { tempo = true; CHECK(std::abs(60.0 / m.getTempoSecondsPerQuarterNote() - 128.0) < 0.01, "tempo"); }
                if (m.isTimeSignatureMetaEvent()) { int nn, dd; m.getTimeSignatureInfo(nn, dd); sig = true; CHECK(nn == s->proj.ctx.sigNum && dd == s->proj.ctx.sigDen, "sig"); }
                if (m.isNoteOn()) { ++ons; got.push_back({ (int) e->message.getTimeStamp(), m.getNoteNumber(), m.getVelocity(), 0 }); }
                if (m.isNoteOff()) { ++offs; lastOff = std::max(lastOff, m.getTimeStamp()); }
            }
            CHECK(tempo && sig, "meta fehlt");
            CHECK(ons == (int) s->proj.part(q).size() && offs == ons, "Notenzahl %d/%d vs %d", ons, offs, (int) s->proj.part(q).size());
            CHECK(lastOff <= s->proj.ctx.totalTicks(), "Note ragt ueber Loop");
            std::sort(got.begin(), got.end());
            std::vector<std::array<int, 4>> want; for (auto& nt : s->proj.part(q)) want.push_back({ nt.tick, nt.pitch, nt.vel, 0 });
            std::sort(want.begin(), want.end()); CHECK(got == want, "Noteninhalt weicht ab (%s)", kPartNames[q]);
        }
        auto cf = dir.getChildFile("all.mid"); CHECK(p.exportCombined(cf, &err), "combined: %s", err.toRawUTF8());
        { juce::FileInputStream in(cf); juce::MidiFile mf; CHECK(in.openedOk() && mf.readFrom(in), "combined lesen"); CHECK(mf.getNumTracks() >= expected, "Spuren %d", mf.getNumTracks());
          int ons = 0; for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* e : *mf.getTrack(t)) if (e->message.isNoteOn()) ++ons;
          int tot = 0; for (int q = 0; q < 5; ++q) tot += (int) s->proj.part(q).size(); CHECK(ons == tot, "combined Noten %d vs %d", ons, tot); }
        CHECK(p.exportPart(0, dir.getChildFile("c.mid"), &err), "exportPart");
        CHECK(! p.exportPart(0, dir.getChildFile("c.mid").getChildFile("x.mid"), &err) && err.isNotEmpty(), "Fehler wird nicht gemeldet");
        dir.deleteRecursively();
    }
    // --- State Round Trip
    {
        p.setParam("bpm", 97.f); p.setParam("m_dens", 0.83f);
        juce::MemoryBlock mb; p.getStateInformation(mb);
        MelodyProcessor q; prep(q); q.setStateInformation(mb.getData(), (int) mb.getSize());
        CHECK(std::abs(q.getRaw("bpm") - 97.f) < 0.01f && std::abs(q.getRaw("m_dens") - 0.83f) < 0.01f, "Parameter nicht wiederhergestellt");
        auto a = p.current(), b = q.current();
        for (int i = 0; i < 5; ++i) CHECK(same(a->proj.part(i), b->proj.part(i)), "Teil %d nach State abweichend", i);
        CHECK(a->proj.prog.size() == b->proj.prog.size(), "Progression");
        CHECK(a->proj.ctx.key == b->proj.ctx.key && a->proj.ctx.bars == b->proj.ctx.bars, "ctx");
        // Unveraendert nach Laden darf nichts als stale gelten
        CHECK(! q.isStale(), "stale nach Laden");
        // beschaedigter / leerer / fremder State
        const char junk[] = "<?xml garbage"; q.setStateInformation(junk, (int) sizeof(junk)); q.setStateInformation(nullptr, 0);
        juce::MemoryBlock half(mb.getData(), mb.getSize() / 2); q.setStateInformation(half.getData(), (int) half.getSize());
        CHECK(q.current() != nullptr, "current nach kaputtem State");
        // Manipulierte Noten (ausserhalb Bereich) werden verworfen
        auto xml = nwa::state::toXml(p.apvts, makePluginInfo().id, makePluginInfo().stateVersion, p.makeExtra());
        if (auto* ex = xml->getChildByName("EXTRA")) if (auto* mel = ex->getChildByName("MEL")) if (auto* part = mel->getChildByName("PART")) part->setAttribute("n", "-5,10,300,5,0;99999999,10,60,60,0;10,0,60,60,0;480,240,64,90,0");
        juce::MemoryBlock bad; juce::AudioProcessor::copyXmlToBinary(*xml, bad);
        MelodyProcessor w; prep(w); w.setStateInformation(bad.getData(), (int) bad.getSize());
        int cnt = 0; for (auto& n : w.current()->proj.chords) { ++cnt; CHECK(n.pitch < 128 && n.len >= 1, "ungueltige Note uebernommen"); }
        CHECK(cnt <= 1, "ungueltige Noten nicht verworfen (%d)", cnt);
    }
    // --- Audio
    {
        MelodyProcessor a; prep(a);
        a.setParam("sync", 0.f); a.setParam("play", 0.f);
        auto idle = run(a, 44100); CHECK(peak(idle) < 1e-6f, "Stille erwartet, wenn Play aus");
        a.setParam("play", 1.f); a.setParam("bpm", 120.f);
        auto o = run(a, 44100 * 5);   // 2.5 s Loop => mehr als 1 Durchlauf
        CHECK(finite(o), "NaN/Inf"); CHECK(rms(o) > 0.005, "zu leise %.5f", rms(o)); CHECK(peak(o) <= 1.0f, "Peak %.3f", peak(o));
        // Mute aller Teile -> (fast) Stille
        for (int i = 0; i < 5; ++i) a.setParam("mute_" + juce::String(i), 1.f);
        run(a, 44100 * 3);         // Ausklingen (Pad-Release)
        auto m = run(a, 22050); CHECK(peak(m) < 1e-4f, "Mute wirkt nicht: %.5f", peak(m));
        for (int i = 0; i < 5; ++i) a.setParam("mute_" + juce::String(i), 0.f);
        a.setParam("play", 0.f); auto t = run(a, 44100); CHECK(peak(t) < 1e-3f, "Nachklingen nach Stop endet nicht: %.5f", peak(t));
        CHECK(a.playTicks.load() < 0, "playTicks nach Stop");
        // Blockgroessen 1, 7, 64, 4096 liefern gleiches Ergebnis
        auto render = [&](int blk) { MelodyProcessor r; prep(r, 44100.0, 4096); r.setParam("sync", 0.f); r.setParam("play", 1.f); return run(r, 44100, blk); };
        auto r0 = render(512); for (int blk : { 1, 7, 64, 4096 }) { auto r1 = render(blk); double d = 0; for (size_t i = 0; i < r0.l.size(); ++i) { const double e = r0.l[i] - r1.l[i]; d += e * e; } d = std::sqrt(d / (double) r0.l.size()) / rms(r0); if (d >= 0.05) { size_t k = 0; while (k < r0.l.size() && std::abs(r0.l[k] - r1.l[k]) < 1e-7f) ++k; std::printf("first diff at sample %zu: %g vs %g\n", k, r0.l[k], r1.l[k]); } CHECK(d < 0.001, "Blockgroesse %d weicht ab (rel. Fehler %.5f; Einsatz auf Sample gerundet)", blk, d); }
        // Sample-Raten und Mono
        for (double sr : { 22050.0, 48000.0, 96000.0 }) { MelodyProcessor r; prep(r, sr, 256); r.setParam("sync", 0.f); r.setParam("play", 1.f); Out x; { juce::AudioBuffer<float> b(2, 256); for (int i = 0; i < 40; ++i) { juce::MidiBuffer mm; r.processBlock(b, mm); } } (void) x; }
        { MelodyProcessor r; r.setPlayConfigDetails(0, 1, 44100.0, 256); r.prepareToPlay(44100.0, 256); r.setParam("sync", 0.f); r.setParam("play", 1.f); juce::AudioBuffer<float> b(1, 256); float pk = 0; for (int i = 0; i < 100; ++i) { juce::MidiBuffer mm; r.processBlock(b, mm); pk = std::max(pk, b.getMagnitude(0, 256)); } CHECK(pk > 0.001f && pk <= 1.f, "Mono"); }
        // Regenerieren waehrend der Wiedergabe (Audio-Thread liest alten Snapshot weiter) + Fuzz
        MelodyProcessor f; prep(f); f.setParam("sync", 0.f); f.setParam("play", 1.f);
        juce::Random rnd(7); juce::AudioBuffer<float> b(2, 333);
        for (int it = 0; it < 300; ++it)
        {
            for (auto* prm : f.getParameters()) if (rnd.nextInt(6) == 0 && prm->getName(64) != "Play" && prm->getName(64) != "Host sync") prm->setValueNotifyingHost(rnd.nextFloat());
            if (it % 3 == 0) f.regenerate(rnd.nextInt(6) - 1, rnd.nextBool());
            if (it % 17 == 0) f.makeTrack(true);
            if (it % 11 == 0) f.undo();
            if (it % 13 == 0) f.variation(1 + rnd.nextInt(4), rnd.nextFloat());
            juce::MidiBuffer mm; f.processBlock(b, mm);
            bool ok = true; for (int c = 0; c < 2; ++c) for (int i = 0; i < 333; ++i) { const float v = b.getSample(c, i); if (! std::isfinite(v) || std::abs(v) > 1.0001f) ok = false; }
            CHECK(ok, "Fuzz it=%d Ausgabe ungueltig", it); if (! ok) break;
        }
    }
    std::printf(failures ? "\n%d FEHLER\n" : "\nALLE OK\n", failures);
    return failures ? 1 : 0;
}

int main()
{
    struct Runner : juce::Thread { int rc = 1; Runner() : juce::Thread("test", 64u * 1024u * 1024u) {} void run() override { rc = realMain(); } } r;
    r.startThread(); r.waitForThreadToExit(-1); return r.rc;
}
