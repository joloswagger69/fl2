#pragma once
// Einfacher Vorhoer-Synth fuer die fuenf Teile (damit man die Ideen im Plugin hoeren kann; MIDI-Datei ist die Hauptausgabe).
#include <juce_audio_basics/juce_audio_basics.h>
#include "Project.h"

namespace mel
{
class PreviewSynth
{
public:
    struct Voice
    {
        bool active = false, held = false; int pitch = 60; float vel = 0.7f; bool accent = false;
        double freq = 440.0, targetFreq = 440.0, glide = 0.0;     // glide = Koeffizient pro Sample
        double ph1 = 0, ph2 = 0, phSub = 0;
        float env = 0.f; int stage = 0; /*0 A,1 D,2 S,3 R*/ float fenv = 0.f;
        float s1 = 0, s2 = 0;                                       // Filter-Zustaende
        long age = 0;
    };

    void prepare(double sampleRate) { sr = sampleRate; allOff(true); for (int p = 0; p < 5; ++p) cfg[(size_t) p] = makeCfg(p); }
    void allOff(bool hard = false) { for (auto& pv : voices) for (auto& v : pv) { if (hard) v = Voice(); else if (v.active) { v.held = false; v.stage = 3; } } }
    void partOff(int part, bool hard = false) { for (auto& v : voices[(size_t) part]) { if (hard) v = Voice(); else if (v.active) { v.held = false; v.stage = 3; } } }

    void noteOn(int part, int pitch, int vel, bool slideFromPrev)
    {
        auto& pv = voices[(size_t) part]; const auto& c = cfg[(size_t) part];
        if (c.mono)
        {
            Voice* held = nullptr; for (auto& v : pv) if (v.active && v.held) held = &v;
            if (held && slideFromPrev) { held->pitch = pitch; held->targetFreq = nhz(pitch); held->glide = 1.0 - std::exp(-1.0 / (c.glideSec * sr)); held->accent = vel > 110; held->vel = vel / 127.f; held->fenv = std::max(held->fenv, held->accent ? 1.f : 0.6f); return; }
            for (auto& v : pv) if (v.active && v.held) { v.held = false; v.stage = 3; }
        }
        Voice* t = nullptr; long oldest = -1;
        for (auto& v : pv) if (! v.active) { t = &v; break; }
        if (! t) { for (auto& v : pv) if (v.age > oldest) { oldest = v.age; t = &v; } }
        *t = Voice(); t->active = true; t->held = true; t->pitch = pitch; t->vel = vel / 127.f; t->accent = vel > 110;
        t->freq = t->targetFreq = nhz(pitch); t->glide = 1.0; t->fenv = 1.f; t->ph1 = (double) (pitch % 7) * 0.13;
    }
    void noteOff(int part, int pitch)
    {
        for (auto& v : voices[(size_t) part]) if (v.active && v.held && v.pitch == pitch) { v.held = false; v.stage = 3; return; }
    }

    // addiert in den Puffer (mono nach stereo)
    void render(int part, float* l, float* r, int n)
    {
        const auto& c = cfg[(size_t) part];
        for (auto& v : voices[(size_t) part])
        {
            if (! v.active) continue;
            for (int i = 0; i < n; ++i)
            {
                v.age++;
                v.freq += (v.targetFreq - v.freq) * v.glide;
                const double dt = v.freq / sr;
                // Huellkurve
                switch (v.stage)
                {
                    case 0: v.env += (float) (1.0 / (std::max(0.001, (double) c.attack) * sr)); if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } break;
                    case 1: v.env += (c.sustain - v.env) * (float) (1.0 - std::exp(-1.0 / (std::max(0.01, (double) c.decay) * sr))); if (std::abs(v.env - c.sustain) < 0.002f) v.stage = 2; break;
                    case 2: break;
                    default: v.env -= v.env * (float) (1.0 - std::exp(-1.0 / (std::max(0.005, (double) c.release) * sr))) + 1.0e-6f; if (v.env <= 0.0008f) { v.active = false; v.env = 0; } break;
                }
                v.fenv -= v.fenv * (float) (1.0 - std::exp(-1.0 / (std::max(0.02, (double) c.fdecay) * (v.accent ? 0.7 : 1.0) * sr)));
                // Oszillatoren (PolyBLEP-Saege / Rechteck)
                float o = 0.f;
                o += saw(v.ph1, dt);
                if (c.detune > 0.f) o += saw(v.ph2, dt * (1.0 + c.detune)) ; else v.ph2 = 0;
                if (c.pulse) o = 0.6f * o + 0.5f * pulse(v.phSub, dt, 0.4f);
                if (c.sub > 0.f) o += c.sub * std::sin((float) (v.ph1 * 6.28318530718));
                v.ph1 += dt; if (v.ph1 >= 1.0) v.ph1 -= 1.0; v.ph2 += dt * (1.0 + c.detune); if (v.ph2 >= 1.0) v.ph2 -= 1.0; v.phSub += dt; if (v.phSub >= 1.0) v.phSub -= 1.0;
                // Zustandsvariablen-Tiefpass
                float cut = c.cutoff * (1.f + c.fenvAmt * v.fenv * (v.accent ? 1.8f : 1.f)) * (0.7f + 0.6f * v.vel);
                cut = std::min(cut, (float) (sr * 0.40)); const float f = 2.f * std::sin(3.14159265f * cut / (float) sr);
                const float q = 1.f - c.res;
                v.s2 += f * v.s1; const float hp = o - v.s2 - q * v.s1; v.s1 += f * hp;
                if (! std::isfinite(v.s1) || ! std::isfinite(v.s2)) { v.s1 = v.s2 = 0; }
                const float g = c.gain * v.env * (0.35f + 0.65f * v.vel) * (v.accent ? 1.25f : 1.f);
                const float y = v.s2 * g;
                l[i] += y * c.panL; r[i] += y * c.panR;
            }
        }
    }
    bool anyActive() const { for (auto& pv : voices) for (auto& v : pv) if (v.active) return true; return false; }

private:
    struct Cfg { bool mono = false, pulse = false; float attack = 0.01f, decay = 0.2f, sustain = 0.8f, release = 0.2f, fdecay = 0.2f, cutoff = 3000.f, fenvAmt = 0.f, res = 0.1f, detune = 0.f, sub = 0.f, gain = 0.2f, panL = 0.7f, panR = 0.7f; double glideSec = 0.06; };
    static Cfg makeCfg(int p)
    {
        Cfg c;
        switch (p)
        {
            case 0: c.attack = 0.02f; c.decay = 0.3f; c.sustain = 0.8f; c.release = 0.25f; c.cutoff = 2400.f; c.detune = 0.006f; c.gain = 0.075f; c.panL = 0.85f; c.panR = 0.55f; break;
            case 1: c.pulse = true; c.attack = 0.006f; c.decay = 0.25f; c.sustain = 0.7f; c.release = 0.14f; c.cutoff = 3800.f; c.fenvAmt = 0.8f; c.gain = 0.11f; c.panL = 0.6f; c.panR = 0.85f; break;
            case 2: c.mono = true; c.attack = 0.004f; c.decay = 0.2f; c.sustain = 0.8f; c.release = 0.06f; c.cutoff = 480.f; c.fenvAmt = 1.6f; c.sub = 0.7f; c.gain = 0.2f; c.panL = c.panR = 0.7f; c.glideSec = 0.03; break;
            case 3: c.attack = 0.002f; c.decay = 0.16f; c.sustain = 0.25f; c.release = 0.08f; c.cutoff = 2200.f; c.fenvAmt = 1.8f; c.fdecay = 0.14f; c.gain = 0.09f; c.panL = 0.9f; c.panR = 0.5f; break;
            default: c.mono = true; c.attack = 0.002f; c.decay = 0.3f; c.sustain = 0.7f; c.release = 0.04f; c.cutoff = 360.f; c.fenvAmt = 5.f; c.fdecay = 0.17f; c.res = 0.78f; c.gain = 0.17f; c.panL = c.panR = 0.7f; c.glideSec = 0.055; break;
        }
        return c;
    }
    static double nhz(int p) { return 440.0 * std::pow(2.0, (p - 69) / 12.0); }
    static float blep(double t, double dt) { if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); } if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); } return 0.f; }
    static float saw(double ph, double dt) { return (float) (2.0 * ph - 1.0) - blep(ph, dt); }
    static float pulse(double ph, double dt, float w) { float v = ph < w ? 1.f : -1.f; v += blep(ph, dt); double p2 = ph - w; if (p2 < 0) p2 += 1.0; v -= blep(p2, dt); return v; }
    double sr = 44100.0;
    std::array<Cfg, 5> cfg {};
    std::array<std::array<Voice, 16>, 5> voices {};
};
} // namespace mel
