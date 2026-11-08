#pragma once
// Kleine DSP-Bausteine: Hüllkurve, LFO, Zufall, Filter. Keine Allokation im Audio-Thread
// (Comb-Puffer werden in prepare() angelegt).
#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

namespace nw
{
constexpr float twoPi = 6.283185307179586f;
constexpr float pi    = 3.141592653589793f;

inline float fastTanh(float x) noexcept
{
    x = x < -3.f ? -3.f : (x > 3.f ? 3.f : x);
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

class Rng
{
public:
    void seed(uint32_t x) noexcept { s = x ? x : 1u; }
    float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.f / 16777216.f); }
    float bi() noexcept { return next() * 2.f - 1.f; }
private:
    uint32_t s = 22222u;
};

// ADSR: linearer Attack, exponentielle Decay/Release (fuehlt sich analoger an)
struct Env
{
    enum State { Idle, Att, Dec, Sus, Rel };
    State st = Idle;
    float v = 0.f, s = 0.7f, ai = 0.01f, dc = 0.99f, rc = 0.99f;
    float sr = 44100.f;

    void set(float a, float d, float sus, float r) noexcept
    {
        s = sus;
        ai = 1.f / (juce::jmax(a, 0.0005f) * sr);
        dc = std::exp(-1.f / (juce::jmax(d, 0.001f) * sr * 0.25f));
        rc = std::exp(-1.f / (juce::jmax(r, 0.001f) * sr * 0.25f));
    }
    void noteOn() noexcept { st = Att; }
    void noteOff() noexcept { if (st != Idle) st = Rel; }
    void reset() noexcept { st = Idle; v = 0.f; }
    bool active() const noexcept { return st != Idle; }

    float next() noexcept
    {
        switch (st)
        {
            case Att: v += ai; if (v >= 1.f) { v = 1.f; st = Dec; } break;
            case Dec: v = s + (v - s) * dc; if (std::abs(v - s) < 1e-4f) { v = s; st = Sus; } break;
            case Sus: v = s; break;
            case Rel: v *= rc; if (v < 1e-4f) { v = 0.f; st = Idle; } break;
            default: break;
        }
        return v;
    }
};

struct Lfo
{
    float ph = 0.f, sh = 0.f, r0 = 0.f, r1 = 0.f;
    Rng rng;

    void init(uint32_t seed) { rng.seed(seed); sh = rng.bi(); r0 = rng.bi(); r1 = rng.bi(); ph = 0.f; }

    // n Samples weiterschalten; liefert -1..1
    float tick(float rate, int shape, int n, float sr) noexcept
    {
        ph += rate * (float) n / sr;
        if (ph >= 1.f)
        {
            ph -= std::floor(ph);
            sh = rng.bi(); r0 = r1; r1 = rng.bi();
        }
        switch (shape)
        {
            case 0: return std::sin(twoPi * ph);
            case 1: return 4.f * std::abs(ph - 0.5f) - 1.f;
            case 2: return 2.f * ph - 1.f;
            case 3: return ph < 0.5f ? 1.f : -1.f;
            case 4: return sh;
            default: { const float t = ph * ph * (3.f - 2.f * ph); return r0 + (r1 - r0) * t; }
        }
    }
};

// Zustandsvariabler Filter (TPT), liefert LP/BP/HP
struct Svf
{
    float ic1 = 0.f, ic2 = 0.f;
    // mode: 0 LP, 1 BP (auf Spitze 1 normiert), 2 HP, 3 Notch
    inline float tick(float x, float a1, float a2, float a3, float k, int mode) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        switch (mode)
        {
            case 0: return v2;
            case 1: return v1 * k;
            case 2: return x - k * v1 - v2;
            default: return x - k * v1;
        }
    }
};

struct SvfCoef
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set(float fc, float k_, float sr) noexcept
    {
        const float g = std::tan(pi * juce::jlimit(10.f, sr * 0.45f, fc) / sr);
        k = k_;
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

// Das Voice-Filter: 10 Typen, stereo
struct VoiceFilter
{
    int type = 1;
    float sr = 44100.f;
    SvfCoef c1, c2, fc[3];
    float lg = 0.f, lk = 0.f, drv = 1.f, comp = 1.f;
    float combDelay = 100.f, combFb = 0.f;
    float fgain[3] = { 1.f, 0.7f, 0.45f };

    Svf s1[2], s2[2], sf[2][3];
    float lad[2][4] {};
    float fbLp[2] {};
    std::vector<float> comb[2];
    int combPos[2] { 0, 0 };

    void prepare(double rate)
    {
        sr = (float) rate;
        for (auto& c : comb) c.assign(16384, 0.f);
        reset();
    }
    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            s1[c] = {}; s2[c] = {};
            for (auto& f : sf[c]) f = {};
            for (auto& l : lad[c]) l = 0.f;
            fbLp[c] = 0.f;
            std::fill(comb[c].begin(), comb[c].end(), 0.f);
            combPos[c] = 0;
        }
    }

    void setup(int t, float cut, float res, float drive) noexcept
    {
        type = t;
        drv = 1.f + drive * 5.f;
        comp = 1.f / (1.f + drive * 1.2f);
        const float kRes = 1.6f - 1.55f * res;

        switch (t)
        {
            case 0: case 2: case 3: case 4: c1.set(cut, kRes, sr); break;
            case 1: c1.set(cut, kRes, sr); c2.set(cut, 1.3f, sr); break;
            case 5: case 6: case 9:
            {
                const float f = juce::jmin(cut, sr * 0.2f);
                lg = 1.f - std::exp(-twoPi * f / sr);
                lk = res * (t == 6 ? 4.4f : (t == 9 ? 3.4f : 4.0f));
                break;
            }
            case 7:
            {
                static const float V[5][3] = { { 800, 1150, 2800 }, { 400, 1600, 2700 }, { 300, 2200, 3000 },
                                               { 450, 800, 2830 },  { 325, 700, 2530 } };
                const float pos = juce::jlimit(0.f, 1.f, std::log(cut / 20.f) / std::log(1000.f)) * 3.999f;
                const int i0 = (int) pos;
                const float fr = pos - (float) i0;
                const float q = 3.f + res * 17.f;
                for (int j = 0; j < 3; ++j)
                    fc[j].set(V[i0][j] + (V[i0 + 1][j] - V[i0][j]) * fr, 1.f / q, sr);
                break;
            }
            default:
                combDelay = juce::jlimit(8.f, 8000.f, sr / juce::jlimit(40.f, 8000.f, cut));
                combFb = 0.15f + res * 0.8f;
                break;
        }
    }

    inline float process(int ch, float x) noexcept
    {
        switch (type)
        {
            case 0: case 2: case 3: case 4:
            {
                x = fastTanh(x * drv) * comp;
                const int mode = type == 0 ? 0 : (type == 2 ? 2 : (type == 3 ? 1 : 3));
                return s1[ch].tick(x, c1.a1, c1.a2, c1.a3, c1.k, mode);
            }
            case 1:
            {
                x = fastTanh(x * drv) * comp;
                const float y = s1[ch].tick(x, c1.a1, c1.a2, c1.a3, c1.k, 0);
                return s2[ch].tick(y, c2.a1, c2.a2, c2.a3, c2.k, 0);
            }
            case 5: // Ladder: 4 Pole, Rückkopplung vom Ausgang
            {
                auto& l = lad[ch];
                const float in = fastTanh(x * drv * 0.8f - lk * l[3]) * (1.f + lk * 0.25f);
                l[0] += lg * (in - l[0]);
                l[1] += lg * (l[0] - l[1]);
                l[2] += lg * (l[1] - l[2]);
                l[3] += lg * (l[2] - l[3]);
                return l[3] * comp * 1.2f;
            }
            case 6: // Acid: 3 Pole (18 dB), Rückkopplung hochpassgefiltert, asymmetrische Saettigung
            {
                auto& l = lad[ch];
                fbLp[ch] += 0.003f * (l[2] - fbLp[ch]);
                const float fb = lk * (l[2] - fbLp[ch]);
                float in = x * drv * 0.9f - fb;
                in = fastTanh(in + 0.15f * in * in);
                l[0] += lg * (in - l[0]);
                l[1] += lg * (l[0] - l[1]);
                l[2] += lg * (l[1] - l[2]);
                return l[2] * comp * 1.4f;
            }
            case 9: // Diode-Stil: Saettigung in jeder Stufe, weniger Resonanz-Kompensation
            {
                auto& l = lad[ch];
                const float in = fastTanh(x * drv * 0.8f - lk * l[3] + 0.1f);
                l[0] += lg * (in - fastTanh(l[0] + 0.1f));
                l[1] += lg * (fastTanh(l[0] + 0.1f) - fastTanh(l[1] + 0.1f));
                l[2] += lg * (fastTanh(l[1] + 0.1f) - fastTanh(l[2] + 0.1f));
                l[3] += lg * (fastTanh(l[2] + 0.1f) - fastTanh(l[3] + 0.1f));
                return l[3] * comp * 1.3f;
            }
            case 7: // Formant: 3 parallele Bandpaesse
            {
                x = fastTanh(x * drv) * comp;
                float y = 0.f;
                for (int j = 0; j < 3; ++j)
                    y += fgain[j] * sf[ch][j].tick(x, fc[j].a1, fc[j].a2, fc[j].a3, fc[j].k, 1);
                return y * 1.3f;
            }
            default: // Comb
            {
                auto& b = comb[ch];
                const int mask = 16383;
                const float d = combDelay;
                const int i0 = (int) d;
                const float fr = d - (float) i0;
                const int p = combPos[ch];
                const float a = b[(size_t) ((p - i0) & mask)];
                const float c = b[(size_t) ((p - i0 - 1) & mask)];
                const float delayed = a + (c - a) * fr;
                const float y = fastTanh(x * (1.f + drv * 0.1f) + combFb * delayed);
                b[(size_t) p] = y;
                combPos[ch] = (p + 1) & mask;
                return y * (1.f - 0.35f * combFb);
            }
        }
    }
};
} // namespace nw
