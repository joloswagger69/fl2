#include "MelodyProcessor.h"
#ifndef MELODY_WITH_GUI
#define MELODY_WITH_GUI 0
#endif
#if MELODY_WITH_GUI
#include "gui/MelodyEditor.h"
#endif

namespace mel
{
namespace
{
juce::StringArray split(const char* s) { return juce::StringArray::fromTokens(s, "|", ""); }
const char* kBarsNames = "1|2|4|8|16";
const char* kSigNames = "4/4|3/4|5/4|6/8";
const char* kStepsNames = "16|32";
inline int barsFromIndex(int i) { static const int b[] = { 1, 2, 4, 8, 16 }; return b[std::clamp(i, 0, 4)]; }
}

juce::AudioProcessorValueTreeState::ParameterLayout MelodyProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto F = [&](const char* id, const char* name, float lo, float hi, float def, const char* label = "") { l.add(std::make_unique<AudioParameterFloat>(ParameterID { id, 1 }, name, NormalisableRange<float>(lo, hi), def, AudioParameterFloatAttributes().withLabel(label))); };
    auto I = [&](const char* id, const char* name, int lo, int hi, int def) { l.add(std::make_unique<AudioParameterInt>(ParameterID { id, 1 }, name, lo, hi, def)); };
    auto C = [&](const char* id, const char* name, const char* items, int def) { l.add(std::make_unique<AudioParameterChoice>(ParameterID { id, 1 }, name, split(items), def)); };
    auto B = [&](const char* id, const char* name, bool def) { l.add(std::make_unique<AudioParameterBool>(ParameterID { id, 1 }, name, def)); };

    C("key", "Key", kKeyNames, 9); C("scale", "Scale", kScaleNames, Minor); F("bpm", "BPM", 40.f, 240.f, 120.f, "BPM");
    C("bars", "Length (bars)", kBarsNames, 2); C("sig", "Time signature", kSigNames, 0); C("mood", "Mood", kMoodNames, Melancholic); C("genre", "Genre", kGenreNames, GHouse);
    F("swing", "Swing", 0.f, 0.5f, 0.f); B("sync", "Host sync", true); B("play", "Play", false); F("vol", "Preview volume", -24.f, 6.f, -3.f, "dB");
    for (int p = 0; p < 5; ++p) B(("mute_" + String(p)).toRawUTF8(), (String(kPartNames[p]) + " mute").toRawUTF8(), false);

    F("c_cplx", "Chords complexity", 0.f, 1.f, 0.4f); I("c_ext", "Chords max extension", 0, 4, 2); C("c_voicing", "Chords voicing", kVoicingNames, VClose); I("c_inv", "Chords inversion", 0, 3, 0);
    B("c_vl", "Chords voice leading", true); C("c_perbar", "Chords per bar", kPerBarNames, 1); C("c_rhythm", "Chords rhythm", kChordRhythmNames, 0);
    B("c_sus", "Chords allow sus", false); B("c_dimaug", "Chords allow dim/aug", false); I("c_seed", "Chords seed", 0, 9999, 1);

    C("m_char", "Melody character", kMelCharNames, McMelody); F("m_dens", "Melody density", 0.f, 1.f, 0.5f); F("m_rcplx", "Melody rhythmic complexity", 0.f, 1.f, 0.4f);
    F("m_rep", "Melody repetition", 0.f, 1.f, 0.6f); F("m_var", "Melody variation", 0.f, 1.f, 0.3f); I("m_leap", "Melody max leap", 2, 12, 7); I("m_oct", "Melody octave", -1, 1, 0);
    B("m_follow", "Melody follows chords", true); I("m_seed", "Melody seed", 0, 9999, 2);

    C("a_pat", "Arp pattern", kArpPatternNames, ApUp); C("a_rate", "Arp rate", kArpRateNames, 2); F("a_gate", "Arp gate", 0.1f, 1.f, 0.7f); I("a_oct", "Arp octaves", 1, 3, 2); I("a_shift", "Arp octave shift", -1, 1, 0); I("a_seed", "Arp seed", 0, 9999, 4);

    C("b_style", "Bass style", kBassStyleNames, BsEighths); F("b_dens", "Bass density", 0.f, 1.f, 0.7f); I("b_oct", "Bass octave", 0, 2, 1); B("b_appr", "Bass approach notes", true); I("b_seed", "Bass seed", 0, 9999, 3);

    C("x_char", "Acid character", kAcidCharNames, AcClassic); F("x_dens", "Acid density", 0.f, 1.f, 0.7f); F("x_slide", "Acid slide", 0.f, 1.f, 0.3f); F("x_acc", "Acid accent", 0.f, 1.f, 0.3f);
    F("x_tie", "Acid tie", 0.f, 1.f, 0.1f); F("x_oct", "Acid octave changes", 0.f, 1.f, 0.2f); F("x_prob", "Acid note probability", 0.f, 1.f, 0.8f); F("x_gate", "Acid gate length", 0.1f, 1.f, 0.5f);
    F("x_var", "Acid pattern variation", 0.f, 1.f, 0.3f); F("x_pmod", "Acid pitch variation", 0.f, 1.f, 0.1f); C("x_steps", "Acid steps", kStepsNames, 0); I("x_octb", "Acid octave", 0, 2, 1); I("x_seed", "Acid seed", 0, 9999, 5);

    B("t_acid", "Track: include acid", false); B("t_tempo", "Track: set genre tempo", true); I("t_seed", "Track seed", 0, 9999, 1);
    return l;
}

MelodyProcessor::MelodyProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NWA_PARAMS", createLayout()),
      presets(apvts, makePluginInfo().id, makePluginInfo().name.replace("//", "-"), makePluginInfo().stateVersion)
{
    for (int p = 0; p < 5; ++p) pMute[(size_t) p] = apvts.getRawParameterValue("mute_" + juce::String(p));
    pBpm = apvts.getRawParameterValue("bpm"); pSync = apvts.getRawParameterValue("sync"); pPlay = apvts.getRawParameterValue("play"); pVol = apvts.getRawParameterValue("vol");
    synth.prepare(44100.0);
    regenerate(-1, false);          // sofort eine erste Idee (A-Moll, House-artige Einstellungen)
    undoStack.clear();
}

bool MelodyProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && (l.inputBuses.isEmpty() || l.getMainInputChannelSet().isDisabled());
}

void MelodyProcessor::prepareToPlay(double sampleRate, int maxBlock)
{
    sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
    synth.prepare(sr); synth.allOff(true);
    scratchL.assign((size_t) std::max(maxBlock, 1) + 8, 0.f); scratchR = scratchL;
    internalTicks = 0.0; expectedTicks = -1.0; wasRunning = false; wasMuted.fill(false);
}

void MelodyProcessor::setParam(const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(plain));
}

// ---------------------------------------------------------------------------------------------------------------- Einstellungen
Ctx MelodyProcessor::ctx() const
{
    Ctx c; c.key = getInt("key"); c.scale = getInt("scale"); c.bars = barsFromIndex(getInt("bars"));
    const int sg = getInt("sig"); c.sigNum = sg == 0 ? 4 : sg == 1 ? 3 : sg == 2 ? 5 : 6; c.sigDen = sg == 3 ? 8 : 4;
    return c;
}
ChordParams MelodyProcessor::chordParams() const
{
    ChordParams c; c.mood = getInt("mood"); c.complexity = getRaw("c_cplx"); c.maxExt = getInt("c_ext"); c.voicing = getInt("c_voicing"); c.inversion = getInt("c_inv");
    c.voiceLeading = getRaw("c_vl") > 0.5f; c.perBar = getInt("c_perbar"); c.rhythm = getInt("c_rhythm"); c.sus = getRaw("c_sus") > 0.5f; c.dimAug = getRaw("c_dimaug") > 0.5f;
    return c;
}
MelodyParams MelodyProcessor::melodyParams() const
{
    MelodyParams m; m.character = getInt("m_char"); m.density = getRaw("m_dens"); m.rhythm = getRaw("m_rcplx"); m.repeat = getRaw("m_rep"); m.variation = getRaw("m_var");
    m.leap = getInt("m_leap"); m.octave = getInt("m_oct"); m.followChords = getRaw("m_follow") > 0.5f; m.swingTicks = swingTicks();
    return m;
}
ArpParams MelodyProcessor::arpParams() const { ArpParams a; a.pattern = getInt("a_pat"); a.rate = getInt("a_rate"); a.gate = getRaw("a_gate"); a.octaves = getInt("a_oct"); a.octave = getInt("a_shift"); return a; }
BassParams MelodyProcessor::bassParams() const { BassParams b; b.style = getInt("b_style"); b.density = getRaw("b_dens"); b.octave = getInt("b_oct"); b.approach = getRaw("b_appr") > 0.5f; b.swingTicks = swingTicks(); return b; }
AcidParams MelodyProcessor::acidParams() const
{
    AcidParams a; a.character = getInt("x_char"); a.density = getRaw("x_dens"); a.slide = getRaw("x_slide"); a.accent = getRaw("x_acc"); a.tie = getRaw("x_tie"); a.octave = getRaw("x_oct");
    a.prob = getRaw("x_prob"); a.gate = getRaw("x_gate"); a.variation = getRaw("x_var"); a.pmod = getRaw("x_pmod"); a.steps = getInt("x_steps") == 1 ? 32 : 16; a.octaveBase = getInt("x_octb"); a.swingTicks = swingTicks();
    return a;
}

// ---------------------------------------------------------------------------------------------------------------- Snapshots
std::shared_ptr<Snapshot> MelodyProcessor::make(Project p) const
{
    auto s = std::make_shared<Snapshot>(); s->ev = buildEvents(p); s->proj = std::move(p); return s;
}
void MelodyProcessor::install(std::shared_ptr<Snapshot> s, bool pushUndo)
{
    if (pushUndo && currentPtr) { undoStack.push_back(currentPtr); if (undoStack.size() > 24) undoStack.erase(undoStack.begin()); }
    currentPtr = s; history.push_back(s); while (history.size() > 24) history.pop_front();
    generatedCtx = s->proj.ctx;
    live.store(s.get(), std::memory_order_release);
    ++revision;
}
bool MelodyProcessor::undo()
{
    if (undoStack.empty()) return false;
    auto s = undoStack.back(); undoStack.pop_back();
    install(s, false); return true;
}
bool MelodyProcessor::isStale() const
{
    const auto c = ctx();
    return currentPtr && (c.key != generatedCtx.key || c.scale != generatedCtx.scale || c.bars != generatedCtx.bars || c.sigNum != generatedCtx.sigNum || c.sigDen != generatedCtx.sigDen);
}

bool MelodyProcessor::regenerate(int part, bool newSeed)
{
    const Ctx cx = ctx();
    Project p = currentPtr ? currentPtr->proj : Project();
    bool all = (part < 0) || ! currentPtr || isStale();
    const bool forced = all && part >= 0;             // Tonart/Laenge geaendert -> alles muss zusammenpassen
    auto fresh = [&]() { return juce::Random::getSystemRandom().nextInt(10000); };
    static const char* seedIds[5] = { "c_seed", "m_seed", "b_seed", "a_seed", "x_seed" };
    if (newSeed) { if (all) { for (int i = 0; i < 4; ++i) setParam(seedIds[i], (float) fresh()); if (part == 4) setParam("x_seed", (float) fresh()); } else setParam(seedIds[part], (float) fresh()); }
    if (all) p.ctx = cx;
    const bool acidWanted = all ? (getRaw("t_acid") > 0.5f || part == 4) : false;
    if (all || part == 0) { p.prog = generateProgression(cx, chordParams(), seedFor("c_seed")); p.chords = chordPart(cx, p.prog, chordParams().rhythm, swingTicks()); }
    if (all || part == 1) p.melody = generateMelody(cx, p.prog, melodyParams(), seedFor("m_seed"));
    if (all || part == 2) p.bass = generateBass(cx, p.prog, bassParams(), seedFor("b_seed"));
    if (all || part == 3) p.arp = generateArp(cx, p.prog, arpParams(), seedFor("a_seed"));
    if (part == 4 || acidWanted) p.acid = generateAcid(cx, acidParams(), seedFor("x_seed"));
    else if (all) p.acid.clear();
    install(make(std::move(p)), true);
    return forced;
}

void MelodyProcessor::clearPart(int part)
{
    if (! currentPtr || part < 0 || part > 4) return;
    Project p = currentPtr->proj; p.part(part).clear();
    install(make(std::move(p)), true);
}

void MelodyProcessor::variation(int part, float amount)
{
    if (! currentPtr || part < 1 || part > 4) return;
    Project p = currentPtr->proj;
    static int round = 0; ++round;
    if (part == 4)
    {   // Acid: gleicher Seed, aber mehr Pattern-Variation -> andere Fuellungen
        auto ap = acidParams(); ap.variation = std::clamp(ap.variation + amount, 0.f, 1.f); p.acid = generateAcid(p.ctx, ap, seedFor("x_seed") + round);
    }
    else p.part(part) = mutatePart(p.part(part), p.ctx, p.prog, amount, round * 31 + part, part != 3);
    install(make(std::move(p)), true);
}

void MelodyProcessor::applyGenreToParams(int genre, int mood, bool setTempo)
{
    const auto g = genreSetup(genre, mood);
    if (setTempo) setParam("bpm", (float) g.bpm);
    setParam("swing", g.swing);
    setParam("c_cplx", g.chords.complexity); setParam("c_ext", (float) g.chords.maxExt); setParam("c_voicing", (float) g.chords.voicing); setParam("c_inv", (float) g.chords.inversion);
    setParam("c_vl", g.chords.voiceLeading ? 1.f : 0.f); setParam("c_perbar", (float) g.chords.perBar); setParam("c_rhythm", (float) g.chords.rhythm); setParam("c_sus", g.chords.sus ? 1.f : 0.f); setParam("c_dimaug", g.chords.dimAug ? 1.f : 0.f);
    setParam("m_char", (float) g.melody.character); setParam("m_dens", g.melody.density); setParam("m_rcplx", g.melody.rhythm); setParam("m_rep", g.melody.repeat); setParam("m_var", g.melody.variation);
    setParam("m_leap", (float) g.melody.leap); setParam("m_oct", (float) g.melody.octave); setParam("m_follow", 1.f);
    setParam("a_pat", (float) g.arp.pattern); setParam("a_rate", (float) g.arp.rate); setParam("a_gate", g.arp.gate); setParam("a_oct", (float) g.arp.octaves); setParam("a_shift", (float) g.arp.octave);
    setParam("b_style", (float) g.bass.style); setParam("b_dens", g.bass.density); setParam("b_oct", (float) g.bass.octave); setParam("b_appr", g.bass.approach ? 1.f : 0.f);
    if (g.acid)
    {
        setParam("x_char", (float) g.acidP.character); setParam("x_dens", g.acidP.density); setParam("x_slide", g.acidP.slide); setParam("x_acc", g.acidP.accent);
    }
}

void MelodyProcessor::makeTrack(bool newIdea)
{
    if (newIdea) setParam("t_seed", (float) juce::Random::getSystemRandom().nextInt(10000));
    const int base = getInt("t_seed");
    applyGenreToParams(getInt("genre"), getInt("mood"), getRaw("t_tempo") > 0.5f);
    setParam("c_seed", (float) base); setParam("m_seed", (float) ((base + 1) % 10000)); setParam("b_seed", (float) ((base + 2) % 10000)); setParam("a_seed", (float) ((base + 3) % 10000)); setParam("x_seed", (float) ((base + 4) % 10000));
    // Parameter sind sofort lesbar (Rohwerte werden synchron gesetzt), daher direkt erzeugen
    regenerate(-1, false);
}

// ---------------------------------------------------------------------------------------------------------------- Export
juce::File MelodyProcessor::exportDirectory() const
{
    auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEOWAVE_AUDIO").getChildFile("MELODY-STUDIO").getChildFile("Exports");
    d.createDirectory(); return d;
}
bool MelodyProcessor::exportPart(int part, const juce::File& file, juce::String* err) const
{
    if (! currentPtr || part < 0 || part > 4) { if (err) *err = "No music generated yet"; return false; }
    const auto& pr = currentPtr->proj;
    std::vector<midi::TrackSpec> t;
    if (! pr.part(part).empty()) t.push_back({ &pr.part(part), part + 1, kPartNames[part] });
    return midi::writeFile(file, t, (double) getRaw("bpm"), pr.ctx.sigNum, pr.ctx.sigDen, pr.ctx.totalTicks(), err);
}
int MelodyProcessor::exportSeparate(const juce::File& dir, const juce::String& baseName, juce::String* err) const
{
    int n = 0;
    for (int p = 0; p < 5; ++p)
    {
        if (! currentPtr || currentPtr->proj.part(p).empty()) continue;
        if (exportPart(p, dir.getChildFile(baseName + "_" + kPartNames[p] + ".mid"), err)) ++n; else return -1;
    }
    return n;
}
bool MelodyProcessor::exportCombined(const juce::File& file, juce::String* err) const
{
    if (! currentPtr) { if (err) *err = "No music generated yet"; return false; }
    const auto& pr = currentPtr->proj;
    std::vector<midi::TrackSpec> t;
    for (int p = 0; p < 5; ++p) if (! pr.part(p).empty()) t.push_back({ &pr.part(p), p + 1, kPartNames[p] });
    return midi::writeFile(file, t, (double) getRaw("bpm"), pr.ctx.sigNum, pr.ctx.sigDen, pr.ctx.totalTicks(), err);
}

// ---------------------------------------------------------------------------------------------------------------- Audio
void MelodyProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    midi.clear();
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    buffer.clear();
    if (n <= 0 || nch <= 0) return;
    if ((int) scratchL.size() < n) return;                   // prepareToPlay nicht aufgerufen / zu grosser Block: lieber Stille als Absturz

    const Snapshot* snap = live.load(std::memory_order_acquire);
    bool hostPlaying = false, hasPpq = false; double ppq = 0.0, hostBpm = 0.0;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition())
    {
        hostPlaying = pos->getIsPlaying();
        if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
        if (auto b = pos->getBpm()) hostBpm = *b;
    }
    const bool sync = pSync->load() > 0.5f;
    const bool running = snap != nullptr && (sync ? hostPlaying : pPlay->load() > 0.5f);
    double bpm = (sync && hostBpm > 20.0) ? hostBpm : (double) pBpm->load();
    bpm = std::clamp(bpm, 20.0, 400.0);
    const double tps = bpm * (double) kPpq / (60.0 * sr);          // Ticks pro Sample
    float* L = scratchL.data(); float* R = scratchR.data();
    std::fill(L, L + n, 0.f); std::fill(R, R + n, 0.f);

    if (snap) for (int p = 0; p < 5; ++p)
    {
        const bool m = pMute[(size_t) p]->load() > 0.5f;
        if (m && ! wasMuted[(size_t) p]) synth.partOff(p);
        wasMuted[(size_t) p] = m;
    }
    if (! running)
    {
        if (wasRunning) synth.allOff();
        wasRunning = false; expectedTicks = -1.0; playTicks.store(-1.0);
    }
    else
    {
        const double loop = (double) snap->proj.ctx.totalTicks();
        double pos;
        if (sync && hasPpq)
        {
            pos = std::fmod(ppq * (double) kPpq, loop); if (pos < 0) pos += loop;
            if (! wasRunning || expectedTicks < 0 || std::abs(pos - expectedTicks) > 4.0 + 2.0 * n * tps)
            {
                const double wrapDiff = std::min(std::abs(pos - expectedTicks), loop - std::abs(pos - expectedTicks));
                if (! wasRunning || expectedTicks < 0 || wrapDiff > 4.0 + 2.0 * n * tps) synth.allOff();      // Sprung im Transport
            }
        }
        else
        {
            if (! wasRunning) { internalTicks = 0.0; synth.allOff(); }
            pos = internalTicks;
        }
        wasRunning = true;
        const auto& ev = snap->ev;
        int s = 0;
        auto renderParts = [&](int from, int to)
        {
            if (to <= from) return;
            for (int p = 0; p < 5; ++p) synth.render(p, L + from, R + from, to - from);
        };
        while (s < n)
        {
            if (pos >= loop) { pos -= loop; synth.allOff(); }
            const double remain = loop - pos;
            int seg = (int) std::ceil(remain / tps); seg = std::clamp(seg, 1, n - s);
            const int e = s + seg; const double segEnd = pos + seg * tps;
            auto it = std::lower_bound(ev.begin(), ev.end(), pos, [](const Ev& a, double t) { return (double) a.tick < t; });
            int cursor = s;
            for (; it != ev.end() && (double) it->tick < segEnd; ++it)
            {
                int off = s + (int) std::ceil(((double) it->tick - pos) / tps - 1.0e-6);   // ceil: Einsatz unabhaengig von der Blockgroesse off = std::clamp(off, cursor, e);
                renderParts(cursor, off); cursor = off;
                if (it->on) { if (pMute[(size_t) it->part]->load() < 0.5f) synth.noteOn(it->part, it->pitch, it->vel, it->slideIn); }
                else synth.noteOff(it->part, it->pitch);
            }
            renderParts(cursor, e);
            // Slides: der Synth erhaelt "slideFromPrev" ueber die Ueberlappung (naechste Note beginnt, solange die vorherige noch gehalten wird) -> siehe noteOn-Aufruf oben
            pos += seg * tps; s = e;
        }
        if (! (sync && hasPpq)) internalTicks = std::fmod(pos, loop);
        expectedTicks = pos >= loop ? pos - loop : pos;
        playTicks.store(expectedTicks);
    }
    if (! running)
        for (int p = 0; p < 5; ++p) synth.render(p, L, R, n);                      // Ausklingen lassen

    const float g = nwa::dbToGain(pVol->load());
    for (int i = 0; i < n; ++i)
    {
        L[i] = std::tanh(L[i] * g); R[i] = std::tanh(R[i] * g);
    }
    if (nch == 1) { auto* o = buffer.getWritePointer(0); for (int i = 0; i < n; ++i) o[i] = 0.5f * (L[i] + R[i]); }
    else { std::copy(L, L + n, buffer.getWritePointer(0)); std::copy(R, R + n, buffer.getWritePointer(1)); }
    nwa::sanitize(buffer);
}

// ---------------------------------------------------------------------------------------------------------------- State
namespace
{
juce::String partToString(const Part& p)
{
    juce::StringArray a;
    for (auto& n : p) a.add(juce::String(n.tick) + "," + juce::String(n.len) + "," + juce::String(n.pitch) + "," + juce::String(n.vel) + "," + (n.slide ? "1" : "0"));
    return a.joinIntoString(";");
}
Part partFromString(const juce::String& s, int totalTicks)
{
    Part out;
    for (auto& tok : juce::StringArray::fromTokens(s, ";", ""))
    {
        auto f = juce::StringArray::fromTokens(tok, ",", "");
        if (f.size() != 5 || out.size() > 20000) continue;
        Note n; n.tick = f[0].getIntValue(); n.len = f[1].getIntValue(); n.pitch = f[2].getIntValue(); n.vel = f[3].getIntValue(); n.slide = f[4] == "1";
        if (n.tick < 0 || n.tick >= totalTicks || n.len < 1 || n.pitch < 0 || n.pitch > 127 || n.vel < 1 || n.vel > 127) continue;
        n.len = std::min(n.len, totalTicks - n.tick);
        out.push_back(n);
    }
    std::stable_sort(out.begin(), out.end(), [](const Note& a, const Note& b) { return a.tick < b.tick; });
    return out;
}
juce::String progToString(const Progression& pr)
{
    juce::StringArray a;
    for (auto& s : pr)
    {
        juce::StringArray iv, vo; for (int v : s.chord.iv) iv.add(juce::String(v)); for (int v : s.voiced) vo.add(juce::String(v));
        a.add(juce::String(s.tick) + "," + juce::String(s.len) + "," + juce::String(s.chord.root) + "," + juce::String(s.chord.degree) + "|" + iv.joinIntoString(" ") + "|" + vo.joinIntoString(" ") + "|" + s.chord.name);
    }
    return a.joinIntoString(";");
}
Progression progFromString(const juce::String& s, int totalTicks)
{
    Progression out;
    for (auto& tok : juce::StringArray::fromTokens(s, ";", ""))
    {
        auto f = juce::StringArray::fromTokens(tok, "|", "");
        if (f.size() != 4 || out.size() > 64) continue;
        auto h = juce::StringArray::fromTokens(f[0], ",", "");
        if (h.size() != 4) continue;
        ChordSlot c; c.tick = h[0].getIntValue(); c.len = h[1].getIntValue(); c.chord.root = ((h[2].getIntValue() % 12) + 12) % 12; c.chord.degree = h[3].getIntValue();
        if (c.tick < 0 || c.len < 1 || c.tick >= totalTicks) continue;
        c.chord.iv.clear(); for (auto& t : juce::StringArray::fromTokens(f[1], " ", "")) { const int v = t.getIntValue(); if (v >= 0 && v < 40 && c.chord.iv.size() < 8) c.chord.iv.push_back(v); }
        for (auto& t : juce::StringArray::fromTokens(f[2], " ", "")) { const int v = t.getIntValue(); if (v >= 0 && v < 128 && c.voiced.size() < 12) c.voiced.push_back(v); }
        if (c.chord.iv.empty()) continue;
        c.chord.name = f[3].substring(0, 20);
        out.push_back(c);
    }
    return out;
}
}

juce::ValueTree MelodyProcessor::makeExtra() const
{
    juce::ValueTree t("MEL");
    if (! currentPtr) return t;
    const auto& pr = currentPtr->proj;
    t.setProperty("key", pr.ctx.key, nullptr); t.setProperty("scale", pr.ctx.scale, nullptr); t.setProperty("bars", pr.ctx.bars, nullptr);
    t.setProperty("sn", pr.ctx.sigNum, nullptr); t.setProperty("sd", pr.ctx.sigDen, nullptr);
    t.setProperty("prog", progToString(pr.prog), nullptr);
    for (int p = 0; p < 5; ++p) { juce::ValueTree c("PART"); c.setProperty("i", p, nullptr); c.setProperty("n", partToString(pr.part(p)), nullptr); t.appendChild(c, nullptr); }
    return t;
}

void MelodyProcessor::applyExtra(const juce::ValueTree& extra)
{
    if (! extra.isValid() || ! extra.hasType("MEL") || ! extra.hasProperty("key")) return;
    Project p;
    p.ctx.key = std::clamp((int) extra["key"], 0, 11); p.ctx.scale = std::clamp((int) extra["scale"], 0, (int) kNumScales - 1);
    p.ctx.bars = std::clamp((int) extra["bars"], 1, 16); p.ctx.sigNum = std::clamp((int) extra["sn"], 1, 12); p.ctx.sigDen = ((int) extra["sd"] == 8) ? 8 : 4;
    const int total = p.ctx.totalTicks();
    p.prog = progFromString(extra["prog"].toString(), total);
    for (auto c : extra) if (c.hasType("PART")) { const int i = c["i"]; if (i >= 0 && i < 5) p.part(i) = partFromString(c["n"].toString(), total); }
    install(make(std::move(p)), false);
    undoStack.clear();
}

void MelodyProcessor::getStateInformation(juce::MemoryBlock& dest) { nwa::state::save(apvts, makePluginInfo().id, makePluginInfo().stateVersion, dest, makeExtra()); }
void MelodyProcessor::setStateInformation(const void* data, int size)
{
    juce::ValueTree extra;
    const auto r = nwa::state::load(apvts, makePluginInfo().id, makePluginInfo().stateVersion, data, size, &extra);
    if (r == nwa::state::Result::ok || r == nwa::state::Result::newerVersion) applyExtra(extra);
}

#if MELODY_WITH_GUI
juce::AudioProcessorEditor* MelodyProcessor::createEditor() { return gui::createMelodyEditor(*this); }
#else
juce::AudioProcessorEditor* MelodyProcessor::createEditor() { return nullptr; }
#endif
} // namespace mel

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new mel::MelodyProcessor(); }
