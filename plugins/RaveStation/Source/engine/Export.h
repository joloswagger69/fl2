#pragma once
// Offline-Rendering und Export: Pattern -> Audio (selbe Engine wie live, deshalb dasselbe Ergebnis), Pad-Bounce, MIDI.
#include <memory>
#include "Engine.h"
#include "Morph.h"
#include "Generator.h"

namespace rave::exporter
{
struct PadSetup
{
    SamplePtr src, layer;
    PadParams params;
};

// Rendert `loops` Durchlaeufe eines Patterns mit der Live-Engine. Mit wrapTail wird der Nachklang auf den Anfang gelegt (nahtloser Loop).
// onlyPad >= 0 rendert nur dieses Pad (Stems).
inline juce::AudioBuffer<float> renderPattern(const PlainPattern& pat, const std::array<PadSetup, kPads>& setup, GlobalParams g, int loops,
                                              double sr, bool wrapTail, int onlyPad = -1)
{
    // Auf dem Heap: Bank + Engine sind zusammen ~360 KB; auf dem Stack (Windows: 1 MB) waere das gefaehrlich
    auto bankPtr = std::make_unique<PatternBank>(); PatternBank& bank = *bankPtr; bank.store(0, pat);
    auto engPtr = std::make_unique<RaveEngine>(bank); RaveEngine& eng = *engPtr;
    constexpr int blk = 512;
    eng.prepare(sr, blk);
    PadParams pads[kPads];
    for (int i = 0; i < kPads; ++i)
    {
        pads[i] = setup[(size_t) i].params;
        if (onlyPad >= 0 && i != onlyPad) pads[i].mute = true;
        else if (onlyPad >= 0) { pads[i].solo = false; pads[i].mute = false; }
        eng.postSample(i, RaveEngine::kSource, setup[(size_t) i].src);
        if (setup[(size_t) i].layer) eng.postSample(i, RaveEngine::kLayer, setup[(size_t) i].layer);
        if (setup[(size_t) i].src && needsProcessing(pads[i]))
            eng.postSample(i, RaveEngine::kProcessed, morph::renderProcessed(*setup[(size_t) i].src, pads[i].stretch, pads[i].keep ? pads[i].pitch : 0.f), processedKey(pads[i]));
    }
    g.hostSync = false; g.playing = true; g.slot = 0;
    eng.setPads(pads); eng.setGlobal(g);

    const double stepBeats = g.stepDiv == 0 ? 0.5 : (g.stepDiv == 1 ? 0.25 : 0.125);
    const double loopSec = pat.length * stepBeats * 60.0 / juce::jlimit(20.0, 400.0, g.bpm);
    const int loopN = (int) std::lround(loopSec * sr);
    const int body = juce::jmax(1, loopN * juce::jmax(1, loops));
    std::vector<float> L, R;
    juce::AudioBuffer<float> b(2, blk);
    int rendered = 0, silent = 0;
    const int maxTail = (int) (6.0 * sr);
    while (rendered < body + maxTail)
    {
        // Bis genau zur Loop-Grenze in passend kurzen Bloecken rendern; dort den Sequencer stoppen (sonst rutscht der erste Schlag des naechsten Loops hinein)
        int n = blk;
        if (rendered < body) n = juce::jmin(blk, body - rendered);
        else if (g.playing) { g.playing = false; eng.setGlobal(g); }
        b.setSize(2, n, false, false, true);
        b.clear();
        eng.process(b, juce::MidiBuffer(), HostInfo());
        eng.drainTrash();
        L.insert(L.end(), b.getReadPointer(0), b.getReadPointer(0) + n);
        R.insert(R.end(), b.getReadPointer(1), b.getReadPointer(1) + n);
        rendered += n;
        if (rendered >= body)
        {
            if (b.getMagnitude(0, n) < 1.0e-5f && eng.voicesActive.load() == 0) { if (++silent > 2) break; } else silent = 0;
        }
    }
    const int total = wrapTail ? body : juce::jmin((int) L.size(), body + (int) (rendered > body ? rendered - body : 0));
    juce::AudioBuffer<float> out(2, juce::jmax(1, total));
    out.clear();
    for (int i = 0; i < (int) L.size(); ++i)
    {
        int at = i;
        if (wrapTail && i >= body) at = i % body;
        if (at < total) { out.getWritePointer(0)[at] += L[(size_t) i]; out.getWritePointer(1)[at] += R[(size_t) i]; }
    }
    // Ende trimmen (Stille entfernen), aber nie unter die Loop-Laenge
    if (! wrapTail)
    {
        int last = body;
        for (int i = out.getNumSamples() - 1; i >= body; --i) if (std::abs(out.getSample(0, i)) > 1.0e-4f || std::abs(out.getSample(1, i)) > 1.0e-4f) { last = i + 1; break; }
        out.setSize(2, juce::jmin(out.getNumSamples(), last + (int) (0.01 * sr)), true, false, true);
    }
    return out;
}

// Ein Pad mit allen Einstellungen in ein neues Sample "backen" (Resample/Bounce).
inline SamplePtr bouncePad(const PadSetup& pad, float velocity, double sr)
{
    if (! pad.src) return nullptr;
    PlainPattern pat; pat.length = 16; Step s; s.vel = (uint8_t) juce::jlimit(1, 127, (int) std::lround(velocity * 127.f)); pat.s[0][0] = s;
    std::array<PadSetup, kPads> setup; setup[0] = pad; setup[0].params.mute = false; setup[0].params.solo = false;
    GlobalParams g; g.bpm = 120.0; g.stepDiv = 1; g.master = 1.f;
    auto buf = renderPattern(pat, setup, g, 1, sr, false, 0);
    // auf die Laenge des Klangs kuerzen (Pattern-Laenge ist nur eine Obergrenze)
    int last = 0;
    for (int i = buf.getNumSamples() - 1; i >= 0; --i) if (std::abs(buf.getSample(0, i)) > 1.0e-4f || std::abs(buf.getSample(1, i)) > 1.0e-4f) { last = i + 1; break; }
    if (last < 16) return nullptr;
    buf.setSize(2, last, true, false, true);
    return makeSample(buf, sr, pad.src->name + " (bounce)");
}

// ------------------------------------------------------------------------------------------------ MIDI
// Tick-genaue Wiedergabe der Engine-Logik (ohne Humanize, das ist ein Live-Regler). Pad i -> Note 36+i, Kanal 10.
// rollProbability: Wahrscheinlichkeitsschritte werden mit demselben Hash wuerfeln wie die Engine (gleicher Seed = gleiches Ergebnis wie im Plugin gehoert);
// sonst werden sie als normale Noten exportiert.
inline bool writeMidi(const PlainPattern& pat, const GlobalParams& g, int sigIndex, int loops, bool rollProbability, const juce::File& file, juce::String* error = nullptr)
{
    constexpr int ppq = 480;
    const int stepTicks = g.stepDiv == 0 ? ppq / 2 : (g.stepDiv == 1 ? ppq / 4 : ppq / 8);
    const double swingTicks = juce::jlimit(0.f, 1.f, g.swing) * 0.5 * stepTicks;
    juce::MidiMessageSequence seq;
    seq.addEvent(juce::MidiMessage::textMetaEvent(3, "RAVE-STATION"), 0);
    seq.addEvent(juce::MidiMessage::tempoMetaEvent((int) std::lround(60000000.0 / juce::jlimit(20.0, 400.0, g.bpm))), 0);
    static const int num[] = { 2, 3, 4, 5, 6, 7, 9, 12 }, den[] = { 4, 4, 4, 4, 8, 8, 8, 8 };
    seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(num[juce::jlimit(0, 7, sigIndex)], den[juce::jlimit(0, 7, sigIndex)]), 0);
    int notes = 0;
    for (int loop = 0; loop < juce::jmax(1, loops); ++loop)
        for (int st = 0; st < pat.length; ++st)
        {
            const long long k = (long long) loop * pat.length + st;       // laufender Schrittindex wie in der Engine
            for (int pad = 0; pad < kPads; ++pad)
            {
                const Step s = pat.s[pad][st];
                if (! s.on()) continue;
                if (rollProbability && s.prob < 100 && hash01((uint64_t) k, (uint64_t) pad, (uint64_t) g.seed * 31u + 7u) * 100.f >= (float) s.prob) continue;
                const bool ghost = (s.flags & kGhost) != 0, roll = (s.flags & kRoll) != 0;
                float vel = (float) s.vel / 127.f * (ghost ? 0.45f : 1.f);
                const int r = juce::jlimit(1, 8, (int) s.ratchet);
                for (int j = 0; j < r; ++j)
                {
                    float v = vel;
                    if (r > 1) v *= roll ? (0.45f + 0.55f * (float) j / (float) (r - 1)) : (1.f - 0.07f * (float) j);
                    const double t = (double) k * stepTicks + ((k & 1) ? swingTicks : 0.0) + (double) j * stepTicks / r;
                    const int on = (int) std::lround(t);
                    const int len = juce::jmax(10, (int) std::lround((double) stepTicks / r * 0.5));
                    const int note = 36 + pad;
                    seq.addEvent(juce::MidiMessage::noteOn(10, note, (juce::uint8) juce::jlimit(1, 127, (int) std::lround(v * 127.f))), on);
                    seq.addEvent(juce::MidiMessage::noteOff(10, note, (juce::uint8) 0), on + len);
                    ++notes;
                }
            }
        }
    seq.updateMatchedPairs();
    seq.sort();
    juce::MidiFile mf; mf.setTicksPerQuarterNote(ppq); mf.addTrack(seq);
    juce::TemporaryFile tmp(file);
    {
        juce::FileOutputStream out(tmp.getFile());
        if (! out.openedOk()) { if (error) *error = "Cannot open " + file.getFullPathName(); return false; }
        out.setPosition(0); out.truncate();
        if (! mf.writeTo(out)) { if (error) *error = "MIDI write failed"; return false; }
    }
    if (! tmp.overwriteTargetFileWithTemporary()) { if (error) *error = "Cannot replace " + file.getFullPathName(); return false; }
    (void) notes;
    return true;
}
} // namespace rave::exporter
