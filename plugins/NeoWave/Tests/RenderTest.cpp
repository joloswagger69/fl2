// Offline-Test fuer NEO//WAVE (kein Audiogeraet noetig):
//   NeoWaveTest              -> rendert alle Presets, meldet NaN/Pegel/CPU/Spektrum
//   NeoWaveTest --calibrate  -> zusaetzlich Lautstaerke angleichen, schreibt Source/PresetLevels.inc
//   NeoWaveTest --state      -> Zustand speichern/laden Roundtrip
//   NeoWaveTest --random     -> 200x Randomize (smart + voll), auf NaN/Pegel pruefen
#include "../Source/PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <algorithm>

struct Metrics { float peak = 0, rmsMax = 0, centroid = 0, dc = 0; int bad = 0; double cpu = 0; };

static Metrics render(NeoProcessor& proc, std::vector<int> notes, float noteSec, float totalSec, double sr = 44100.0, int block = 512)
{
    Metrics m;
    const int total = (int) (totalSec * sr);
    juce::AudioBuffer<float> buf(2, block);
    std::vector<float> mono((size_t) total, 0.f);
    juce::MidiBuffer midi;
    int pos = 0;
    bool on = false, off = false;
    const auto t0 = std::chrono::steady_clock::now();
    while (pos < total)
    {
        midi.clear();
        if (!on) { for (int n : notes) midi.addEvent(juce::MidiMessage::noteOn(1, n, 0.8f), 0); on = true; }
        if (!off && pos >= (int) (noteSec * sr)) { for (int n : notes) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0); off = true; }
        proc.processBlock(buf, midi);
        const int len = juce::jmin(block, total - pos);
        for (int i = 0; i < len; ++i)
        {
            const float l = buf.getSample(0, i), r = buf.getSample(1, i);
            if (!std::isfinite(l) || !std::isfinite(r)) ++m.bad;
            m.peak = juce::jmax(m.peak, std::abs(l), std::abs(r));
            mono[(size_t) (pos + i)] = 0.5f * (l + r);
        }
        pos += len;
    }
    const auto t1 = std::chrono::steady_clock::now();
    m.cpu = std::chrono::duration<double>(t1 - t0).count() / (double) totalSec;

    const int w = (int) (0.05 * sr);
    int bestStart = 0;
    for (int s = 0; s + w < total; s += w / 2)
    {
        double e = 0;
        for (int i = 0; i < w; ++i) e += mono[(size_t) (s + i)] * mono[(size_t) (s + i)];
        const float rms = (float) std::sqrt(e / w);
        if (rms > m.rmsMax) { m.rmsMax = rms; bestStart = s; }
    }
    double mean = 0;
    for (float v : mono) mean += v;
    m.dc = (float) (mean / total);

    // Spektralschwerpunkt (2048er FFT am lautesten Punkt)
    juce::dsp::FFT fft(11);
    std::vector<float> fd(4096, 0.f);
    for (int i = 0; i < 2048 && bestStart + i < total; ++i)
        fd[(size_t) i] = mono[(size_t) (bestStart + i)] * (0.5f - 0.5f * std::cos(2 * 3.14159265f * i / 2048.f));
    fft.performFrequencyOnlyForwardTransform(fd.data());
    double num = 0, den = 1e-12;
    for (int k = 1; k < 1024; ++k) { num += (double) k * fd[(size_t) k]; den += fd[(size_t) k]; }
    m.centroid = (float) (num / den * sr / 2048.0);
    return m;
}

int main(int argc, char** argv)
{
    juce::String mode = argc > 1 ? argv[1] : "";
    NeoProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    const auto& lib = presetLibrary();
    int failures = 0;

    if (mode == "--state")
    {
        proc.loadFactoryPreset(juce::jmin(5, (int) lib.size() - 1));
        std::vector<float> before;
        for (int i = 0; i < P::kCount; ++i) before.push_back(proc.getParamReal(i));
        juce::MemoryBlock mb; proc.getStateInformation(mb);
        proc.setAllToDefault(false);
        proc.setStateInformation(mb.getData(), (int) mb.getSize());
        int diff = 0;
        for (int i = 0; i < P::kCount; ++i)
            if (std::abs(before[(size_t) i] - proc.getParamReal(i)) > 1e-3f * (1 + std::abs(before[(size_t) i]))) { ++diff; std::printf("  diff %s: %f -> %f\n", pdef(i).id, before[(size_t) i], proc.getParamReal(i)); }
        std::printf("STATE roundtrip: %d differences (%s)\n", diff, proc.presetName.toRawUTF8());
        return diff ? 1 : 0;
    }

    if (mode == "--random")
    {
        int bad = 0;
        for (int i = 0; i < 200; ++i)
        {
            proc.randomize(i % 2 == 0);
            auto m = render(proc, { 48, 55, 64 }, 0.8f, 1.2f);
            if (m.bad) { ++bad; std::printf("random %d: NaN!\n", i); }
            if (i < 12) std::printf("random(%s) peak %.2f rms %.3f centroid %.0f Hz\n", i % 2 == 0 ? "smart" : "full ", m.peak, m.rmsMax, m.centroid);
        }
        std::printf("RANDOM test: %d bad runs\n", bad);
        return bad ? 1 : 0;
    }

    if (mode == "--repeat")
    {
        const int idx = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;
        proc.loadFactoryPreset(idx);
        for (int k = 0; k < 6; ++k)
        {
            proc.panic();
            auto m = render(proc, { 48, 55, 64 }, 1.2f, 2.6f);
            std::printf("run %d: master %.3f peak %.2f rms %.3f\n", k, proc.getParamReal(P::master), m.peak, m.rmsMax);
        }
        return 0;
    }
    const bool calibrate = mode == "--calibrate";
    std::vector<float> levels;
    double worstCpu = 0;
    std::printf("%-3s %-14s %-24s %6s %6s %7s %7s %5s\n", "#", "category", "name", "peak", "rms", "centrd", "level", "cpu%");

    for (int i = 0; i < (int) lib.size(); ++i)
    {
        proc.loadFactoryPreset(i);
        float lvl = lib[(size_t) i].level;
        Metrics m;
        const int iters = calibrate ? 5 : 1;
        for (int it = 0; it < iters; ++it)
        {
            const int nanBefore = proc.nanEvents.load();
            std::vector<Metrics> runs;
            for (int k = 0; k < (calibrate ? 3 : 1); ++k)
            {
                proc.panic();
                runs.push_back(render(proc, { 48, 55, 64 }, 1.2f, 2.6f));
            }
            std::sort(runs.begin(), runs.end(), [](const Metrics& a, const Metrics& b) { return a.rmsMax < b.rmsMax; });
            m = runs[runs.size() / 2];
            for (auto& r : runs) m.peak = juce::jmax(m.peak, r.peak);
            m.bad += proc.nanEvents.load() - nanBefore;
            if (calibrate && std::getenv("NW_DEBUG") && i == 79) std::printf("  it %d master %.3f peak %.2f rms %.3f\n", it, proc.getParamReal(P::master), m.peak, m.rmsMax);
            if (calibrate && m.rmsMax > 1e-5f)
            {
                const float scale = juce::jmin(0.16f / m.rmsMax, 0.8f / juce::jmax(m.peak, 1e-3f));
                lvl = juce::jlimit(0.05f, 1.0f, lvl * std::pow(scale, 0.85f));
                proc.setParamReal(P::master, lvl);
            }
        }
        levels.push_back(calibrate ? lvl : lib[(size_t) i].level);
        worstCpu = juce::jmax(worstCpu, m.cpu);
        const bool silent = m.rmsMax < 0.01f;
        const bool hot = m.peak > 0.95f;
        if (m.bad || silent) ++failures;
        std::printf("%-3d %-14s %-24s %6.2f %6.3f %7.0f %7.3f %5.1f %s%s%s\n", i, lib[(size_t) i].category.c_str(), lib[(size_t) i].name.c_str(),
                    m.peak, m.rmsMax, m.centroid, levels.back(), m.cpu * 100, m.bad ? " NAN!" : "", silent ? " SILENT!" : "", hot ? " hot" : "");
    }
    std::printf("\n%d presets, %d failures, worst CPU %.1f%% of realtime (single core)\n", (int) lib.size(), failures, worstCpu * 100);

    if (calibrate)
    {
        juce::String s;
        for (float l : levels) s << juce::String(l, 3) << "f,\n";
        const juce::File f(juce::File::getCurrentWorkingDirectory().getChildFile("Source/PresetLevels.inc"));
        f.replaceWithText(s);
        std::printf("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    return failures ? 1 : 0;
}
