#pragma once
// NEO//WAVE - zentrale Parameter-Definition (Single Source of Truth).
// Enum, Parametertabelle und Host-Layout werden aus derselben X-Macro-Liste erzeugt,
// damit sie nie auseinanderlaufen.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

#define NW_SRC_LIST "None|LFO 1|LFO 2|LFO 3|Mod Env|Step Seq|Velocity|Mod Wheel|Aftertouch|Keytrack|Random|Macro 1|Macro 2|Macro 3|Macro 4|Amp Env|Filter Env"
#define NW_DST_LIST "None|Pitch|A Pitch|B Pitch|A Morph|B Morph|A Color|B Color|Sub Level|Noise Level|Cutoff|Resonance|Filter Drive|Uni Detune|Amp|Cross FM|Ring Mod|A Level|B Level"
#define NW_TABLES   "Chrome Saw|Aqua Glass|Vox Machine|Neon Bell|Pulse Drift|Cathedral|Harmonic Rise|Static Bloom"
#define NW_ENGINES  "Analog|Wavetable|FM|Phase Dist|Granular"
#define NW_WAVES    "Saw|Square|Triangle|Sine"
#define NW_LFOSHAPE "Sine|Triangle|Saw|Square|S&H|Smooth Rnd"
#define NW_FILTERS  "LP 12|LP 24|HP|BP|Notch|Ladder|Acid|Formant|Comb|Diode"

// X(id, Anzeigename, Art, min, max, default, skew, Auswahlliste)
// Art: F=Float, I=Int, C=Choice. Reihenfolge der mm*/lfo*-Gruppen ist wichtig (Zugriff per Offset).
#define NW_PARAMS(X) \
 X(master,"Master",'F',0,1,0.7f,1,"") \
 X(voiceMode,"Mode",'C',0,2,0,1,"Poly|Mono|Legato") \
 X(glide,"Glide",'F',0,2,0,0.4f,"") \
 X(bendRange,"Bend",'I',1,24,2,1,"") \
 X(polyphony,"Voices",'I',1,16,16,1,"") \
 X(stealMode,"Steal",'C',0,3,0,1,"Smart|Oldest|Lowest|Highest") \
 X(macro1,"Macro 1",'F',0,1,0,1,"") \
 X(macro2,"Macro 2",'F',0,1,0,1,"") \
 X(macro3,"Macro 3",'F',0,1,0,1,"") \
 X(macro4,"Macro 4",'F',0,1,0,1,"") \
 X(aEngine,"Engine",'C',0,4,0,1,NW_ENGINES) \
 X(aWave,"Wave",'C',0,3,0,1,NW_WAVES) \
 X(aTable,"Table",'C',0,7,0,1,NW_TABLES) \
 X(aMorph,"Morph",'F',0,1,0,1,"") \
 X(aColor,"Color",'F',0,1,0,1,"") \
 X(aOct,"Octave",'I',-3,3,0,1,"") \
 X(aSemi,"Semi",'I',-12,12,0,1,"") \
 X(aFine,"Fine",'F',-100,100,0,1,"") \
 X(aLevel,"Level",'F',0,1,0.8f,1,"") \
 X(bEngine,"Engine",'C',0,4,0,1,NW_ENGINES) \
 X(bWave,"Wave",'C',0,3,0,1,NW_WAVES) \
 X(bTable,"Table",'C',0,7,0,1,NW_TABLES) \
 X(bMorph,"Morph",'F',0,1,0,1,"") \
 X(bColor,"Color",'F',0,1,0,1,"") \
 X(bOct,"Octave",'I',-3,3,0,1,"") \
 X(bSemi,"Semi",'I',-12,12,0,1,"") \
 X(bFine,"Fine",'F',-100,100,7,1,"") \
 X(bLevel,"Level",'F',0,1,0.5f,1,"") \
 X(bSync,"Sync",'C',0,1,0,1,"Off|Hard Sync") \
 X(subLevel,"Sub",'F',0,1,0,1,"") \
 X(subWave,"Sub Wave",'C',0,1,0,1,"Sine|Square") \
 X(subOct,"Sub Oct",'C',0,1,0,1,"-1 Oct|-2 Oct") \
 X(noiseLevel,"Noise",'F',0,1,0,1,"") \
 X(noiseType,"Noise Type",'C',0,3,0,1,"White|Pink|Crackle|Dust") \
 X(ringMod,"Ring",'F',0,1,0,1,"") \
 X(crossFm,"Cross FM",'F',0,1,0,1,"") \
 X(uniVoices,"Unison",'I',1,16,1,1,"") \
 X(uniDetune,"Detune",'F',0,1,0.25f,1,"") \
 X(uniSpread,"Spread",'F',0,1,0.7f,1,"") \
 X(fType,"Filter",'C',0,9,1,1,NW_FILTERS) \
 X(fCut,"Cutoff",'F',20,20000,8000,0.3f,"") \
 X(fRes,"Resonance",'F',0,1,0.15f,1,"") \
 X(fDrive,"Drive",'F',0,1,0,1,"") \
 X(fKey,"Key Track",'F',-1,1,0,1,"") \
 X(fEnv,"Env Amt",'F',-1,1,0.3f,1,"") \
 X(ampA,"Attack",'F',0.001f,8,0.005f,0.35f,"") \
 X(ampD,"Decay",'F',0.001f,8,0.3f,0.35f,"") \
 X(ampS,"Sustain",'F',0,1,0.7f,1,"") \
 X(ampR,"Release",'F',0.001f,10,0.25f,0.35f,"") \
 X(fltA,"Attack",'F',0.001f,8,0.005f,0.35f,"") \
 X(fltD,"Decay",'F',0.001f,8,0.4f,0.35f,"") \
 X(fltS,"Sustain",'F',0,1,0.3f,1,"") \
 X(fltR,"Release",'F',0.001f,10,0.3f,0.35f,"") \
 X(modEnvA,"Attack",'F',0.001f,8,0.01f,0.35f,"") \
 X(modEnvD,"Decay",'F',0.001f,8,0.5f,0.35f,"") \
 X(modEnvS,"Sustain",'F',0,1,0,1,"") \
 X(modEnvR,"Release",'F',0.001f,10,0.3f,0.35f,"") \
 X(lfo1Rate,"Rate",'F',0.02f,40,2,0.35f,"") \
 X(lfo1Shape,"Shape",'C',0,5,0,1,NW_LFOSHAPE) \
 X(lfo1Key,"Retrig",'C',0,1,0,1,"Free|Key") \
 X(lfo2Rate,"Rate",'F',0.02f,40,0.5f,0.35f,"") \
 X(lfo2Shape,"Shape",'C',0,5,1,1,NW_LFOSHAPE) \
 X(lfo2Key,"Retrig",'C',0,1,0,1,"Free|Key") \
 X(lfo3Rate,"Rate",'F',0.02f,40,6,0.35f,"") \
 X(lfo3Shape,"Shape",'C',0,5,4,1,NW_LFOSHAPE) \
 X(lfo3Key,"Retrig",'C',0,1,1,1,"Free|Key") \
 X(stepRate,"Rate",'F',0.1f,30,4,0.5f,"") \
 X(stepSync,"Sync",'C',0,5,0,1,"Free|1/4|1/8|1/16|1/16T|1/32") \
 X(stepLen,"Length",'I',2,16,8,1,"") \
 X(stepSmooth,"Smooth",'F',0,1,0,1,"") \
 X(step1,"Step 1",'F',-1,1,0,1,"") X(step2,"Step 2",'F',-1,1,0,1,"") \
 X(step3,"Step 3",'F',-1,1,0,1,"") X(step4,"Step 4",'F',-1,1,0,1,"") \
 X(step5,"Step 5",'F',-1,1,0,1,"") X(step6,"Step 6",'F',-1,1,0,1,"") \
 X(step7,"Step 7",'F',-1,1,0,1,"") X(step8,"Step 8",'F',-1,1,0,1,"") \
 X(step9,"Step 9",'F',-1,1,0,1,"") X(step10,"Step 10",'F',-1,1,0,1,"") \
 X(step11,"Step 11",'F',-1,1,0,1,"") X(step12,"Step 12",'F',-1,1,0,1,"") \
 X(step13,"Step 13",'F',-1,1,0,1,"") X(step14,"Step 14",'F',-1,1,0,1,"") \
 X(step15,"Step 15",'F',-1,1,0,1,"") X(step16,"Step 16",'F',-1,1,0,1,"") \
 X(mmSrc1,"Source 1",'C',0,16,0,1,NW_SRC_LIST) X(mmDst1,"Dest 1",'C',0,18,0,1,NW_DST_LIST) X(mmAmt1,"Amount 1",'F',-1,1,0,1,"") \
 X(mmSrc2,"Source 2",'C',0,16,0,1,NW_SRC_LIST) X(mmDst2,"Dest 2",'C',0,18,0,1,NW_DST_LIST) X(mmAmt2,"Amount 2",'F',-1,1,0,1,"") \
 X(mmSrc3,"Source 3",'C',0,16,0,1,NW_SRC_LIST) X(mmDst3,"Dest 3",'C',0,18,0,1,NW_DST_LIST) X(mmAmt3,"Amount 3",'F',-1,1,0,1,"") \
 X(mmSrc4,"Source 4",'C',0,16,0,1,NW_SRC_LIST) X(mmDst4,"Dest 4",'C',0,18,0,1,NW_DST_LIST) X(mmAmt4,"Amount 4",'F',-1,1,0,1,"") \
 X(mmSrc5,"Source 5",'C',0,16,0,1,NW_SRC_LIST) X(mmDst5,"Dest 5",'C',0,18,0,1,NW_DST_LIST) X(mmAmt5,"Amount 5",'F',-1,1,0,1,"") \
 X(mmSrc6,"Source 6",'C',0,16,0,1,NW_SRC_LIST) X(mmDst6,"Dest 6",'C',0,18,0,1,NW_DST_LIST) X(mmAmt6,"Amount 6",'F',-1,1,0,1,"") \
 X(mmSrc7,"Source 7",'C',0,16,0,1,NW_SRC_LIST) X(mmDst7,"Dest 7",'C',0,18,0,1,NW_DST_LIST) X(mmAmt7,"Amount 7",'F',-1,1,0,1,"") \
 X(mmSrc8,"Source 8",'C',0,16,0,1,NW_SRC_LIST) X(mmDst8,"Dest 8",'C',0,18,0,1,NW_DST_LIST) X(mmAmt8,"Amount 8",'F',-1,1,0,1,"") \
 X(satType,"Type",'C',0,4,0,1,"Off|Analog|Distortion|Wavefolder|Bitcrush") \
 X(satAmt,"Amount",'F',0,1,0.3f,1,"") \
 X(satMix,"Mix",'F',0,1,1,1,"") \
 X(mfType,"Type",'C',0,4,0,1,"Off|Chorus|Dimension|Flanger|Phaser") \
 X(mfRate,"Rate",'F',0.05f,8,0.6f,0.5f,"") \
 X(mfDepth,"Depth",'F',0,1,0.5f,1,"") \
 X(mfFb,"Feedback",'F',0,0.9f,0.3f,1,"") \
 X(mfMix,"Mix",'F',0,1,0.5f,1,"") \
 X(fsShift,"Shift Hz",'F',-500,500,0,1,"") \
 X(fsMix,"Mix",'F',0,1,0,1,"") \
 X(dlType,"Type",'C',0,3,0,1,"Off|Stereo|Ping Pong|Tape") \
 X(dlTime,"Time",'F',0.02f,1.5f,0.375f,0.5f,"") \
 X(dlFb,"Feedback",'F',0,0.95f,0.4f,1,"") \
 X(dlTone,"Tone",'F',0,1,0.6f,1,"") \
 X(dlMix,"Mix",'F',0,1,0.25f,1,"") \
 X(rvType,"Type",'C',0,2,0,1,"Off|Hall|Shimmer") \
 X(rvSize,"Size",'F',0,1,0.5f,1,"") \
 X(rvDamp,"Damping",'F',0,1,0.5f,1,"") \
 X(rvMix,"Mix",'F',0,1,0.2f,1,"") \
 X(mbAmt,"Multiband",'F',0,1,0,1,"") \
 X(tsAmt,"Transient",'F',-1,1,0,1,"") \
 X(cmpAmt,"Compressor",'F',0,1,0,1,"") \
 X(width,"Width",'F',0,2,1,1,"")

namespace P
{
enum Id {
#define X(id, ...) id,
    NW_PARAMS(X)
#undef X
    kCount
};
}

enum Src { sNone, sLfo1, sLfo2, sLfo3, sModEnv, sStep, sVel, sWheel, sAft, sKey, sRand,
           sMac1, sMac2, sMac3, sMac4, sAmpEnv, sFltEnv, sCount };
enum Dst { dNone, dPitch, dAPitch, dBPitch, dAMorph, dBMorph, dAColor, dBColor, dSub, dNoise,
           dCutoff, dRes, dDrive, dUniDet, dAmp, dXfm, dRing, dALevel, dBLevel, dCount };

struct ParamDef
{
    const char* id;
    const char* name;
    char kind;
    float lo, hi, def, skew;
    const char* choices;
};

inline const ParamDef* paramDefs()
{
    static const ParamDef d[] = {
#define X(id, name, kind, lo, hi, def, skew, ch) { #id, name, kind, (float) (lo), (float) (hi), (float) (def), (float) (skew), ch },
        NW_PARAMS(X)
#undef X
    };
    return d;
}
inline const ParamDef& pdef(int i) { return paramDefs()[i]; }

inline int paramIndexByName(const juce::String& name)
{
    for (int i = 0; i < P::kCount; ++i)
        if (name == pdef(i).id) return i;
    return -1;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < P::kCount; ++i)
    {
        const auto& d = pdef(i);
        juce::ParameterID pid { d.id, 1 };
        if (d.kind == 'C')
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                pid, d.name, juce::StringArray::fromTokens(d.choices, "|", ""), (int) d.def));
        else if (d.kind == 'I')
            layout.add(std::make_unique<juce::AudioParameterInt>(pid, d.name, (int) d.lo, (int) d.hi, (int) d.def));
        else
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                pid, d.name, juce::NormalisableRange<float>(d.lo, d.hi, 0.f, d.skew), d.def));
    }
    return layout;
}

// Momentaufnahme aller Parameter - wird pro 32-Sample-Chunk im Audio-Thread aktualisiert.
struct Snap
{
    float v[P::kCount] {};
    float operator[](int i) const noexcept { return v[i]; }
    int   ch(int i) const noexcept { return (int) (v[i] + 0.5f); }
};
