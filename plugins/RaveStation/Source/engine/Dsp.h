#pragma once
// Kleine DSP-Bausteine fuer die Sample-Engine (audio-thread-tauglich, ohne Allokation).
#include <juce_core/juce_core.h>
#include <cmath>

namespace rave
{
inline constexpr float kPi = 3.14159265358979f;

// 4-Punkt-Hermite-Interpolation (cubic). pos in Samples; ausserhalb [0, n-1] wird mit 0 aufgefuellt.
inline float readHermite(const float* d, int n, double pos) noexcept
{
    const int i = (int) std::floor(pos);
    const float t = (float) (pos - i);
    auto at = [&](int k) { return (k >= 0 && k < n) ? d[k] : 0.f; };
    const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
    const float c0 = x0, c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + c0;
}

// TPT State-Variable-Filter (Zavalishin). res: 0..1
struct Svf
{
    float s1 = 0.f, s2 = 0.f;
    void reset() noexcept { s1 = s2 = 0.f; }
    static float coefG(float cutoffHz, float sr) noexcept
    {
        const float fc = juce::jlimit(10.f, sr * 0.45f, cutoffHz);
        return std::tan(kPi * fc / sr);
    }
    // liefert Lowpass; Highpass wird ueber hp ausgegeben
    inline void process(float x, float g, float res, float& lp, float& hp) noexcept
    {
        const float k = 2.f - 1.9f * juce::jlimit(0.f, 1.f, res);
        const float a1 = 1.f / (1.f + g * (g + k));
        const float a2 = g * a1, a3 = g * a2;
        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;
        lp = v2;
        hp = x - k * v1 - v2;
    }
};

inline float softSat(float x, float drive) noexcept   // drive 0..1
{
    if (drive <= 0.0001f) return x;
    const float g = 1.f + drive * 12.f;
    return std::tanh(x * g) / std::tanh(g);
}

// Bitcrusher: amount 0..1  ->  16..3 Bit und Downsampling-Faktor 1..16 (Zustand im Aufrufer)
struct Crusher
{
    float held = 0.f; float phase = 0.f;
    void reset() noexcept { held = 0.f; phase = 0.f; }
    inline float process(float x, float amount) noexcept
    {
        if (amount <= 0.0001f) return x;
        const float bits = 16.f - amount * 13.f;
        const float q = std::pow(2.f, bits - 1.f);
        const float rate = 1.f + amount * amount * 15.f;
        phase += 1.f;
        if (phase >= rate) { phase -= rate; held = std::round(x * q) / q; }
        return held;
    }
};

// Deterministischer Hash -> 0..1 (fuer reproduzierbare Wahrscheinlichkeit/Humanize)
inline float hash01(uint64_t a, uint64_t b = 0, uint64_t c = 0) noexcept
{
    uint64_t x = a * 0x9E3779B97F4A7C15ull ^ (b + 0x7F4A7C15ull) * 0xBF58476D1CE4E5B9ull ^ (c + 0x94D049BBull) * 0x94D049BB133111EBull;
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 27; x *= 0x94D049BB133111EBull; x ^= x >> 31;
    return (float) ((x >> 11) * (1.0 / 9007199254740992.0));
}
} // namespace rave
