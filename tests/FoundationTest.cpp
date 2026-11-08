// Automatische Tests des technischen Fundaments (Phase 1).
// Prueft alle drei Foundation-Plugins ohne DAW: Layouts, Samplerates, Puffergroessen, Mono/Stereo,
// State (inkl. kaputter Daten), Presets (inkl. boesartiger Namen), NaN-Eingaben, Plugin-Identitaet.
#include <nwa/FoundationProcessor.h>
#include <nwa/FoundationEditor.h>
#include <cstdio>
#include <set>

namespace rave   { namespace { 
#include "../plugins/RaveStation/Source/PluginInfo.h"
} }
namespace melody { namespace {
#include "../plugins/MelodyStudio/Source/PluginInfo.h"
} }
namespace space  { namespace {
#include "../plugins/SpaceFX/Source/PluginInfo.h"
} }

static int g_checks = 0, g_fail = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_MSG(cond, msg) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d  %s  [%s]\n", __FILE__, __LINE__, #cond, juce::String(msg).toRawUTF8()); } } while (0)

using nwa::FoundationProcessor;

static bool allFinite(const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (!std::isfinite(b.getReadPointer(c)[i])) return false;
    return true;
}

static bool setLayout(FoundationProcessor& p, int channels)
{
    auto l = p.getBusesLayout();
    const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    if (l.outputBuses.size() > 0) l.outputBuses.set(0, set);
    if (!p.info.instrument && l.inputBuses.size() > 0) l.inputBuses.set(0, set);
    return p.setBusesLayout(l);
}

static void setGainDb(FoundationProcessor& p, float db)
{
    auto* prm = p.apvts.getParameter("gain");
    prm->setValueNotifyingHost(prm->convertTo0to1(db));
}
static float getGainDb(FoundationProcessor& p) { return p.apvts.getRawParameterValue("gain")->load(); }

// ---------------------------------------------------------------------------------------------
static void testIdentity(const std::vector<nwa::PluginInfo>& infos)
{
    std::printf("[identity]\n");
    std::set<juce::String> ids, names;
    for (auto& i : infos)
    {
        CHECK(i.id.isNotEmpty() && i.name.isNotEmpty());
        CHECK(ids.insert(i.id).second);
        CHECK(names.insert(i.name).second);
        CHECK(i.phase >= 2);
    }
}

static void testAudio(const nwa::PluginInfo& info)
{
    std::printf("[audio] %s\n", info.name.toRawUTF8());
    const double rates[] = { 22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int blocks[]   = { 1, 2, 7, 32, 64, 128, 480, 512, 1024, 4096 };

    for (int channels : { 1, 2 })
        for (double sr : rates)
            for (int bs : blocks)
            {
                FoundationProcessor p(info);
                CHECK_MSG(setLayout(p, channels), "layout");
                p.setRateAndBufferSizeDetails(sr, bs);
                p.prepareToPlay(sr, bs);

                const int nch = juce::jmax(p.getTotalNumInputChannels(), p.getTotalNumOutputChannels());
                juce::AudioBuffer<float> buf(nch, bs);
                juce::MidiBuffer midi;
                const int totalBlocks = juce::jmax(4, (int) (sr * 0.25 / bs));
                double phase = 0.0;
                float lastPeak = 0.f, maxDiff = 0.f; bool finite = true;

                for (int b = 0; b < totalBlocks; ++b)
                {
                    midi.clear();
                    if (info.instrument)
                    {
                        if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8) 100), bs / 2);
                        if (b == totalBlocks - 2) midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
                    }
                    juce::AudioBuffer<float> in(nch, bs);
                    for (int c = 0; c < nch; ++c)
                        for (int i = 0; i < bs; ++i)
                        {
                            const float v = info.instrument ? 0.f : 0.5f * (float) std::sin(phase + 2.0 * juce::MathConstants<double>::pi * 440.0 / sr * i);
                            buf.setSample(c, i, v);
                            in.setSample(c, i, v);
                        }
                    phase += 2.0 * juce::MathConstants<double>::pi * 440.0 / sr * bs;
                    p.processBlock(buf, midi);
                    if (!info.instrument)
                        for (int c = 0; c < nch; ++c)
                            for (int i = 0; i < bs; ++i) maxDiff = juce::jmax(maxDiff, std::abs(buf.getSample(c, i) - in.getSample(c, i)));
                    finite &= allFinite(buf);
                    if (b == totalBlocks - 3) lastPeak = buf.getMagnitude(0, bs);
                }
                CHECK_MSG(finite, "finite");
                if (info.instrument) CHECK_MSG(lastPeak > 0.001f && lastPeak < 1.f, juce::String(lastPeak) + " sr " + juce::String(sr) + " bs " + juce::String(bs));
                else                 CHECK_MSG(maxDiff < 1e-6f, maxDiff);   // 0 dB = bitgenaue Durchleitung
            }

    // Effekt: Gain ist exakt und wirkt tatsaechlich; Instrument: Gain skaliert den Testton
    {
        FoundationProcessor p(info);
        p.setRateAndBufferSizeDetails(48000.0, 512);
        p.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        auto run = [&](float db) {
            setGainDb(p, db);
            float pk = 0.f;
            for (int b = 0; b < 40; ++b)
            {
                midi.clear();
                if (info.instrument && b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i) buf.setSample(c, i, info.instrument ? 0.f : (i % 2 ? 0.5f : -0.5f));
                p.processBlock(buf, midi);
                pk = buf.getMagnitude(0, 512);
            }
            return pk;
        };
        const float a = run(-12.f), c = run(0.f);
        CHECK_MSG(std::abs(c / a - 3.981f) < 0.05f, juce::String(c / a));
        if (!info.instrument) CHECK_MSG(std::abs(run(0.f) - 0.5f) < 1e-6f, "unity gain exact");
        setGainDb(p, 6.f);
        if (!info.instrument) CHECK(std::abs(run(6.f) - 0.5f * 1.9953f) < 0.01f);
    }

    // NaN / Inf am Eingang (nur Effekt) darf nie am Ausgang ankommen
    if (!info.instrument)
    {
        FoundationProcessor p(info);
        p.setRateAndBufferSizeDetails(48000.0, 256);
        p.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buf(2, 256);
        juce::MidiBuffer midi;
        buf.clear();
        buf.setSample(0, 3, std::numeric_limits<float>::quiet_NaN());
        buf.setSample(1, 9, std::numeric_limits<float>::infinity());
        buf.setSample(1, 10, -std::numeric_limits<float>::infinity());
        p.processBlock(buf, midi);
        CHECK(allFinite(buf));
        CHECK(p.sanitizedBlocks.load() == 1);
    }

    // Null-Samples, unregelmaessige Blockgroessen, MIDI-Muell
    {
        FoundationProcessor p(info);
        p.setRateAndBufferSizeDetails(44100.0, 1024);
        p.prepareToPlay(44100.0, 1024);
        juce::AudioBuffer<float> big(2, 1024);
        juce::Random r(42);
        bool ok = true;
        for (int k = 0; k < 400; ++k)
        {
            const int n = k % 7 == 0 ? 0 : 1 + r.nextInt(1024);
            juce::AudioBuffer<float> view(big.getArrayOfWritePointers(), 2, 0, n);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) view.setSample(c, i, r.nextFloat() * 2.f - 1.f);
            juce::MidiBuffer m;
            for (int e = 0; e < 5 && n > 0; ++e)
            {
                const int pos = r.nextInt(n);
                switch (r.nextInt(5))
                {
                    case 0: m.addEvent(juce::MidiMessage::noteOn(1 + r.nextInt(16), r.nextInt(128), (juce::uint8) r.nextInt(128)), pos); break;
                    case 1: m.addEvent(juce::MidiMessage::noteOff(1, r.nextInt(128)), pos); break;
                    case 2: m.addEvent(juce::MidiMessage::allNotesOff(1), pos); break;
                    case 3: m.addEvent(juce::MidiMessage::pitchWheel(1, r.nextInt(16384)), pos); break;
                    default: m.addEvent(juce::MidiMessage::controllerEvent(1, r.nextInt(128), r.nextInt(128)), pos); break;
                }
            }
            p.processBlock(view, m);
            ok &= allFinite(view);
        }
        CHECK(ok);
    }

    // Layouts: nur Mono/Stereo
    {
        FoundationProcessor p(info);
        auto l = p.getBusesLayout();
        l.outputBuses.set(0, juce::AudioChannelSet::create5point1());
        CHECK(!p.checkBusesLayoutSupported(l));
        CHECK(setLayout(p, 1));
        CHECK(setLayout(p, 2));
    }

    // Parameter: Standardwert, Bereich, automatisierbar
    {
        FoundationProcessor p(info);
        auto* prm = p.apvts.getParameter("gain");
        CHECK(prm != nullptr && prm->isAutomatable());
        CHECK(p.getParameters().size() == 1);
        prm->setValueNotifyingHost(5.f);   // ausserhalb von 0..1 -> muss geklemmt werden
        CHECK(getGainDb(p) <= 12.f + 1e-4f);
        prm->setValueNotifyingHost(-5.f);
        CHECK(getGainDb(p) >= -60.f - 1e-4f);
    }
}

static void testState(const nwa::PluginInfo& info, const nwa::PluginInfo& other)
{
    std::printf("[state] %s\n", info.name.toRawUTF8());
    FoundationProcessor a(info);
    setGainDb(a, -7.3f);
    juce::MemoryBlock mb;
    a.getStateInformation(mb);
    CHECK(mb.getSize() > 0);

    FoundationProcessor b(info);
    b.setStateInformation(mb.getData(), (int) mb.getSize());
    CHECK(std::abs(getGainDb(b) - (-7.3f)) < 0.05f);

    // Fremder State (anderes Plugin) darf nichts veraendern
    FoundationProcessor o(other);
    setGainDb(o, 3.f);
    juce::MemoryBlock ob;
    o.getStateInformation(ob);
    FoundationProcessor c(info);
    setGainDb(c, -20.f);
    c.setStateInformation(ob.getData(), (int) ob.getSize());
    CHECK(std::abs(getGainDb(c) - (-20.f)) < 0.05f);

    // Muell, leere und abgeschnittene Daten
    juce::Random r(7);
    for (int k = 0; k < 200; ++k)
    {
        juce::MemoryBlock junk;
        const int n = r.nextInt(300);
        for (int i = 0; i < n; ++i) { auto byte = (juce::uint8) r.nextInt(256); junk.append(&byte, 1); }
        c.setStateInformation(junk.getData(), (int) junk.getSize());
    }
    c.setStateInformation(nullptr, 0);
    c.setStateInformation(mb.getData(), (int) mb.getSize() / 2);
    CHECK(std::abs(getGainDb(c) - (-7.3f)) < 0.05f || std::abs(getGainDb(c) - (-20.f)) < 0.05f);  // nie ausserhalb des Gueltigen
    CHECK(getGainDb(c) >= -60.f && getGainDb(c) <= 12.f);

    // Manipulierter Wert ausserhalb des Bereichs
    {
        auto xml = nwa::state::toXml(a.apvts, info.id, info.stateVersion);
        if (auto* params = xml->getChildByName("NWA_PARAMS"))
            for (auto* prm : params->getChildIterator())
                if (prm->getStringAttribute("id") == "gain") prm->setAttribute("value", 999.0);
        juce::MemoryBlock evil;
        juce::AudioProcessor::copyXmlToBinary(*xml, evil);
        FoundationProcessor d(info);
        d.setStateInformation(evil.getData(), (int) evil.getSize());
        CHECK_MSG(getGainDb(d) >= -60.f && getGainDb(d) <= 12.f, getGainDb(d));
    }

    // Neuere Plugin-Version im State wird geladen (vorwaerts-tolerant)
    {
        auto xml = nwa::state::toXml(a.apvts, info.id, info.stateVersion + 5);
        FoundationProcessor d(info);
        CHECK(nwa::state::fromXml(d.apvts, info.id, info.stateVersion, *xml) == nwa::state::Result::newerVersion);
    }
}

static void testPresets(const nwa::PluginInfo& info, const nwa::PluginInfo& other)
{
    std::printf("[presets] %s\n", info.name.toRawUTF8());
    auto base = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("nwa_test_" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()));
    auto root = base.getChildFile("sandbox").getChildFile("Presets");
    base.createDirectory();

    FoundationProcessor p(info), o(other);
    nwa::PresetManager pm(p.apvts, info.id, info.name, info.stateVersion, root);
    nwa::PresetManager pmOther(o.apvts, other.id, other.name, other.stateVersion, root);   // schreibt in dasselbe Verzeichnis
    using S = nwa::PresetManager::Status;

    CHECK(pm.list().isEmpty());
    setGainDb(p, -9.f);
    CHECK(pm.save("My Preset") == S::ok);
    setGainDb(p, -30.f);
    CHECK(pm.save("Zeta") == S::ok);
    setGainDb(p, 4.f);
    CHECK(pm.save("alpha") == S::ok);
    CHECK(pm.list().size() == 3);
    CHECK(pm.list()[0] == "alpha");   // alphabetisch

    CHECK(pm.load("My Preset") == S::ok);
    CHECK(std::abs(getGainDb(p) - (-9.f)) < 0.05f);
    CHECK(pm.currentName == "My Preset");
    CHECK(pm.load("nope") == S::notFound);

    // Ueberschreiben
    setGainDb(p, -1.f);
    CHECK(pm.save("My Preset") == S::ok);
    setGainDb(p, -50.f);
    CHECK(pm.load("My Preset") == S::ok && std::abs(getGainDb(p) - (-1.f)) < 0.05f);
    CHECK(pm.list().size() == 3);

    // Preset eines anderen Plugins wird abgelehnt und veraendert nichts
    setGainDb(o, 2.f);
    CHECK(pmOther.save("From Other") == S::ok);
    setGainDb(p, -33.f);
    CHECK(pm.load("From Other") == S::wrongPlugin);
    CHECK(std::abs(getGainDb(p) - (-33.f)) < 0.05f);

    // Beschaedigte Datei
    root.getChildFile("Broken.nwapreset").replaceWithText("<NWA_STATE format=\"1\"");
    root.getChildFile("Junk.nwapreset").replaceWithText("not xml at all \x01\x02");
    root.getChildFile("Empty.nwapreset").replaceWithText("");
    CHECK(pm.load("Broken") == S::corrupt);
    CHECK(pm.load("Junk") == S::corrupt);
    CHECK(pm.load("Empty") == S::corrupt);
    CHECK(std::abs(getGainDb(p) - (-33.f)) < 0.05f);

    // Boesartige / ungueltige Namen: nichts darf ausserhalb des Preset-Ordners landen
    const char* evil[] = { "../../escape", "..\\..\\escape", "/etc/passwd", "C:\\Windows\\x", "a/b/c", "", "   ", "...", "..", "." };
    for (auto* e : evil)
    {
        const auto st = pm.save(e);
        CHECK_MSG(st == S::ok || st == S::invalidName, juce::String(e));
    }
    for (auto& f : base.findChildFiles(juce::File::findFiles, true, "*"))
        CHECK_MSG(f.isAChildOf(root) || f.isAChildOf(base), f.getFullPathName());
    CHECK(!base.getChildFile("escape.nwapreset").existsAsFile());
    CHECK(!base.getChildFile("sandbox").getChildFile("escape.nwapreset").existsAsFile());
    for (auto& f : root.findChildFiles(juce::File::findFiles, true, "*"))
        CHECK_MSG(f.getParentDirectory() == root, f.getFullPathName());   // alles flach im Preset-Ordner
    CHECK(pm.save("CON") == S::ok);                 // reservierter Windows-Name wird entschaerft
    CHECK(pm.list().contains("_CON"));
    CHECK(pm.save(juce::String::repeatedString("x", 500)) == S::ok);
    for (auto& n : pm.list()) CHECK(n.length() <= 64);

    CHECK(nwa::PresetManager::sanitizeName("a<b>c:d\"e|f?g*h").containsOnly("abcdefgh_"));
    CHECK(nwa::PresetManager::sanitizeName("Nice Name.v2") == "Nice Name.v2");
    CHECK(nwa::PresetManager::sanitizeName("trailing. . ") == "trailing");

    // Loeschen
    CHECK(pm.remove("alpha"));
    CHECK(!pm.remove("alpha"));
    CHECK(!pm.remove("../x"));
    CHECK(!pm.list().contains("alpha"));

    base.deleteRecursively();
}

static juce::File g_shotDir;
static void testEditor(const nwa::PluginInfo& info)
{
    std::printf("[editor] %s\n", info.name.toRawUTF8());
    FoundationProcessor p(info);
    CHECK(p.hasEditor());
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    CHECK(ed != nullptr);
    if (ed)
    {
        CHECK(ed->getWidth() > 100 && ed->getHeight() > 100);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        CHECK(img.isValid());
        if (g_shotDir != juce::File())
        {
            juce::FileOutputStream fo(g_shotDir.getChildFile(info.id + ".png"));
            if (fo.openedOk()) { fo.setPosition(0); fo.truncate(); juce::PNGImageFormat().writeImageToStream(img, fo); }
        }
        ed.reset();
    }
    // Editor mehrfach oeffnen/schliessen darf nicht abstuerzen oder Zustand verlieren
    setGainDb(p, -4.f);
    for (int i = 0; i < 5; ++i) { std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor()); }
    CHECK(std::abs(getGainDb(p) - (-4.f)) < 0.05f);
}

static void testUtil()
{
    std::printf("[util]\n");
    float d[6] = { 1.f, std::nanf(""), INFINITY, -INFINITY, 0.25f, -0.f };
    CHECK(nwa::sanitize(d, 6));
    CHECK(d[0] == 1.f && d[1] == 0.f && d[2] == 0.f && d[3] == 0.f && d[4] == 0.25f);
    CHECK(!nwa::sanitize(d, 6));
    CHECK(std::abs(nwa::dbToGain(-6.0206f) - 0.5f) < 1e-3f);
    CHECK(std::abs(nwa::gainToDb(1.f)) < 1e-4f);
    CHECK(std::abs(nwa::midiToHz(69) - 440.f) < 1e-3f);
    CHECK(std::abs(nwa::midiToHz(60) - 261.6256f) < 1e-2f);
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String(argv[1]) == "--shots") { g_shotDir = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]); g_shotDir.createDirectory(); }
    std::vector<nwa::PluginInfo> infos { rave::makePluginInfo(), melody::makePluginInfo(), space::makePluginInfo() };

    testUtil();
    testIdentity(infos);
    for (size_t i = 0; i < infos.size(); ++i)
    {
        const auto& other = infos[(i + 1) % infos.size()];
        testAudio(infos[i]);
        testState(infos[i], other);
        testPresets(infos[i], other);
        testEditor(infos[i]);
    }
    std::printf("\n%d checks, %d failures\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
