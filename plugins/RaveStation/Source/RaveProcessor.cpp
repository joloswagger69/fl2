#include "RaveProcessor.h"
#ifndef RAVE_WITH_GUI
#define RAVE_WITH_GUI 0
#endif
#if RAVE_WITH_GUI
#include "gui/RaveEditor.h"
#endif
#include "engine/KitSynth.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace rave
{
namespace
{
struct PDef { const char* id; const char* name; float lo, hi, def, skew; const char* label; int kind; /*0 float,1 bool,2 int,3 choice*/ };
const PDef kDefs[kPadParamCount] = {
    { "vol",     "Volume",   -60.f, 12.f, 0.f,     1.f,  "dB",  0 },
    { "pan",     "Pan",      -1.f,  1.f,  0.f,     1.f,  "",    0 },
    { "pitch",   "Pitch",    -24.f, 24.f, 0.f,     1.f,  "st",  0 },
    { "atk",     "Attack",   0.f,   0.5f, 0.f,     0.35f,"s",   0 },
    { "dec",     "Decay",    0.005f,10.f, 10.f,    0.3f, "s",   0 },
    { "start",   "Start",    0.f,   1.f,  0.f,     1.f,  "",    0 },
    { "end",     "End",      0.f,   1.f,  1.f,     1.f,  "",    0 },
    { "rev",     "Reverse",  0.f,   1.f,  0.f,     1.f,  "",    1 },
    { "mute",    "Mute",     0.f,   1.f,  0.f,     1.f,  "",    1 },
    { "solo",    "Solo",     0.f,   1.f,  0.f,     1.f,  "",    1 },
    { "lp",      "LP Cutoff",40.f,  20000.f,20000.f,0.3f,"Hz",  0 },
    { "hp",      "HP Cutoff",20.f,  8000.f, 20.f,   0.3f,"Hz",  0 },
    { "res",     "Resonance",0.f,   0.9f, 0.f,     1.f,  "",    0 },
    { "drv",     "Drive",    0.f,   1.f,  0.f,     1.f,  "",    0 },
    { "crush",   "Bitcrush", 0.f,   1.f,  0.f,     1.f,  "",    0 },
    { "choke",   "Choke Grp",0.f,   8.f,  0.f,     1.f,  "",    2 },
    { "keep",    "Pitch Keeps Length", 0.f, 1.f, 0.f, 1.f, "",  1 },
    { "stretch", "Time Stretch", 0.25f, 4.f, 1.f,  0.5f, "x",   0 },
    { "layer",   "Layer Level", 0.f, 1.f,  0.f,    1.f,  "",    0 },
    { "stutter", "Stutter",  0.f,   7.f,  0.f,     1.f,  "",    3 },
    { "glitch",  "Glitch",   0.f,   1.f,  0.f,     1.f,  "",    0 },
    { "role",    "Role",     0.f,   8.f,  8.f,     1.f,  "",    3 },
};
inline double padGainFromDb(float db) { return db <= -59.9f ? 0.0 : std::pow(10.0, db / 20.0); }
}

juce::String padParamId(int pad, int p) { return "p" + juce::String(pad + 1).paddedLeft('0', 2) + "_" + kDefs[p].id; }
juce::String padParamName(int p) { return kDefs[p].name; }

juce::AudioProcessorValueTreeState::ParameterLayout RaveProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (int pad = 0; pad < kPads; ++pad)
        for (int p = 0; p < kPadParamCount; ++p)
        {
            const auto& d = kDefs[p];
            const ParameterID id { padParamId(pad, p), 1 };
            const String name = "Pad " + String(pad + 1) + " " + d.name;
            if (d.kind == 1) l.add(std::make_unique<AudioParameterBool>(id, name, d.def > 0.5f));
            else if (d.kind == 2) l.add(std::make_unique<AudioParameterInt>(id, name, (int) d.lo, (int) d.hi, (int) d.def));
            else if (d.kind == 3)
            {
                StringArray ch = StringArray::fromTokens(p == pStutter ? kStutterNames : kRoleNames, "|", "");
                int def = (int) d.def; if (p == pRole) def = kit::kDefaultRoles[pad];
                l.add(std::make_unique<AudioParameterChoice>(id, name, ch, def));
            }
            else
            {
                NormalisableRange<float> r(d.lo, d.hi);
                if (d.skew < 0.99f && d.lo >= 0.f) r.setSkewForCentre(d.lo + (d.hi - d.lo) * (p == pLp || p == pHp ? 0.03f : 0.25f));
                if (p == pStretch) r.setSkewForCentre(1.f);
                l.add(std::make_unique<AudioParameterFloat>(id, name, r, d.def, AudioParameterFloatAttributes().withLabel(d.label)));
            }
        }
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "bpm", 1 }, "Internal BPM", NormalisableRange<float>(40.f, 300.f, 0.01f), 140.f, AudioParameterFloatAttributes().withLabel("BPM")));
    l.add(std::make_unique<AudioParameterBool>(ParameterID { "sync", 1 }, "Host Sync", true));
    l.add(std::make_unique<AudioParameterBool>(ParameterID { "play", 1 }, "Internal Play", false));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "swing", 1 }, "Swing", NormalisableRange<float>(0.f, 1.f), 0.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "humt", 1 }, "Humanize Timing", NormalisableRange<float>(0.f, 1.f), 0.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "humv", 1 }, "Humanize Velocity", NormalisableRange<float>(0.f, 1.f), 0.f));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID { "div", 1 }, "Step Division", StringArray::fromTokens(kStepDivNames, "|", ""), 1));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID { "sig", 1 }, "Time Signature", StringArray::fromTokens(kTimeSigNames, "|", ""), 2));
    l.add(std::make_unique<AudioParameterInt>(ParameterID { "pattern", 1 }, "Pattern (16-19 = EVOLVE variants)", 0, kPatternSlots - 1, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "master", 1 }, "Master", NormalisableRange<float>(-60.f, 6.f, 0.1f), 0.f, AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterInt>(ParameterID { "seed", 1 }, "Random Seed", 1, 9999, 1));
    // Generator
    l.add(std::make_unique<AudioParameterChoice>(ParameterID { "genre", 1 }, "Genre", StringArray::fromTokens(kGenreNames, "|", ""), 0));
    for (auto [id, nm, def] : std::initializer_list<std::tuple<const char*, const char*, float>> {
             { "g_int", "Gen Intensity", 0.6f }, { "g_cplx", "Gen Complexity", 0.5f }, { "g_dens", "Gen Density", 0.5f }, { "g_ghost", "Gen Ghost Notes", 0.4f },
             { "g_var", "Gen Variation", 0.4f }, { "g_rand", "Gen Randomness", 0.3f }, { "g_swing", "Gen Swing", 0.f } })
        l.add(std::make_unique<AudioParameterFloat>(ParameterID { id, 1 }, nm, NormalisableRange<float>(0.f, 1.f), def));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID { "g_tempo", 1 }, "Gen Tempo (0=genre default)", NormalisableRange<float>(0.f, 300.f, 1.f), 0.f));
    return l;
}

// ------------------------------------------------------------------------------------------------ Worker
class RaveProcessor::Worker : public juce::Thread
{
public:
    explicit Worker(RaveProcessor& p) : juce::Thread("RAVE worker"), owner(p) {}
    void run() override
    {
        std::array<uint32_t, kPads> done {}; std::array<int, kPads> doneGen {}; std::array<bool, kPads> doneValid {};
        while (! threadShouldExit())
        {
            wait(60);
            owner.engine.drainTrash();
            for (int pad = 0; pad < kPads && ! threadShouldExit(); ++pad)
            {
                const auto pp = owner.readPad(pad);
                const int gen = owner.generation[(size_t) pad].load();
                SamplePtr src; { const juce::ScopedLock sl(owner.sampleLock); src = owner.source[(size_t) pad]; }
                if (! src) { doneValid[(size_t) pad] = false; continue; }
                if (! needsProcessing(pp)) { doneValid[(size_t) pad] = false; continue; }
                const uint32_t key = processedKey(pp);
                if (doneValid[(size_t) pad] && done[(size_t) pad] == key && doneGen[(size_t) pad] == gen) continue;
                // Entprellen: Parameter muessen kurz stillstehen
                wait(120);
                const auto again = owner.readPad(pad);
                if (processedKey(again) != key || owner.generation[(size_t) pad].load() != gen) continue;
                auto res = morph::renderProcessed(*src, pp.stretch, pp.keep ? pp.pitch : 0.f);
                const juce::ScopedLock sl(owner.sampleLock);
                if (owner.generation[(size_t) pad].load() != gen) continue;       // Quelle wurde inzwischen ersetzt
                if (owner.engine.postSample(pad, RaveEngine::kProcessed, res, key))
                { done[(size_t) pad] = key; doneGen[(size_t) pad] = gen; doneValid[(size_t) pad] = true; }
            }
        }
    }
private:
    RaveProcessor& owner;
};

// ------------------------------------------------------------------------------------------------ Prozessor
RaveProcessor::RaveProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NWA_PARAMS", createLayout()),
      presets(apvts, makePluginInfo().id, makePluginInfo().name.replace("//", "-"), makePluginInfo().stateVersion)
{
    for (auto& g : generation) g = 0;
    for (int pad = 0; pad < kPads; ++pad)
        for (int p = 0; p < kPadParamCount; ++p) pp[pad][p] = apvts.getRawParameterValue(padParamId(pad, p));
    gBpm = apvts.getRawParameterValue("bpm"); gSync = apvts.getRawParameterValue("sync"); gPlay = apvts.getRawParameterValue("play");
    gSwing = apvts.getRawParameterValue("swing"); gHumT = apvts.getRawParameterValue("humt"); gHumV = apvts.getRawParameterValue("humv");
    gDiv = apvts.getRawParameterValue("div"); gPat = apvts.getRawParameterValue("pattern"); gMaster = apvts.getRawParameterValue("master");
    gSeed = apvts.getRawParameterValue("seed");
    loadDefaultKit();
    worker = std::make_unique<Worker>(*this);
    worker->startThread(juce::Thread::Priority::low);
}

RaveProcessor::~RaveProcessor()
{
    worker->stopThread(2000);
    engine.drainTrash();
}

bool RaveProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && (l.inputBuses.isEmpty() || l.getMainInputChannelSet().isDisabled());
}

void RaveProcessor::prepareToPlay(double sampleRate, int maxBlock)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    engine.prepare(sr, maxBlock);
}

PadParams RaveProcessor::readPad(int pad) const
{
    PadParams p;
    auto v = [&](int k) { return pp[pad][k]->load(); };
    p.gain = (float) padGainFromDb(v(pVol)); p.pan = v(pPan); p.pitch = v(pPitch); p.atk = v(pAtk); p.dec = v(pDec);
    p.start = v(pStart); p.end = v(pEnd); p.rev = v(pRev) > 0.5f; p.mute = v(pMute) > 0.5f; p.solo = v(pSolo) > 0.5f;
    p.lp = v(pLp); p.hp = v(pHp); p.res = v(pRes); p.drv = v(pDrv); p.crush = v(pCrush);
    p.choke = (int) std::lround(v(pChoke)); p.keep = v(pKeep) > 0.5f; p.stretch = v(pStretch); p.layer = v(pLayer);
    p.stutter = (int) std::lround(v(pStutter)); p.glitch = v(pGlitch); p.role = (int) std::lround(v(pRole));
    return p;
}

GlobalParams RaveProcessor::readGlobal() const
{
    GlobalParams g;
    g.bpm = gBpm->load(); g.hostSync = gSync->load() > 0.5f; g.playing = gPlay->load() > 0.5f; g.swing = gSwing->load();
    g.humTime = gHumT->load(); g.humVel = gHumV->load(); g.stepDiv = (int) std::lround(gDiv->load());
    g.slot = (int) std::lround(gPat->load()); g.master = (float) padGainFromDb(gMaster->load()); g.seed = (int) std::lround(gSeed->load());
    return g;
}

void RaveProcessor::setParam(const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); }
}

void RaveProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    PadParams pads[kPads];
    for (int i = 0; i < kPads; ++i) pads[i] = readPad(i);
    engine.setPads(pads);
    engine.setGlobal(readGlobal());

    HostInfo h;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            h.valid = true; h.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { h.ppq = *q; h.hasPpq = true; }
            if (auto b = pos->getBpm()) { h.bpm = *b; h.hasBpm = true; }
        }
    engine.process(buffer, midi, h);

    {   // Vorhoeren (Slicer / Kit-Browser)
        {
            const juce::SpinLock::ScopedTryLockType sl(previewLock);
            if (sl.isLocked() && previewNew) { previewActive = previewPending; previewPos = 0.0; previewNew = false; }
        }
        if (previewActive)
        {
            const auto& ps = *previewActive;
            const double step = ps.rate / sr;
            const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
            const float* l = ps.data.getReadPointer(0);
            const float* r = ps.channels() > 1 ? ps.data.getReadPointer(1) : l;
            for (int i = 0; i < n; ++i)
            {
                if (previewPos >= ps.length() - 1) { previewActive = nullptr; break; }
                const float a = readHermite(l, ps.length(), previewPos) * 0.8f, b = readHermite(r, ps.length(), previewPos) * 0.8f;
                if (nch > 1) { buffer.getWritePointer(0)[i] += a; buffer.getWritePointer(1)[i] += b; }
                else buffer.getWritePointer(0)[i] += 0.5f * (a + b);
                previewPos += step;
            }
        }
    }
    nwa::sanitize(buffer);
    midi.clear();
}

// ------------------------------------------------------------------------------------------------ Samples
bool RaveProcessor::setPadSample(int pad, SamplePtr s)
{
    if (pad < 0 || pad >= kPads) return false;
    const juce::ScopedLock sl(sampleLock);
    if (! engine.postSample(pad, RaveEngine::kSource, s)) return false;
    source[(size_t) pad] = s;
    ++generation[(size_t) pad];
    engine.postSample(pad, RaveEngine::kProcessed, nullptr, 0xFFFFFFFFu);   // alte verarbeitete Fassung verwerfen
    ++editRevision;
    return true;
}

bool RaveProcessor::setPadLayer(int pad, SamplePtr s)
{
    if (pad < 0 || pad >= kPads) return false;
    const juce::ScopedLock sl(sampleLock);
    if (! engine.postSample(pad, RaveEngine::kLayer, s)) return false;
    layerS[(size_t) pad] = s;
    ++editRevision;
    return true;
}

void RaveProcessor::loadDefaultKit()
{
    for (int i = 0; i < kPads; ++i) setPadSample(i, kit::makePad(i));
}

// ------------------------------------------------------------------------------------------------ Slicer / Vorhoeren
void RaveProcessor::previewSample(SamplePtr s)
{
    {
        const juce::SpinLock::ScopedLockType sl(previewLock);
        previewPending = s; previewNew = true;
    }
    if (s) { previewHolder.push_back(s); while (previewHolder.size() > 32) previewHolder.pop_front(); }
}
void RaveProcessor::stopPreview() { previewSample(nullptr); }

void RaveProcessor::previewSlice(int idx)
{
    SamplePtr out;
    {
        const juce::ScopedLock sl(modelLock);
        if (! sliceModel.source || idx < 0 || idx >= sliceModel.numSlices()) return;
        auto b = slicer::extractSlice(*sliceModel.source, sliceModel.startOf(idx), sliceModel.endOf(idx));
        out = makeSample(b, sliceModel.source->rate, "slice");
    }
    previewSample(out);
}

void RaveProcessor::setBreak(SamplePtr s, bool detect)
{
    const juce::ScopedLock sl(modelLock);
    sliceModel.source = s; sliceModel.markers.clear(); arrangement.clear(); sliceLoopSamples = 0;
    if (s) { if (detect) sliceModel.detect(sliceSensitivity); else sliceModel.setMarkers({ 0 }); }
    ++editRevision;
}

bool RaveProcessor::loadBreakFile(const juce::File& f, juce::String* error)
{
    auto s = loadSampleFile(f, error);
    if (! s) return false;
    setBreak(s, true);
    return true;
}

void RaveProcessor::detectSlices(float sensitivity)
{
    const juce::ScopedLock sl(modelLock);
    sliceSensitivity = sensitivity;
    sliceModel.detect(sensitivity);
    arrangement.clear();
    ++editRevision;
}

int RaveProcessor::distributeSlicesToPads(int firstPad)
{
    std::vector<std::pair<int, SamplePtr>> parts;
    {
        const juce::ScopedLock sl(modelLock);
        parts = slicer::slicesToPads(sliceModel, firstPad, sliceModel.source ? sliceModel.source->name : juce::String("Slice"));
    }
    int n = 0;
    for (auto& p : parts)
    {
        // Pad-Fenster auf volle Laenge, Rueckwaerts/Stretch bleiben die Entscheidung des Nutzers
        setParam(padParamId(p.first, pStart), 0.f); setParam(padParamId(p.first, pEnd), 1.f);
        if (setPadSample(p.first, p.second)) ++n;
    }
    return n;
}

void RaveProcessor::slicesToPatternSlot(int slot, int firstPad, int steps)
{
    slicer::SliceModel copy; int loop;
    { const juce::ScopedLock sl(modelLock); copy = sliceModel; loop = sliceLoopSamples; }
    auto p = slicer::slicesToPattern(copy, firstPad, steps, loop);
    bank.store(juce::jlimit(0, kPatternSlots - 1, slot), p);
    ++editRevision;
}

SamplePtr RaveProcessor::renderArranged(const juce::String& name)
{
    const juce::ScopedLock sl(modelLock);
    return slicer::renderArrangement(sliceModel, arrangement, name);
}

// ------------------------------------------------------------------------------------------------ Generator / EVOLVE
int RaveProcessor::stepsPerBarNow() const { return gen::stepsPerBar((int) std::lround(getRaw("sig")), (int) std::lround(getRaw("div"))); }

gen::RoleMap RaveProcessor::roleMap() const
{
    int roles[kPads];
    for (int i = 0; i < kPads; ++i) roles[i] = juce::jlimit(0, (int) gen::kRoles - 1, (int) std::lround(pp[i][pRole]->load()));
    return gen::RoleMap::fromRoles(roles);
}

gen::GenParams RaveProcessor::readGenParams(int lengthSteps) const
{
    gen::GenParams g;
    g.genre = (int) std::lround(getRaw("genre")); g.intensity = getRaw("g_int"); g.complexity = getRaw("g_cplx"); g.density = getRaw("g_dens");
    g.ghost = getRaw("g_ghost"); g.variation = getRaw("g_var"); g.randomness = getRaw("g_rand"); g.seed = (int) std::lround(getRaw("seed"));
    g.stepDiv = (int) std::lround(getRaw("div")); g.sig = (int) std::lround(getRaw("sig")); g.lengthSteps = lengthSteps;
    return g;
}

gen::GenResult RaveProcessor::generatePattern(int slot, bool applyTempoAndSwing)
{
    slot = juce::jlimit(0, kUserPatterns - 1, slot);
    auto res = gen::generate(readGenParams(bank.length(slot)), roleMap());
    bank.store(slot, res.pattern);
    if (applyTempoAndSwing)
    {
        const float t = getRaw("g_tempo"), sw = getRaw("g_swing");
        setParam("bpm", t > 0.5f ? t : (float) res.bpm);
        setParam("swing", sw > 0.001f ? sw : res.swing);
    }
    ++editRevision;
    return res;
}

void RaveProcessor::evolveFrom(int sourceSlot, float amount)
{
    sourceSlot = juce::jlimit(0, kUserPatterns - 1, sourceSlot);
    PlainPattern base; bank.load(sourceSlot, base);
    const int r = ++evolveCounter;
    const auto rm = roleMap();
    const int spb = stepsPerBarNow();
    const int seed = (int) std::lround(getRaw("seed"));
    for (int k = 0; k < kVariants; ++k)
        bank.store(kUserPatterns + k, gen::evolve(base, amount, seed * 131 + r * 17 + k * 7919, rm, spb));
    evolveSource = sourceSlot;
    ++editRevision;
}

bool RaveProcessor::acceptVariant(int v, int dest)
{
    if (v < 0 || v >= kVariants) return false;
    dest = juce::jlimit(0, kUserPatterns - 1, dest);
    PlainPattern p; bank.load(kUserPatterns + v, p);
    bank.store(dest, p);
    ++editRevision;
    return true;
}

void RaveProcessor::discardVariants()
{
    for (int k = 0; k < kVariants; ++k) bank.clear(kUserPatterns + k);
    if (std::lround(getRaw("pattern")) >= kUserPatterns) setParam("pattern", (float) juce::jmax(0, evolveSource.load()));
    evolveSource = -1;
    ++editRevision;
}

void RaveProcessor::copyPattern(int from, int to)
{
    PlainPattern p; bank.load(juce::jlimit(0, kPatternSlots - 1, from), p);
    bank.store(juce::jlimit(0, kUserPatterns - 1, to), p);
    ++editRevision;
}
void RaveProcessor::clearPattern(int slot) { bank.clear(juce::jlimit(0, kUserPatterns - 1, slot)); ++editRevision; }

// ------------------------------------------------------------------------------------------------ Export / Bounce
SamplePtr RaveProcessor::bounce(int pad) const
{
    if (pad < 0 || pad >= kPads || ! source[(size_t) pad]) return nullptr;
    exporter::PadSetup ps; ps.src = source[(size_t) pad]; ps.layer = layerS[(size_t) pad]; ps.params = readPad(pad);
    return exporter::bouncePad(ps, 1.f, sr);
}

bool RaveProcessor::resamplePad(int pad)
{
    auto s = bounce(pad);
    if (! s) return false;
    // Alle klangformenden Einstellungen sind jetzt im Sample enthalten und werden neutralisiert (sonst waeren sie doppelt wirksam)
    for (int p : { (int) pVol, (int) pPan, (int) pPitch, (int) pAtk, (int) pDec, (int) pStart, (int) pEnd, (int) pRev, (int) pLp, (int) pHp, (int) pRes, (int) pDrv,
                   (int) pCrush, (int) pKeep, (int) pStretch, (int) pLayer, (int) pStutter, (int) pGlitch })
    {
        const auto* prm = apvts.getParameter(padParamId(pad, p));
        if (prm) setParam(padParamId(pad, p), prm->convertFrom0to1(prm->getDefaultValue()));
    }
    setPadLayer(pad, nullptr);
    return setPadSample(pad, s);
}

bool RaveProcessor::exportMidi(int slot, const juce::File& f, int loops, bool roll, juce::String* error) const
{
    PlainPattern p; bank.load(juce::jlimit(0, kPatternSlots - 1, slot), p);
    return exporter::writeMidi(p, readGlobal(), (int) std::lround(getRaw("sig")), loops, roll, f, error);
}

bool RaveProcessor::exportPatternWav(int slot, const juce::File& f, int loops, bool wrapTail, double rate, juce::String* error)
{
    PlainPattern p; bank.load(juce::jlimit(0, kPatternSlots - 1, slot), p);
    if (p.hits() == 0) { if (error) *error = "Pattern is empty"; return false; }
    std::array<exporter::PadSetup, kPads> setup;
    for (int i = 0; i < kPads; ++i) { setup[(size_t) i].src = source[(size_t) i]; setup[(size_t) i].layer = layerS[(size_t) i]; setup[(size_t) i].params = readPad(i); }
    auto buf = exporter::renderPattern(p, setup, readGlobal(), loops, rate, wrapTail);
    if (! writeWavFile(buf, rate, f, 24)) { if (error) *error = "Cannot write " + f.getFullPathName(); return false; }
    return true;
}

int RaveProcessor::exportStems(int slot, const juce::File& dir, int loops, bool wrapTail, double rate)
{
    PlainPattern p; bank.load(juce::jlimit(0, kPatternSlots - 1, slot), p);
    std::array<exporter::PadSetup, kPads> setup;
    for (int i = 0; i < kPads; ++i) { setup[(size_t) i].src = source[(size_t) i]; setup[(size_t) i].layer = layerS[(size_t) i]; setup[(size_t) i].params = readPad(i); }
    dir.createDirectory();
    int n = 0;
    for (int i = 0; i < kPads; ++i)
    {
        if (p.hits(i) == 0 || ! source[(size_t) i]) continue;
        auto buf = exporter::renderPattern(p, setup, readGlobal(), loops, rate, wrapTail, i);
        const auto name = nwa::PresetManager::sanitizeName("pad" + juce::String(i + 1).paddedLeft('0', 2) + "_" + source[(size_t) i]->name);
        if (writeWavFile(buf, rate, dir.getChildFile(name + ".wav"), 24)) ++n;
    }
    return n;
}

int RaveProcessor::exportSlices(const juce::File& dir, const juce::String& baseName, juce::String* error)
{
    slicer::SliceModel m;
    { const juce::ScopedLock sl(modelLock); m = sliceModel; }
    if (! m.source) { if (error) *error = "No break loaded"; return 0; }
    if (! dir.createDirectory()) { if (error) *error = "Cannot create " + dir.getFullPathName(); return 0; }
    int n = 0;
    for (int i = 0; i < m.numSlices(); ++i)
    {
        auto b = slicer::extractSlice(*m.source, m.startOf(i), m.endOf(i));
        const auto name = nwa::PresetManager::sanitizeName(baseName) + "_" + juce::String(i + 1).paddedLeft('0', 2) + ".wav";
        if (writeWavFile(b, m.source->rate, dir.getChildFile(name), 24)) ++n;
    }
    return n;
}

bool RaveProcessor::exportPadWav(int pad, const juce::File& f, bool processed, juce::String* error)
{
    auto s = processed ? bounce(pad) : padSample(pad);
    if (! s) { if (error) *error = "Pad is empty"; return false; }
    if (! writeWavFile(s->data, s->rate, f, 24)) { if (error) *error = "Cannot write " + f.getFullPathName(); return false; }
    return true;
}

// ------------------------------------------------------------------------------------------------ Kits
juce::File RaveProcessor::kitsDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEOWAVE_AUDIO").getChildFile("RAVE-STATION").getChildFile("Kits");
}

juce::StringArray RaveProcessor::listKits() const
{
    juce::StringArray r;
    for (const auto& f : kitsDirectory().findChildFiles(juce::File::findDirectories, false))
        if (f.getChildFile("kit.xml").existsAsFile()) r.add(f.getFileName());
    r.sort(true);
    return r;
}

bool RaveProcessor::saveKit(const juce::String& rawName, juce::String* error)
{
    const auto name = nwa::PresetManager::sanitizeName(rawName);
    if (name.isEmpty()) { if (error) *error = "Invalid kit name"; return false; }
    const auto dir = kitsDirectory().getChildFile(name);
    if (! dir.createDirectory()) { if (error) *error = "Cannot create folder " + dir.getFullPathName(); return false; }

    juce::XmlElement root("RAVE_KIT"); root.setAttribute("version", 1);
    for (int pad = 0; pad < kPads; ++pad)
    {
        auto* e = root.createNewChildElement("PAD"); e->setAttribute("index", pad);
        for (int p = 0; p < kPadParamCount; ++p) e->setAttribute(padParamId(pad, p).fromFirstOccurrenceOf("_", false, false), (double) pp[pad][p]->load());
        for (int layer = 0; layer < 2; ++layer)
        {
            const auto sp = layer == 0 ? source[(size_t) pad] : layerS[(size_t) pad];
            if (! sp) continue;
            const juce::String file = juce::String("pad") + juce::String(pad + 1).paddedLeft('0', 2) + (layer ? "_layer.wav" : ".wav");
            if (! writeWavFile(sp->data, sp->rate, dir.getChildFile(file), 24)) { if (error) *error = "Cannot write " + file; return false; }
            e->setAttribute(layer ? "layerFile" : "file", file);
            e->setAttribute(layer ? "layerName" : "name", sp->name);
        }
    }
    juce::TemporaryFile tmp(dir.getChildFile("kit.xml"));
    if (! root.writeTo(tmp.getFile()) || ! tmp.overwriteTargetFileWithTemporary()) { if (error) *error = "Cannot write kit.xml"; return false; }
    return true;
}

bool RaveProcessor::loadKit(const juce::String& rawName, juce::String* error)
{
    const auto dir = kitsDirectory().getChildFile(nwa::PresetManager::sanitizeName(rawName));
    auto xml = juce::XmlDocument::parse(dir.getChildFile("kit.xml"));
    if (! xml || ! xml->hasTagName("RAVE_KIT")) { if (error) *error = "kit.xml missing or invalid"; return false; }
    // Erst alles laden, dann anwenden: ein kaputtes Kit veraendert den aktuellen Zustand nicht.
    struct Loaded { int pad; SamplePtr s, layer; juce::XmlElement* e; };
    std::vector<Loaded> all;
    for (auto* e : xml->getChildWithTagNameIterator("PAD"))
    {
        const int pad = e->getIntAttribute("index", -1);
        if (pad < 0 || pad >= kPads) continue;
        Loaded l { pad, nullptr, nullptr, e };
        if (e->hasAttribute("file")) { l.s = loadSampleFile(dir.getChildFile(e->getStringAttribute("file")), error); if (! l.s) return false; l.s = makeSample(l.s->data, l.s->rate, e->getStringAttribute("name", l.s->name), l.s->sourcePath); }
        if (e->hasAttribute("layerFile")) { l.layer = loadSampleFile(dir.getChildFile(e->getStringAttribute("layerFile")), error); if (! l.layer) return false; }
        all.push_back(l);
    }
    for (auto& l : all)
    {
        for (int p = 0; p < kPadParamCount; ++p)
        {
            const auto key = padParamId(l.pad, p).fromFirstOccurrenceOf("_", false, false);
            if (l.e->hasAttribute(key)) setParam(padParamId(l.pad, p), (float) l.e->getDoubleAttribute(key));
        }
        setPadSample(l.pad, l.s); setPadLayer(l.pad, l.layer);
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ State
juce::ValueTree RaveProcessor::makeExtra() const
{
    juce::ValueTree t("RAVE");
    t.setProperty("version", 1, nullptr);
    for (int s = 0; s < kPatternSlots; ++s)
    {
        juce::ValueTree c("PATTERN"); c.setProperty("slot", s, nullptr); c.setProperty("data", bank.toString(s), nullptr);
        t.appendChild(c, nullptr);
    }
    auto add = [&](const char* type, int pad, const SamplePtr& sp)
    {
        if (! sp) return;
        juce::MemoryBlock mb;
        if (! encodeSample(*sp, mb)) return;
        juce::ValueTree c(type);
        c.setProperty("pad", pad, nullptr); c.setProperty("name", sp->name, nullptr); c.setProperty("path", sp->sourcePath, nullptr);
        c.setProperty("flac", juce::Base64::toBase64(mb.getData(), mb.getSize()), nullptr);
        t.appendChild(c, nullptr);
    };
    for (int i = 0; i < kPads; ++i) { add("SAMPLE", i, source[(size_t) i]); add("LAYER", i, layerS[(size_t) i]); }
    {
        const juce::ScopedLock sl(modelLock);
        if (sliceModel.source)
        {
            juce::MemoryBlock mb;
            if (encodeSample(*sliceModel.source, mb))
            {
                juce::ValueTree c("SLICER");
                juce::StringArray mk; for (int m : sliceModel.markers) mk.add(juce::String(m));
                c.setProperty("name", sliceModel.source->name, nullptr); c.setProperty("path", sliceModel.source->sourcePath, nullptr);
                c.setProperty("markers", mk.joinIntoString(","), nullptr); c.setProperty("loop", sliceLoopSamples, nullptr);
                c.setProperty("sens", sliceSensitivity, nullptr);
                c.setProperty("flac", juce::Base64::toBase64(mb.getData(), mb.getSize()), nullptr);
                juce::StringArray ar; for (auto& o : arrangement) ar.add(juce::String(o.slice) + ":" + (o.reverse ? "1" : "0") + ":" + juce::String(o.semis) + ":" + juce::String(o.stretch) + ":" + juce::String(o.stutter) + ":" + juce::String(o.gain));
                c.setProperty("arrangement", ar.joinIntoString(","), nullptr);
                t.appendChild(c, nullptr);
            }
        }
    }
    return t;
}

void RaveProcessor::applyExtra(const juce::ValueTree& t)
{
    if (! t.isValid() || ! t.hasType("RAVE")) return;
    for (int s = 0; s < kPatternSlots; ++s) bank.clear(s);
    { const juce::ScopedLock sl(modelLock); sliceModel = slicer::SliceModel(); arrangement.clear(); sliceLoopSamples = 0; }
    std::array<bool, kPads> gotSample {}, gotLayer {};
    for (int i = 0; i < t.getNumChildren(); ++i)
    {
        const auto c = t.getChild(i);
        if (c.hasType("PATTERN"))
        {
            const int slot = c.getProperty("slot", -1);
            if (slot >= 0 && slot < kPatternSlots) bank.fromString(slot, c.getProperty("data").toString());
        }
        else if (c.hasType("SLICER"))
        {
            juce::MemoryBlock mb; juce::MemoryOutputStream mos(mb, false);
            if (! juce::Base64::convertFromBase64(mos, c.getProperty("flac").toString())) continue;
            auto s = decodeSample(mb.getData(), mb.getSize(), c.getProperty("name").toString(), c.getProperty("path").toString());
            if (! s) continue;
            const juce::ScopedLock sl(modelLock);
            sliceModel.source = s;
            std::vector<int> mk; for (auto& tok : juce::StringArray::fromTokens(c.getProperty("markers").toString(), ",", "")) mk.push_back(tok.getIntValue());
            sliceModel.setMarkers(mk);
            sliceLoopSamples = c.getProperty("loop", 0); sliceSensitivity = c.getProperty("sens", 0.6f);
            arrangement.clear();
            for (auto& tok : juce::StringArray::fromTokens(c.getProperty("arrangement").toString(), ",", ""))
            {
                auto f = juce::StringArray::fromTokens(tok, ":", ""); if (f.size() != 6) continue;
                slicer::SliceOp o; o.slice = f[0].getIntValue(); o.reverse = f[1] == "1"; o.semis = f[2].getFloatValue(); o.stretch = f[3].getFloatValue(); o.stutter = f[4].getIntValue(); o.gain = f[5].getFloatValue();
                arrangement.push_back(o);
            }
        }
        else if (c.hasType("SAMPLE") || c.hasType("LAYER"))
        {
            const int pad = c.getProperty("pad", -1);
            if (pad < 0 || pad >= kPads) continue;
            juce::MemoryBlock mb;
            juce::MemoryOutputStream mos(mb, false);
            if (! juce::Base64::convertFromBase64(mos, c.getProperty("flac").toString())) continue;
            auto s = decodeSample(mb.getData(), mb.getSize(), c.getProperty("name").toString(), c.getProperty("path").toString());
            if (! s) continue;
            if (c.hasType("SAMPLE")) { setPadSample(pad, s); gotSample[(size_t) pad] = true; }
            else { setPadLayer(pad, s); gotLayer[(size_t) pad] = true; }
        }
    }
    // Pads ohne gespeichertes Sample bleiben leer (der Zustand ist massgeblich, nicht das Standard-Kit)
    for (int i = 0; i < kPads; ++i) { if (! gotSample[(size_t) i]) setPadSample(i, nullptr); if (! gotLayer[(size_t) i]) setPadLayer(i, nullptr); }
    ++editRevision;
}

void RaveProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    nwa::state::save(apvts, makePluginInfo().id, makePluginInfo().stateVersion, dest, makeExtra());
}

void RaveProcessor::setStateInformation(const void* data, int size)
{
    juce::ValueTree extra;
    const auto r = nwa::state::load(apvts, makePluginInfo().id, makePluginInfo().stateVersion, data, size, &extra);
    if (r == nwa::state::Result::ok || r == nwa::state::Result::newerVersion) applyExtra(extra);
}

#if RAVE_WITH_GUI
juce::AudioProcessorEditor* RaveProcessor::createEditor() { return gui::createRaveEditor(*this); }
#else
juce::AudioProcessorEditor* RaveProcessor::createEditor() { return nullptr; }   // Test-Builds ohne GUI
#endif
} // namespace rave

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new rave::RaveProcessor(); }
