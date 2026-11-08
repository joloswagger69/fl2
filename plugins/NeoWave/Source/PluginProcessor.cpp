#include "PluginProcessor.h"
#include "PluginEditor.h"

static const WaveTables& sharedTables()
{
    static const WaveTables t;
    return t;
}

NeoProcessor::NeoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, &undo, "NEOWAVE", createLayout())
{
    for (int i = 0; i < P::kCount; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue(pdef(i).id);
        params[(size_t) i] = apvts.getParameter(pdef(i).id);
    }
    for (auto& c : ccMap) c = -1;
    for (auto& l : visLfo) l = 0.f;

    tables = &sharedTables();
    for (int i = 0; i < 3; ++i) glfo[i].init(4242u + (uint32_t) i * 17u);
    for (int i = 0; i < 16; ++i) voices[(size_t) i].prepare(44100.0, tables, i);
    fx.prepare(44100.0);
    updateSnap();
}

void NeoProcessor::prepareToPlay(double rate, int)
{
    sr = rate;
    for (int i = 0; i < 16; ++i) voices[(size_t) i].prepare(rate, tables, i);
    fx.prepare(rate);
    for (auto& v : dcX) v = 0.f;
    for (auto& v : dcY) v = 0.f;
}

void NeoProcessor::updateSnap()
{
    for (int i = 0; i < P::kCount; ++i) snap.v[i] = raw[(size_t) i]->load();
}

// ------------------------------------------------------------------ MIDI / Stimmen
void NeoProcessor::handleMidi(const juce::MidiMessage& m)
{
    if (m.isNoteOn())            noteOn(m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())      noteOff(m.getNoteNumber());
    else if (m.isPitchWheel())   gm.bend = (float) (m.getPitchWheelValue() - 8192) / 8192.f;
    else if (m.isChannelPressure()) gm.aftertouch = m.getChannelPressureValue() / 127.f;
    else if (m.isAftertouch())   gm.aftertouch = juce::jmax(gm.aftertouch, m.getAfterTouchValue() / 127.f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { for (auto& v : voices) v.kill(); nHeld = 0; }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float val = m.getControllerValue() / 127.f;

        const int learn = learnParam.load();
        if (learn >= 0 && cc != 64)
        {
            ccMap[(size_t) cc] = learn;
            learnParam = -1;
        }
        const int mapped = ccMap[(size_t) cc].load();
        if (mapped >= 0 && mapped < P::kCount)
            params[(size_t) mapped]->setValueNotifyingHost(val);

        if (cc == 1) gm.wheel = val;
        else if (cc == 64) pedal(val >= 0.5f);
    }
}

int NeoProcessor::pickVoice()
{
    const int limit = juce::jlimit(1, 16, snap.ch(P::polyphony));
    for (int i = 0; i < limit; ++i)
        if (!voices[(size_t) i].isActive()) return i;

    const int mode = snap.ch(P::stealMode);
    int best = 0;
    float bestScore = 1e30f;
    for (int i = 0; i < limit; ++i)
    {
        const auto& v = voices[(size_t) i];
        float score;
        switch (mode)
        {
            case 1: score = (float) v.getAge(); break;                    // Oldest
            case 2: score = (float) v.getNote(); break;                   // Lowest
            case 3: score = -(float) v.getNote(); break;                  // Highest
            default: score = (v.isReleasing() ? 0.f : 1e6f) + v.getLevel() * 1000.f + (float) v.getAge() * 1e-3f; // Smart
        }
        if (score < bestScore) { bestScore = score; best = i; }
    }
    return best;
}

void NeoProcessor::noteOn(int note, float vel)
{
    const int mode = snap.ch(P::voiceMode);
    ++stamp;
    if (mode == 0)
    {
        // gleiche Note erneut: dieselbe Stimme wiederverwenden
        int idx = -1;
        for (int i = 0; i < 16; ++i)
            if (voices[(size_t) i].isActive() && voices[(size_t) i].getNote() == note) { idx = i; break; }
        if (idx < 0 || idx >= juce::jlimit(1, 16, snap.ch(P::polyphony))) idx = pickVoice();
        voices[(size_t) idx].noteOn(note, vel, true, lastNote, stamp, snap);
    }
    else
    {
        // Mono / Legato: Note auf den Stapel, Stimme 0 spielt die letzte
        for (int i = 0; i < nHeld; ++i) if (held[i] == note) { for (int j = i; j < nHeld - 1; ++j) held[j] = held[j + 1]; --nHeld; break; }
        if (nHeld < 32) held[nHeld++] = note;
        for (int i = 1; i < 16; ++i) voices[(size_t) i].kill();
        auto& v = voices[0];
        if (mode == 2 && v.isActive() && !v.isReleasing()) v.setNoteLegato(note);
        else v.noteOn(note, vel, true, lastNote, stamp, snap);
    }
    lastNote = (float) note;
}

void NeoProcessor::noteOff(int note)
{
    const int mode = snap.ch(P::voiceMode);
    if (mode == 0)
    {
        for (auto& v : voices)
            if (v.isActive() && v.getNote() == note && !v.isReleasing())
            {
                if (sustain) v.holdBySustain = true;
                else v.noteOff();
            }
    }
    else
    {
        for (int i = 0; i < nHeld; ++i) if (held[i] == note) { for (int j = i; j < nHeld - 1; ++j) held[j] = held[j + 1]; --nHeld; break; }
        auto& v = voices[0];
        if (nHeld > 0)
        {
            if (mode == 2) v.setNoteLegato(held[nHeld - 1]);
            else v.noteOn(held[nHeld - 1], 0.8f, true, lastNote, ++stamp, snap);
            lastNote = (float) held[nHeld - 1];
        }
        else if (sustain) v.holdBySustain = true;
        else v.noteOff();
    }
}

void NeoProcessor::pedal(bool down)
{
    sustain = down;
    if (!down)
        for (auto& v : voices)
            if (v.holdBySustain) { v.holdBySustain = false; v.noteOff(); }
}

void NeoProcessor::panic()
{
    for (auto& v : voices) v.kill();
    nHeld = 0;
    fxResetRequested = true;
}

// ------------------------------------------------------------------ Audio
void NeoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int total = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2) return;

    if (fxResetRequested.exchange(false)) fx.reset();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;

    auto it = midi.begin();
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);
    static const float stepsPerBeat[6] = { 0.f, 1.f, 2.f, 4.f, 6.f, 8.f };

    for (int pos = 0; pos < total; pos += 32)
    {
        const int len = juce::jmin(32, total - pos);
        updateSnap();

        while (it != midi.end() && (*it).samplePosition < pos + len)
        {
            handleMidi((*it).getMessage());
            ++it;
        }

        // globale Modulation
        for (int i = 0; i < 3; ++i)
        {
            const int base = P::lfo1Rate + i * 3;
            gm.lfo[i] = glfo[i].tick(snap[base], snap.ch(base + 1), len, (float) sr);
            visLfo[i] = gm.lfo[i];
        }
        {
            const int sync = snap.ch(P::stepSync);
            const float rate = sync == 0 ? snap[P::stepRate] : (float) (bpm / 60.0) * stepsPerBeat[sync];
            const int stepLen = juce::jlimit(2, 16, snap.ch(P::stepLen));
            stepPh += rate * (float) len / (float) sr;
            if (stepPh >= (float) stepLen) stepPh = std::fmod(stepPh, (float) stepLen);
            const float target = snap[P::step1 + juce::jmin(15, (int) stepPh)];
            const float c = 1.f - std::exp(-(float) len / ((float) sr * (0.0005f + 0.15f * snap[P::stepSmooth])));
            stepVal += (target - stepVal) * c;
            gm.step = stepVal;
            visStep = stepVal;
            visStepIdx = juce::jmin(15, (int) stepPh);
        }
        for (int i = 0; i < 4; ++i) gm.macro[i] = snap[P::macro1 + i];

        float L[32] = {}, R[32] = {};
        int nActive = 0;
        for (auto& v : voices)
        {
            if (v.isActive()) { v.render(L, R, len, snap, gm); ++nActive; }
        }
        activeVoices = nActive;

        float* io[2] = { L, R };
        fx.process(io, len, snap);

        const float vol = snap[P::master] * 2.0f;
        int wp = scopePos.load();
        for (int i = 0; i < len; ++i)
        {
            float s[2] = { L[i] * vol, R[i] * vol };
            for (int ch = 0; ch < 2; ++ch)
            {
                // DC-Blocker
                const float y = s[ch] - dcX[ch] + 0.995f * dcY[ch];
                dcX[ch] = s[ch]; dcY[ch] = y;
                float a = std::abs(y);
                // sanfter Limiter ab 0.9
                float o = y;
                if (a > 0.9f) o = (y > 0.f ? 1.f : -1.f) * (0.9f + 0.1f * nw::fastTanh((a - 0.9f) * 10.f));
                s[ch] = o;
            }
            outL[pos + i] = s[0];
            outR[pos + i] = s[1];
            peak = juce::jmax(peak * 0.9995f, std::abs(s[0]), std::abs(s[1]));
            scope[(size_t) (wp & (kScope - 1))] = 0.5f * (s[0] + s[1]);
            ++wp;
        }
        scopePos = wp;
        level = peak;
    }
    for (int i = 0; i < total; ++i)
        if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) { outL[i] = outR[i] = 0.f; ++nanEvents; fxResetRequested = true; }
}

// ------------------------------------------------------------------ Parameter-Helfer
void NeoProcessor::setParamReal(int i, float v)
{
    auto* p = params[(size_t) i];
    p->setValueNotifyingHost(p->convertTo0to1(v));
}
float NeoProcessor::getParamReal(int i) const
{
    auto* p = params[(size_t) i];
    return p->convertFrom0to1(p->getValue());
}

void NeoProcessor::setAllToDefault(bool keepGlobals)
{
    for (int i = 0; i < P::kCount; ++i)
    {
        if (keepGlobals && (i == P::bendRange || i == P::polyphony || i == P::stealMode)) continue;
        setParamReal(i, pdef(i).def);
    }
}

// ------------------------------------------------------------------ Presets
const juce::String NeoProcessor::getProgramName(int i)
{
    const auto& lib = presetLibrary();
    return (i >= 0 && i < (int) lib.size()) ? juce::String(lib[(size_t) i].name) : juce::String();
}

void NeoProcessor::applyValues(const std::vector<std::pair<int, float>>& v, float lvl)
{
    undo.beginNewTransaction("Preset");
    setAllToDefault(true);
    for (auto& [id, val] : v) setParamReal(id, val);
    setParamReal(P::master, lvl);
}

void NeoProcessor::loadFactoryPreset(int index)
{
    const auto& lib = presetLibrary();
    if (index < 0 || index >= (int) lib.size()) return;
    currentProgram = index;
    const auto& p = lib[(size_t) index];
    presetName = p.name;
    presetCategory = p.category;
    applyValues(p.v, p.level);
}

bool NeoProcessor::saveUserPreset(const juce::String& name, const juce::String& category)
{
    if (name.trim().isEmpty()) return false;
    juce::XmlElement xml("NEOWAVE_PRESET");
    xml.setAttribute("name", name);
    xml.setAttribute("category", category.isEmpty() ? "User" : category);
    for (int i = 0; i < P::kCount; ++i)
    {
        auto* e = xml.createNewChildElement("PARAM");
        e->setAttribute("id", pdef(i).id);
        e->setAttribute("value", (double) getParamReal(i));
    }
    auto f = PresetStore::userDir().getChildFile(juce::File::createLegalFileName(name) + ".nwpreset");
    if (!xml.writeTo(f)) return false;
    presetName = name;
    presetCategory = category.isEmpty() ? "User" : category;
    return true;
}

bool NeoProcessor::loadUserPreset(const juce::File& f)
{
    auto xml = juce::XmlDocument::parse(f);
    if (xml == nullptr || !xml->hasTagName("NEOWAVE_PRESET")) return false;
    undo.beginNewTransaction("Preset");
    setAllToDefault(true);
    for (auto* e : xml->getChildWithTagNameIterator("PARAM"))
    {
        const int idx = paramIndexByName(e->getStringAttribute("id"));
        if (idx >= 0 && idx != P::bendRange && idx != P::polyphony && idx != P::stealMode)
            setParamReal(idx, (float) e->getDoubleAttribute("value", pdef(idx).def));
    }
    presetName = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
    presetCategory = xml->getStringAttribute("category", "User");
    return true;
}

void NeoProcessor::toggleAB()
{
    abSlot[abActive] = apvts.copyState();
    abActive = 1 - abActive;
    if (abSlot[abActive].isValid()) apvts.replaceState(abSlot[abActive]);
    else abSlot[abActive] = apvts.copyState();
}

// ------------------------------------------------------------------ Randomize
void NeoProcessor::randomize(bool smart)
{
    juce::Random r;
    undo.beginNewTransaction(smart ? "Smart Randomize" : "Randomize");
    auto rf = [&](float a, float b) { return a + (b - a) * r.nextFloat(); };
    auto ri = [&](int a, int b) { return a + r.nextInt(b - a + 1); };
    auto chance = [&](float p) { return r.nextFloat() < p; };
    auto set = [&](int id, float v) { setParamReal(id, v); };
    auto pick = [&](std::initializer_list<int> l) { return *(l.begin() + r.nextInt((int) l.size())); };

    auto isGlobal = [](int i) { return i == P::bendRange || i == P::polyphony || i == P::stealMode
                                     || i == P::master || i == P::voiceMode || i == P::glide
                                     || (i >= P::macro1 && i <= P::macro4); };

    if (!smart)
    {
        for (int i = 0; i < P::kCount; ++i)
        {
            if (isGlobal(i)) continue;
            const auto& d = pdef(i);
            float v = rf(d.lo, d.hi);
            if (d.kind == 'C') v = (float) ri(0, juce::StringArray::fromTokens(d.choices, "|", "").size() - 1);
            if (d.kind == 'I') v = (float) ri((int) d.lo, (int) d.hi);
            if (i == P::fCut) v = 200.f * std::pow(80.f, r.nextFloat());
            if (i == P::ampA || i == P::fltA) v = rf(0.001f, 0.4f);
            if (i == P::satMix || i == P::dlMix || i == P::rvMix || i == P::mfMix) v = rf(0.f, 0.5f);
            if (i == P::fsMix) v = 0.f;
            if (i == P::uniVoices) v = (float) ri(1, 8);
            set(i, v);
        }
        return;
    }

    setAllToDefault(true);
    const int arche = ri(0, 4); // 0 Bass, 1 Lead, 2 Pad, 3 Pluck, 4 Textur

    auto engine = [&]() { return pick({ 0, 0, 0, 1, 1, 1, 2, 3, 4 }); };
    set(P::aEngine, (float) (arche == 4 ? pick({ 1, 4, 2, 3 }) : engine()));
    set(P::bEngine, (float) (chance(0.5f) ? (int) getParamReal(P::aEngine) : engine()));
    set(P::aWave, (float) ri(0, 3));  set(P::bWave, (float) ri(0, 3));
    set(P::aTable, (float) ri(0, 7)); set(P::bTable, (float) ri(0, 7));
    set(P::aMorph, rf(0.f, 0.8f));    set(P::bMorph, rf(0.f, 0.8f));
    set(P::aColor, rf(0.f, 0.5f));    set(P::bColor, rf(0.f, 0.5f));
    set(P::aOct, (float) (arche == 0 ? -1 : pick({ 0, 0, 0, 1 })));
    set(P::bOct, (float) pick({ -1, 0, 0, 0, 1 }));
    set(P::aLevel, rf(0.6f, 0.9f));
    set(P::bLevel, chance(0.25f) ? 0.f : rf(0.25f, 0.8f));
    set(P::bFine, rf(-14.f, 14.f));
    set(P::ringMod, chance(0.12f) ? rf(0.1f, 0.5f) : 0.f);
    set(P::crossFm, chance(0.15f) ? rf(0.05f, 0.4f) : 0.f);
    set(P::bSync, chance(0.1f) ? 1.f : 0.f);
    set(P::subLevel, arche == 0 ? rf(0.3f, 0.8f) : (chance(0.2f) ? rf(0.1f, 0.4f) : 0.f));
    set(P::noiseLevel, arche == 4 ? rf(0.05f, 0.35f) : (chance(0.1f) ? rf(0.02f, 0.1f) : 0.f));
    set(P::noiseType, (float) ri(0, 3));

    const int uni[5][2] = { { 1, 3 }, { 3, 9 }, { 4, 12 }, { 1, 5 }, { 1, 4 } };
    set(P::uniVoices, (float) ri(uni[arche][0], uni[arche][1]));
    set(P::uniDetune, rf(0.1f, 0.45f));
    set(P::uniSpread, rf(0.4f, 1.f));

    const int ft = arche == 0 ? pick({ 1, 5, 6, 9, 5 }) : pick({ 0, 1, 1, 5, 5, 3, 7, 9 });
    set(P::fType, (float) ft);
    const float cutLo[5] = { 150.f, 1500.f, 800.f, 500.f, 400.f }, cutHi[5] = { 1500.f, 12000.f, 6000.f, 5000.f, 9000.f };
    set(P::fCut, cutLo[arche] * std::pow(cutHi[arche] / cutLo[arche], r.nextFloat()));
    set(P::fRes, rf(0.05f, ft == 6 ? 0.7f : 0.5f));
    set(P::fDrive, rf(0.f, 0.4f));
    set(P::fEnv, arche == 3 ? rf(0.4f, 0.9f) : rf(-0.2f, 0.6f));

    switch (arche)
    {
        case 0: set(P::ampA, 0.003f); set(P::ampS, rf(0.6f, 1.f)); set(P::ampR, rf(0.08f, 0.3f)); set(P::ampD, rf(0.2f, 0.8f)); break;
        case 1: set(P::ampA, rf(0.002f, 0.03f)); set(P::ampS, rf(0.7f, 1.f)); set(P::ampR, rf(0.15f, 0.6f)); break;
        case 2: set(P::ampA, rf(0.3f, 2.f)); set(P::ampD, rf(0.5f, 2.f)); set(P::ampS, rf(0.6f, 0.95f)); set(P::ampR, rf(0.8f, 3.5f));
                set(P::fltA, rf(0.2f, 1.5f)); break;
        case 3: set(P::ampA, 0.002f); set(P::ampD, rf(0.12f, 0.6f)); set(P::ampS, 0.f); set(P::ampR, rf(0.1f, 0.4f));
                set(P::fltD, rf(0.1f, 0.35f)); set(P::fltS, 0.f); break;
        default: set(P::ampA, rf(0.05f, 1.5f)); set(P::ampS, rf(0.5f, 1.f)); set(P::ampR, rf(0.5f, 3.f)); break;
    }

    set(P::lfo1Rate, rf(0.1f, 6.f)); set(P::lfo1Shape, (float) ri(0, 5));
    set(P::lfo2Rate, rf(0.05f, 2.f)); set(P::lfo2Shape, (float) pick({ 0, 1, 5 }));
    set(P::lfo3Rate, rf(2.f, 10.f)); set(P::lfo3Shape, (float) ri(0, 5));
    auto slot = [&](int s, int src, int dst, float amt) {
        set(P::mmSrc1 + s * 3, (float) src); set(P::mmDst1 + s * 3, (float) dst); set(P::mmAmt1 + s * 3, amt); };
    auto sgn = [&]() { return chance(0.5f) ? 1.f : -1.f; };
    slot(0, sLfo1, dAMorph, sgn() * rf(0.15f, 0.5f));
    slot(1, sLfo2, dCutoff, sgn() * rf(0.05f, 0.3f));
    if (chance(0.7f))
    {
        const int dst = pick({ dBMorph, dAColor, dPitch });
        slot(2, sModEnv, dst, rf(0.1f, 0.5f) * (dst == dPitch ? 0.1f : 1.f));
    }
    if (chance(0.5f)) slot(3, sVel, dCutoff, rf(0.1f, 0.3f));
    if (chance(0.35f)) slot(4, sRand, dUniDet, rf(0.1f, 0.4f));

    if (chance(0.4f)) { set(P::satType, (float) pick({ 1, 2, 3 })); set(P::satAmt, rf(0.1f, 0.4f)); }
    if (chance(0.5f)) { set(P::mfType, (float) ri(1, 4)); set(P::mfMix, rf(0.2f, 0.5f)); set(P::mfRate, rf(0.1f, 1.5f)); }
    if (chance(0.5f)) { set(P::dlType, (float) ri(1, 3)); set(P::dlMix, rf(0.1f, 0.3f)); set(P::dlFb, rf(0.3f, 0.55f));
                        set(P::dlTime, (float) pick({ 0, 1, 2, 3 }) * 0.125f + 0.1875f); }
    if (chance(0.7f)) { set(P::rvType, chance(0.2f) ? 2.f : 1.f); set(P::rvMix, rf(0.1f, 0.35f)); set(P::rvSize, rf(0.3f, 0.8f)); }
    set(P::width, rf(0.9f, 1.4f));
}

// ------------------------------------------------------------------ State
void NeoProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty("presetName", presetName, nullptr);
    state.setProperty("presetCategory", presetCategory, nullptr);
    state.setProperty("program", currentProgram, nullptr);
    juce::String map;
    for (int cc = 0; cc < 128; ++cc)
        if (ccMap[(size_t) cc] >= 0) map << cc << ":" << ccMap[(size_t) cc].load() << ",";
    state.setProperty("midiMap", map, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void NeoProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(tree);
            presetName = tree.getProperty("presetName", "Init").toString();
            presetCategory = tree.getProperty("presetCategory", "Init").toString();
            currentProgram = juce::jlimit(0, juce::jmax(0, getNumPrograms() - 1), (int) tree.getProperty("program", 0));
            clearMidiMap();
            for (auto& tok : juce::StringArray::fromTokens(tree.getProperty("midiMap").toString(), ",", ""))
            {
                const int c = tok.upToFirstOccurrenceOf(":", false, false).getIntValue();
                const int p = tok.fromFirstOccurrenceOf(":", false, false).getIntValue();
                if (tok.contains(":") && c >= 0 && c < 128 && p >= 0 && p < P::kCount) ccMap[(size_t) c] = p;
            }
        }
}

juce::AudioProcessorEditor* NeoProcessor::createEditor() { return new NeoEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NeoProcessor(); }
