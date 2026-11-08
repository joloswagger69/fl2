#pragma once
#include "../Params.h"
#include "DspCore.h"
#include "Tables.h"

namespace nw
{
inline float polyBlep(float t, float dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

struct Grain { float age = 1e9f, len = 1.f, ph = 0.f, frame = 0.f; };

struct OscState
{
    float ph = 0.f, phM = 0.f, gTimer = 0.f;
    Grain g[4];
};

struct OscParams
{
    int engine = 0, wave = 0, table = 0;
    float morph = 0.f, color = 0.f;
};

// Pro Chunk berechnete Modulationsquellen, die global (voiceunabhaengig) sind
struct GlobalMod
{
    float lfo[3] {};
    float step = 0.f, wheel = 0.f, aftertouch = 0.f, bend = 0.f;
    float macro[4] {};
};

// Ein Oszillator-Sample. Engines: 0 Analog, 1 Wavetable, 2 FM, 3 Phase Distortion, 4 Granular
inline float oscRender(OscState& st, const OscParams& o, float dt, int mip, float pm,
                       Rng& rng, const WaveTables& wt, float sr) noexcept
{
    float p = st.ph + pm;
    p -= std::floor(p);

    switch (o.engine)
    {
        case 0: // Analog Modeling (PolyBLEP)
        {
            float v = 0.f;
            switch (o.wave)
            {
                case 0:
                {
                    v = 2.f * p - 1.f - polyBlep(p, dt);
                    if (o.morph > 0.001f)
                    {
                        float p2 = p * 2.f; p2 -= std::floor(p2);
                        const float v2 = 2.f * p2 - 1.f - polyBlep(p2, juce::jmin(0.45f, dt * 2.f));
                        v = (v + o.morph * v2) / (1.f + o.morph * 0.7f);
                    }
                    break;
                }
                case 1:
                {
                    const float pw = 0.5f - 0.45f * o.morph;
                    v = p < pw ? 1.f : -1.f;
                    v += polyBlep(p, dt);
                    float t2 = p - pw; if (t2 < 0.f) t2 += 1.f;
                    v -= polyBlep(t2, dt);
                    v -= (2.f * pw - 1.f);
                    break;
                }
                case 2:
                {
                    const float sk = 0.5f + 0.49f * o.morph;
                    v = p < sk ? -1.f + 2.f * p / sk : 1.f - 2.f * (p - sk) / (1.f - sk);
                    break;
                }
                default:
                {
                    const float s = std::sin(twoPi * p);
                    v = std::sin(twoPi * p + o.morph * 2.5f * s);
                    break;
                }
            }
            if (o.color > 0.001f)
            {
                const float g = 1.f + o.color * 6.f;
                v = fastTanh(v * g) / fastTanh(g * 0.9f);
            }
            return v;
        }
        case 1: // Wavetable (bandbegrenzt, Morph zwischen Frames, Color = Phase-Bend)
        {
            float pw = p + o.color * 0.2f * std::sin(twoPi * p);
            pw -= std::floor(pw);
            return wt.read(o.table, o.morph, mip, pw);
        }
        case 2: // 2-Operator FM, Color = Frequenzverhaeltnis, Morph = Index
        {
            const float ratio = 0.5f + o.color * 7.5f;
            const float idx = o.morph * 8.f * juce::jlimit(0.1f, 1.f, 1.f - dt * 5.f);
            const float mp = st.phM + pm * ratio;
            return std::sin(twoPi * (p + idx * std::sin(twoPi * mp)));
        }
        case 3: // Phase Distortion (CZ-Stil) + resonante Variante
        {
            const float m = o.morph * juce::jlimit(0.2f, 1.f, 1.f - dt * 6.f);
            const float d = 0.5f - 0.48f * m;
            const float pp = p < d ? 0.5f * p / d : 0.5f + 0.5f * (p - d) / (1.f - d);
            float v = std::cos(twoPi * pp);
            if (o.color > 0.001f)
            {
                const float res = std::sin(twoPi * p * (1.f + o.color * 10.f)) * (1.f - p) * 1.3f;
                v += (res - v) * o.color;
            }
            return v;
        }
        default: // Granular: Wolke aus Hann-Grains, die Wavetable-Zyklen abspielen
        {
            st.gTimer -= 1.f;
            if (st.gTimer <= 0.f)
            {
                int idx = 0; float oldest = -1.f;
                for (int i = 0; i < 4; ++i)
                {
                    if (st.g[i].age >= st.g[i].len) { idx = i; oldest = 1e30f; break; }
                    if (st.g[i].age > oldest) { oldest = st.g[i].age; idx = i; }
                }
                auto& g = st.g[idx];
                const float lenSec = 0.1f + (0.006f - 0.1f) * o.color;
                g.len = lenSec * sr * (0.7f + 0.6f * rng.next());
                g.age = 0.f;
                g.ph = rng.next();
                g.frame = juce::jlimit(0.f, 1.f, o.morph + (rng.next() - 0.5f) * (0.15f + 0.5f * o.color));
                st.gTimer = g.len * 0.5f * (0.8f + 0.4f * rng.next());
            }
            float sum = 0.f;
            for (auto& g : st.g)
            {
                if (g.age >= g.len) continue;
                const float w = 0.5f - 0.5f * std::cos(twoPi * g.age / g.len);
                sum += w * wt.read(o.table, g.frame, mip, g.ph);
                g.ph += dt; if (g.ph >= 1.f) g.ph -= 1.f;
                g.age += 1.f;
            }
            return sum * 0.85f;
        }
    }
}

inline bool oscAdvance(OscState& st, const OscParams& o, float dt, float& frac) noexcept
{
    st.ph += dt;
    if (o.engine == 2)
    {
        st.phM += dt * (0.5f + o.color * 7.5f);
        st.phM -= std::floor(st.phM);
    }
    if (st.ph >= 1.f)
    {
        st.ph -= 1.f;
        frac = st.ph / dt;
        return true;
    }
    return false;
}

class Voice
{
public:
    static constexpr int kMaxUni = 16;

    void prepare(double rate, const WaveTables* tables, int index)
    {
        sr = (float) rate;
        wt = tables;
        ampEnv.sr = fltEnv.sr = modEnv.sr = sr;
        filt.prepare(rate);
        rng.seed(1234u + (uint32_t) index * 7919u);
        for (int i = 0; i < 3; ++i) vlfo[i].init(99u + (uint32_t) (index * 3 + i));
    }

    bool isActive() const noexcept { return active; }
    bool isReleasing() const noexcept { return active && released; }
    int  getNote() const noexcept { return note; }
    uint32_t getAge() const noexcept { return age; }
    float getLevel() const noexcept { return ampEnv.v; }
    bool holdBySustain = false;

    void noteOn(int n, float vel, bool retrigger, float glideFrom, uint32_t stamp, const Snap& S)
    {
        const bool fresh = !active;
        note = n; velocity = vel; age = stamp; released = false; holdBySustain = false;
        targetNote = (float) n;
        if (fresh || retrigger)
        {
            curNote = (S[P::glide] > 0.0005f) ? glideFrom : (float) n;
            randVal = rng.bi();
            for (int i = 0; i < 3; ++i) if (S.ch(P::lfo1Key + i * 3) == 1) vlfo[i].init(rng.next() * 1e6f);
            if (fresh)
            {
                filt.reset();
                for (auto& o : oa) o = {};
                for (auto& o : ob) o = {};
                for (auto& o : oa) o.ph = rng.next();
                for (auto& o : ob) o.ph = rng.next();
                subPh = 0.f; pink[0] = pink[1] = pink[2] = 0.f; crack = 0.f;
            }
            ampEnv.noteOn(); fltEnv.noteOn(); modEnv.noteOn();
        }
        active = true;
    }

    void setNoteLegato(int n) noexcept { note = n; targetNote = (float) n; released = false; }

    void noteOff() noexcept
    {
        if (!active) return;
        released = true;
        ampEnv.noteOff(); fltEnv.noteOff(); modEnv.noteOff();
    }

    void kill() noexcept { active = false; ampEnv.reset(); fltEnv.reset(); modEnv.reset(); }

    void render(float* L, float* R, int n, const Snap& S, const GlobalMod& G) noexcept
    {
        if (!active) return;

        ampEnv.set(S[P::ampA], S[P::ampD], S[P::ampS], S[P::ampR]);
        fltEnv.set(S[P::fltA], S[P::fltD], S[P::fltS], S[P::fltR]);
        modEnv.set(S[P::modEnvA], S[P::modEnvD], S[P::modEnvS], S[P::modEnvR]);

        const float gl = S[P::glide];
        if (gl <= 0.0005f) curNote = targetNote;
        else curNote += (targetNote - curNote) * (1.f - std::exp(-(float) n / (gl * sr * 0.35f)));

        // ---- Modulationsquellen ----
        float src[sCount] {};
        for (int i = 0; i < 3; ++i)
        {
            const int base = P::lfo1Rate + i * 3;
            src[sLfo1 + i] = S.ch(base + 2) == 1 ? vlfo[i].tick(S[base], S.ch(base + 1), n, sr) : G.lfo[i];
        }
        src[sModEnv] = modEnvV; src[sStep] = G.step; src[sVel] = velocity;
        src[sWheel] = G.wheel; src[sAft] = G.aftertouch;
        src[sKey] = juce::jlimit(-1.f, 1.f, ((float) note - 60.f) / 36.f);
        src[sRand] = randVal;
        for (int i = 0; i < 4; ++i) src[sMac1 + i] = G.macro[i];
        src[sAmpEnv] = ampEnv.v; src[sFltEnv] = fltEnvV;

        float mod[dCount] {};
        for (int s = 0; s < 8; ++s)
        {
            const int base = P::mmSrc1 + s * 3;
            const int a = S.ch(base), d = S.ch(base + 1);
            if (a != 0 && d != 0) mod[d] += src[a] * S[base + 2];
        }

        // ---- Parameter dieses Chunks ----
        const int nUni = juce::jlimit(1, kMaxUni, S.ch(P::uniVoices));
        const float detCents = juce::jlimit(0.f, 1.f, S[P::uniDetune] + mod[dUniDet] * 0.5f) * 80.f;
        const float spread = S[P::uniSpread];

        OscParams A, B;
        A.engine = S.ch(P::aEngine); A.wave = S.ch(P::aWave); A.table = S.ch(P::aTable);
        A.morph = juce::jlimit(0.f, 1.f, S[P::aMorph] + mod[dAMorph]);
        A.color = juce::jlimit(0.f, 1.f, S[P::aColor] + mod[dAColor]);
        B.engine = S.ch(P::bEngine); B.wave = S.ch(P::bWave); B.table = S.ch(P::bTable);
        B.morph = juce::jlimit(0.f, 1.f, S[P::bMorph] + mod[dBMorph]);
        B.color = juce::jlimit(0.f, 1.f, S[P::bColor] + mod[dBColor]);

        const float bendSemi = G.bend * (float) S.ch(P::bendRange);
        const float base = curNote + bendSemi + mod[dPitch] * 24.f;
        const float pitchA = base + mod[dAPitch] * 24.f + (float) S.ch(P::aOct) * 12.f + (float) S.ch(P::aSemi) + S[P::aFine] * 0.01f;
        const float pitchB = base + mod[dBPitch] * 24.f + (float) S.ch(P::bOct) * 12.f + (float) S.ch(P::bSemi) + S[P::bFine] * 0.01f;

        float dtA[kMaxUni], dtB[kMaxUni], gl_[kMaxUni], gr_[kMaxUni];
        int mA[kMaxUni], mB[kMaxUni];
        const float norm = 1.f / std::sqrt((float) nUni);
        for (int u = 0; u < nUni; ++u)
        {
            const float pos = nUni > 1 ? ((float) u / (float) (nUni - 1)) * 2.f - 1.f : 0.f;
            const float cents = pos * detCents * 0.01f;
            dtA[u] = juce::jlimit(1e-5f, 0.45f, 440.f * std::exp2((pitchA + cents - 69.f) / 12.f) / sr);
            dtB[u] = juce::jlimit(1e-5f, 0.45f, 440.f * std::exp2((pitchB - cents * 0.8f - 69.f) / 12.f) / sr);
            mA[u] = WaveTables::mipFor(dtA[u]);
            mB[u] = WaveTables::mipFor(dtB[u]);
            const float ang = (pos * spread + 1.f) * 0.25f * pi;
            gl_[u] = std::cos(ang) * norm * 1.2f;
            gr_[u] = std::sin(ang) * norm * 1.2f;
        }

        const float lvA = juce::jlimit(0.f, 1.5f, S[P::aLevel] + mod[dALevel]);
        const float lvB = juce::jlimit(0.f, 1.5f, S[P::bLevel] + mod[dBLevel]);
        const float ring = juce::jlimit(0.f, 1.f, S[P::ringMod] + mod[dRing]);
        const float xfm = juce::jlimit(0.f, 1.f, S[P::crossFm] + mod[dXfm]);
        const bool sync = S.ch(P::bSync) == 1;
        const float subLv = juce::jlimit(0.f, 1.f, S[P::subLevel] + mod[dSub]);
        const float noiseLv = juce::jlimit(0.f, 1.f, S[P::noiseLevel] + mod[dNoise]);
        const int noiseType = S.ch(P::noiseType);
        const bool subSq = S.ch(P::subWave) == 1;
        const float subPitch = curNote + bendSemi + mod[dPitch] * 24.f - 12.f * (float) (S.ch(P::subOct) + 1);
        const float dtSub = juce::jlimit(1e-5f, 0.45f, 440.f * std::exp2((subPitch - 69.f) / 12.f) / sr);
        const float ampMod = juce::jlimit(0.f, 2.f, 1.f + mod[dAmp]);

        // Filter
        float cutoff = S[P::fCut] * std::exp2(S[P::fEnv] * 6.f * fltEnvV + mod[dCutoff] * 6.f
                                              + S[P::fKey] * ((float) note - 60.f) / 12.f);
        cutoff = juce::jlimit(20.f, 20000.f, cutoff);
        const float res = juce::jlimit(0.f, 1.f, S[P::fRes] + mod[dRes]);
        const float drive = juce::jlimit(0.f, 1.f, S[P::fDrive] + mod[dDrive]);
        filt.setup(S.ch(P::fType), cutoff, res, drive);

        const float gain = (0.3f + 0.7f * velocity) * 0.5f * ampMod;
        const float pmScale = 0.5f * xfm;

        for (int i = 0; i < n; ++i)
        {
            float l = 0.f, r = 0.f;
            for (int u = 0; u < nUni; ++u)
            {
                const float a = oscRender(oa[u], A, dtA[u], mA[u], 0.f, rng, *wt, sr);
                const float b = oscRender(ob[u], B, dtB[u], mB[u], pmScale * a, rng, *wt, sr);
                float frac = 0.f;
                if (oscAdvance(oa[u], A, dtA[u], frac) && sync)
                {
                    ob[u].ph = frac * dtB[u];
                    ob[u].phM = 0.f;
                }
                float fb = 0.f;
                oscAdvance(ob[u], B, dtB[u], fb);

                const float m = (a * lvA + b * lvB) * (1.f - 0.5f * ring) + ring * 1.5f * (a * lvA) * (b * lvB);
                l += m * gl_[u];
                r += m * gr_[u];
            }

            if (subLv > 0.0001f)
            {
                float sv;
                if (subSq) { sv = subPh < 0.5f ? 0.7f : -0.7f; sv += 0.7f * polyBlep(subPh, dtSub); float t2 = subPh + 0.5f; if (t2 >= 1.f) t2 -= 1.f; sv -= 0.7f * polyBlep(t2, dtSub); }
                else sv = std::sin(twoPi * subPh);
                subPh += dtSub; if (subPh >= 1.f) subPh -= 1.f;
                l += sv * subLv * 0.8f; r += sv * subLv * 0.8f;
            }
            if (noiseLv > 0.0001f)
            {
                float nz;
                const float w = rng.bi();
                switch (noiseType)
                {
                    case 0: nz = w * 0.6f; break;
                    case 1: pink[0] = 0.99765f * pink[0] + w * 0.0990460f; pink[1] = 0.96300f * pink[1] + w * 0.2965164f;
                            pink[2] = 0.57000f * pink[2] + w * 1.0526913f; nz = (pink[0] + pink[1] + pink[2] + w * 0.1848f) * 0.3f; break;
                    case 2: crack = crack * 0.92f + (rng.next() < 0.002f ? rng.bi() * 3.f : 0.f); nz = crack; break;
                    default: crack = crack * 0.995f + (rng.next() < 0.0004f ? rng.bi() * 4.f : 0.f); nz = std::sin(crack * 3.f) * 0.8f; break;
                }
                l += nz * noiseLv; r += nz * noiseLv;
            }

            l = filt.process(0, l);
            r = filt.process(1, r);

            const float a = ampEnv.next();
            fltEnvV = fltEnv.next();
            modEnvV = modEnv.next();
            if (!ampEnv.active()) { active = false; break; }

            L[i] += l * a * gain;
            R[i] += r * a * gain;
        }
    }

private:
    float sr = 44100.f;
    const WaveTables* wt = nullptr;
    bool active = false, released = false;
    int note = 60;
    uint32_t age = 0;
    float velocity = 1.f, curNote = 60.f, targetNote = 60.f, randVal = 0.f;
    float fltEnvV = 0.f, modEnvV = 0.f;
    float subPh = 0.f, pink[3] {}, crack = 0.f;

    Env ampEnv, fltEnv, modEnv;
    Lfo vlfo[3];
    OscState oa[kMaxUni], ob[kMaxUni];
    VoiceFilter filt;
    Rng rng;
};
} // namespace nw
