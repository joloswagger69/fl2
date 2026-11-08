// NEO//WAVE Factory-Bibliothek. Jedes Preset ist von Hand gebaut; nur abweichende Parameter stehen hier,
// alles andere ist Default. Die Lautstaerken (PresetLevels.inc) werden vom Test-Tool kalibriert:
//   NeoWaveTest --calibrate
#include "Presets.h"

namespace
{
const float kLevels[] = {
#include "PresetLevels.inc"
    0.7f
};

// Engines
enum { An = 0, Wt = 1, FM = 2, PD = 3, Gr = 4 };
// Wellenformen
enum { Saw = 0, Sq = 1, Tri = 2, Sin = 3 };
// Tabellen
enum { tChrome = 0, tGlass = 1, tVox = 2, tBell = 3, tPulse = 4, tOrgan = 5, tRise = 6, tBloom = 7 };
// Filter
enum { LP12 = 0, LP24 = 1, HP = 2, BP = 3, Notch = 4, Ladder = 5, Acid = 6, Formant = 7, Comb = 8, Diode = 9 };
// LFO
enum { Lsin = 0, Ltri = 1, Lsaw = 2, Lsq = 3, Lsh = 4, Lsm = 5 };
// FX
enum { Analog = 1, Dist = 2, Fold = 3, Crush = 4 };
enum { Chorus = 1, Dimension = 2, Flanger = 3, Phaser = 4 };
enum { Stereo = 1, PingPong = 2, Tape = 3 };
enum { Hall = 1, Shimmer = 2 };
// Sync fuer Step-Sequencer
enum { Free = 0, S4 = 1, S8 = 2, S16 = 3, S16T = 4, S32 = 5 };

struct B
{
    std::vector<std::pair<int, float>> v;
    B& p(int id, float x) { v.push_back({ id, x }); return *this; }

    B& A(int eng, int wave, int table, float morph, float color, float lvl, int oct = 0)
    { return p(P::aEngine, (float) eng).p(P::aWave, (float) wave).p(P::aTable, (float) table).p(P::aMorph, morph).p(P::aColor, color).p(P::aLevel, lvl).p(P::aOct, (float) oct); }
    B& Bo(int eng, int wave, int table, float morph, float color, float lvl, int oct = 0, float fine = 7.f)
    { return p(P::bEngine, (float) eng).p(P::bWave, (float) wave).p(P::bTable, (float) table).p(P::bMorph, morph).p(P::bColor, color).p(P::bLevel, lvl).p(P::bOct, (float) oct).p(P::bFine, fine); }
    B& uni(int n, float det, float spread = 0.7f) { return p(P::uniVoices, (float) n).p(P::uniDetune, det).p(P::uniSpread, spread); }
    B& F(int type, float cut, float res, float env, float drive = 0.f)
    { return p(P::fType, (float) type).p(P::fCut, cut).p(P::fRes, res).p(P::fEnv, env).p(P::fDrive, drive); }
    B& amp(float a, float d, float s, float r) { return p(P::ampA, a).p(P::ampD, d).p(P::ampS, s).p(P::ampR, r); }
    B& flt(float a, float d, float s, float r) { return p(P::fltA, a).p(P::fltD, d).p(P::fltS, s).p(P::fltR, r); }
    B& men(float a, float d, float s, float r) { return p(P::modEnvA, a).p(P::modEnvD, d).p(P::modEnvS, s).p(P::modEnvR, r); }
    B& lfo(int n, float rate, int shape, int key = 0)
    { const int b = P::lfo1Rate + (n - 1) * 3; return p(b, rate).p(b + 1, (float) shape).p(b + 2, (float) key); }
    B& mm(int slot, int src, int dst, float amt)
    { const int b = P::mmSrc1 + slot * 3; return p(b, (float) src).p(b + 1, (float) dst).p(b + 2, amt); }
    B& steps(std::initializer_list<float> s, int sync, float smooth = 0.f, float rate = 4.f)
    {
        int i = 0;
        for (float x : s) p(P::step1 + i++, x);
        return p(P::stepLen, (float) i).p(P::stepSync, (float) sync).p(P::stepSmooth, smooth).p(P::stepRate, rate);
    }
    B& mono(float glide = 0.f, bool legato = false) { return p(P::voiceMode, legato ? 2.f : 1.f).p(P::glide, glide); }
    B& sub(float lvl, int wave = 0, int oct = 0) { return p(P::subLevel, lvl).p(P::subWave, (float) wave).p(P::subOct, (float) oct); }
    B& noise(float lvl, int type = 0) { return p(P::noiseLevel, lvl).p(P::noiseType, (float) type); }
    B& ring(float x) { return p(P::ringMod, x); }
    B& xfm(float x) { return p(P::crossFm, x); }
    B& sync() { return p(P::bSync, 1.f); }
    B& sat(int type, float amt, float mix = 1.f) { return p(P::satType, (float) type).p(P::satAmt, amt).p(P::satMix, mix); }
    B& mod(int type, float rate, float depth, float fb, float mix)
    { return p(P::mfType, (float) type).p(P::mfRate, rate).p(P::mfDepth, depth).p(P::mfFb, fb).p(P::mfMix, mix); }
    B& dly(int type, float time, float fb, float tone, float mix)
    { return p(P::dlType, (float) type).p(P::dlTime, time).p(P::dlFb, fb).p(P::dlTone, tone).p(P::dlMix, mix); }
    B& rev(int type, float size, float damp, float mix)
    { return p(P::rvType, (float) type).p(P::rvSize, size).p(P::rvDamp, damp).p(P::rvMix, mix); }
    B& dyn(float mb, float ts, float cmp) { return p(P::mbAmt, mb).p(P::tsAmt, ts).p(P::cmpAmt, cmp); }
    B& shift(float hz, float mix) { return p(P::fsShift, hz).p(P::fsMix, mix); }
    B& wide(float w) { return p(P::width, w); }
};
} // namespace

const std::vector<PresetDef>& presetLibrary()
{
    static const std::vector<PresetDef> lib = [] {
        using namespace P;
        std::vector<PresetDef> l;
        auto add = [&](const char* cat, const char* name, B b) { l.push_back({ cat, name, std::move(b.v), 0.7f }); };

        add("Init", "Init", B());

        // ======================================================================= Y2K FUTURE
        const char* Y = "Y2K Future";
        add(Y, "Digital Dreams", B().A(Wt, Saw, tGlass, 0.35f, 0.1f, 0.8f).Bo(Wt, Saw, tChrome, 0.3f, 0, 0.3f, 1).uni(5, 0.3f, 1.f)
            .F(LP24, 6000, 0.2f, 0.2f).amp(0.02f, 0.6f, 0.7f, 1.2f).lfo(1, 0.3f, Lsin).mm(0, sLfo1, dAMorph, 0.3f)
            .mod(Chorus, 0.5f, 0.5f, 0, 0.4f).dly(Stereo, 0.375f, 0.35f, 0.6f, 0.2f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(Y, "Aqua Memories", B().A(FM, Sin, 0, 0.25f, 0.15f, 0.8f).Bo(An, Sin, 0, 0.1f, 0, 0.4f, 1).uni(3, 0.15f, 0.9f)
            .F(LP12, 4500, 0.15f, 0.1f).amp(0.01f, 1.2f, 0.3f, 1.8f).lfo(2, 0.35f, Ltri).mm(0, sLfo2, dCutoff, 0.2f)
            .mod(Chorus, 0.4f, 0.6f, 0, 0.5f).dly(PingPong, 0.5f, 0.5f, 0.5f, 0.3f).rev(Hall, 0.7f, 0.4f, 0.35f));
        add(Y, "Millennium Chime", B().A(FM, Sin, 0, 0.35f, 0.5f, 0.8f).Bo(FM, Sin, 0, 0.15f, 0.2f, 0.3f, 1).F(LP12, 12000, 0.1f, 0.f)
            .amp(0.002f, 2.5f, 0.f, 2.f).men(0.001f, 1.2f, 0.f, 0.5f).mm(0, sModEnv, dAMorph, -0.3f)
            .rev(Hall, 0.75f, 0.35f, 0.35f).dly(PingPong, 0.375f, 0.4f, 0.6f, 0.2f));
        add(Y, "Plastic Future", B().A(PD, Saw, 0, 0.55f, 0.3f, 0.8f).Bo(An, Sq, 0, 0.3f, 0, 0.3f, -1).uni(3, 0.2f, 0.8f)
            .F(Ladder, 2500, 0.4f, 0.5f, 0.2f).amp(0.004f, 0.35f, 0.6f, 0.3f).flt(0.003f, 0.25f, 0.2f, 0.3f)
            .sat(Analog, 0.25f).mod(Flanger, 0.2f, 0.4f, 0.4f, 0.35f).rev(Hall, 0.4f, 0.5f, 0.15f));
        add(Y, "Liquid Chrome", B().A(An, Saw, 0, 0.3f, 0.2f, 0.7f).Bo(Wt, Saw, tGlass, 0.5f, 0, 0.6f, 0, -5.f).uni(7, 0.35f, 0.9f)
            .F(Formant, 1200, 0.4f, 0.f).lfo(1, 0.25f, Ltri).mm(0, sLfo1, dCutoff, 0.5f).amp(0.15f, 0.5f, 0.8f, 0.9f)
            .mod(Phaser, 0.3f, 0.6f, 0.5f, 0.5f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(Y, "Blue Horizon", B().A(Wt, Saw, tRise, 0.3f, 0.f, 0.8f).Bo(An, Saw, 0, 0, 0, 0.5f, -1).uni(6, 0.3f, 1.f)
            .F(LP24, 3500, 0.1f, 0.35f).flt(0.6f, 1.5f, 0.5f, 1.f).amp(0.5f, 1.f, 0.8f, 2.f).lfo(1, 0.15f, Lsm).mm(0, sLfo1, dAMorph, 0.5f)
            .dly(Tape, 0.5f, 0.5f, 0.45f, 0.3f).rev(Hall, 0.8f, 0.5f, 0.35f));
        add(Y, "Virtual Paradise", B().A(PD, Saw, 0, 0.3f, 0.5f, 0.8f).Bo(FM, Sin, 0, 0.1f, 0.25f, 0.35f, 1).uni(4, 0.25f, 0.9f)
            .F(LP12, 7000, 0.1f, 0.15f).amp(0.03f, 0.8f, 0.6f, 1.4f).shift(3.f, 0.3f).mod(Chorus, 0.6f, 0.5f, 0, 0.4f).rev(Hall, 0.55f, 0.45f, 0.3f));
        add(Y, "Frutiger Glass", B().A(Wt, Saw, tGlass, 0.6f, 0.2f, 0.8f).Bo(Wt, Saw, tBell, 0.2f, 0, 0.35f, 1).F(LP12, 9000, 0.1f, 0.2f)
            .amp(0.005f, 1.5f, 0.15f, 1.2f).mm(0, sVel, dCutoff, 0.3f).rev(Hall, 0.65f, 0.4f, 0.4f).dly(Stereo, 0.25f, 0.35f, 0.6f, 0.2f));
        add(Y, "Cyber Sunrise", B().A(An, Saw, 0, 0.2f, 0.f, 0.7f).Bo(Wt, Saw, tChrome, 0.4f, 0, 0.5f, 1).uni(5, 0.3f, 1.f)
            .F(Ladder, 1500, 0.3f, 0.6f).flt(2.f, 3.f, 0.6f, 2.f).amp(1.2f, 2.f, 0.8f, 3.f).lfo(2, 0.08f, Lsin).mm(0, sLfo2, dBMorph, 0.6f)
            .dly(Tape, 0.4f, 0.4f, 0.5f, 0.3f).rev(Hall, 0.8f, 0.5f, 0.45f));
        add(Y, "Chrome Pulse", B().A(An, Sq, 0, 0.4f, 0.f, 0.8f).uni(3, 0.2f, 0.8f).F(LP24, 2200, 0.35f, 0.4f).amp(0.003f, 0.2f, 0.8f, 0.15f)
            .steps({ 0, -1, 0, -0.6f, 0, -1, -0.3f, -1 }, S16).mm(0, sStep, dAmp, 1.f).dly(PingPong, 0.375f, 0.4f, 0.5f, 0.3f).rev(Hall, 0.5f, 0.5f, 0.2f));

        // ======================================================================= JUNGLE / DNB
        const char* J = "Jungle / DnB";
        add(J, "Jungle Pressure", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.6f, -1).uni(3, 0.15f).F(LP24, 700, 0.35f, 0.5f, 0.3f)
            .flt(0.002f, 0.25f, 0.2f, 0.2f).amp(0.002f, 0.3f, 0.8f, 0.1f).sub(0.4f).mono(0.05f).sat(Analog, 0.3f).dyn(0, 0, 0.4f));
        add(J, "Amen Machine", B().A(Wt, Saw, tBloom, 0.5f, 0.2f, 0.8f).noise(0.2f, 2).F(BP, 1800, 0.5f, 0.6f).flt(0.001f, 0.12f, 0.f, 0.1f)
            .amp(0.001f, 0.18f, 0.f, 0.12f).sat(Dist, 0.4f).men(0.001f, 0.2f, 0.f, 0.1f).mm(0, sModEnv, dAMorph, 0.5f)
            .dly(Stereo, 0.1875f, 0.4f, 0.6f, 0.25f).dyn(0, 0.5f, 0.5f));
        add(J, "Rave Stab 2001", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.7f, 0, 15.f).uni(5, 0.4f, 0.9f).F(LP24, 1800, 0.25f, 0.5f)
            .flt(0.002f, 0.3f, 0.2f, 0.2f).amp(0.002f, 0.45f, 0.35f, 0.25f).sat(Analog, 0.3f).mod(Chorus, 0.5f, 0.5f, 0, 0.3f).rev(Hall, 0.5f, 0.5f, 0.2f));
        add(J, "Reese Mutation", B().A(An, Saw, 0, 0, 0, 0.8f, -1).Bo(An, Saw, 0, 0, 0, 0.8f, -1, -12.f).uni(3, 0.2f).F(LP24, 600, 0.25f, 0.15f)
            .lfo(1, 0.3f, Lsin).lfo(2, 0.25f, Ltri).mm(0, sLfo1, dCutoff, 0.4f).mm(1, sLfo2, dUniDet, 0.5f).amp(0.005f, 0.3f, 1.f, 0.2f)
            .mono(0.02f).sat(Dist, 0.25f).mod(Flanger, 0.15f, 0.4f, 0.3f, 0.3f).dyn(0, 0, 0.3f));
        add(J, "Deep Jungle", B().A(Wt, Saw, tOrgan, 0.3f, 0.f, 0.7f).sub(0.5f).uni(3, 0.2f, 0.9f).F(LP12, 1400, 0.1f, 0.2f)
            .amp(0.6f, 1.f, 0.8f, 1.5f).lfo(1, 0.2f, Lsm).mm(0, sLfo1, dAMorph, 0.4f).dly(Tape, 0.5f, 0.5f, 0.4f, 0.3f).rev(Hall, 0.7f, 0.5f, 0.35f));
        add(J, "Breakstorm", B().A(PD, Saw, 0, 0.7f, 0.6f, 0.8f).Bo(An, Sq, 0, 0.3f, 0, 0.5f, -1).ring(0.3f).F(Diode, 900, 0.55f, 0.5f, 0.5f)
            .flt(0.001f, 0.2f, 0.2f, 0.2f).amp(0.001f, 0.25f, 0.5f, 0.15f).sat(Dist, 0.5f).lfo(3, 6.f, Lsh, 1).mm(0, sLfo3, dCutoff, 0.3f).dyn(0.5f, 0, 0.4f));
        add(J, "Neuro Pulse", B().A(FM, Sin, 0, 0.45f, 0.3f, 0.8f, -1).Bo(An, Saw, 0, 0, 0, 0.5f, -1).F(Ladder, 500, 0.5f, 0.f)
            .lfo(1, 4.f, Ltri).lfo(2, 0.5f, Lsin).mm(0, sLfo1, dCutoff, 0.6f).mm(1, sLfo2, dAMorph, 0.5f).amp(0.003f, 0.3f, 1.f, 0.15f)
            .mono().sat(Fold, 0.4f).dyn(0.6f, 0, 0.4f));
        add(J, "Sub Pressure", B().A(An, Sin, 0, 0.15f, 0, 0.9f).sub(0.6f).F(LP12, 300, 0.1f, 0.f).amp(0.004f, 0.3f, 1.f, 0.15f).mono(0.03f).sat(Analog, 0.35f));
        add(J, "Ragga Hoover", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.7f, 0, -9.f).uni(7, 0.6f, 1.f).F(LP12, 3000, 0.3f, 0.3f)
            .men(0.001f, 0.25f, 0.f, 0.2f).mm(0, sModEnv, dPitch, -0.5f).amp(0.005f, 0.4f, 0.9f, 0.3f).sat(Dist, 0.3f).mod(Chorus, 0.6f, 0.6f, 0, 0.5f));
        add(J, "Tear Out Stab", B().A(An, Saw, 0, 0.6f, 0, 0.8f).Bo(Wt, Saw, tChrome, 0.7f, 0, 0.4f, 1).uni(5, 0.35f).F(LP24, 1200, 0.45f, 0.8f)
            .flt(0.001f, 0.15f, 0.f, 0.15f).amp(0.001f, 0.3f, 0.f, 0.2f).sat(Analog, 0.4f).dly(Stereo, 0.2f, 0.35f, 0.5f, 0.2f).rev(Hall, 0.4f, 0.5f, 0.2f));

        // ======================================================================= TECHNO
        const char* T = "Techno";
        add(T, "Warehouse 1999", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Sq, 0, 0.2f, 0, 0.4f, 0, 10.f).uni(2, 0.2f).F(LP24, 1800, 0.2f, 0.4f)
            .flt(0.002f, 0.35f, 0.1f, 0.3f).amp(0.003f, 0.4f, 0.f, 0.35f).dly(PingPong, 0.375f, 0.5f, 0.4f, 0.35f).rev(Hall, 0.6f, 0.5f, 0.3f).sat(Analog, 0.15f));
        add(T, "Concrete Pulse", B().A(An, Sq, 0, 0.2f, 0, 0.8f, -1).sub(0.5f).F(LP24, 400, 0.3f, 0.6f).flt(0.001f, 0.15f, 0.1f, 0.1f)
            .amp(0.001f, 0.2f, 0.f, 0.08f).dyn(0, 0, 0.5f).sat(Dist, 0.2f));
        add(T, "Hypnotic Machine", B().A(An, Tri, 0, 0.1f, 0, 0.8f).Bo(PD, Saw, 0, 0.4f, 0, 0.3f, 1).F(Ladder, 1500, 0.45f, 0.5f).flt(0.002f, 0.2f, 0.f, 0.2f)
            .amp(0.002f, 0.28f, 0.f, 0.2f).lfo(1, 0.3f, Lsin).mm(0, sLfo1, dCutoff, 0.2f).dly(Tape, 0.3f, 0.5f, 0.45f, 0.35f).rev(Hall, 0.5f, 0.5f, 0.25f));
        add(T, "Acid Transmission", B().A(An, Saw, 0, 0, 0, 0.9f).F(Acid, 450, 0.75f, 0.8f, 0.3f).flt(0.002f, 0.22f, 0.f, 0.15f)
            .amp(0.002f, 0.3f, 0.6f, 0.08f).mono(0.06f, true).sat(Dist, 0.25f).dly(Stereo, 0.2f, 0.4f, 0.5f, 0.25f));
        add(T, "Dark Matter", B().A(Wt, Saw, tBloom, 0.4f, 0.1f, 0.8f).Bo(An, Saw, 0, 0, 0, 0.5f, -1).uni(4, 0.3f, 1.f).F(LP24, 700, 0.3f, 0.1f)
            .lfo(1, 0.1f, Lsin).mm(0, sLfo1, dCutoff, 0.5f).amp(1.5f, 1.f, 1.f, 3.f).rev(Hall, 0.9f, 0.6f, 0.5f).dly(Tape, 0.5f, 0.5f, 0.3f, 0.3f));
        add(T, "Berlin Afterhours", B().A(An, Saw, 0, 0.3f, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.5f, 0, -7.f).uni(3, 0.2f).F(Ladder, 900, 0.35f, 0.35f)
            .flt(0.003f, 0.5f, 0.3f, 0.3f).amp(0.01f, 0.5f, 0.8f, 0.3f).lfo(2, 0.15f, Ltri).mm(0, sLfo2, dCutoff, 0.35f)
            .mod(Phaser, 0.25f, 0.6f, 0.5f, 0.35f).dly(Tape, 0.375f, 0.5f, 0.4f, 0.25f).rev(Hall, 0.55f, 0.5f, 0.25f));
        add(T, "Industrial Dreams", B().A(FM, Sin, 0, 0.6f, 0.55f, 0.8f).Bo(An, Sq, 0, 0.3f, 0, 0.5f).ring(0.35f).F(Comb, 220, 0.5f, 0.f)
            .amp(0.004f, 0.8f, 0.3f, 0.6f).sat(Fold, 0.45f).rev(Hall, 0.6f, 0.4f, 0.3f).dyn(0.4f, 0, 0));
        add(T, "Rave Generator", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.35f, 0, 12.f).sync().uni(3, 0.25f).F(LP24, 3000, 0.3f, 0.3f)
            .lfo(1, 0.6f, Lsaw).mm(0, sLfo1, dBPitch, 0.5f).amp(0.003f, 0.4f, 0.9f, 0.2f).mod(Chorus, 0.5f, 0.5f, 0, 0.3f).dly(Stereo, 0.3f, 0.35f, 0.5f, 0.2f));
        add(T, "Basement Chord", B().A(Wt, Saw, tOrgan, 0.5f, 0, 0.8f).Bo(Wt, Saw, tChrome, 0.3f, 0, 0.5f).uni(4, 0.25f).F(LP12, 1100, 0.15f, 0.3f)
            .flt(0.004f, 0.6f, 0.2f, 0.4f).amp(0.004f, 0.9f, 0.1f, 0.5f).dly(Tape, 0.375f, 0.6f, 0.35f, 0.4f).rev(Hall, 0.6f, 0.5f, 0.35f).sat(Analog, 0.2f));
        add(T, "Peak Time Stab", B().A(An, Saw, 0, 0, 0, 0.9f).uni(7, 0.45f, 0.9f).F(LP24, 2600, 0.3f, 0.6f).flt(0.001f, 0.2f, 0.1f, 0.15f)
            .amp(0.001f, 0.3f, 0.f, 0.2f).sat(Dist, 0.3f).dly(PingPong, 0.1875f, 0.4f, 0.5f, 0.2f).dyn(0.3f, 0.4f, 0.3f));

        // ======================================================================= TRANCE / PROGRESSIVE
        const char* R = "Trance / Progressive";
        add(R, "Eternal Sunrise", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.35f, 1, 5.f).uni(8, 0.4f, 1.f).F(LP24, 5000, 0.15f, 0.25f)
            .flt(0.02f, 1.5f, 0.6f, 1.f).amp(0.4f, 1.f, 0.85f, 1.8f).mod(Chorus, 0.5f, 0.5f, 0, 0.4f).dly(PingPong, 0.375f, 0.45f, 0.5f, 0.3f).rev(Hall, 0.8f, 0.5f, 0.4f));
        add(R, "Millennium Gate", B().A(An, Saw, 0, 0, 0, 0.8f).uni(7, 0.45f, 1.f).F(LP12, 8000, 0.1f, 0.f).amp(0.003f, 0.3f, 1.f, 0.3f)
            .steps({ 0, -1, 0, -1, 0, -1, 0, -1 }, S16).mm(0, sStep, dAmp, 1.f).dly(PingPong, 0.375f, 0.45f, 0.5f, 0.3f).rev(Hall, 0.5f, 0.5f, 0.3f));
        add(R, "Euphoric Motion", B().A(An, Saw, 0, 0.15f, 0, 0.8f).Bo(Wt, Saw, tChrome, 0.5f, 0, 0.4f, 1).uni(9, 0.5f, 1.f).F(Ladder, 3500, 0.2f, 0.4f)
            .flt(0.01f, 0.8f, 0.5f, 0.5f).amp(0.005f, 0.5f, 0.85f, 0.6f).lfo(1, 0.25f, Ltri).mm(0, sLfo1, dBMorph, 0.4f)
            .mod(Chorus, 0.5f, 0.5f, 0, 0.4f).dly(Tape, 0.375f, 0.45f, 0.5f, 0.3f).rev(Hall, 0.6f, 0.5f, 0.4f));
        add(R, "Digital Angels", B().A(Wt, Saw, tVox, 0.3f, 0.f, 0.8f).Bo(Wt, Saw, tVox, 0.6f, 0, 0.6f, 0, 9.f).uni(5, 0.25f, 1.f).F(LP12, 7000, 0.1f, 0.f)
            .amp(0.8f, 1.f, 0.8f, 2.2f).lfo(1, 0.2f, Lsin).lfo(2, 0.13f, Lsin).mm(0, sLfo1, dAMorph, 0.4f).mm(1, sLfo2, dBMorph, -0.4f)
            .mod(Chorus, 0.4f, 0.5f, 0, 0.35f).rev(Shimmer, 0.85f, 0.4f, 0.45f));
        add(R, "Lost Dimension", B().A(PD, Saw, 0, 0.5f, 0.4f, 0.8f).Bo(Wt, Saw, tRise, 0.5f, 0, 0.5f).uni(6, 0.3f, 1.f).F(Formant, 900, 0.3f, 0.f)
            .lfo(1, 0.15f, Lsin).mm(0, sLfo1, dCutoff, 0.3f).amp(0.9f, 1.f, 0.9f, 2.5f).dly(PingPong, 0.5f, 0.5f, 0.5f, 0.3f).shift(0.8f, 0.3f).rev(Hall, 0.75f, 0.5f, 0.45f));
        add(R, "Cosmic Horizon", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Tri, 0, 0, 0, 0.5f, 1).uni(10, 0.6f, 1.f).sub(0.2f).F(LP24, 2000, 0.15f, 0.5f)
            .flt(2.5f, 2.f, 0.7f, 2.f).amp(1.5f, 1.f, 0.9f, 3.5f).rev(Shimmer, 0.9f, 0.4f, 0.5f).mod(Dimension, 0.3f, 0.5f, 0, 0.4f));
        add(R, "Future Nostalgia", B().A(Wt, Saw, tGlass, 0.4f, 0.1f, 0.8f).Bo(An, Saw, 0, 0, 0, 0.4f, -1).uni(6, 0.3f, 0.9f).F(LP24, 4200, 0.2f, 0.4f)
            .flt(0.003f, 0.4f, 0.2f, 0.4f).amp(0.003f, 0.5f, 0.5f, 0.5f).mod(Chorus, 0.5f, 0.5f, 0, 0.35f).dly(PingPong, 0.375f, 0.4f, 0.5f, 0.25f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(R, "Gate Pluck 138", B().A(An, Sq, 0, 0.2f, 0, 0.8f).uni(3, 0.2f).F(LP24, 2400, 0.25f, 0.5f).flt(0.001f, 0.15f, 0.2f, 0.15f)
            .amp(0.002f, 0.2f, 0.5f, 0.1f).steps({ 0, 0, -1, 0, 0, -1, 0, -1 }, S16).mm(0, sStep, dAmp, 1.f)
            .dly(PingPong, 0.375f, 0.45f, 0.5f, 0.3f).rev(Hall, 0.5f, 0.5f, 0.2f));
        add(R, "Trance Riff Lead", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.6f, 0, 12.f).uni(4, 0.3f).F(LP24, 3800, 0.35f, 0.5f)
            .flt(0.002f, 0.4f, 0.3f, 0.3f).amp(0.004f, 0.3f, 0.9f, 0.25f).mono(0.08f, true).dly(Stereo, 0.3f, 0.35f, 0.5f, 0.25f).rev(Hall, 0.5f, 0.5f, 0.25f));

        // ======================================================================= BASS
        const char* Ba = "Bass";
        add(Ba, "Deep Sub", B().A(An, Sin, 0, 0, 0, 0.9f).sub(0.5f).F(LP12, 250, 0.05f, 0.f).amp(0.005f, 0.3f, 1.f, 0.12f).mono().sat(Analog, 0.2f));
        add(Ba, "Reese Monster", B().A(An, Saw, 0, 0, 0, 0.8f, -1).Bo(An, Saw, 0, 0, 0, 0.8f, -1, -14.f).uni(3, 0.25f).F(LP24, 500, 0.3f, 0.2f)
            .lfo(2, 0.2f, Ltri).mm(0, sLfo2, dCutoff, 0.5f).amp(0.003f, 0.3f, 1.f, 0.15f).mono().sat(Dist, 0.3f).mod(Flanger, 0.2f, 0.4f, 0.3f, 0.25f).dyn(0, 0, 0.5f));
        add(Ba, "Liquid Bass", B().A(Wt, Saw, tGlass, 0.4f, 0.1f, 0.8f, -1).Bo(An, Sin, 0, 0, 0, 0.5f, -1).F(Ladder, 800, 0.35f, 0.5f).flt(0.004f, 0.3f, 0.3f, 0.2f)
            .amp(0.005f, 0.4f, 0.7f, 0.2f).lfo(1, 0.5f, Lsin).mm(0, sLfo1, dAMorph, 0.3f).mono(0.05f, true).mod(Chorus, 0.5f, 0.4f, 0, 0.2f));
        add(Ba, "Neuro Core", B().A(FM, Sin, 0, 0.55f, 0.25f, 0.8f, -1).Bo(PD, Saw, 0, 0.6f, 0.3f, 0.5f, -1).F(Diode, 700, 0.5f, 0.4f, 0.4f)
            .lfo(1, 3.f, Ltri).lfo(2, 0.4f, Lsin).mm(0, sLfo1, dAMorph, 0.4f).mm(1, sLfo2, dCutoff, 0.5f).amp(0.003f, 0.3f, 1.f, 0.15f)
            .mono().sat(Fold, 0.35f).dyn(0.7f, 0, 0.5f));
        add(Ba, "Rubber Bass", B().A(An, Tri, 0, 0.6f, 0, 0.8f, -1).Bo(An, Sq, 0, 0.3f, 0, 0.4f, -1).F(Acid, 400, 0.55f, 0.8f).flt(0.001f, 0.15f, 0.1f, 0.1f)
            .amp(0.001f, 0.25f, 0.3f, 0.1f).mono().sat(Analog, 0.3f));
        add(Ba, "Metallic Growl", B().A(FM, Sin, 0, 0.7f, 0.45f, 0.8f, -1).Bo(An, Saw, 0, 0, 0, 0.5f, -1).ring(0.3f).F(BP, 600, 0.6f, 0.2f)
            .lfo(1, 5.f, Ltri).mm(0, sLfo1, dCutoff, 0.5f).amp(0.003f, 0.3f, 1.f, 0.15f).mono().sat(Dist, 0.5f).dyn(0.6f, 0, 0.4f));
        add(Ba, "Underground Pressure", B().A(An, Saw, 0, 0, 0, 0.8f, -1).sub(0.5f).uni(2, 0.1f).F(LP24, 350, 0.4f, 0.45f).flt(0.002f, 0.22f, 0.3f, 0.2f)
            .amp(0.002f, 0.3f, 0.9f, 0.1f).sat(Analog, 0.3f).dyn(0, 0.3f, 0.6f).mono());
        add(Ba, "FM Thunder", B().A(FM, Sin, 0, 0.5f, 0.1f, 0.8f, -2).Bo(FM, Sin, 0, 0.3f, 0.1f, 0.5f, -1).F(LP12, 900, 0.1f, 0.3f)
            .amp(0.003f, 0.6f, 0.8f, 0.4f).men(0.001f, 0.5f, 0.f, 0.3f).mm(0, sModEnv, dAMorph, -0.3f).sat(Analog, 0.25f).rev(Hall, 0.6f, 0.5f, 0.15f));
        add(Ba, "Phase Bass", B().A(PD, Saw, 0, 0.6f, 0.2f, 0.8f, -1).sub(0.4f).F(Ladder, 1200, 0.25f, 0.35f).men(0.001f, 0.3f, 0.f, 0.2f)
            .mm(0, sModEnv, dAMorph, -0.4f).amp(0.003f, 0.3f, 0.9f, 0.15f).mono(0.04f, true));
        add(Ba, "Wobble Machine", B().A(An, Saw, 0, 0, 0, 0.8f, -1).Bo(An, Sq, 0, 0.3f, 0, 0.5f, -1).F(LP24, 450, 0.5f, 0.f).lfo(1, 2.f, Lsin)
            .mm(0, sLfo1, dCutoff, 0.8f).amp(0.003f, 0.3f, 1.f, 0.15f).mono().sat(Dist, 0.3f).dyn(0, 0, 0.5f));

        // ======================================================================= LEADS
        const char* Le = "Leads";
        add(Le, "Supersaw Evolution", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(Wt, Saw, tChrome, 0.3f, 0, 0.5f, 0, -6.f).uni(10, 0.5f, 1.f).F(LP24, 9000, 0.1f, 0.2f)
            .amp(0.005f, 0.4f, 0.9f, 0.5f).lfo(2, 0.15f, Lsm).mm(0, sLfo2, dBMorph, 0.6f).mod(Chorus, 0.5f, 0.5f, 0, 0.4f)
            .dly(Stereo, 0.3f, 0.35f, 0.5f, 0.25f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(Le, "Digital Anthem", B().A(PD, Saw, 0, 0.45f, 0.3f, 0.8f).Bo(An, Saw, 0, 0, 0, 0.35f, 1).uni(5, 0.3f).F(LP12, 7000, 0.1f, 0.2f)
            .amp(0.004f, 0.3f, 0.9f, 0.4f).lfo(1, 5.5f, Lsin, 1).mm(0, sLfo1, dPitch, 0.02f).dly(PingPong, 0.375f, 0.4f, 0.5f, 0.25f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(Le, "Chrome Lead", B().A(An, Saw, 0, 0.5f, 0.25f, 0.8f).Bo(An, Sq, 0, 0.3f, 0, 0.2f, 1).uni(3, 0.2f).F(LP24, 5000, 0.3f, 0.35f)
            .amp(0.003f, 0.3f, 0.85f, 0.3f).mono(0.05f, true).sat(Analog, 0.2f).dly(Stereo, 0.3f, 0.35f, 0.5f, 0.2f).rev(Hall, 0.5f, 0.5f, 0.2f));
        add(Le, "Future Rave", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(Wt, Saw, tPulse, 0.5f, 0, 0.5f).uni(5, 0.35f).F(Ladder, 3000, 0.35f, 0.45f)
            .lfo(1, 0.3f, Lsin).mm(0, sLfo1, dBMorph, 0.5f).men(0.001f, 0.2f, 0.f, 0.2f).mm(1, sModEnv, dPitch, 0.02f).amp(0.003f, 0.4f, 0.85f, 0.3f)
            .mod(Chorus, 0.5f, 0.5f, 0, 0.35f).rev(Hall, 0.5f, 0.5f, 0.25f));
        add(Le, "Acid Prophet", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Sq, 0, 0.2f, 0, 0.4f, 0, 10.f).F(Acid, 1500, 0.5f, 0.5f).lfo(1, 0.4f, Ltri).mm(0, sLfo1, dCutoff, 0.25f)
            .amp(0.003f, 0.3f, 0.8f, 0.2f).mono(0.1f, true).sat(Dist, 0.2f).dly(Stereo, 0.3f, 0.4f, 0.5f, 0.25f));
        add(Le, "Hyperdrive", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.6f, 1).uni(8, 0.55f, 1.f).F(LP12, 12000, 0.1f, 0.f).men(0.001f, 0.12f, 0.f, 0.1f)
            .mm(0, sModEnv, dPitch, -0.1f).amp(0.002f, 0.3f, 1.f, 0.3f).sat(Dist, 0.25f).mod(Chorus, 0.5f, 0.5f, 0, 0.4f).dly(PingPong, 0.25f, 0.4f, 0.5f, 0.25f).rev(Hall, 0.5f, 0.5f, 0.25f));
        add(Le, "Formant Singer", B().A(An, Saw, 0, 0, 0, 0.9f).uni(3, 0.15f).F(Formant, 600, 0.5f, 0.f).lfo(1, 0.4f, Lsin).lfo(2, 5.3f, Lsin, 1)
            .mm(0, sLfo1, dCutoff, 0.6f).mm(1, sLfo2, dPitch, 0.02f).amp(0.08f, 0.3f, 0.9f, 0.5f).rev(Hall, 0.6f, 0.4f, 0.3f));
        add(Le, "Mono Glide", B().A(An, Saw, 0, 0, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.6f, 0, -10.f).F(LP24, 2800, 0.4f, 0.5f).amp(0.004f, 0.3f, 0.8f, 0.25f)
            .mono(0.2f, true).sat(Analog, 0.25f).dly(Tape, 0.375f, 0.45f, 0.45f, 0.25f));
        add(Le, "Pixel Flute", B().A(An, Sin, 0, 0.2f, 0, 0.9f).Bo(Wt, Saw, tRise, 0.2f, 0, 0.25f, 1).noise(0.08f, 1).F(LP12, 6000, 0.15f, 0.2f)
            .amp(0.06f, 0.3f, 0.85f, 0.2f).lfo(1, 5.f, Lsin).mm(0, sLfo1, dPitch, 0.015f).rev(Hall, 0.5f, 0.5f, 0.3f));
        add(Le, "Siren Lead", B().A(An, Saw, 0, 0.2f, 0, 0.9f).F(LP12, 6000, 0.2f, 0.f).lfo(1, 1.2f, Ltri).mm(0, sLfo1, dPitch, 0.5f)
            .amp(0.01f, 0.2f, 1.f, 0.2f).mono().dly(PingPong, 0.375f, 0.5f, 0.5f, 0.3f).rev(Hall, 0.5f, 0.5f, 0.2f));

        // ======================================================================= PADS / ATMOSPHERES
        const char* Pa = "Pads / Atmospheres";
        add(Pa, "Aqua Cathedral", B().A(Wt, Saw, tOrgan, 0.5f, 0, 0.7f).Bo(An, Saw, 0, 0, 0, 0.3f, -1).uni(6, 0.25f, 1.f).F(LP12, 3000, 0.1f, 0.2f)
            .flt(1.2f, 1.f, 0.7f, 2.f).amp(1.5f, 1.f, 0.85f, 3.f).mod(Chorus, 0.3f, 0.6f, 0, 0.45f).rev(Hall, 0.9f, 0.4f, 0.5f));
        add(Pa, "Endless Summer", B().A(An, Saw, 0, 0, 0, 0.7f).Bo(An, Tri, 0, 0, 0, 0.5f, 1).uni(7, 0.35f, 1.f).F(LP24, 3800, 0.15f, 0.2f).lfo(1, 0.2f, Lsin)
            .mm(0, sLfo1, dCutoff, 0.25f).amp(1.f, 1.f, 0.9f, 2.2f).mod(Chorus, 0.4f, 0.6f, 0, 0.45f).dly(Tape, 0.5f, 0.45f, 0.4f, 0.25f).rev(Hall, 0.8f, 0.5f, 0.4f));
        add(Pa, "Forgotten Planet", B().A(Wt, Saw, tBloom, 0.5f, 0.2f, 0.7f).Bo(FM, Sin, 0, 0.2f, 0.3f, 0.3f, 1).noise(0.06f, 1).F(LP12, 2500, 0.2f, 0.1f)
            .lfo(1, 0.07f, Lsm).lfo(2, 0.11f, Lsm).mm(0, sLfo1, dAMorph, 0.7f).mm(1, sLfo2, dCutoff, 0.3f).amp(2.f, 1.f, 1.f, 4.f).rev(Shimmer, 0.9f, 0.5f, 0.55f));
        add(Pa, "Glass Ocean", B().A(Wt, Saw, tGlass, 0.6f, 0, 0.7f).Bo(Wt, Saw, tBell, 0.3f, 0, 0.25f, 1).uni(4, 0.3f, 1.f).F(LP12, 6000, 0.1f, 0.15f)
            .amp(1.2f, 1.f, 0.8f, 3.f).mod(Flanger, 0.1f, 0.5f, 0.3f, 0.3f).rev(Hall, 0.9f, 0.4f, 0.5f));
        add(Pa, "Synthetic Rain", B().A(Gr, Saw, tBell, 0.5f, 0.6f, 0.9f).F(BP, 4000, 0.2f, 0.f).lfo(1, 0.2f, Lsm).mm(0, sLfo1, dAMorph, 0.4f)
            .amp(1.f, 1.f, 1.f, 2.f).dly(PingPong, 0.375f, 0.5f, 0.5f, 0.3f).rev(Hall, 0.8f, 0.4f, 0.5f));
        add(Pa, "Dream Sequence", B().A(PD, Saw, 0, 0.4f, 0.35f, 0.7f).Bo(Wt, Saw, tRise, 0.4f, 0, 0.4f).uni(6, 0.3f, 1.f).F(LP24, 2800, 0.15f, 0.2f)
            .lfo(2, 0.1f, Lsin).mm(0, sLfo2, dAMorph, 0.5f).amp(0.8f, 1.f, 0.9f, 2.5f).shift(0.4f, 0.35f).rev(Hall, 0.8f, 0.5f, 0.45f));
        add(Pa, "Liquid Memories", B().A(Gr, Saw, tGlass, 0.4f, 0.35f, 0.8f).Bo(An, Sin, 0, 0, 0, 0.4f).F(LP12, 3500, 0.15f, 0.1f).amp(1.2f, 1.f, 1.f, 3.f)
            .mod(Dimension, 0.3f, 0.6f, 0, 0.45f).rev(Hall, 0.8f, 0.5f, 0.45f));
        add(Pa, "Sunken Choir", B().A(Wt, Saw, tVox, 0.1f, 0, 0.7f).Bo(Wt, Saw, tVox, 0.5f, 0, 0.6f, 0, -8.f).uni(4, 0.2f, 1.f).sub(0.15f).F(LP12, 2200, 0.1f, 0.1f)
            .lfo(1, 0.12f, Lsm).mm(0, sLfo1, dAMorph, 0.8f).amp(1.8f, 1.f, 1.f, 3.5f).rev(Hall, 0.95f, 0.5f, 0.5f));
        add(Pa, "Vapor Strings", B().A(An, Saw, 0, 0, 0, 0.7f).Bo(An, Saw, 0, 0, 0, 0.6f, 0, 12.f).uni(12, 0.3f, 1.f).F(LP24, 2400, 0.1f, 0.2f)
            .amp(0.6f, 1.f, 0.9f, 1.5f).mod(Chorus, 0.4f, 0.6f, 0, 0.45f).rev(Hall, 0.7f, 0.5f, 0.35f));
        add(Pa, "Warm Static", B().A(An, Tri, 0, 0, 0, 0.8f).noise(0.12f, 1).Bo(An, Saw, 0, 0, 0, 0.3f, -1).F(LP12, 1500, 0.1f, 0.1f).amp(1.4f, 1.f, 1.f, 2.5f)
            .sat(Analog, 0.15f).dly(Tape, 0.5f, 0.5f, 0.35f, 0.25f).rev(Hall, 0.7f, 0.6f, 0.35f));
        add(Pa, "Neon Strings", B().A(An, Saw, 0, 0.3f, 0, 0.7f).Bo(Wt, Saw, tChrome, 0.8f, 0, 0.5f).uni(8, 0.3f, 1.f).F(LP24, 3500, 0.15f, 0.3f)
            .flt(1.f, 1.f, 0.6f, 1.f).amp(0.5f, 1.f, 0.9f, 1.8f).mod(Chorus, 0.5f, 0.5f, 0, 0.4f).rev(Hall, 0.7f, 0.5f, 0.35f));

        // ======================================================================= PLUCKS / BELLS
        const char* Pl = "Plucks / Bells";
        add(Pl, "Crystal Circuit", B().A(FM, Sin, 0, 0.3f, 0.55f, 0.8f).Bo(Wt, Saw, tBell, 0.3f, 0, 0.3f, 1).F(LP12, 14000, 0.1f, 0.f).amp(0.001f, 1.6f, 0.f, 1.2f)
            .rev(Hall, 0.7f, 0.4f, 0.35f).dly(PingPong, 0.375f, 0.4f, 0.6f, 0.25f));
        add(Pl, "Digital Raindrops", B().A(Wt, Saw, tBell, 0.3f, 0.f, 0.8f).Bo(FM, Sin, 0, 0.2f, 0.3f, 0.3f, 1).F(LP12, 6000, 0.1f, 0.2f).amp(0.001f, 0.5f, 0.f, 0.6f)
            .mm(0, sRand, dCutoff, 0.4f).dly(PingPong, 0.3f, 0.55f, 0.5f, 0.4f).rev(Hall, 0.8f, 0.4f, 0.5f));
        add(Pl, "Glass Frequency", B().A(FM, Sin, 0, 0.25f, 0.7f, 0.8f).Bo(An, Sin, 0, 0, 0, 0.25f, 2).F(LP12, 15000, 0.05f, 0.f).amp(0.001f, 2.f, 0.f, 2.f)
            .rev(Shimmer, 0.8f, 0.4f, 0.4f));
        add(Pl, "Retro Future", B().A(PD, Saw, 0, 0.5f, 0.f, 0.8f).Bo(An, Sq, 0, 0.4f, 0, 0.3f, 1).F(LP24, 2400, 0.25f, 0.5f).flt(0.002f, 0.2f, 0.f, 0.2f)
            .amp(0.002f, 0.35f, 0.f, 0.25f).mod(Chorus, 0.5f, 0.5f, 0, 0.35f).dly(Stereo, 0.25f, 0.4f, 0.5f, 0.25f).rev(Hall, 0.5f, 0.5f, 0.2f));
        add(Pl, "Neon Bells", B().A(Wt, Saw, tBell, 0.1f, 0, 0.8f).Bo(FM, Sin, 0, 0.2f, 0.5f, 0.35f, 1).F(LP12, 12000, 0.1f, 0.f).amp(0.001f, 2.2f, 0.f, 1.8f)
            .men(0.001f, 1.f, 0.f, 0.5f).mm(0, sModEnv, dAMorph, 0.5f).rev(Hall, 0.8f, 0.4f, 0.45f).dly(Tape, 0.375f, 0.5f, 0.5f, 0.3f));
        add(Pl, "Marimba Chip", B().A(Wt, Saw, tRise, 0.15f, 0, 0.8f).Bo(An, Sin, 0, 0, 0, 0.25f, 2).F(LP12, 3500, 0.1f, 0.2f).amp(0.001f, 0.35f, 0.f, 0.2f).rev(Hall, 0.4f, 0.5f, 0.2f));
        add(Pl, "Water Harp", B().A(An, Tri, 0, 0.3f, 0, 0.8f).Bo(FM, Sin, 0, 0.15f, 0.2f, 0.3f, 1).F(LP12, 5000, 0.1f, 0.3f).flt(0.001f, 0.4f, 0.f, 0.3f)
            .amp(0.001f, 1.2f, 0.f, 1.1f).mod(Chorus, 0.5f, 0.5f, 0, 0.3f).dly(PingPong, 0.375f, 0.55f, 0.5f, 0.4f).rev(Hall, 0.7f, 0.4f, 0.4f));
        add(Pl, "Tin Pluck", B().A(An, Sq, 0, 0.7f, 0, 0.9f).F(BP, 2500, 0.4f, 0.3f).flt(0.001f, 0.12f, 0.f, 0.1f).amp(0.001f, 0.22f, 0.f, 0.12f)
            .sat(Analog, 0.25f).dly(Stereo, 0.2f, 0.35f, 0.5f, 0.2f));
        add(Pl, "Rubber Pizz", B().A(PD, Saw, 0, 0.7f, 0.1f, 0.8f).Bo(An, Sin, 0, 0, 0, 0.5f).F(Ladder, 900, 0.3f, 0.6f).flt(0.001f, 0.12f, 0.f, 0.1f)
            .amp(0.001f, 0.18f, 0.f, 0.1f).sat(Analog, 0.2f).rev(Hall, 0.3f, 0.5f, 0.15f));
        add(Pl, "Echo Kalimba", B().A(FM, Sin, 0, 0.22f, 0.35f, 0.9f).F(LP12, 8000, 0.1f, 0.f).amp(0.001f, 0.6f, 0.f, 0.5f)
            .dly(Tape, 0.375f, 0.55f, 0.45f, 0.45f).rev(Hall, 0.6f, 0.5f, 0.3f));

        // ======================================================================= KEYS / ARPS
        const char* K = "Keys / Arps";
        add(K, "Chrome Keys", B().A(FM, Sin, 0, 0.3f, 0.2f, 0.8f).Bo(An, Sin, 0, 0, 0, 0.3f).F(LP12, 9000, 0.05f, 0.f).amp(0.002f, 1.4f, 0.25f, 0.5f)
            .men(0.001f, 0.6f, 0.f, 0.3f).mm(0, sModEnv, dAMorph, -0.25f).mm(1, sVel, dAMorph, 0.3f).mod(Chorus, 0.5f, 0.4f, 0, 0.35f).rev(Hall, 0.5f, 0.5f, 0.2f));
        add(K, "Millennium Organ", B().A(Wt, Saw, tOrgan, 0.6f, 0, 0.8f).Bo(Wt, Saw, tOrgan, 0.2f, 0, 0.3f, 1).F(LP12, 7000, 0.1f, 0.f).amp(0.006f, 0.1f, 1.f, 0.08f)
            .sat(Analog, 0.2f).mod(Phaser, 0.8f, 0.4f, 0.2f, 0.3f).rev(Hall, 0.4f, 0.5f, 0.2f));
        add(K, "Electric Dawn", B().A(FM, Sin, 0, 0.45f, 0.2f, 0.8f).Bo(FM, Sin, 0, 0.2f, 0.07f, 0.4f, 0).F(LP12, 8000, 0.05f, 0.f).amp(0.002f, 1.8f, 0.1f, 0.7f)
            .lfo(2, 4.5f, Lsin).mm(0, sLfo2, dAmp, 0.15f).mod(Chorus, 0.4f, 0.5f, 0, 0.4f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(K, "Arp Machine", B().A(An, Sq, 0, 0.3f, 0, 0.8f).Bo(An, Saw, 0, 0, 0, 0.3f, 1).F(LP24, 2500, 0.3f, 0.5f).flt(0.001f, 0.15f, 0.f, 0.1f)
            .amp(0.001f, 0.15f, 0.2f, 0.1f).steps({ 0, 0.5f, 0.2917f, 0.5f, 0, 0.5f, 0.1667f, 0.5f }, S16).mm(0, sStep, dPitch, 1.f)
            .dly(PingPong, 0.375f, 0.4f, 0.5f, 0.3f).rev(Hall, 0.5f, 0.5f, 0.2f));
        add(K, "Bitpop Arp", B().A(An, Sq, 0, 0.5f, 0, 0.8f).F(LP12, 6000, 0.1f, 0.2f).amp(0.001f, 0.2f, 0.f, 0.1f)
            .steps({ 0, 0.1667f, 0.2917f, 0.5f, 0.2917f, 0.1667f, 0, -0.5f }, S8).mm(0, sStep, dPitch, 1.f).sat(Crush, 0.35f)
            .dly(Stereo, 0.25f, 0.4f, 0.5f, 0.25f).rev(Hall, 0.4f, 0.5f, 0.2f));
        add(K, "Retro Clav", B().A(An, Sq, 0, 0.5f, 0, 0.9f).Bo(An, Saw, 0, 0, 0, 0.2f, 1).F(BP, 2200, 0.4f, 0.5f).flt(0.001f, 0.1f, 0.f, 0.08f)
            .amp(0.001f, 0.3f, 0.f, 0.08f).mod(Chorus, 0.5f, 0.4f, 0, 0.3f));
        add(K, "Velvet Tines", B().A(FM, Sin, 0, 0.2f, 0.2f, 0.8f).Bo(Wt, Saw, tBell, 0.f, 0, 0.12f, 1).F(LP12, 8000, 0.05f, 0.f).amp(0.003f, 1.6f, 0.15f, 0.6f)
            .mod(Phaser, 0.35f, 0.5f, 0.3f, 0.3f).lfo(2, 4.f, Lsin).mm(0, sLfo2, dAmp, 0.12f).rev(Hall, 0.6f, 0.5f, 0.3f));
        add(K, "Dust Piano", B().A(Wt, Saw, tOrgan, 0.4f, 0, 0.9f).noise(0.03f, 1).F(LP12, 2800, 0.1f, 0.3f).flt(0.001f, 0.5f, 0.2f, 0.3f).amp(0.001f, 0.9f, 0.f, 0.4f)
            .sat(Analog, 0.15f).rev(Hall, 0.5f, 0.6f, 0.25f));

        // ======================================================================= FX / EXPERIMENTAL
        const char* X = "FX / Experimental";
        add(X, "Data Corruption", B().A(PD, Saw, 0, 0.9f, 0.8f, 0.8f).Bo(FM, Sin, 0, 0.7f, 0.9f, 0.6f).ring(0.4f).sat(Crush, 0.6f).F(Comb, 400, 0.6f, 0.f)
            .lfo(3, 8.f, Lsh).mm(0, sLfo3, dAColor, 0.8f).mm(1, sLfo3, dCutoff, 0.5f).amp(0.003f, 0.3f, 1.f, 0.3f).dly(PingPong, 0.1875f, 0.5f, 0.5f, 0.3f));
        add(X, "Alien Communication", B().A(FM, Sin, 0, 0.5f, 0.3f, 0.8f).F(Formant, 1500, 0.5f, 0.f).lfo(1, 6.f, Lsh).lfo(2, 0.4f, Lsm)
            .mm(0, sLfo1, dPitch, 0.15f).mm(1, sLfo2, dAMorph, 0.5f).amp(0.01f, 0.3f, 1.f, 0.4f).shift(40.f, 0.4f).dly(PingPong, 0.3f, 0.5f, 0.4f, 0.3f).rev(Hall, 0.7f, 0.4f, 0.4f));
        add(X, "Cybernetic Storm", B().A(Gr, Saw, tBloom, 0.5f, 0.8f, 0.8f).noise(0.2f, 0).F(BP, 3000, 0.4f, 0.f).lfo(1, 0.3f, Lsm).mm(0, sLfo1, dCutoff, 0.6f)
            .amp(0.8f, 1.f, 1.f, 2.f).sat(Dist, 0.4f).rev(Shimmer, 0.8f, 0.4f, 0.5f).dyn(0.5f, 0, 0));
        add(X, "Liquid Portal", B().A(Wt, Saw, tRise, 0.f, 0.f, 0.8f).Bo(Wt, Saw, tVox, 0.5f, 0, 0.5f, 1).F(Formant, 700, 0.4f, 0.f).men(3.f, 1.f, 1.f, 2.f)
            .mm(0, sModEnv, dAMorph, 1.f).lfo(1, 0.3f, Lsin).mm(1, sLfo1, dCutoff, 0.4f).amp(1.f, 1.f, 1.f, 3.f).mod(Phaser, 0.2f, 0.7f, 0.5f, 0.5f).rev(Hall, 0.9f, 0.4f, 0.5f));
        add(X, "Digital Collapse", B().A(PD, Saw, 0, 0.8f, 0.2f, 0.8f).F(LP12, 4000, 0.2f, -0.5f).men(0.001f, 1.5f, 0.f, 1.f).mm(0, sModEnv, dPitch, -0.5f)
            .amp(0.002f, 1.8f, 0.f, 1.f).sat(Crush, 0.5f).dly(PingPong, 0.25f, 0.5f, 0.4f, 0.3f).rev(Hall, 0.7f, 0.4f, 0.4f));
        add(X, "Unknown Transmission", B().A(Wt, Saw, tVox, 0.4f, 0, 0.5f).noise(0.4f, 0).F(BP, 1800, 0.6f, 0.f).lfo(3, 3.f, Lsh).mm(0, sLfo3, dCutoff, 0.7f)
            .amp(0.3f, 0.5f, 1.f, 1.5f).shift(-70.f, 0.5f).dly(Tape, 0.4f, 0.6f, 0.4f, 0.4f).rev(Hall, 0.7f, 0.4f, 0.4f));
        add(X, "Riser Rave", B().A(An, Saw, 0, 0, 0, 0.7f).noise(0.6f, 0).uni(8, 0.8f, 1.f).F(LP24, 200, 0.2f, 0.9f).flt(6.f, 0.1f, 1.f, 1.f)
            .men(6.f, 1.f, 1.f, 0.5f).mm(0, sModEnv, dPitch, 0.5f).amp(5.f, 1.f, 1.f, 1.f).rev(Hall, 0.7f, 0.4f, 0.3f));
        add(X, "Downlifter", B().A(An, Saw, 0, 0, 0, 0.8f).noise(0.2f, 0).uni(6, 0.6f, 1.f).F(LP24, 9000, 0.15f, -0.8f).flt(0.001f, 3.f, 0.f, 1.f)
            .men(0.001f, 3.f, 0.f, 1.f).mm(0, sModEnv, dPitch, -0.5f).amp(0.002f, 3.f, 0.f, 1.f).rev(Hall, 0.8f, 0.4f, 0.4f));
        add(X, "Granular Dust", B().A(Gr, Saw, tBloom, 0.3f, 0.5f, 0.8f).noise(0.2f, 3).F(LP12, 7000, 0.1f, 0.f).lfo(1, 0.15f, Lsm).mm(0, sLfo1, dAMorph, 0.6f)
            .amp(0.5f, 1.f, 1.f, 2.f).dly(PingPong, 0.4f, 0.55f, 0.4f, 0.3f).rev(Shimmer, 0.85f, 0.4f, 0.45f));
        add(X, "Modem Choir", B().A(PD, Saw, 0, 0.6f, 0.2f, 0.7f).Bo(Wt, Saw, tVox, 0.3f, 0, 0.6f).ring(0.3f).F(Formant, 800, 0.4f, 0.f)
            .steps({ 0, 0.1667f, -0.125f, 0.2917f, 0.0417f, -0.2083f, 0.125f, 0 }, S16).mm(0, sStep, dPitch, 1.f).amp(0.01f, 0.3f, 1.f, 0.3f).sat(Crush, 0.25f)
            .dly(PingPong, 0.1875f, 0.5f, 0.5f, 0.3f).rev(Hall, 0.6f, 0.4f, 0.3f));
        add(X, "Sweep Gate", B().A(An, Saw, 0, 0, 0, 0.8f).uni(4, 0.35f, 0.9f).F(LP24, 800, 0.35f, 0.f).lfo(1, 0.2f, Ltri).mm(0, sLfo1, dCutoff, 0.9f)
            .steps({ 0, -1, 0, -1, 0, 0, -1, 0 }, S8).mm(1, sStep, dAmp, 1.f).amp(0.003f, 0.3f, 1.f, 0.2f).mod(Chorus, 0.5f, 0.5f, 0, 0.35f).rev(Hall, 0.6f, 0.5f, 0.3f));

        // ======================================================================= AMBIENT / IDM
        const char* Am = "Ambient / IDM";
        add(Am, "Slow Glass", B().A(Wt, Saw, tGlass, 0.2f, 0, 0.7f).Bo(Wt, Saw, tRise, 0.3f, 0, 0.25f, 1).F(LP12, 4500, 0.1f, 0.1f).lfo(1, 0.05f, Lsm)
            .mm(0, sLfo1, dAMorph, 0.9f).amp(3.f, 1.f, 1.f, 5.f).mod(Dimension, 0.2f, 0.6f, 0, 0.45f).rev(Shimmer, 0.95f, 0.4f, 0.6f));
        add(Am, "Circuit Garden", B().A(FM, Sin, 0, 0.25f, 0.4f, 0.8f).F(LP12, 6000, 0.1f, 0.2f).amp(0.002f, 0.5f, 0.f, 0.5f)
            .steps({ 0, 0.2917f, 0.125f, 0.4167f, 0.2083f, 0.5f, 0.125f, 0.2917f }, S8).mm(0, sStep, dPitch, 1.f)
            .dly(PingPong, 0.375f, 0.55f, 0.45f, 0.4f).rev(Hall, 0.85f, 0.4f, 0.5f));
        add(Am, "Fractal Fog", B().A(Gr, Saw, tChrome, 0.5f, 0.4f, 0.7f).Bo(Gr, Saw, tOrgan, 0.4f, 0.3f, 0.5f).uni(3, 0.3f, 1.f).F(LP12, 2200, 0.15f, 0.f)
            .amp(3.f, 1.f, 1.f, 6.f).rev(Hall, 0.97f, 0.5f, 0.6f));
        add(Am, "Tape Memory", B().A(An, Tri, 0, 0, 0, 0.8f).Bo(An, Sin, 0, 0, 0, 0.4f, 1).sat(Analog, 0.25f).dly(Tape, 0.45f, 0.65f, 0.3f, 0.4f)
            .F(LP12, 2000, 0.1f, 0.2f).amp(0.1f, 1.f, 0.6f, 2.f).rev(Hall, 0.7f, 0.6f, 0.35f));
        add(Am, "Binary Rain", B().A(PD, Saw, 0, 0.5f, 0.1f, 0.8f, 1).F(LP12, 8000, 0.1f, 0.2f).amp(0.001f, 0.25f, 0.f, 0.3f)
            .steps({ 0, 0.2083f, 0.0833f, 0.2917f, 0.1667f, 0.0417f, 0.25f, 0.125f, 0, 0.2917f }, S16).mm(0, sStep, dPitch, 1.f).sat(Crush, 0.3f)
            .dly(PingPong, 0.4f, 0.55f, 0.5f, 0.45f).rev(Shimmer, 0.85f, 0.4f, 0.4f));
        add(Am, "Dust Orbit", B().A(Wt, Saw, tBloom, 0.5f, 0.2f, 0.7f).noise(0.1f, 2).F(LP12, 3500, 0.15f, 0.f).shift(1.5f, 0.5f)
            .amp(2.f, 1.f, 1.f, 4.f).lfo(1, 0.08f, Lsm).mm(0, sLfo1, dAMorph, 0.7f).rev(Hall, 0.9f, 0.5f, 0.5f));
        add(Am, "Quiet Machine", B().A(An, Sin, 0, 0.1f, 0, 0.8f).Bo(FM, Sin, 0, 0.1f, 0.2f, 0.3f, -1).F(LP12, 1200, 0.1f, 0.1f).amp(1.f, 1.f, 1.f, 3.f)
            .lfo(1, 0.1f, Lsin).mm(0, sLfo1, dCutoff, 0.2f).dly(Tape, 0.5f, 0.5f, 0.3f, 0.3f).rev(Hall, 0.8f, 0.6f, 0.4f));
        add(Am, "Hollow Planet", B().A(Wt, Saw, tOrgan, 0.8f, 0, 0.7f, -1).Bo(An, Saw, 0, 0, 0, 0.4f, -2).uni(3, 0.2f, 1.f).F(LP24, 900, 0.3f, 0.1f).lfo(1, 0.06f, Lsm)
            .mm(0, sLfo1, dCutoff, 0.6f).amp(2.5f, 1.f, 1.f, 5.f).rev(Hall, 0.95f, 0.6f, 0.55f));

        const size_t nLevels = sizeof(kLevels) / sizeof(float) - 1;
        for (size_t i = 0; i < l.size() && i < nLevels; ++i) l[i].level = kLevels[i];
        return l;
    }();
    return lib;
}
