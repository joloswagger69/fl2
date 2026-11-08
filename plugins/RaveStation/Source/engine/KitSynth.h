#pragma once
// Synthetisch erzeugtes Standard-Kit und Demo-Break. Keine Dateien, keine Lizenzfragen.
// Gedacht als funktionierende Grundausstattung; eigene Samples ersetzen jeden Pad-Sound.
#include "Sample.h"
#include "Dsp.h"
#include <random>

namespace rave::kit
{
inline constexpr double kRate = 44100.0;
inline constexpr int kPadCount = 16;

inline const char* const kPadNames[kPadCount] = { "Kick", "Kick Sub", "Snare", "Snare Tight", "Clap", "Closed Hat", "Open Hat", "Shaker",
                                                   "Tom Low", "Tom High", "Conga", "Metal", "Rim", "Bell", "Riser", "Zap" };
// Rollen fuer den Generator: 0 Kick, 1 Snare, 2 Clap, 3 ClosedHat, 4 OpenHat, 5 Perc, 6 Tom, 7 FX, 8 None
inline constexpr int kDefaultRoles[kPadCount] = { 0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 5, 5, 5, 5, 7, 7 };

namespace detail
{
    struct Out { juce::AudioBuffer<float> b; Out(double sec) : b(1, (int) (sec * kRate)) { b.clear(); } float* p() { return b.getWritePointer(0); } int n() const { return b.getNumSamples(); } };

    inline float env(double t, double decay) { return (float) std::exp(-t / decay); }
    inline void normalise(juce::AudioBuffer<float>& b, float peak = 0.9f)
    {
        float m = b.getMagnitude(0, b.getNumSamples());
        if (m > 1e-6f) b.applyGain(peak / m);
        // kurze Ausblendung gegen Klicks am Ende
        const int fade = juce::jmin(b.getNumSamples(), (int) (0.004 * kRate));
        b.applyGainRamp(0, b.getNumSamples() - fade, fade, 1.f, 0.f);
    }
    inline SamplePtr finish(Out& o, const char* name, float peak = 0.9f) { normalise(o.b, peak); return makeSample(o.b, kRate, name); }
    inline void hp(float* d, int n, float fc) { Svf f; const float g = Svf::coefG(fc, (float) kRate); for (int i = 0; i < n; ++i) { float lp, h; f.process(d[i], g, 0.f, lp, h); d[i] = h; } }
    inline void bp(float* d, int n, float lo, float hi) { hp(d, n, lo); Svf f; const float g = Svf::coefG(hi, (float) kRate); for (int i = 0; i < n; ++i) { float l, h; f.process(d[i], g, 0.f, l, h); d[i] = l; } }
}

inline SamplePtr makeKick(double startHz, double endHz, double sweepTau, double decay, double len, const char* name, float drive)
{
    using namespace detail;
    Out o(len); auto* d = o.p(); double ph = 0.0; std::mt19937 rng(11);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    for (int i = 0; i < o.n(); ++i)
    {
        const double t = i / kRate;
        const double f = endHz + (startHz - endHz) * std::exp(-t / sweepTau);
        ph += 2.0 * juce::MathConstants<double>::pi * f / kRate;
        float s = (float) std::sin(ph) * env(t, decay);
        s += (t < 0.003 ? u(rng) * 0.25f * (float) (1.0 - t / 0.003) : 0.f);
        d[i] = softSat(s, drive);
    }
    return finish(o, name);
}

inline SamplePtr makeSnare(double toneHz, double toneDecay, double noiseDecay, double len, const char* name, float noiseAmt)
{
    using namespace detail;
    Out o(len); auto* d = o.p(); std::mt19937 rng(21); std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::vector<float> noise((size_t) o.n());
    for (auto& v : noise) v = u(rng);
    hp(noise.data(), o.n(), 1800.f);
    for (int i = 0; i < o.n(); ++i)
    {
        const double t = i / kRate;
        const float tone = (float) (std::sin(2 * juce::MathConstants<double>::pi * toneHz * t) * 0.6 + std::sin(2 * juce::MathConstants<double>::pi * toneHz * 1.8 * t) * 0.3) * env(t, toneDecay);
        d[i] = tone + noise[(size_t) i] * noiseAmt * env(t, noiseDecay);
    }
    return finish(o, name);
}

inline SamplePtr makeClap()
{
    using namespace detail;
    Out o(0.4); auto* d = o.p(); std::mt19937 rng(31); std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::vector<float> n((size_t) o.n());
    for (auto& v : n) v = u(rng);
    bp(n.data(), o.n(), 900.f, 3800.f);
    for (int i = 0; i < o.n(); ++i)
    {
        const double t = i / kRate; float e = 0.f;
        for (int k = 0; k < 4; ++k) { const double tk = t - k * 0.011; if (tk >= 0.0) e += (k < 3 ? 0.7f : 1.f) * env(tk, k < 3 ? 0.004 : 0.09); }
        d[i] = n[(size_t) i] * e;
    }
    return finish(o, "Clap");
}

inline SamplePtr makeHat(double decay, double len, const char* name)
{
    using namespace detail;
    Out o(len); auto* d = o.p();
    static const double ratios[6] = { 2.0, 3.0, 4.16, 5.43, 6.79, 8.21 };
    double ph[6] = {};
    for (int i = 0; i < o.n(); ++i)
    {
        const double t = i / kRate; float s = 0.f;
        for (int k = 0; k < 6; ++k) { ph[k] += 205.3 * ratios[k] / kRate; s += (ph[k] - std::floor(ph[k]) < 0.5) ? 1.f : -1.f; }
        d[i] = s * env(t, decay);
    }
    bp(d, o.n(), 6500.f, 15000.f);
    return finish(o, name, 0.8f);
}

inline SamplePtr makeShaker()
{
    using namespace detail;
    Out o(0.25); auto* d = o.p(); std::mt19937 rng(41); std::uniform_real_distribution<float> u(-1.f, 1.f);
    for (int i = 0; i < o.n(); ++i) { const double t = i / kRate; d[i] = u(rng) * (float) (std::min(1.0, t / 0.02) * std::exp(-t / 0.05)); }
    bp(d, o.n(), 4500.f, 11000.f);
    return finish(o, "Shaker", 0.7f);
}

inline SamplePtr makeTone(double f0, double f1, double decay, double len, const char* name, double fm = 0.0)
{
    using namespace detail;
    Out o(len); auto* d = o.p(); double ph = 0.0;
    for (int i = 0; i < o.n(); ++i)
    {
        const double t = i / kRate; const double f = f1 + (f0 - f1) * std::exp(-t / 0.04);
        ph += 2.0 * juce::MathConstants<double>::pi * f / kRate;
        d[i] = (float) std::sin(ph + fm * std::sin(ph * 1.4142)) * env(t, decay);
    }
    return finish(o, name);
}

inline SamplePtr makeRim()
{
    using namespace detail;
    Out o(0.12); auto* d = o.p();
    for (int i = 0; i < o.n(); ++i) { const double t = i / kRate; d[i] = (float) ((std::sin(2 * juce::MathConstants<double>::pi * 1700 * t) + 0.8 * std::sin(2 * juce::MathConstants<double>::pi * 820 * t)) * std::exp(-t / 0.012)); }
    return finish(o, "Rim");
}

inline SamplePtr makeBell()
{
    using namespace detail;
    Out o(0.8); auto* d = o.p();
    static const double r[4] = { 1.0, 1.5, 2.76, 4.07 };
    for (int i = 0; i < o.n(); ++i) { const double t = i / kRate; float s = 0.f; for (int k = 0; k < 4; ++k) s += (float) std::sin(2 * juce::MathConstants<double>::pi * 760 * r[k] * t) * (1.f / (1.f + k)); d[i] = s * env(t, 0.22); }
    return finish(o, "Bell");
}

inline SamplePtr makeRiser()
{
    using namespace detail;
    Out o(1.0); auto* d = o.p(); std::mt19937 rng(51); std::uniform_real_distribution<float> u(-1.f, 1.f);
    Svf f; float lpv, hpv;
    for (int i = 0; i < o.n(); ++i)
    {
        const double t = i / kRate; const float fc = (float) (300.0 * std::pow(40.0, t));
        f.process(u(rng), Svf::coefG(fc, (float) kRate), 0.6f, lpv, hpv);
        d[i] = lpv * (float) std::min(1.0, t / 0.8) * (t > 0.95 ? (float) ((1.0 - t) / 0.05) : 1.f);
    }
    return finish(o, "Riser", 0.8f);
}

inline SamplePtr makeZap()
{
    using namespace detail;
    Out o(0.35); auto* d = o.p(); double ph = 0.0;
    for (int i = 0; i < o.n(); ++i) { const double t = i / kRate; const double f = 80.0 + 3200.0 * std::exp(-t / 0.05); ph += 2.0 * juce::MathConstants<double>::pi * f / kRate; d[i] = (float) std::sin(ph) * env(t, 0.1); }
    return finish(o, "Zap");
}

inline SamplePtr makePad(int i)
{
    switch (i)
    {
        case 0:  return makeKick(180, 48, 0.022, 0.20, 0.55, "Kick", 0.25f);
        case 1:  return makeKick(110, 38, 0.05, 0.55, 1.0, "Kick Sub", 0.1f);
        case 2:  return makeSnare(185, 0.07, 0.16, 0.4, "Snare", 0.8f);
        case 3:  return makeSnare(230, 0.035, 0.07, 0.2, "Snare Tight", 0.9f);
        case 4:  return makeClap();
        case 5:  return makeHat(0.018, 0.12, "Closed Hat");
        case 6:  return makeHat(0.16, 0.6, "Open Hat");
        case 7:  return makeShaker();
        case 8:  return makeTone(130, 85, 0.25, 0.6, "Tom Low");
        case 9:  return makeTone(220, 150, 0.2, 0.5, "Tom High");
        case 10: return makeTone(380, 270, 0.09, 0.3, "Conga");
        case 11: return makeTone(540, 520, 0.12, 0.4, "Metal", 4.0);
        case 12: return makeRim();
        case 13: return makeBell();
        case 14: return makeRiser();
        default: return makeZap();
    }
}

// Demo-Break: 2 Takte bei 170 BPM aus dem Kit, Ghost-Notes inklusive. onsets = Startpositionen (Samples) aller Schlaege.
inline SamplePtr makeDemoBreak(std::vector<int>* onsets = nullptr, double bpm = 170.0)
{
    const double stepSec = 60.0 / bpm / 4.0;
    const int steps = 32;
    const int total = (int) (stepSec * steps * kRate) + (int) (0.5 * kRate);
    juce::AudioBuffer<float> b(1, total); b.clear();
    struct Hit { int step, pad; float gain; };
    std::vector<Hit> hits;
    for (int s : { 0, 10, 16, 26 }) hits.push_back({ s, 0, 1.f });
    for (int s : { 4, 12, 20, 28 }) hits.push_back({ s, 2, 1.f });
    for (int s : { 7, 15, 23, 31 }) hits.push_back({ s, 3, 0.3f });
    for (int s = 0; s < steps; s += 2) hits.push_back({ s, 5, (s % 4 == 0) ? 0.55f : 0.35f });
    hits.push_back({ 14, 6, 0.5f }); hits.push_back({ 30, 6, 0.5f });
    std::vector<int> times;
    for (auto& h : hits)
    {
        auto s = makePad(h.pad);
        const int at = (int) std::round(h.step * stepSec * kRate);
        for (int i = 0; i < s->length() && at + i < total; ++i) b.getWritePointer(0)[at + i] += s->data.getSample(0, i) * h.gain;
        times.push_back(at);
    }
    std::sort(times.begin(), times.end()); times.erase(std::unique(times.begin(), times.end()), times.end());
    if (onsets) *onsets = times;
    const float m = b.getMagnitude(0, total);
    if (m > 0.f) b.applyGain(0.9f / m);
    return makeSample(b, kRate, "Demo Break (synth)");
}
} // namespace rave::kit
