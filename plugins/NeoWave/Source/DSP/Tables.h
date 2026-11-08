#pragma once
// Bandbegrenzte Wavetables: 8 Tabellen x 16 Frames x 10 Mip-Stufen x 2048 Samples.
// Die Mip-Stufen werden per inverser FFT mit abgeschnittenen Obertönen erzeugt (alias-frei).
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

struct WaveTables
{
    static constexpr int N = 2048, F = 16, T = 8, M = 10;

    WaveTables() { build(); }

    const float* get(int t, int f, int m) const noexcept
    {
        return data.data() + ((size_t) ((t * F + f) * M + m)) * (N + 1);
    }

    // Mip-Stufe passend zur Phasen-Inkrement dt (Obertöne unter Nyquist halten)
    static int mipFor(float dt) noexcept
    {
        const float x = 2048.f * dt;
        if (x <= 1.f) return 0;
        return juce::jmin(M - 1, (int) std::ceil(std::log2(x)));
    }

    // Lineare Interpolation innerhalb eines Frames, Morph zwischen zwei Frames
    inline float read(int t, float pos, int m, float ph) const noexcept
    {
        const float fp = juce::jlimit(0.f, 1.f, pos) * (float) (F - 1);
        const int f0 = (int) fp;
        const int f1 = juce::jmin(F - 1, f0 + 1);
        const float fr = fp - (float) f0;
        const float x = ph * (float) N;
        const int i = (int) x;
        const float xf = x - (float) i;
        const float* a = get(t, f0, m);
        const float* b = get(t, f1, m);
        const float va = a[i] + (a[i + 1] - a[i]) * xf;
        const float vb = b[i] + (b[i + 1] - b[i]) * xf;
        return va + (vb - va) * fr;
    }

private:
    std::vector<float> data;

    static float hash(int a, int b) noexcept
    {
        uint32_t h = (uint32_t) a * 374761393u + (uint32_t) b * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.f;
    }

    static float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

    static void spectrum(int table, int frame, float* amp, float* ph)
    {
        const float t = (float) frame / (float) (F - 1);
        const float pi = juce::MathConstants<float>::pi;
        for (int k = 0; k <= N / 2; ++k) { amp[k] = 0.f; ph[k] = 0.f; }
        const int K = N / 2 - 1;

        switch (table)
        {
            case 0: // Chrome Saw: Saw -> Square -> weiches Dreieck
            {
                const float s = juce::jlimit(0.f, 1.f, t * 2.f);
                const float u = juce::jlimit(0.f, 1.f, t * 2.f - 1.f);
                for (int k = 1; k <= K; ++k)
                {
                    const bool odd = (k & 1) != 0;
                    float a = lerp(1.f / (float) k, odd ? 1.f / (float) k : 0.f, s);
                    a = lerp(a, odd ? 1.5f / std::pow((float) k, 1.6f) : 0.f, u);
                    amp[k] = a;
                }
                break;
            }
            case 1: // Aqua Glass: wandernder Formantberg ueber Obertönen
            {
                const float c = 2.f + t * 40.f, w = 3.f + t * 4.f;
                for (int k = 1; k <= K; ++k)
                {
                    const float d = ((float) k - c) / w;
                    amp[k] = std::pow((float) k, -0.7f) * std::exp(-d * d) + 0.25f / (float) k;
                    ph[k] = 0.3f * (float) k;
                }
                break;
            }
            case 2: // Vox Machine: A-E-I-O-U Formanten (f0 ~ 130 Hz)
            {
                static const float V[5][3] = { { 800, 1150, 2800 }, { 400, 1600, 2700 }, { 300, 2200, 3000 },
                                               { 450, 800, 2830 },  { 325, 700, 2530 } };
                static const float G[3] = { 1.f, 0.7f, 0.45f };
                const float x = t * 4.f;
                const int i0 = juce::jmin(3, (int) x);
                const float fr = x - (float) i0;
                for (int k = 1; k <= K; ++k)
                {
                    float sum = 0.04f;
                    for (int j = 0; j < 3; ++j)
                    {
                        const float fh = lerp(V[i0][j], V[i0 + 1][j], fr) / 130.f;
                        const float d = ((float) k - fh) / (0.12f * fh + 1.5f);
                        sum += G[j] * std::exp(-d * d);
                    }
                    amp[k] = std::pow((float) k, -0.9f) * sum * 3.f;
                }
                break;
            }
            case 3: // Neon Bell: unregelmaessige Gewichte, Helligkeit sinkt
            {
                for (int k = 1; k <= 60; ++k)
                {
                    const float r = 0.2f + 0.8f * hash(k, 17);
                    amp[k] = r * std::exp(-(float) (k - 1) * (0.03f + 0.12f * t)) / std::sqrt((float) k);
                    ph[k] = 2.f * pi * hash(k, 91);
                }
                break;
            }
            case 4: // Pulse Drift: Pulsbreite 50% -> 5%
            {
                const float d = 0.5f - 0.45f * t;
                for (int k = 1; k <= K; ++k)
                {
                    amp[k] = 2.f * std::sin(pi * (float) k * d) / ((float) k * pi);
                    ph[k] = pi * 0.5f - pi * (float) k * d;
                }
                break;
            }
            case 5: // Cathedral: Orgel-Zugriegel
            {
                static const int H[9] = { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
                for (int i = 0; i < 9; ++i)
                {
                    const float w = 0.5f + 0.5f * std::sin(t * pi * (float) (i + 1) * 0.9f + (float) i);
                    amp[H[i]] = w * (i == 0 ? 1.f : 0.8f);
                }
                break;
            }
            case 6: // Harmonic Rise: Sinus -> immer mehr Obertöne
            {
                const float top = 1.f + t * 40.f;
                for (int k = 1; k <= K; ++k)
                {
                    const float edge = juce::jlimit(0.f, 1.f, top - (float) k + 1.f);
                    amp[k] = std::pow((float) k, -0.8f) * edge;
                }
                break;
            }
            default: // Static Bloom: weich verlaufende Pseudo-Zufallsspektren
            {
                const float x = t * 4.f;
                const int j0 = juce::jmin(3, (int) x);
                const float fr = x - (float) j0;
                for (int k = 1; k <= 300; ++k)
                {
                    const float a = lerp(hash(k, j0), hash(k, j0 + 1), fr);
                    amp[k] = a * a / std::pow((float) k, 0.75f);
                    ph[k] = 2.f * pi * hash(k, 555);
                }
                break;
            }
        }
    }

    void build()
    {
        data.assign((size_t) T * F * M * (N + 1), 0.f);
        juce::dsp::FFT fft(11);
        std::vector<float> buf(2 * N);
        std::vector<float> amp(N / 2 + 1), ph(N / 2 + 1);

        for (int t = 0; t < T; ++t)
            for (int f = 0; f < F; ++f)
            {
                spectrum(t, f, amp.data(), ph.data());
                float scale = 1.f;
                for (int m = 0; m < M; ++m)
                {
                    const int maxH = juce::jmin(N / 2 - 1, 1024 >> m);
                    std::fill(buf.begin(), buf.end(), 0.f);
                    for (int k = 1; k <= maxH; ++k)
                    {
                        buf[(size_t) (2 * k)]     = amp[(size_t) k] * std::cos(ph[(size_t) k]);
                        buf[(size_t) (2 * k + 1)] = amp[(size_t) k] * std::sin(ph[(size_t) k]);
                    }
                    fft.performRealOnlyInverseTransform(buf.data());

                    float* out = data.data() + ((size_t) ((t * F + f) * M + m)) * (N + 1);
                    if (m == 0)
                    {
                        float pk = 1e-9f;
                        for (int n = 0; n < N; ++n) pk = juce::jmax(pk, std::abs(buf[(size_t) n]));
                        scale = 0.95f / pk;
                    }
                    double mean = 0;
                    for (int n = 0; n < N; ++n) mean += buf[(size_t) n];
                    mean /= N;
                    for (int n = 0; n < N; ++n) out[n] = ((float) (buf[(size_t) n] - mean)) * scale;
                    out[N] = out[0];
                }
            }
    }
};
