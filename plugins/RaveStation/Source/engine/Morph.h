#pragma once
// Offline-Verarbeitung (laeuft im Worker-Thread, nie im Audio-Thread):
//   * wsolaStretch    - Zeitdehnung ohne Tonhoehenaenderung (WSOLA)
//   * pitchKeepLength - Tonhoehe ohne Laengenaenderung (Dehnung + Resampling)
//   * renderProcessed - was ein Pad mit "stretch" / "keep" braucht
// Ehrlicher Hinweis: WSOLA verschmiert Transienten (Drums!) bei grossen Faktoren. Bei Faktoren nahe 1 klingt es
// sauber, bei 0.5x / 2x hoert man Flattern. Das ist eine bekannte Eigenschaft des Verfahrens.
#include "Sample.h"
#include "Dsp.h"

namespace rave::morph
{
inline juce::AudioBuffer<float> wsolaStretch(const juce::AudioBuffer<float>& in, double factor, double rate)
{
    const int nch = in.getNumChannels(), len = in.getNumSamples();
    factor = juce::jlimit(0.1, 10.0, factor);
    if (len < 64 || std::abs(factor - 1.0) < 1.0e-4) return in;

    int N = (int) (0.046 * rate) & ~1;
    N = juce::jlimit(256, 8192, N);
    if (N * 2 > len) N = juce::jmax(64, (len / 2) & ~1);
    const int Hs = N / 4, tol = N / 4;
    const int outLen = (int) std::ceil(len * factor) + N;

    std::vector<float> win((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(2.0 * juce::MathConstants<double>::pi * (i + 0.5) / N);

    std::vector<float> mono((size_t) len, 0.f);
    for (int c = 0; c < nch; ++c) { const float* s = in.getReadPointer(c); for (int i = 0; i < len; ++i) mono[(size_t) i] += s[i] / (float) nch; }

    juce::AudioBuffer<float> out(nch, outLen); out.clear();
    std::vector<float> norm((size_t) outLen, 0.f);
    auto addGrain = [&](int srcPos, int dstPos)
    {
        for (int c = 0; c < nch; ++c)
        {
            const float* s = in.getReadPointer(c); float* o = out.getWritePointer(c);
            for (int i = 0; i < N; ++i)
            {
                const int si = srcPos + i, di = dstPos + i;
                if (di >= outLen) break;
                if (si >= 0 && si < len) o[di] += s[si] * win[(size_t) i];
            }
        }
        for (int i = 0; i < N && dstPos + i < outLen; ++i) norm[(size_t) (dstPos + i)] += win[(size_t) i];
    };

    int prevPos = 0;
    addGrain(0, 0);
    for (int k = 1;; ++k)
    {
        const int dst = k * Hs;
        if (dst >= outLen - N) break;
        const double nominal = (double) dst / factor;
        if (nominal >= len + tol) break;
        const int natural = prevPos + Hs;          // so wuerde das Original weiterlaufen
        int best = (int) std::lround(nominal);
        if (natural + N <= len && natural >= 0)
        {
            double bestC = -1.0e30;
            const int lo = juce::jmax(0, (int) std::lround(nominal) - tol), hi = juce::jmin(len - N, (int) std::lround(nominal) + tol);
            for (int cand = lo; cand <= hi; cand += 2)
            {
                double c = 0.0;
                for (int i = 0; i < N; i += 2) c += (double) mono[(size_t) (natural + i)] * mono[(size_t) (cand + i)];
                if (c > bestC) { bestC = c; best = cand; }
            }
        }
        addGrain(best, dst);
        prevPos = best;
    }
    for (int c = 0; c < nch; ++c)
    {
        float* o = out.getWritePointer(c);
        for (int i = 0; i < outLen; ++i) o[i] = norm[(size_t) i] > 1.0e-3f ? o[i] / norm[(size_t) i] : 0.f;
    }
    out.setSize(nch, juce::jmax(1, (int) std::lround(len * factor)), true, false, true);
    return out;
}

// Liest Puffer mit Schrittweite `step` (Resampling), Ergebnislaenge = len / step.
inline juce::AudioBuffer<float> resampleBy(const juce::AudioBuffer<float>& in, double step)
{
    const int nch = in.getNumChannels(), len = in.getNumSamples();
    const int outLen = juce::jmax(1, (int) std::floor(len / step));
    juce::AudioBuffer<float> out(nch, outLen);
    for (int c = 0; c < nch; ++c)
    {
        const float* s = in.getReadPointer(c); float* o = out.getWritePointer(c);
        for (int i = 0; i < outLen; ++i) o[i] = readHermite(s, len, i * step);
    }
    return out;
}

inline juce::AudioBuffer<float> pitchKeepLength(const juce::AudioBuffer<float>& in, double semis, double rate)
{
    if (std::abs(semis) < 0.005) return in;
    const double r = std::pow(2.0, semis / 12.0);
    auto stretched = wsolaStretch(in, r, rate);   // r-mal laenger, gleiche Tonhoehe
    auto out = resampleBy(stretched, r);          // r-mal schneller abgespielt: Tonhoehe * r, Laenge wieder wie vorher
    return out;
}

// stretch: Zeitfaktor; keepSemis: Tonhoehe ohne Laengenaenderung (0 = aus)
inline SamplePtr renderProcessed(const Sample& src, double stretch, double keepSemis)
{
    juce::AudioBuffer<float> b;
    b.makeCopyOf(src.data);
    if (std::abs(keepSemis) > 0.005) b = pitchKeepLength(b, keepSemis, src.rate);
    if (std::abs(stretch - 1.0) > 0.001) b = wsolaStretch(b, stretch, src.rate);
    return makeSample(b, src.rate, src.name, src.sourcePath);
}
} // namespace rave::morph
