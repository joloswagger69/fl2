#include "Engine.h"
#include <algorithm>

namespace rave
{
namespace
{
inline long long floorDiv(double x) { return (long long) std::floor(x); }
inline int floorMod(long long a, int m) { const long long r = a % m; return (int) (r < 0 ? r + m : r); }
inline double stepBeatsFor(int div) { return div == 0 ? 0.5 : (div == 1 ? 0.25 : 0.125); }
}

RaveEngine::RaveEngine(PatternBank& b) : bank(b)
{
    for (auto& h : padHits) h = 0;
}

void RaveEngine::prepare(double sampleRate, int maxBlockSize)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    maxBlock = maxBlockSize;
    reset();
}

void RaveEngine::reset()
{
    for (auto& v : voices) if (v.active) killVoice(v);
    wasRunning = false;
    cur = Cur(); prev = Cur(); switchStep = 0; lastStepSeen = -1000000;
    masterSmoothed = global.master;
    uiStep = -1;
}

// ---------------------------------------------------------------------------------------------- Mailbox
bool RaveEngine::postSample(int pad, SlotType type, SamplePtr s, uint32_t key)
{
    if (pad < 0 || pad >= kPads) return false;
    const juce::ScopedLock sl(postLock);
    int s1, n1, s2, n2;
    inFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 < 1) return false;
    inBuf[(size_t) s1].pad = pad; inBuf[(size_t) s1].type = (int) type; inBuf[(size_t) s1].key = key; inBuf[(size_t) s1].s = std::move(s);
    inFifo.finishedWrite(1);
    return true;
}

void RaveEngine::auditionPad(int pad, float vel)
{
    if (pad < 0 || pad >= kPads) return;
    const juce::ScopedLock sl(audLock);
    int s1, n1, s2, n2;
    audFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 < 1) return;
    audBuf[(size_t) s1] = { pad, juce::jlimit(0.f, 1.f, vel) };
    audFifo.finishedWrite(1);
}

void RaveEngine::drainTrash()
{
    const juce::ScopedLock sl(trashLock);
    int s1, n1, s2, n2;
    trashFifo.prepareToRead(trashFifo.getNumReady(), s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) trashBuf[(size_t) (s1 + i)].s.reset();
    for (int i = 0; i < n2; ++i) trashBuf[(size_t) (s2 + i)].s.reset();
    trashFifo.finishedRead(n1 + n2);
}

void RaveEngine::releaseSample(SamplePtr&& s)
{
    if (! s) return;
    int s1, n1, s2, n2;
    trashFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0) { trashBuf[(size_t) s1].s = std::move(s); trashFifo.finishedWrite(1); }
    else s.reset();   // Notfall (Papierkorb voll): selten, nur dann im Audio-Thread freigeben
}

void RaveEngine::killVoice(Voice& v)
{
    v.active = false; v.dying = false;
    releaseSample(std::move(v.a)); releaseSample(std::move(v.b));
    v.a.reset(); v.b.reset();
}

bool RaveEngine::padAudible(int pad) const
{
    const auto& p = pads[pad];
    if (p.mute) return false;
    return ! anySolo || p.solo;
}

// ---------------------------------------------------------------------------------------------- Sequencer
void RaveEngine::pushEvent(int& count, const Event& e)
{
    if (count >= kMaxEvents) return;
    events[(size_t) count] = e;
    events[(size_t) count].order = count;
    ++count;
}

void RaveEngine::scheduleSequencer(const HostInfo& host, int n, int& count)
{
    bool running; double beat0, bpm;
    if (global.hostSync)
    {
        running = host.valid && host.playing && host.hasPpq;
        beat0 = host.ppq;
        bpm = host.hasBpm ? host.bpm : global.bpm;
    }
    else
    {
        running = global.playing;
        if (running && ! wasRunning) internalBeat = 0.0;
        beat0 = internalBeat;
        bpm = global.bpm;
    }
    bpm = juce::jlimit(20.0, 400.0, bpm);
    bpmNow = bpm;
    const double stepBeats = stepBeatsFor(global.stepDiv);
    stepBeatsNow = stepBeats;

    if (! running) { wasRunning = false; uiStep = -1; return; }

    const double bps = bpm / 60.0 / sr;
    const double beat1 = beat0 + n * bps;
    const double eps = 1.0e-7;

    const bool jump = ! wasRunning || std::abs(beat0 - lastEndBeat) > 0.02;
    if (jump)
    {
        cur = { juce::jlimit(0, kPatternSlots - 1, global.slot), 0 };
        prev = cur;
        const long long k0 = floorDiv(beat0 / stepBeats - eps);
        switchStep = k0 - 1;
        lastStepSeen = k0 - 1;
    }
    wasRunning = true;
    lastEndBeat = beat1;
    if (! global.hostSync) internalBeat = beat1;

    // 1) Pattern-Grenzen (Wechsel nur am Pattern-Ende / -Anfang)
    const long long kLast = floorDiv((beat1 - eps) / stepBeats);
    for (long long k = lastStepSeen + 1; k <= kLast; ++k)
    {
        if (k * stepBeats >= beat1 - eps) break;
        const int L = bank.length(cur.slot);
        if (floorMod(k - cur.startStep, L) == 0 && cur.slot != global.slot)
        {
            prev = cur;
            cur = { juce::jlimit(0, kPatternSlots - 1, global.slot), k };
            switchStep = k;
        }
        lastStepSeen = k;
    }
    {
        const long long ku = floorDiv(beat0 / stepBeats + eps);
        uiStep = floorMod(ku - cur.startStep, bank.length(cur.slot));
        uiSlot = cur.slot;
    }

    // 2) Ereignisse
    const double swingOff = juce::jlimit(0.f, 1.f, global.swing) * 0.5 * stepBeats;
    const double humMax = juce::jlimit(0.f, 1.f, global.humTime) * 0.4 * stepBeats;
    const double lookBehind = swingOff + humMax + stepBeats;
    const long long kStart = floorDiv((beat0 - lookBehind) / stepBeats);
    const long long kEnd = floorDiv(beat1 / stepBeats);

    for (long long k = kStart; k <= kEnd; ++k)
    {
        const Cur& c = (k >= switchStep) ? cur : prev;
        const int L = bank.length(c.slot);
        const int pos = floorMod(k - c.startStep, L);
        const double base = k * stepBeats + ((k & 1) ? swingOff : 0.0);

        for (int pad = 0; pad < kPads; ++pad)
        {
            const Step st = bank.get(c.slot, pad, pos);
            if (! st.on() || ! padAudible(pad)) continue;
            if (st.prob < 100 && hash01((uint64_t) k, (uint64_t) pad, (uint64_t) global.seed * 31u + 7u) * 100.f >= (float) st.prob) continue;

            const double hd = hash01((uint64_t) k, (uint64_t) pad, (uint64_t) global.seed) * humMax;
            float vel = (float) st.vel / 127.f;
            const bool ghost = (st.flags & kGhost) != 0, roll = (st.flags & kRoll) != 0;
            if (ghost) vel *= 0.45f;
            if (global.humVel > 0.f) vel *= 1.f + (hash01((uint64_t) k, (uint64_t) pad, (uint64_t) global.seed + 99u) - 0.5f) * 2.f * global.humVel * 0.35f;
            vel = juce::jlimit(0.01f, 1.f, vel);

            const int r = juce::jlimit(1, 8, (int) st.ratchet);
            for (int j = 0; j < r; ++j)
            {
                const double t = base + hd + j * stepBeats / r;
                if (t < beat0 - eps || t >= beat1 - eps) continue;
                float v = vel;
                if (r > 1) v *= roll ? (0.45f + 0.55f * (float) j / (float) (r - 1)) : (1.f - 0.07f * (float) j);
                Event e;
                e.offset = juce::jlimit(0, n - 1, (int) std::lround((t - beat0) / bps));
                e.pad = pad; e.vel = juce::jlimit(0.01f, 1.f, v); e.pitch = st.pitch; e.ghost = ghost;
                e.ratchetCount = r; e.lifeBeats = r > 1 ? stepBeats / r * 1.25 : 0.0; e.sub = j;
                pushEvent(count, e);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- Stimmen
void RaveEngine::trigger(const Event& e)
{
    if (! padAudible(e.pad)) return;
    const auto& pp = pads[e.pad];
    auto& sl = slots[e.pad];

    SamplePtr src = (sl.proc && needsProcessing(pp) && sl.procKey == processedKey(pp)) ? sl.proc : sl.src;
    if (! src || src->length() < 2) return;
    const bool baked = (src == sl.proc) && pp.keep;

    if (pp.choke > 0)
        for (auto& o : voices)
            if (o.active && ! o.dying && pads[o.pad].choke == pp.choke) { o.dying = true; o.dieDec = 1.f / (float) (0.002 * sr); }

    Voice* v = nullptr;
    for (auto& o : voices) if (! o.active) { v = &o; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& o : voices) if (o.age < v->age) v = &o;
        killVoice(*v);
    }

    // Glitch: reproduzierbar ueber Hash
    bool rev = pp.rev; int stut = pp.stutter; float extraSemis = 0.f;
    if (pp.glitch > 0.f && hash01(ageCounter + 1, (uint64_t) e.pad, (uint64_t) global.seed + 5u) < pp.glitch)
    {
        switch ((int) (hash01(ageCounter + 1, (uint64_t) e.pad, (uint64_t) global.seed + 6u) * 3.f))
        {
            case 0:  rev = ! rev; break;
            case 1:  extraSemis = -12.f; break;
            default: stut = juce::jmax(stut, 3); break;
        }
    }

    const float semis = (baked ? 0.f : pp.pitch) + (float) e.pitch + extraSemis;
    const double lenA = src->length();
    double sA = juce::jlimit(0.f, 1.f, pp.start) * lenA, eA = juce::jlimit(0.f, 1.f, pp.end) * lenA;
    if (eA - sA < 16.0) eA = juce::jmin(lenA, sA + 16.0);
    if (eA - sA < 2.0) return;

    *v = Voice();
    v->active = true; v->pad = e.pad; v->age = ++ageCounter;
    v->a = src;
    v->rev = rev;
    v->startA = sA; v->endA = eA;
    v->incA = src->rate / sr * std::pow(2.0, semis / 12.0) * (rev ? -1.0 : 1.0);
    v->posA = rev ? eA - 1.0 : sA;

    if (pp.layer > 0.001f && sl.layer && sl.layer->length() > 2)
    {
        v->b = sl.layer;
        const double lenB = sl.layer->length();
        v->startB = juce::jlimit(0.f, 1.f, pp.start) * lenB; v->endB = juce::jlimit(0.f, 1.f, pp.end) * lenB;
        if (v->endB - v->startB < 16.0) v->endB = juce::jmin(lenB, v->startB + 16.0);
        v->incB = sl.layer->rate / sr * std::pow(2.0, (semis + (baked ? pp.pitch : 0.f)) / 12.0) * (rev ? -1.0 : 1.0);
        v->posB = rev ? v->endB - 1.0 : v->startB;
        v->layerGain = pp.layer;
    }

    const float g = pp.gain * e.vel;
    v->gl = g * juce::jmin(1.f, 1.f - pp.pan);
    v->gr = g * juce::jmin(1.f, 1.f + pp.pan);
    v->atkGain = 0.f; v->atkInc = 1.f / (float) (juce::jmax(pp.atk, 0.0005f) * sr);
    if (pp.dec < 9.99f) { v->decOn = 1.f; v->decMul = std::exp(-6.9078f / (juce::jmax(pp.dec, 0.003f) * (float) sr)); }
    v->fadeIn = 0.f; v->fadeInInc = 1.f / (float) (0.0005 * sr);
    v->dieGain = 1.f; v->dieDec = 1.f / (float) (0.003 * sr);

    const double secPerBeat = 60.0 / bpmNow;
    if (e.lifeBeats > 0.0) v->life = (long long) (e.lifeBeats * secPerBeat * sr);
    if (stut > 0)
    {
        const double loopSec = stutterBeats(stut) * secPerBeat;
        v->loopSrcA = loopSec * sr * std::abs(v->incA);
        v->loopSrcB = v->b ? loopSec * sr * std::abs(v->incB) : 0.0;
        if (v->loopSrcA < 8.0) v->loopSrcA = 0.0;
        if (v->life < 0) v->life = (long long) (stepBeatsNow * secPerBeat * sr);
    }
    v->loopFade = 1.f;

    v->fx = pp.lp < 19000.f || pp.hp > 25.f || pp.drv > 0.f || pp.crush > 0.f;
    v->gLP = Svf::coefG(pp.lp, (float) sr); v->gHP = Svf::coefG(pp.hp, (float) sr);
    v->drive = pp.drv; v->crush = pp.crush; v->res = pp.res;

    padHits[(size_t) e.pad].fetch_add(1, std::memory_order_relaxed);
}

void RaveEngine::renderSegment(float* L, float* R, int start, int end)
{
    const int len = end - start;
    if (len <= 0) return;
    const float fs = (float) sr;
    const float fadeOutSrc = 0.0015f * fs;

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const auto& pp = pads[v.pad];
        const bool fxNow = pp.lp < 19000.f || pp.hp > 25.f || pp.drv > 0.f || pp.crush > 0.f;
        if (fxNow && ! v.fx) { v.lpL.reset(); v.lpR.reset(); v.hpL.reset(); v.hpR.reset(); v.cL.reset(); v.cR.reset(); v.gLP = Svf::coefG(pp.lp, fs); v.gHP = Svf::coefG(pp.hp, fs); }
        v.fx = fxNow;
        v.drive = pp.drv; v.crush = pp.crush; v.res = pp.res;
        const float tLP = Svf::coefG(pp.lp, fs), tHP = Svf::coefG(pp.hp, fs);
        const float dLP = (tLP - v.gLP) / (float) len, dHP = (tHP - v.gHP) / (float) len;

        const float* a0 = v.a->data.getReadPointer(0);
        const float* a1 = v.a->channels() > 1 ? v.a->data.getReadPointer(1) : a0;
        const int na = v.a->length();
        const float* b0 = nullptr; const float* b1 = nullptr; int nb = 0;
        if (v.b) { b0 = v.b->data.getReadPointer(0); b1 = v.b->channels() > 1 ? v.b->data.getReadPointer(1) : b0; nb = v.b->length(); }

        bool finished = false;
        for (int i = 0; i < len; ++i)
        {
            // Ende der Quelle erreicht?
            const bool doneA = v.rev ? (v.posA <= v.startA) : (v.posA >= v.endA);
            const bool doneB = ! v.b || (v.rev ? (v.posB <= v.startB) : (v.posB >= v.endB));
            if (doneA && doneB && v.loopSrcA <= 0.0) { finished = true; break; }

            // Stutter-Schleife
            if (v.loopSrcA > 0.0)
            {
                if (! v.rev && v.posA >= v.startA + v.loopSrcA) { v.posA = v.startA; v.posB = v.startB; v.loopFade = 0.f; }
                else if (v.rev && v.posA <= v.endA - 1.0 - v.loopSrcA) { v.posA = v.endA - 1.0; v.posB = v.endB - 1.0; v.loopFade = 0.f; }
            }

            float l = 0.f, r = 0.f;
            if (! doneA)
            {
                const double dist = v.rev ? v.posA - v.startA : v.endA - v.posA;
                const float fo = (float) juce::jmin(1.0, dist / (fadeOutSrc * std::abs(v.incA)));
                l = readHermite(a0, na, v.posA) * fo; r = readHermite(a1, na, v.posA) * fo;
            }
            if (v.b && ! doneB)
            {
                const double dist = v.rev ? v.posB - v.startB : v.endB - v.posB;
                const float fo = (float) juce::jmin(1.0, dist / (fadeOutSrc * std::abs(v.incB)));
                l += readHermite(b0, nb, v.posB) * fo * v.layerGain; r += readHermite(b1, nb, v.posB) * fo * v.layerGain;
            }

            v.atkGain = juce::jmin(1.f, v.atkGain + v.atkInc);
            v.fadeIn = juce::jmin(1.f, v.fadeIn + v.fadeInInc);
            v.loopFade = juce::jmin(1.f, v.loopFade + v.fadeInInc);
            if (v.decOn > 0.f) v.amp *= v.decMul;
            if (v.dying) { v.dieGain -= v.dieDec; if (v.dieGain <= 0.f) { finished = true; break; } }
            if (v.life >= 0 && --v.life < 0 && ! v.dying) { v.dying = true; }

            const float env = v.atkGain * v.amp * v.fadeIn * v.loopFade * v.dieGain;
            l *= env; r *= env;

            if (v.fx)
            {
                if (v.crush > 0.f) { l = v.cL.process(l, v.crush); r = v.cR.process(r, v.crush); }
                if (v.drive > 0.f) { l = softSat(l, v.drive); r = softSat(r, v.drive); }
                v.gLP += dLP; v.gHP += dHP;
                float lp, hp;
                v.hpL.process(l, v.gHP, 0.f, lp, hp); l = hp;
                v.hpR.process(r, v.gHP, 0.f, lp, hp); r = hp;
                v.lpL.process(l, v.gLP, v.res, lp, hp); l = lp;
                v.lpR.process(r, v.gLP, v.res, lp, hp); r = lp;
            }

            L[start + i] += l * v.gl;
            R[start + i] += r * v.gr;

            v.posA += v.incA;
            if (v.b) v.posB += v.incB;

            if (v.decOn > 0.f && v.amp < 1.0e-4f) { finished = true; break; }
        }
        if (finished) killVoice(v);
    }
}

// ---------------------------------------------------------------------------------------------- Block
void RaveEngine::process(juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const HostInfo& host)
{
    const int n = out.getNumSamples();
    const int nch = out.getNumChannels();
    out.clear();
    if (n <= 0 || nch <= 0) return;

    for (int i = 0; i < kPads; ++i) pads[i] = padsPending[i];
    anySolo = false;
    for (auto& p : pads) anySolo |= p.solo;

    // Mailbox: neue Samples uebernehmen, alte in den Papierkorb
    {
        int s1, n1, s2, n2;
        inFifo.prepareToRead(inFifo.getNumReady(), s1, n1, s2, n2);
        auto apply = [&](Msg& m) {
            auto& sl = slots[m.pad];
            SamplePtr* dst = m.type == kSource ? &sl.src : (m.type == kProcessed ? &sl.proc : &sl.layer);
            releaseSample(std::move(*dst));
            *dst = std::move(m.s);
            if (m.type == kProcessed) sl.procKey = m.key;
        };
        for (int i = 0; i < n1; ++i) apply(inBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) apply(inBuf[(size_t) (s2 + i)]);
        inFifo.finishedRead(n1 + n2);
    }

    bpmNow = juce::jlimit(20.0, 400.0, (global.hostSync && host.valid && host.hasBpm) ? host.bpm : global.bpm);
    stepBeatsNow = stepBeatsFor(global.stepDiv);

    int count = 0;
    {   // GUI-Vorhoeren
        int s1, n1, s2, n2;
        audFifo.prepareToRead(audFifo.getNumReady(), s1, n1, s2, n2);
        auto push = [&](const Aud& a) { Event e; e.offset = 0; e.pad = a.pad; e.vel = juce::jmax(0.05f, a.vel); pushEvent(count, e); };
        for (int i = 0; i < n1; ++i) push(audBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) push(audBuf[(size_t) (s2 + i)]);
        audFifo.finishedRead(n1 + n2);
    }
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && m.getNoteNumber() >= 36 && m.getNoteNumber() < 36 + kPads)
        {
            Event e; e.offset = juce::jlimit(0, n - 1, meta.samplePosition); e.pad = m.getNoteNumber() - 36; e.vel = juce::jmax(0.01f, m.getFloatVelocity());
            pushEvent(count, e);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) if (v.active) killVoice(v);
    }
    scheduleSequencer(host, n, count);

    std::sort(events.begin(), events.begin() + count, [](const Event& a, const Event& b) { return a.offset != b.offset ? a.offset < b.offset : a.order < b.order; });

    float* L = out.getWritePointer(0);
    float* R = nch > 1 ? out.getWritePointer(1) : nullptr;
    // Arbeitspuffer: bei Mono rendern wir in zwei Hilfspuffer
    static thread_local std::vector<float> scratchR;
    if (R == nullptr) { if ((int) scratchR.size() < n) scratchR.assign((size_t) n, 0.f); std::fill(scratchR.begin(), scratchR.begin() + n, 0.f); R = scratchR.data(); }

    int pos = 0;
    for (int i = 0; i < count; ++i)
    {
        if (events[(size_t) i].offset > pos) { renderSegment(L, R, pos, events[(size_t) i].offset); pos = events[(size_t) i].offset; }
        trigger(events[(size_t) i]);
    }
    renderSegment(L, R, pos, n);

    // Master mit Rampe, Sicherheits-Limiter, Mono-Faltung
    const float target = global.master;
    const float step = (target - masterSmoothed) / (float) n;
    float pk = 0.f;
    auto limit = [](float x) { const float a = std::abs(x); if (a <= 0.9f) return x; const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f); return x < 0.f ? -y : y; };
    for (int i = 0; i < n; ++i)
    {
        masterSmoothed += step;
        float l = L[i] * masterSmoothed, r = R[i] * masterSmoothed;
        if (! std::isfinite(l)) l = 0.f;
        if (! std::isfinite(r)) r = 0.f;
        if (nch > 1) { L[i] = limit(l); R[i] = limit(r); pk = juce::jmax(pk, std::abs(L[i]), std::abs(R[i])); }
        else { L[i] = limit((l + r) * 0.7071f); pk = juce::jmax(pk, std::abs(L[i])); }
    }
    masterSmoothed = target;
    peak = pk;
    int act = 0; for (auto& v : voices) act += v.active;
    voicesActive = act;
}
} // namespace rave
