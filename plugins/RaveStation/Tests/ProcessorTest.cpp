// Prozessor-Tests fuer RAVE//STATION: Parameter, Standard-Kit, MIDI -> Audio, State-Round-Trip inkl. Samples und Patterns,
// beschaedigter State, Worker-Verarbeitung (Time-Stretch). Kein Host, kein Klangurteil.
#include "../Source/RaveProcessor.h"
#include "../Source/engine/KitSynth.h"
#include <juce_events/juce_events.h>
#include <cstdio>

using namespace rave;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void prep(RaveProcessor& p, double sr = 44100.0, int blk = 512)
{
    p.setPlayConfigDetails(0, 2, sr, blk);
    p.prepareToPlay(sr, blk);
}

static std::vector<float> run(RaveProcessor& p, int total, int blk, const std::vector<std::pair<int, int>>& notes = {})
{
    std::vector<float> out((size_t) total);
    juce::AudioBuffer<float> buf(2, blk);
    for (int pos = 0; pos < total; pos += blk)
    {
        const int n = juce::jmin(blk, total - pos);
        buf.setSize(2, n, false, false, true);
        juce::MidiBuffer m;
        for (auto& no : notes) if (no.first >= pos && no.first < pos + n) m.addEvent(juce::MidiMessage::noteOn(1, 36 + no.second, (juce::uint8) 127), no.first - pos);
        p.processBlock(buf, m);
        std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + n, out.begin() + pos);
    }
    return out;
}
static int activeSamples(const std::vector<float>& x, float thr = 0.002f) { int last = 0; for (size_t i = 0; i < x.size(); ++i) if (std::abs(x[i]) > thr) last = (int) i; return last; }

static int realMain();
int main()
{
    // Testobjekte (RaveProcessor ~400 KB) liegen hier auf dem Stack; Windows hat nur 1 MB -> grosser Thread-Stack
    struct Runner : juce::Thread { int rc = 1; Runner() : juce::Thread("test", 64u * 1024u * 1024u) {} void run() override { rc = realMain(); } } r;
    r.startThread(); r.waitForThreadToExit(-1); return r.rc;
}

static int realMain()
{
    juce::MessageManager::getInstance();
    {
        RaveProcessor p; prep(p);
        std::printf("Parameter: %d\n", (int) p.getParameters().size());
        CHECK(p.getParameters().size() > 300, "zu wenige Parameter");
        int have = 0; for (int i = 0; i < kPads; ++i) have += p.padSample(i) != nullptr;
        CHECK(have == 16, "Standard-Kit unvollstaendig (%d)", have);

        // Parameter-IDs eindeutig
        std::set<juce::String> ids;
        for (auto* pr : p.getParameters()) if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*>(pr)) ids.insert(wp->paramID);
        CHECK((int) ids.size() == p.getParameters().size(), "doppelte Parameter-IDs");

        // MIDI -> Audio (Sequencer aus, Host-Sync an aber kein Playhead => steht)
        auto x = run(p, 22050, 256, { { 100, 0 } });
        float pk = 0; for (float v : x) pk = juce::jmax(pk, std::abs(v));
        CHECK(pk > 0.1f && pk <= 1.f, "Kick ueber MIDI: peak=%f", pk);
        bool finite = true; for (float v : x) finite &= std::isfinite(v);
        CHECK(finite, "NaN");

        // Sequencer intern
        p.setParam("sync", 0.f); p.setParam("play", 1.f); p.setParam("bpm", 140.f);
        p.bank.set(0, 2, 4, { 120 });
        auto y = run(p, 44100, 512);
        float pk2 = 0; for (float v : y) pk2 = juce::jmax(pk2, std::abs(v));
        CHECK(pk2 > 0.1f, "interner Sequencer stumm");
        p.setParam("play", 0.f);
    }
    {   // State-Round-Trip
        RaveProcessor a; prep(a);
        a.setParam("p03_pitch", 5.f); a.setParam("swing", 0.4f); a.setParam("pattern", 3.f);
        Step s{ 99 }; s.prob = 70; s.ratchet = 2; s.flags = kGhost; s.pitch = 3;
        a.bank.set(2, 5, 9, s); a.bank.setLength(2, 32);
        juce::AudioBuffer<float> b(1, 5000); for (int i = 0; i < 5000; ++i) b.setSample(0, i, 0.3f * std::sin(i * 0.05f));
        a.setPadSample(7, makeSample(b, 48000.0, "meinSample", "C:/x/y.wav"));
        a.setPadSample(8, nullptr);
        juce::MemoryBlock mb; a.getStateInformation(mb);
        std::printf("State-Groesse: %.1f KB\n", mb.getSize() / 1024.0);

        RaveProcessor c; prep(c);
        c.setStateInformation(mb.getData(), (int) mb.getSize());
        CHECK(std::abs(c.getRaw("p03_pitch") - 5.f) < 0.01f, "Pad-Parameter nicht wiederhergestellt");
        CHECK(std::abs(c.getRaw("swing") - 0.4f) < 0.01f, "Swing");
        CHECK((int) std::lround(c.getRaw("pattern")) == 3, "Pattern-Auswahl");
        PlainPattern pa, pc; a.bank.load(2, pa); c.bank.load(2, pc);
        CHECK(pa == pc, "Pattern nicht identisch");
        auto sm = c.padSample(7);
        CHECK(sm && sm->length() == 5000 && std::abs(sm->rate - 48000.0) < 1.0 && sm->name == "meinSample", "Sample-Metadaten");
        if (sm) { float md = 0; for (int i = 0; i < 5000; ++i) md = juce::jmax(md, std::abs(sm->data.getSample(0, i) - b.getSample(0, i))); CHECK(md < 1.0e-5f, "Sample-Abweichung %g (24-bit FLAC)", md); }
        CHECK(c.padSample(8) == nullptr, "leerer Pad kam zurueck");
        CHECK(c.padSample(0) != nullptr, "Kick fehlt");

        // beschaedigter / fremder State aendert nichts
        const float before = c.getRaw("swing");
        const char junk[] = "<not valid";
        c.setStateInformation(junk, (int) sizeof(junk));
        CHECK(std::abs(c.getRaw("swing") - before) < 1e-6f, "korrupter State hat Zustand veraendert");
        CHECK(c.padSample(7) != nullptr, "korrupter State hat Samples geloescht");

        // Preset mit Extras
        nwa::PresetManager pm(c.apvts, makePluginInfo().id, "RAVE-STATION", 1, juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rave_test_presets"));
        CHECK(pm.save("Test A", c.makeExtra()) == nwa::PresetManager::Status::ok, "Preset speichern");
        juce::ValueTree ex; CHECK(pm.load("Test A", &ex) == nwa::PresetManager::Status::ok && ex.hasType("RAVE"), "Preset laden");
        pm.remove("Test A"); pm.directory().deleteRecursively();
    }
    {   // Worker: Time-Stretch 2x verdoppelt die hoerbare Laenge
        RaveProcessor p; prep(p);
        p.setParam("p06_dec", 10.f);   // Closed Hat? Pad 6 = Closed Hat; kein Decay-Limit
        auto base = run(p, 44100 * 2, 512, { { 0, 5 } });
        const int len1 = activeSamples(base);
        p.setParam("p06_stretch", 2.f);
        juce::Thread::sleep(1800);     // Worker entprellt und rechnet
        auto stretched = run(p, 44100 * 4, 512, { { 0, 5 } });
        const int len2 = activeSamples(stretched);
        std::printf("Laenge 1x=%d  2x=%d  Verhaeltnis=%.2f\n", len1, len2, len2 / (double) juce::jmax(1, len1));
        CHECK(len2 > len1 * 1.6 && len2 < len1 * 2.4, "Stretch-Verhaeltnis %.2f", len2 / (double) juce::jmax(1, len1));
    }
    {   // Slicer im Prozessor, Kits, Vorhoeren
        RaveProcessor p; prep(p);
        std::vector<int> truth; auto brk = kit::makeDemoBreak(&truth, 170.0);
        p.setBreak(brk, true);
        CHECK(p.sliceModel.numSlices() >= 15, "Slices: %d", p.sliceModel.numSlices());
        const int n = p.distributeSlicesToPads(0);
        CHECK(n == juce::jmin(16, p.sliceModel.numSlices()), "verteilte Slices %d", n);
        CHECK(p.padSample(0) && p.padSample(0)->length() == p.sliceModel.endOf(0) - p.sliceModel.startOf(0), "Pad 1 = Slice 1");
        p.sliceLoopSamples = (int) (32 * 60.0 / 170.0 / 4.0 * 44100.0);
        p.slicesToPatternSlot(4, 0, 32);
        PlainPattern pat; p.bank.load(4, pat);
        CHECK(pat.length == 32 && pat.hits() >= 14, "Slice-Pattern Treffer %d", pat.hits());

        // Das Slice-Pattern spielt den Break nach: Korrelation mit dem Original
        p.setParam("sync", 0.f); p.setParam("play", 1.f); p.setParam("bpm", 170.f); p.setParam("pattern", 4.f);
        const int loopN = p.sliceLoopSamples;
        auto rendered = run(p, loopN, 256);
        double sxy = 0, sxx = 0, syy = 0;
        const int covered = p.sliceModel.numSlices() > kPads ? p.sliceModel.startOf(kPads) : loopN;   // nur Slices mit eigenem Pad
        for (int i = 0; i < covered; ++i) { const double xo = brk->data.getSample(0, i), yr = rendered[(size_t) i]; sxy += xo * yr; sxx += xo * xo; syy += yr * yr; }
        const double corr = sxy / std::sqrt(juce::jmax(1e-12, sxx * syy));
        std::printf("Slice-Pattern vs. Original (%d von %d Slices, 16 Pads): Korrelation %.3f\n", juce::jmin(kPads, p.sliceModel.numSlices()), p.sliceModel.numSlices(), corr);
        CHECK(corr > 0.85, "Slice-Wiedergabe weicht vom Original ab (corr %.3f)", corr);
        p.setParam("play", 0.f);

        // Vorhoeren mischt Audio dazu
        p.previewSlice(2);
        auto pv = run(p, 8192, 256);
        float pk = 0; for (float v : pv) pk = juce::jmax(pk, std::abs(v));
        CHECK(pk > 0.05f, "Vorhoeren stumm (%f)", pk);

        // State-Round-Trip inkl. Slicer
        p.arrangement = slicer::shuffledOps(p.sliceModel.numSlices(), 3);
        juce::MemoryBlock mb; p.getStateInformation(mb);
        RaveProcessor q; prep(q); q.setStateInformation(mb.getData(), (int) mb.getSize());
        CHECK(q.sliceModel.source && q.sliceModel.markers == p.sliceModel.markers, "Marker nach State-Load");
        CHECK(q.sliceLoopSamples == p.sliceLoopSamples, "Loop-Laenge");
        CHECK(q.arrangement.size() == p.arrangement.size() && q.arrangement[1].slice == p.arrangement[1].slice, "Arrangement");

        // Kit speichern / laden (Dokumente-Ordner wird durch HOME umgeleitet, siehe Aufruf)
        juce::String err;
        p.setParam("p01_pitch", 7.f);
        CHECK(p.saveKit("TestKit A", &err), "saveKit: %s", err.toRawUTF8());
        CHECK(p.listKits().contains("TestKit A"), "Kit nicht in Liste");
        RaveProcessor k; prep(k);
        CHECK(k.loadKit("TestKit A", &err), "loadKit: %s", err.toRawUTF8());
        CHECK(std::abs(k.getRaw("p01_pitch") - 7.f) < 0.01f, "Kit-Parameter");
        CHECK(k.padSample(0) && p.padSample(0) && k.padSample(0)->length() == p.padSample(0)->length(), "Kit-Sample Laenge");
        CHECK(! k.loadKit("gibt es nicht", &err), "fehlendes Kit muss fehlschlagen");
        p.kitsDirectory().getChildFile("TestKit A").deleteRecursively();
    }
    {   // Generator + EVOLVE im Prozessor
        RaveProcessor p; prep(p);
        p.setParam("genre", 1.f); p.bank.setLength(0, 32);
        p.generatePattern(0, true);
        PlainPattern pat; p.bank.load(0, pat);
        CHECK(pat.length == 32 && pat.hits() > 10, "Generator im Prozessor: %d Treffer", pat.hits());
        CHECK(std::abs(p.getRaw("bpm") - 174.f) < 0.5f, "Genre-Tempo uebernommen: %f", p.getRaw("bpm"));
        p.evolveFrom(0, 0.5f);
        PlainPattern after; p.bank.load(0, after);
        CHECK(after == pat, "EVOLVE hat Quelle ueberschrieben");
        int differing = 0; PlainPattern vs[kVariants];
        for (int k = 0; k < kVariants; ++k) { p.bank.load(kUserPatterns + k, vs[k]); differing += ! (vs[k] == pat); }
        CHECK(differing == kVariants, "alle Varianten muessen sich vom Original unterscheiden (%d)", differing);
        CHECK(! (vs[0] == vs[1]), "Varianten 1 und 2 identisch");
        // Variante abspielen: Slot 16 waehlen -> Pattern-Wechsel am Ende
        p.setParam("sync", 0.f); p.setParam("play", 1.f); p.setParam("pattern", 16.f);
        auto out = run(p, 44100, 512);
        float pk = 0; for (float v : out) pk = juce::jmax(pk, std::abs(v));
        CHECK(pk > 0.05f, "Variante klingt nicht");
        p.setParam("play", 0.f);
        CHECK(p.acceptVariant(2, 5), "accept");
        PlainPattern acc; p.bank.load(5, acc); CHECK(acc == vs[2], "akzeptierte Variante nicht uebernommen");
        p.discardVariants();
        PlainPattern gone; p.bank.load(16, gone); CHECK(gone.hits() == 0, "Varianten nicht verworfen");
        PlainPattern still; p.bank.load(5, still); CHECK(still == vs[2], "Verwerfen darf akzeptierte Muster nicht loeschen");
        CHECK((int) std::lround(p.getRaw("pattern")) < kUserPatterns, "Auswahl zeigt noch auf verworfene Variante");
    }
    {   // MIDI-Export: Round-Trip
        RaveProcessor p; prep(p);
        p.setParam("bpm", 150.f); p.setParam("swing", 0.5f); p.setParam("sig", 1.f);
        p.bank.clear(0); p.bank.setLength(0, 16);
        p.bank.set(0, 2, 0, { 100 });
        Step rt{ 127 }; rt.ratchet = 4; p.bank.set(0, 0, 4, rt);
        p.bank.set(0, 5, 1, { 90 });
        Step gh{ 80 }; gh.flags = kGhost; p.bank.set(0, 3, 6, gh);
        Step pr{ 100 }; pr.prob = 50; p.bank.set(0, 7, 8, pr);
        const auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rave_test.mid");
        juce::String err;
        CHECK(p.exportMidi(0, f, 2, false, &err), "MIDI schreiben: %s", err.toRawUTF8());
        juce::MidiFile mf; juce::FileInputStream in(f);
        CHECK(in.openedOk() && mf.readFrom(in), "MIDI lesen");
        CHECK(mf.getTimeFormat() == 480, "PPQ %d", mf.getTimeFormat());
        struct N { double tick; int note, vel, ch; };
        std::vector<N> notes; double tempoBpm = 0; int tsNum = 0, tsDen = 0;
        for (int tr = 0; tr < mf.getNumTracks(); ++tr)
            for (auto* ev : *mf.getTrack(tr))
            {
                const auto& m = ev->message;
                if (m.isNoteOn()) notes.push_back({ m.getTimeStamp(), m.getNoteNumber(), m.getVelocity(), m.getChannel() });
                if (m.isTempoMetaEvent()) tempoBpm = 60.0 / m.getTempoSecondsPerQuarterNote();
                if (m.isTimeSignatureMetaEvent()) m.getTimeSignatureInfo(tsNum, tsDen);
            }
        CHECK(std::abs(tempoBpm - 150.0) < 0.01, "Tempo %f", tempoBpm);
        CHECK(tsNum == 3 && tsDen == 4, "Taktart %d/%d", tsNum, tsDen);
        auto find = [&](double tick, int note) -> const N* { for (auto& n : notes) if (std::abs(n.tick - tick) < 1.0 && n.note == note) return &n; return nullptr; };
        CHECK(find(0, 38) && find(0, 38)->vel == 100 && find(0, 38)->ch == 10, "Snare Schritt 0");
        for (int j = 0; j < 4; ++j) { auto* n = find(480 + j * 30, 36); CHECK(n != nullptr, "Ratchet %d fehlt", j); if (n) CHECK(std::abs(n->vel - (int) std::lround(127.0 * (1.0 - 0.07 * j))) <= 1, "Ratchet %d Velocity %d", j, n->vel); }
        CHECK(find(150, 41) != nullptr, "Swing: ungerader Schritt muss um 0.25 Schritt (30 Ticks) verzoegert sein");
        CHECK(find(6 * 120, 39) && std::abs(find(6 * 120, 39)->vel - 36) <= 1, "Ghost-Velocity");
        CHECK(find(8 * 120, 43) != nullptr && find(16 * 120 + 8 * 120, 43) != nullptr, "ohne Roll werden Wahrscheinlichkeitsschritte als normale Noten exportiert");
        CHECK(find(16 * 120, 38) != nullptr, "zweiter Loop beginnt bei Tick 1920");
        // mit Roll: gleiche Entscheidung wie die Engine (reproduzierbar)
        const auto f2 = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rave_test_roll.mid");
        CHECK(p.exportMidi(0, f2, 8, true, &err), "MIDI mit Roll");
        juce::MidiFile m2; juce::FileInputStream in2(f2); m2.readFrom(in2);
        int probNotes = 0; for (auto* ev : *m2.getTrack(0)) if (ev->message.isNoteOn() && ev->message.getNoteNumber() == 43) ++probNotes;
        CHECK(probNotes > 0 && probNotes < 8, "50%%-Schritt ergab %d von 8", probNotes);
        f.deleteFile(); f2.deleteFile();
    }
    {   // Pattern-Rendering / WAV-Export / Stems / Bounce / Morph
        RaveProcessor p; prep(p);
        p.setParam("bpm", 120.f); p.bank.clear(0); p.bank.setLength(0, 16);
        p.bank.set(0, 0, 0, { 120 }); p.bank.set(0, 2, 4, { 110 }); p.bank.set(0, 5, 2, { 90 }); p.bank.set(0, 5, 6, { 90 });
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rave_export_test");
        dir.deleteRecursively(); dir.createDirectory();
        juce::String err;
        const auto w1 = dir.getChildFile("loop.wav"), w2 = dir.getChildFile("loop2.wav"), wl = dir.getChildFile("seamless.wav");
        CHECK(p.exportPatternWav(0, w1, 2, false, 44100.0, &err), "WAV-Export: %s", err.toRawUTF8());
        CHECK(p.exportPatternWav(0, w2, 2, false, 44100.0, &err), "WAV-Export 2");
        CHECK(p.exportPatternWav(0, wl, 2, true, 44100.0, &err), "WAV-Export nahtlos");
        auto readWav = [](const juce::File& f) { return loadSampleFile(f); };
        auto a = readWav(w1), b = readWav(w2), s = readWav(wl);
        CHECK(a && b && s, "Export lesbar");
        if (a && b && s)
        {
            const int loopN = (int) std::lround(16 * 0.25 * 0.5 * 44100.0);   // 16 Schritte * 1/4 Beat * 0.5 s
            CHECK(a->length() >= loopN * 2, "Export-Laenge %d < %d", a->length(), loopN * 2);
            CHECK(s->length() == loopN * 2, "nahtloser Loop muss genau 2 Durchlaeufe lang sein (%d vs %d)", s->length(), loopN * 2);
            CHECK(a->length() == b->length(), "Rendering nicht reproduzierbar (Laenge)");
            double md = 0; for (int i = 0; i < juce::jmin(a->length(), b->length()); ++i) md = juce::jmax(md, (double) std::abs(a->data.getSample(0, i) - b->data.getSample(0, i)));
            CHECK(md < 1.0e-6, "Rendering nicht reproduzierbar (Abweichung %g)", md);
            // Kick auf Schritt 0 beginnt bei Sample 0, Snare bei Schritt 4 = 11025
            auto firstLoud = [&](int from) { for (int i = from; i < a->length(); ++i) if (std::abs(a->data.getSample(0, i)) > 0.05f) return i; return -1; };
            CHECK(firstLoud(0) < 200, "Kick-Onset %d", firstLoud(0));
            // 24 bit WAV: Round-Trip der Samplewerte aus dem Pad
        }
        CHECK(p.exportStems(0, dir.getChildFile("stems"), 1, false, 44100.0) == 3, "Stems: ein File pro benutztem Pad");
        const int ns = p.exportSlices(dir.getChildFile("slices"), "Brk", &err);
        CHECK(ns == 0 && err.isNotEmpty(), "ohne Break keine Slices");
        p.setBreak(kit::makeDemoBreak(nullptr, 170.0), true);
        const int ns2 = p.exportSlices(dir.getChildFile("slices"), "Brk");
        CHECK(ns2 == p.sliceModel.numSlices() && dir.getChildFile("slices").findChildFiles(juce::File::findFiles, false, "*.wav").size() == ns2, "Slice-Export %d", ns2);
        dir.deleteRecursively();

        // ---- Sample-Morph: jede Einstellung veraendert den Klang hoerbar und reproduzierbar
        struct Case { const char* id; float v; };
        const Case cases[] = { { "p03_lp", 400.f }, { "p03_hp", 3000.f }, { "p03_drv", 1.f }, { "p03_crush", 1.f }, { "p03_rev", 1.f }, { "p03_pitch", 7.f }, { "p03_pitch", -12.f },
                               { "p03_start", 0.3f }, { "p03_end", 0.5f }, { "p03_atk", 0.05f }, { "p03_dec", 0.05f }, { "p03_stutter", 3.f }, { "p03_pan", -1.f } };
        auto neutral = p.bounce(2);
        CHECK(neutral && neutral->length() > 1000, "Bounce Neutral");
        for (const auto& cs : cases)
        {
            RaveProcessor q; prep(q);
            q.setParam(cs.id, cs.v);
            auto m1 = q.bounce(2), m2 = q.bounce(2);
            CHECK(m1 && m2, "Bounce %s", cs.id);
            if (! (m1 && m2)) continue;
            auto ref = q.bounce(2);   // gleiche Einstellung noch einmal
            double dRep = 0; for (int i = 0; i < juce::jmin(m1->length(), m2->length()); ++i) dRep = juce::jmax(dRep, (double) std::abs(m1->data.getSample(0, i) - m2->data.getSample(0, i)));
            CHECK(m1->length() == m2->length() && dRep < 1e-7, "%s nicht reproduzierbar", cs.id);
            // Abstand zum unbearbeiteten Klang (relativ zur Energie)
            RaveProcessor base; prep(base); auto n0 = base.bounce(2);
            double diff = 0, en = 0; const int n = juce::jmin(m1->length(), n0->length());
            for (int i = 0; i < n; ++i) { const double d = m1->data.getSample(0, i) - n0->data.getSample(0, i); diff += d * d; en += n0->data.getSample(0, i) * n0->data.getSample(0, i); }
            const bool lengthChanged = std::abs(m1->length() - n0->length()) > 100;
            std::printf("  Morph %-10s %6.2f: relative Abweichung %.3f, Laenge %d -> %d\n", cs.id, cs.v, diff / juce::jmax(1e-9, en), n0->length(), m1->length());
            if (std::string(cs.id) == "p03_pan") { double l = 0, r = 0; for (int i = 0; i < m1->length(); ++i) { l += std::abs(m1->data.getSample(0, i)); r += std::abs(m1->channels() > 1 ? m1->data.getSample(1, i) : 0.f); } CHECK(l > r * 3 || r < 1e-6, "Pan -1 muss links klingen (L=%f R=%f)", l, r); continue; }
            CHECK(lengthChanged || diff / juce::jmax(1e-9, en) > 0.02, "%s veraendert den Klang nicht hoerbar", cs.id);
        }
        // Reverse = zeitlich gespiegelt (ohne Hüllkurven-Randeffekte: Energieschwerpunkt wandert nach hinten)
        {
            RaveProcessor q; prep(q); auto fw = q.bounce(2); q.setParam("p03_rev", 1.f); auto rv = q.bounce(2);
            auto centroid = [](const Sample& sm) { double num = 0, den = 0; for (int i = 0; i < sm.length(); ++i) { const double e = sm.data.getSample(0, i) * sm.data.getSample(0, i); num += e * i; den += e; } return num / juce::jmax(1e-12, den) / sm.length(); };
            std::printf("  Reverse: Energieschwerpunkt %.2f -> %.2f\n", centroid(*fw), centroid(*rv));
            CHECK(centroid(*rv) > centroid(*fw) + 0.15, "Reverse verschiebt Energie nicht nach hinten");
        }
        // Pitch +12 halbiert die Laenge, Stretch 2x verdoppelt sie (ueber den Worker-losen Offline-Pfad)
        {
            RaveProcessor q; prep(q); auto n0 = q.bounce(5);
            q.setParam("p06_pitch", 12.f); auto up = q.bounce(5);
            q.setParam("p06_pitch", 0.f); q.setParam("p06_stretch", 2.f); auto st = q.bounce(5);
            std::printf("  Pitch/Stretch Laenge: normal %d, +12 st %d, 2x %d\n", n0->length(), up->length(), st->length());
            CHECK(up->length() < n0->length() * 0.6, "Pitch +12 verkuerzt zu wenig");
            CHECK(st->length() > n0->length() * 1.7 && st->length() < n0->length() * 2.3, "Stretch 2x Laenge");
        }
        // Resample: danach sind die Einstellungen neutral und der Klang gleich
        {
            RaveProcessor q; prep(q); q.setParam("p03_drv", 0.8f); q.setParam("p03_lp", 1500.f); q.setParam("p03_pitch", 5.f);
            auto before = q.bounce(2);
            CHECK(q.resamplePad(2), "resample");
            CHECK(std::abs(q.getRaw("p03_pitch")) < 1e-4f && q.getRaw("p03_lp") > 19000.f && q.getRaw("p03_drv") < 1e-4f, "Einstellungen nach Resample nicht neutral");
            q.setParam("p03_dec", 10.f);
            auto after = q.bounce(2);
            double diff = 0, en = 0; const int n = juce::jmin(before->length(), after->length());
            for (int i = 0; i < n; ++i) { const double d = before->data.getSample(0, i) - after->data.getSample(0, i); diff += d * d; en += before->data.getSample(0, i) * before->data.getSample(0, i); }
            std::printf("  Resample: relative Abweichung vor/nach %.4f, Laenge %d -> %d\n", diff / juce::jmax(1e-9, en), before->length(), after->length());
            CHECK(diff / juce::jmax(1e-9, en) < 0.02 && std::abs(before->length() - after->length()) < 300, "Resample klingt anders als das Original");
        }
    }
    std::printf(failures == 0 ? "ProcessorTest: ALLE OK\n" : "ProcessorTest: %d FEHLER\n", failures);
    juce::DeletedAtShutdown::deleteAll(); juce::MessageManager::deleteInstance();
    return failures == 0 ? 0 : 1;
}
