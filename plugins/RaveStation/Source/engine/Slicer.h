#pragma once
// Break-Slicer: Transientenerkennung, Schnittmarken, Slice-Operationen, Neuanordnung, Slice->Pattern.
// Alles offline (Message-/Worker-Thread), nie im Audio-Thread.
#include "Sample.h"
#include "Morph.h"
#include "Pattern.h"
#include <algorithm>
#include <complex>

namespace rave::slicer
{
// ------------------------------------------------------------------------------------------ FFT (radix-2, klein und ausreichend)
inline void fft(std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.f * juce::MathConstants<float>::pi / (float) len;
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<float> w(1.f, 0.f);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl;
            }
        }
    }
}

inline std::vector<float> toMono(const juce::AudioBuffer<float>& b)
{
    std::vector<float> m((size_t) b.getNumSamples(), 0.f);
    const int nch = b.getNumChannels();
    for (int c = 0; c < nch; ++c) { const float* s = b.getReadPointer(c); for (size_t i = 0; i < m.size(); ++i) m[i] += s[i] / (float) nch; }
    return m;
}

// ------------------------------------------------------------------------------------------ Onset-Erkennung
// sensitivity 0..1 (hoeher = mehr Treffer). Rueckgabe: sortierte Samplepositionen (auf Nulldurchgang gerastet).
inline std::vector<int> detectOnsets(const juce::AudioBuffer<float>& buf, double rate, float sensitivity, double minGapMs = 35.0)
{
    const auto x = toMono(buf);
    const int len = (int) x.size();
    constexpr int N = 1024, hop = 256, K = N / 2;
    if (len < N * 2) return {};

    std::vector<float> win((size_t) N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos(2.f * juce::MathConstants<float>::pi * (float) i / (float) N);

    const int frames = (len - N) / hop + 1;
    std::vector<float> flux((size_t) frames, 0.f), prevL((size_t) K, 0.f);
    std::vector<std::complex<float>> fr((size_t) N);
    for (int t = 0; t < frames; ++t)
    {
        for (int i = 0; i < N; ++i) fr[(size_t) i] = { x[(size_t) (t * hop + i)] * win[(size_t) i], 0.f };
        fft(fr);
        float f = 0.f;
        for (int k = 2; k < K; ++k)   // unterhalb ~2 Bins (< 90 Hz bei 44.1k) ist Gleichanteil/Rumpeln -> ignorieren
        {
            const float mag = std::abs(fr[(size_t) k]) / (float) K;
            const float l = std::log1p(1000.f * mag);
            if (t > 0) f += juce::jmax(0.f, l - prevL[(size_t) k]);
            prevL[(size_t) k] = l;
        }
        flux[(size_t) t] = f / (float) K;
    }

    // Adaptive Schwelle: lokaler Median + Anteil des 95-Perzentils
    std::vector<float> sorted(flux); std::sort(sorted.begin(), sorted.end());
    const float p95 = sorted[(size_t) ((sorted.size() - 1) * 0.95)];
    if (p95 < 1.0e-6f) return {};
    const float s = juce::jlimit(0.f, 1.f, sensitivity);
    const float delta = (0.40f - 0.34f * s) * p95;
    const int halfW = (int) (0.12 * rate / hop);
    std::vector<int> picks;
    const int minGapFrames = juce::jmax(1, (int) (minGapMs * 0.001 * rate / hop));
    float lastVal = 0.f;
    for (int t = 1; t < frames - 1; ++t)
    {
        std::vector<float> w;
        for (int k = juce::jmax(0, t - halfW); k <= juce::jmin(frames - 1, t + halfW); ++k) w.push_back(flux[(size_t) k]);
        std::nth_element(w.begin(), w.begin() + (long) w.size() / 2, w.end());
        const float thr = w[w.size() / 2] + delta;
        bool localMax = true;
        for (int k = juce::jmax(0, t - 3); k <= juce::jmin(frames - 1, t + 3); ++k) if (flux[(size_t) k] > flux[(size_t) t]) { localMax = false; break; }
        if (! localMax || flux[(size_t) t] < thr) continue;
        if (! picks.empty() && t - picks.back() < minGapFrames)
        {
            if (flux[(size_t) t] > lastVal) { picks.back() = t; lastVal = flux[(size_t) t]; }
            continue;
        }
        picks.push_back(t); lastVal = flux[(size_t) t];
    }

    // Verfeinerung im Zeitbereich auf einer hochpass-gefilterten Kopie (Bass-Schwingungen taeuschen sonst Anstiege vor)
    constexpr int W = 32;
    std::vector<double> cum((size_t) len + 1, 0.0);
    {
        const double a = std::exp(-2.0 * juce::MathConstants<double>::pi * 600.0 / rate);
        double lp = 0.0;
        for (int i = 0; i < len; ++i) { lp = (1.0 - a) * x[(size_t) i] + a * lp; const double y = x[(size_t) i] - lp; cum[(size_t) i + 1] = cum[(size_t) i] + y * y; }
    }
    auto env = [&](int i) { const int a = juce::jmax(0, i - W / 2), b = juce::jmin(len, i + W / 2); return (float) std::sqrt(juce::jmax(0.0, cum[(size_t) b] - cum[(size_t) a]) / juce::jmax(1, b - a)); };
    std::vector<int> out;
    for (int t : picks)
    {
        const int c = t * hop + N / 2;
        const int lo = juce::jmax(W, c - N), hi = juce::jmin(len - W - 1, c + hop);
        int bestI = lo; float bestRise = -1.f;
        for (int i = lo; i <= hi; i += 4)
        {
            const float rise = env(i + 24) - env(i - 24);
            if (rise > bestRise) { bestRise = rise; bestI = i; }
        }
        // Tal vor dem Anstieg suchen (funktioniert auch, wenn ein lauter Vorgaenger noch ausklingt) ...
        int valley = bestI; float vmin = env(bestI);
        for (int i = bestI; i >= juce::jmax(W, bestI - 700); i -= 4) { const float e = env(i); if (e < vmin) { vmin = e; valley = i; } }
        // ... dann vom Anstiegsmaximum rueckwaerts bis der Pegel unter 25 % der Anstiegshoehe faellt (naechster Durchgang an der Spitze)
        const float peakEnv = env(bestI + 24);
        const float level = vmin + 0.25f * juce::jmax(0.f, peakEnv - vmin);
        int on = bestI + 24;
        while (on > valley && env(on) > level) on -= 2;
        on = juce::jmax(valley, on - 8);
        // auf Nulldurchgang rasten (bis 64 Samples rueckwaerts)
        for (int k = on; k > juce::jmax(1, on - 64); --k)
            if ((x[(size_t) k - 1] <= 0.f) != (x[(size_t) k] <= 0.f)) { on = k; break; }
        on = juce::jlimit(0, len - 1, on);
        if (! out.empty() && on - out.back() < (int) (minGapMs * 0.001 * rate * 0.5)) continue;
        out.push_back(on);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// ------------------------------------------------------------------------------------------ Slice-Modell
struct SliceOp
{
    int slice = 0;          // Index der Quell-Slice
    bool reverse = false;
    float semis = 0.f;      // Tonhoehe (mit Laengenaenderung, wie Plattenspieler)
    float stretch = 1.f;    // Zeitdehnung ohne Tonhoehenaenderung
    int stutter = 1;        // Wiederholungen (1 = normal) - jede Wiederholung wird auf 1/stutter gekuerzt
    float gain = 1.f;
};

struct SliceModel
{
    SamplePtr source;
    std::vector<int> markers;   // aufsteigend; Slice i = [markers[i], markers[i+1]) ; letzte Slice endet am Sampleende
    int numSlices() const { return markers.empty() ? 0 : (int) markers.size(); }
    int startOf(int i) const { return markers[(size_t) i]; }
    int endOf(int i) const { return i + 1 < (int) markers.size() ? markers[(size_t) i + 1] : (source ? source->length() : 0); }

    void setMarkers(std::vector<int> m)
    {
        std::sort(m.begin(), m.end()); m.erase(std::unique(m.begin(), m.end()), m.end());
        const int len = source ? source->length() : 0;
        m.erase(std::remove_if(m.begin(), m.end(), [len](int v) { return v < 0 || v >= len; }), m.end());
        if (m.empty() || m.front() != 0) m.insert(m.begin(), 0);
        markers = std::move(m);
    }
    void detect(float sensitivity) { if (source) setMarkers(detectOnsets(source->data, source->rate, sensitivity)); }
    void sliceEqual(int n) { if (! source) return; std::vector<int> m; for (int i = 0; i < juce::jlimit(1, 64, n); ++i) m.push_back((int) ((double) source->length() * i / n)); setMarkers(m); }
    bool addMarker(int pos, int minDist = 64)
    {
        if (! source || pos <= 0 || pos >= source->length()) return false;
        for (int m : markers) if (std::abs(m - pos) < minDist) return false;
        auto m = markers; m.push_back(pos); setMarkers(m); return true;
    }
    bool removeMarker(int idx) { if (idx <= 0 || idx >= numSlices()) return false; markers.erase(markers.begin() + idx); return true; }   // Marker 0 bleibt
    bool moveMarker(int idx, int pos)
    {
        if (idx <= 0 || idx >= numSlices() || ! source) return false;
        const int lo = markers[(size_t) idx - 1] + 32, hi = (idx + 1 < numSlices() ? markers[(size_t) idx + 1] : source->length()) - 32;
        if (lo > hi) return false;
        markers[(size_t) idx] = juce::jlimit(lo, hi, pos); return true;
    }
    int sliceAt(int pos) const { int r = 0; for (int i = 0; i < numSlices(); ++i) if (markers[(size_t) i] <= pos) r = i; return r; }
};

// ------------------------------------------------------------------------------------------ Slice-Verarbeitung
inline juce::AudioBuffer<float> extractSlice(const Sample& s, int a, int b)
{
    a = juce::jlimit(0, s.length(), a); b = juce::jlimit(a, s.length(), b);
    juce::AudioBuffer<float> out(s.channels(), juce::jmax(1, b - a));
    out.clear();
    for (int c = 0; c < s.channels(); ++c) out.copyFrom(c, 0, s.data, c, a, b - a);
    // kurze Ein-/Ausblendung gegen Klicks (nur wenige Samples, Transienten bleiben erhalten)
    const int f = juce::jmin(out.getNumSamples() / 2, (int) (0.0005 * s.rate));
    if (f > 1) { out.applyGainRamp(0, f, 0.f, 1.f); out.applyGainRamp(out.getNumSamples() - f, f, 1.f, 0.f); }
    return out;
}

inline juce::AudioBuffer<float> processSlice(const Sample& src, int a, int b, const SliceOp& op)
{
    auto buf = extractSlice(src, a, b);
    if (op.reverse) buf.reverse(0, buf.getNumSamples());
    if (std::abs(op.stretch - 1.f) > 0.001f) buf = morph::wsolaStretch(buf, op.stretch, src.rate);
    if (std::abs(op.semis) > 0.005f) buf = morph::resampleBy(buf, std::pow(2.0, op.semis / 12.0));
    if (op.stutter > 1)
    {
        const int piece = juce::jmax(16, buf.getNumSamples() / op.stutter);
        juce::AudioBuffer<float> out(buf.getNumChannels(), piece * op.stutter);
        for (int r = 0; r < op.stutter; ++r)
            for (int c = 0; c < buf.getNumChannels(); ++c) out.copyFrom(c, r * piece, buf, c, 0, juce::jmin(piece, buf.getNumSamples()));
        const int f = juce::jmin(piece / 4, (int) (0.001 * src.rate));
        for (int r = 0; r < op.stutter; ++r)
        {
            if (f > 1) { out.applyGainRamp(r * piece, f, 0.f, 1.f); out.applyGainRamp((r + 1) * piece - f, f, 1.f, 0.f); }
        }
        buf = std::move(out);
    }
    if (std::abs(op.gain - 1.f) > 0.001f) buf.applyGain(op.gain);
    return buf;
}

// Alle Slices in der Reihenfolge `ops` hintereinander (Neuanordnung)
inline SamplePtr renderArrangement(const SliceModel& m, const std::vector<SliceOp>& ops, const juce::String& name)
{
    if (! m.source || ops.empty()) return nullptr;
    std::vector<juce::AudioBuffer<float>> parts;
    int total = 0;
    for (const auto& op : ops)
    {
        if (op.slice < 0 || op.slice >= m.numSlices()) continue;
        parts.push_back(processSlice(*m.source, m.startOf(op.slice), m.endOf(op.slice), op));
        total += parts.back().getNumSamples();
    }
    if (parts.empty()) return nullptr;
    juce::AudioBuffer<float> out(m.source->channels(), total); out.clear();
    int pos = 0;
    for (auto& p : parts)
    {
        for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom(c, pos, p, juce::jmin(c, p.getNumChannels() - 1), 0, p.getNumSamples());
        pos += p.getNumSamples();
    }
    return makeSample(out, m.source->rate, name);
}

// Zufaellige, aber reproduzierbare Neuordnung (Seed). Die erste Slice bleibt vorn, wenn keepFirst.
inline std::vector<SliceOp> shuffledOps(int n, int seed, bool keepFirst = true)
{
    std::vector<int> idx; for (int i = 0; i < n; ++i) idx.push_back(i);
    // Eigenes Fisher-Yates mit festem Zufallsgenerator: std::shuffle liefert je nach Standardbibliothek (MSVC/libstdc++) andere Ergebnisse.
    uint64_t state = (uint64_t) (unsigned) seed * 0x9E3779B97F4A7C15ull + 0x1234567ull;
    auto next = [&state]() { state += 0x9E3779B97F4A7C15ull; uint64_t z = state; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); };
    const int first = keepFirst ? 1 : 0;
    for (int i = n - 1; i > first; --i) { const int j = first + (int) (next() % (uint64_t) (i - first + 1)); std::swap(idx[(size_t) i], idx[(size_t) j]); }
    std::vector<SliceOp> ops; for (int i : idx) { SliceOp o; o.slice = i; ops.push_back(o); }
    return ops;
}

// Slices auf Pads verteilen: Slice i -> Pad firstPad+i (max. 16). Jede Slice wird eigenes Sample (kleiner State, einzeln bearbeitbar).
inline std::vector<std::pair<int, SamplePtr>> slicesToPads(const SliceModel& m, int firstPad, const juce::String& baseName)
{
    std::vector<std::pair<int, SamplePtr>> out;
    if (! m.source) return out;
    for (int i = 0; i < m.numSlices() && firstPad + i < kPads; ++i)
    {
        auto b = extractSlice(*m.source, m.startOf(i), m.endOf(i));
        out.emplace_back(firstPad + i, makeSample(b, m.source->rate, baseName + " " + juce::String(i + 1).paddedLeft('0', 2)));
    }
    return out;
}

// Slice-Beginne auf ein Raster legen: `loopSamples` (<= 0: ganzes Sample) werden auf `steps` Schritte abgebildet.
// Slice i liegt auf Pad firstPad+i. Rueckgabe: Pattern, das den Break aus den Slices nachspielt.
inline PlainPattern slicesToPattern(const SliceModel& m, int firstPad, int steps, int loopSamples = 0)
{
    PlainPattern p; p.length = juce::jlimit(1, kMaxSteps, steps);
    if (! m.source || m.source->length() < 2) return p;
    const double loop = loopSamples > 0 ? (double) loopSamples : (double) m.source->length();
    for (int i = 0; i < m.numSlices() && firstPad + i < kPads; ++i)
    {
        const int st = (int) std::lround((double) m.startOf(i) / loop * p.length);
        if (st >= 0 && st < p.length) { Step s; s.vel = 110; p.s[firstPad + i][st] = s; }
    }
    return p;
}
} // namespace rave::slicer
