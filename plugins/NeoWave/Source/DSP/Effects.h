#pragma once
// Effektkette (stereo, 32-Sample-Chunks):
// Saturator -> Mod-FX -> Frequency Shifter -> Delay -> Reverb/Shimmer -> Multiband -> Transient -> Compressor -> Widener
#include "../Params.h"
#include "DspCore.h"
#include <vector>

namespace nw
{
inline float dbToLin(float db) noexcept { return std::pow(10.f, db * 0.05f); }

// ---------------------------------------------------------------- Saturator
struct Saturator
{
    float hold[2] {}, cnt[2] {};

    void process(float* const* io, int n, int type, float amt, float mix) noexcept
    {
        if (type == 0 || mix < 0.001f) return;
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = io[ch];
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                float y = x;
                switch (type)
                {
                    case 1: { const float g = 1.f + amt * 9.f; y = fastTanh(x * g) / (0.6f + 0.4f * g); break; }
                    case 2: { const float g = 1.f + amt * 40.f; y = std::atan(x * g) * (2.f / pi) * 0.7f; break; }
                    case 3: y = std::sin(x * (1.f + amt * 6.f) * pi * 0.5f) * 0.8f; break;
                    default:
                    {
                        const float levels = std::pow(2.f, 15.f - amt * 12.f);
                        const float factor = 1.f + amt * 24.f;
                        cnt[ch] += 1.f;
                        if (cnt[ch] >= factor) { cnt[ch] -= factor; hold[ch] = std::round(x * levels) / levels; }
                        y = hold[ch];
                        break;
                    }
                }
                d[i] = x + (y - x) * mix;
            }
        }
    }
};

// ---------------------------------------------------------------- Mod-FX (Chorus / Dimension / Flanger / Phaser)
struct ModFx
{
    static constexpr int kSize = 16384, kMask = kSize - 1;
    float sr = 44100.f;
    std::vector<float> buf[2];
    int wp = 0;
    float ph = 0.f;
    float apState[2][6] {};
    float fbState[2] {};

    void prepare(double rate) { sr = (float) rate; for (auto& b : buf) b.assign(kSize, 0.f); wp = 0; ph = 0; }
    void reset() noexcept
    {
        for (auto& b : buf) std::fill(b.begin(), b.end(), 0.f);
        std::memset(apState, 0, sizeof(apState)); fbState[0] = fbState[1] = 0.f;
    }

    inline float readDelay(int ch, float delaySamples) const noexcept
    {
        float rp = (float) wp - delaySamples + (float) kSize;
        const int i = (int) rp;
        const float fr = rp - (float) i;
        const float a = buf[ch][(size_t) (i & kMask)], b = buf[ch][(size_t) ((i + 1) & kMask)];
        return a + (b - a) * fr;
    }

    void process(float* const* io, int n, int type, float rate, float depth, float fb, float mix) noexcept
    {
        if (type == 0 || mix < 0.001f) return;
        const float ms = sr * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            ph += rate / sr; if (ph >= 1.f) ph -= 1.f;
            float in[2] = { io[0][i], io[1][i] }, wet[2] = { 0, 0 };

            for (int ch = 0; ch < 2; ++ch)
            {
                const float p = ph + (ch ? 0.25f : 0.f);
                const float s1 = std::sin(twoPi * p), s2 = std::sin(twoPi * (p + 0.5f));
                switch (type)
                {
                    case 1: // Chorus: zwei Stimmen
                    {
                        const float d1 = (14.f + s1 * depth * 6.f) * ms, d2 = (17.f + s2 * depth * 6.f) * ms;
                        wet[ch] = 0.5f * (readDelay(ch, d1) + readDelay(ch, d2));
                        break;
                    }
                    case 2: // Dimension: feste, sehr langsame Modulation + Kreuzkopplung
                    {
                        const float sl = std::sin(twoPi * (p * 0.25f));
                        const float d1 = (9.f + sl * depth * 1.5f) * ms, d2 = (13.f - sl * depth * 1.5f) * ms;
                        wet[ch] = 0.65f * readDelay(ch, d1) + 0.35f * readDelay(1 - ch, d2);
                        break;
                    }
                    case 3: // Flanger mit Feedback
                    {
                        const float d = (0.6f + (0.5f + 0.5f * s1) * depth * 6.f) * ms;
                        wet[ch] = readDelay(ch, d);
                        break;
                    }
                    default: // Phaser: 6 Allpass-Stufen
                    {
                        const float f = 200.f * std::exp2((0.5f + 0.5f * s1) * (2.f + depth * 3.f));
                        const float t = std::tan(pi * juce::jmin(f, sr * 0.4f) / sr);
                        const float c = (t - 1.f) / (t + 1.f);
                        float x = in[ch] + fbState[ch] * fb;
                        for (int s = 0; s < 6; ++s)
                        {
                            const float y = c * x + apState[ch][s];
                            apState[ch][s] = x - c * y;
                            x = y;
                        }
                        fbState[ch] = x;
                        wet[ch] = x;
                        break;
                    }
                }
            }

            for (int ch = 0; ch < 2; ++ch)
            {
                float w = wet[ch];
                if (type == 3) { buf[ch][(size_t) wp] = in[ch] + w * fb * 0.9f; }
                else           { buf[ch][(size_t) wp] = in[ch]; }
                if (type == 4) io[ch][i] = in[ch] + (w - in[ch]) * mix * 0.5f - 0.0f;
                else           io[ch][i] = in[ch] * (1.f - mix * 0.5f) + w * mix * (type == 3 ? 0.7f : 1.f);
            }
            wp = (wp + 1) & kMask;
        }
    }
};

// ---------------------------------------------------------------- Frequency Shifter (Hilbert via Allpass-Paare)
struct FreqShifter
{
    float sr = 44100.f, ph = 0.f;
    struct Chain
    {
        float x1[4] {}, x2[4] {}, y1[4] {}, y2[4] {};
        float tick(float x, const float* a2) noexcept
        {
            for (int k = 0; k < 4; ++k)
            {
                const float y = a2[k] * (x + y2[k]) - x2[k];
                x2[k] = x1[k]; x1[k] = x;
                y2[k] = y1[k]; y1[k] = y;
                x = y;
            }
            return x;
        }
    };
    Chain ci[2], cq[2];
    float qDelay[2] {};

    static constexpr float A1[4] = { 0.6923878f * 0.6923878f, 0.9360654322959f * 0.9360654322959f,
                                     0.9882295226860f * 0.9882295226860f, 0.9987488452737f * 0.9987488452737f };
    static constexpr float A2[4] = { 0.4021921162426f * 0.4021921162426f, 0.8561710882420f * 0.8561710882420f,
                                     0.9722909545651f * 0.9722909545651f, 0.9952884791278f * 0.9952884791278f };

    void prepare(double rate) { sr = (float) rate; *this = FreqShifter(); sr = (float) rate; }

    void process(float* const* io, int n, float shift, float mix) noexcept
    {
        if (mix < 0.001f) return;
        for (int i = 0; i < n; ++i)
        {
            ph += shift / sr;
            ph -= std::floor(ph);
            const float c = std::cos(twoPi * ph), s = std::sin(twoPi * ph);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = io[ch][i];
                const float I = ci[ch].tick(x, A1);
                const float Q = cq[ch].tick(qDelay[ch], A2);
                qDelay[ch] = x;
                const float y = I * c - Q * s;
                io[ch][i] = x + (y - x) * mix;
            }
        }
    }
};

// ---------------------------------------------------------------- Delay (Stereo / Ping Pong / Tape)
struct DelayFx
{
    float sr = 44100.f;
    std::vector<float> buf[2];
    int size = 1, wp = 0;
    float curL = 4410.f, curR = 4410.f, lpState[2] {}, wow = 0.f, flut = 0.f;

    void prepare(double rate)
    {
        sr = (float) rate;
        size = (int) (rate * 2.2);
        for (auto& b : buf) b.assign((size_t) size, 0.f);
        wp = 0; curL = curR = 0.375f * sr; lpState[0] = lpState[1] = 0;
    }
    void reset() noexcept { for (auto& b : buf) std::fill(b.begin(), b.end(), 0.f); lpState[0] = lpState[1] = 0.f; }

    inline float rd(int ch, float d) const noexcept
    {
        float rp = (float) wp - d;
        while (rp < 0.f) rp += (float) size;
        const int i = (int) rp;
        const float fr = rp - (float) i;
        const float a = buf[ch][(size_t) (i % size)], b = buf[ch][(size_t) ((i + 1) % size)];
        return a + (b - a) * fr;
    }

    void process(float* const* io, int n, int type, float time, float fb, float tone, float mix) noexcept
    {
        if (type == 0 || mix < 0.001f) return;
        float tL = time * sr, tR = time * sr;
        if (type == 1) tR = tL * 0.75f;
        const float lpCoef = 1.f - std::exp(-twoPi * (800.f * std::pow(15.f, tone)) / sr);
        for (int i = 0; i < n; ++i)
        {
            curL += (tL - curL) * 0.0006f;
            curR += (tR - curR) * 0.0006f;
            float dl = curL, dr = curR;
            if (type == 3)
            {
                wow += 0.6f / sr; if (wow > 1.f) wow -= 1.f;
                flut += 5.5f / sr; if (flut > 1.f) flut -= 1.f;
                const float m = std::sin(twoPi * wow) * 0.0012f + std::sin(twoPi * flut) * 0.0003f;
                dl *= (1.f + m); dr *= (1.f + m);
            }
            dl = juce::jlimit(2.f, (float) size - 4.f, dl);
            dr = juce::jlimit(2.f, (float) size - 4.f, dr);
            const float wl = rd(0, dl), wr = rd(1, dr);

            lpState[0] += lpCoef * (wl - lpState[0]);
            lpState[1] += lpCoef * (wr - lpState[1]);
            float fl = lpState[0], fr = lpState[1];
            if (type == 3) { fl = fastTanh(fl * 1.3f); fr = fastTanh(fr * 1.3f); }

            const float inL = io[0][i], inR = io[1][i];
            if (type == 2) // Ping Pong: Mono-Eingang -> L, kreuzweise Rueckkopplung
            {
                buf[0][(size_t) wp] = (inL + inR) * 0.5f + fr * fb;
                buf[1][(size_t) wp] = fl * fb;
            }
            else
            {
                buf[0][(size_t) wp] = inL + fl * fb;
                buf[1][(size_t) wp] = inR + fr * fb;
            }
            wp = (wp + 1) % size;
            io[0][i] = inL + wl * mix;
            io[1][i] = inR + wr * mix;
        }
    }
};

// ---------------------------------------------------------------- Reverb + Shimmer (Oktav-Pitchshifter im Feedback)
struct PitchUp
{
    static constexpr int kSize = 8192, kMask = kSize - 1;
    float buf[kSize] {};
    int wp = 0;
    float phase = 0.f;
    float win = 2400.f;

    float process(float x) noexcept
    {
        buf[wp] = x;
        phase -= 1.f / win;               // Lesezeiger laeuft doppelt so schnell wie der Schreibzeiger
        if (phase < 0.f) phase += 1.f;
        float out = 0.f;
        for (int t = 0; t < 2; ++t)
        {
            float p = phase + 0.5f * (float) t; if (p >= 1.f) p -= 1.f;
            const float d = p * win + 2.f;
            float rp = (float) wp - d + (float) kSize;
            const int i = (int) rp;
            const float fr = rp - (float) i;
            const float v = buf[i & kMask] + (buf[(i + 1) & kMask] - buf[i & kMask]) * fr;
            const float w = std::sin(pi * p);
            out += v * w * w;
        }
        wp = (wp + 1) & kMask;
        return out;
    }
};

struct ReverbFx
{
    juce::Reverb rv;
    PitchUp shift[2];
    float shim[2][32] {};

    void prepare(double rate) { rv.setSampleRate(rate); reset(); }
    void reset() noexcept { rv.reset(); shift[0] = PitchUp(); shift[1] = PitchUp(); std::memset(shim, 0, sizeof(shim)); }

    void process(float* const* io, int n, int type, float size, float damp, float mix) noexcept
    {
        if (type == 0 || mix < 0.001f) return;
        juce::Reverb::Parameters p;
        p.roomSize = 0.35f + size * 0.64f;
        p.damping = damp;
        p.width = 1.f;
        p.freezeMode = 0.f;
        p.dryLevel = 0.f;
        p.wetLevel = 1.f;
        rv.setParameters(p);

        float dry[2][32], t[2][32];
        for (int i = 0; i < n; ++i)
        {
            dry[0][i] = io[0][i]; dry[1][i] = io[1][i];
            const float amt = type == 2 ? 0.3f : 0.f;
            t[0][i] = dry[0][i] + shim[0][i] * amt;
            t[1][i] = dry[1][i] + shim[1][i] * amt;
        }
        rv.processStereo(t[0], t[1], n);
        for (int i = 0; i < n; ++i)
            if (!std::isfinite(t[0][i]) || !std::isfinite(t[1][i])) { reset(); for (int j = 0; j < n; ++j) t[0][j] = t[1][j] = 0.f; break; }
        const float wetGain = 0.35f;
        for (int i = 0; i < n; ++i)
        {
            if (type == 2)
            {
                // begrenzte Rueckkopplung (tanh) -> auch bei hoher Schleifenverstaerkung stabil
                shim[0][i] = fastTanh(shift[0].process(t[0][i]) * 1.2f) * 0.5f;
                shim[1][i] = fastTanh(shift[1].process(t[1][i]) * 1.2f) * 0.5f;
            }
            io[0][i] = dry[0][i] * (1.f - mix * 0.3f) + t[0][i] * wetGain * mix * 2.f;
            io[1][i] = dry[1][i] * (1.f - mix * 0.3f) + t[1][i] * wetGain * mix * 2.f;
        }
    }
};

// ---------------------------------------------------------------- Dynamik
struct Multiband
{
    juce::dsp::LinkwitzRileyFilter<float> lo[2], hi[2], ap[2];
    float env[3] {};
    float sr = 44100.f;

    void prepare(double rate)
    {
        sr = (float) rate;
        juce::dsp::ProcessSpec spec { rate, 32, 1 };
        for (int c = 0; c < 2; ++c)
        {
            lo[c].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);  lo[c].setCutoffFrequency(200.f);  lo[c].prepare(spec);
            hi[c].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);  hi[c].setCutoffFrequency(2500.f); hi[c].prepare(spec);
            ap[c].setType(juce::dsp::LinkwitzRileyFilterType::allpass);  ap[c].setCutoffFrequency(2500.f); ap[c].prepare(spec);
        }
    }

    void process(float* const* io, int n, float amt) noexcept
    {
        if (amt < 0.001f) return;
        const float thr = dbToLin(-8.f - amt * 22.f);
        const float ratio = 1.f + amt * 4.f;
        const float att = std::exp(-1.f / (0.008f * sr)), rel = std::exp(-1.f / (0.12f * sr));
        const float makeup = dbToLin((8.f + amt * 22.f) * (1.f - 1.f / ratio) * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            float bandL[3], bandR[3];
            for (int ch = 0; ch < 2; ++ch)
            {
                const float x = io[ch][i];
                float low, rest;
                lo[ch].processSample(0, x, low, rest);     // low | rest
                float mid, high;
                hi[ch].processSample(0, rest, mid, high);   // mid | high
                low = ap[ch].processSample(0, low);          // Phasengleichheit
                (ch == 0 ? bandL : bandR)[0] = low;
                (ch == 0 ? bandL : bandR)[1] = mid;
                (ch == 0 ? bandL : bandR)[2] = high;
            }
            float outL = 0.f, outR = 0.f;
            for (int b = 0; b < 3; ++b)
            {
                const float lvl = juce::jmax(std::abs(bandL[b]), std::abs(bandR[b]));
                env[b] = lvl > env[b] ? lvl + (env[b] - lvl) * att : lvl + (env[b] - lvl) * rel;
                float g = 1.f;
                if (env[b] > thr) g = std::pow(env[b] / thr, 1.f / ratio - 1.f);
                outL += bandL[b] * g; outR += bandR[b] * g;
            }
            io[0][i] = outL * makeup;
            io[1][i] = outR * makeup;
        }
    }
};

struct TransientShaper
{
    float fast = 0.f, slow = 0.f, sr = 44100.f;
    void process(float* const* io, int n, float amt) noexcept
    {
        if (std::abs(amt) < 0.01f) return;
        const float fa = std::exp(-1.f / (0.0005f * sr)), fr = std::exp(-1.f / (0.03f * sr));
        const float sa = std::exp(-1.f / (0.02f * sr)),  sRel = std::exp(-1.f / (0.15f * sr));
        for (int i = 0; i < n; ++i)
        {
            const float lvl = juce::jmax(std::abs(io[0][i]), std::abs(io[1][i]));
            fast = lvl > fast ? lvl + (fast - lvl) * fa : lvl + (fast - lvl) * fr;
            slow = lvl > slow ? lvl + (slow - lvl) * sa : lvl + (slow - lvl) * sRel;
            const float ratio = fast / (slow + 1e-5f);
            const float g = 1.f + amt * juce::jlimit(0.f, 2.f, ratio - 1.f) * 0.8f;
            io[0][i] *= g; io[1][i] *= g;
        }
    }
};

struct Compressor
{
    float env = 0.f, sr = 44100.f;
    void process(float* const* io, int n, float amt) noexcept
    {
        if (amt < 0.001f) return;
        const float thr = dbToLin(-6.f - amt * 24.f);
        const float ratio = 2.f + amt * 6.f;
        const float att = std::exp(-1.f / (0.005f * sr)), rel = std::exp(-1.f / (0.1f * sr));
        const float makeup = dbToLin((6.f + amt * 24.f) * (1.f - 1.f / ratio) * 0.5f);
        for (int i = 0; i < n; ++i)
        {
            const float lvl = juce::jmax(std::abs(io[0][i]), std::abs(io[1][i]));
            env = lvl > env ? lvl + (env - lvl) * att : lvl + (env - lvl) * rel;
            float g = 1.f;
            if (env > thr) g = std::pow(env / thr, 1.f / ratio - 1.f);
            io[0][i] *= g * makeup; io[1][i] *= g * makeup;
        }
    }
};

struct FxChain
{
    Saturator sat; ModFx mod; FreqShifter fs; DelayFx dl; ReverbFx rv;
    Multiband mb; TransientShaper ts; Compressor cmp;

    void reset() noexcept
    {
        mod.reset(); dl.reset(); rv.reset();
        fs.ci[0] = fs.ci[1] = {}; fs.cq[0] = fs.cq[1] = {}; fs.qDelay[0] = fs.qDelay[1] = 0.f;
        mb.env[0] = mb.env[1] = mb.env[2] = 0.f; ts.fast = ts.slow = 0.f; cmp.env = 0.f;
    }

    void prepare(double sr)
    {
        mod.prepare(sr); fs.prepare(sr); dl.prepare(sr); rv.prepare(sr); mb.prepare(sr);
        ts.sr = (float) sr; cmp.sr = (float) sr;
        sat = Saturator();
    }

    // io[0], io[1] = L/R, n <= 32
    void process(float* const* io, int n, const Snap& S) noexcept
    {
        sat.process(io, n, S.ch(P::satType), S[P::satAmt], S[P::satMix]);
        mod.process(io, n, S.ch(P::mfType), S[P::mfRate], S[P::mfDepth], S[P::mfFb], S[P::mfMix]);
        fs.process(io, n, S[P::fsShift], S[P::fsMix]);
        dl.process(io, n, S.ch(P::dlType), S[P::dlTime], S[P::dlFb], S[P::dlTone], S[P::dlMix]);
        rv.process(io, n, S.ch(P::rvType), S[P::rvSize], S[P::rvDamp], S[P::rvMix]);
        mb.process(io, n, S[P::mbAmt]);
        ts.process(io, n, S[P::tsAmt]);
        cmp.process(io, n, S[P::cmpAmt]);

        const float w = S[P::width];
        if (std::abs(w - 1.f) > 0.001f)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (io[0][i] + io[1][i]), s = 0.5f * (io[0][i] - io[1][i]) * w;
                io[0][i] = m + s; io[1][i] = m - s;
            }
    }
};
} // namespace nw
