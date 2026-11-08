#pragma once
// Sample-Daten, Laden (WAV/AIFF), verlustfreie Serialisierung (FLAC) fuer den Plugin-State.
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace rave
{
struct Sample
{
    juce::AudioBuffer<float> data;   // 1 oder 2 Kanaele
    double rate = 44100.0;
    juce::String name;
    juce::String sourcePath;

    int length() const noexcept   { return data.getNumSamples(); }
    int channels() const noexcept { return data.getNumChannels(); }
    double seconds() const noexcept { return rate > 0 ? length() / rate : 0.0; }
};
using SamplePtr = std::shared_ptr<const Sample>;

inline constexpr double kMaxSampleSeconds = 90.0;

inline SamplePtr makeSample(const juce::AudioBuffer<float>& b, double rate, const juce::String& name, const juce::String& path = {})
{
    if (b.getNumSamples() <= 0 || b.getNumChannels() <= 0 || rate < 1000.0) return nullptr;
    auto s = std::make_shared<Sample>();
    const int ch = juce::jmin(2, b.getNumChannels());
    s->data.setSize(ch, b.getNumSamples());
    for (int c = 0; c < ch; ++c) s->data.copyFrom(c, 0, b, c, 0, b.getNumSamples());
    // Mehrkanal -> Stereo (Kanaele 3+ werden verworfen), NaN/Inf entfernen
    for (int c = 0; c < ch; ++c)
    {
        auto* d = s->data.getWritePointer(c);
        for (int i = 0; i < s->data.getNumSamples(); ++i) if (! std::isfinite(d[i])) d[i] = 0.f;
    }
    s->rate = rate; s->name = name; s->sourcePath = path;
    return s;
}

// Laedt WAV/AIFF (weitere von JUCE registrierte Formate werden toleriert). Gibt nullptr + Fehlertext zurueck.
inline SamplePtr loadSampleFile(const juce::File& f, juce::String* error = nullptr)
{
    auto fail = [&](const juce::String& m) -> SamplePtr { if (error) *error = m; return nullptr; };
    if (! f.existsAsFile()) return fail("File not found");
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
    if (r == nullptr) return fail("Unsupported or damaged audio file");
    if (r->lengthInSamples <= 0 || r->numChannels == 0) return fail("File contains no audio");
    if (r->sampleRate < 8000.0 || r->sampleRate > 384000.0) return fail("Unsupported sample rate");
    if ((double) r->lengthInSamples / r->sampleRate > kMaxSampleSeconds) return fail("File is longer than 90 seconds");
    juce::AudioBuffer<float> b((int) juce::jmin<unsigned>(r->numChannels, 2u), (int) r->lengthInSamples);
    if (! r->read(&b, 0, (int) r->lengthInSamples, 0, true, b.getNumChannels() > 1)) return fail("Could not read audio data");
    return makeSample(b, r->sampleRate, f.getFileNameWithoutExtension(), f.getFullPathName());
}

// ---- Serialisierung (FLAC 24 Bit, verlustfrei fuer <= 24 Bit Material) ----------------------------------------------------
inline bool encodeSample(const Sample& s, juce::MemoryBlock& out)
{
    juce::FlacAudioFormat flac;
    auto mos = std::make_unique<juce::MemoryOutputStream>(out, false);
    std::unique_ptr<juce::AudioFormatWriter> w(flac.createWriterFor(mos.get(), s.rate, (unsigned) s.channels(), 24, {}, 5));
    if (w == nullptr) return false;
    mos.release();   // gehoert jetzt dem Writer
    juce::AudioBuffer<float> tmp(s.data);
    for (int c = 0; c < tmp.getNumChannels(); ++c) juce::FloatVectorOperations::clip(tmp.getWritePointer(c), tmp.getReadPointer(c), -1.f, 1.f, tmp.getNumSamples());
    const bool ok = w->writeFromAudioSampleBuffer(tmp, 0, tmp.getNumSamples());
    w.reset();   // flush
    return ok;
}

inline SamplePtr decodeSample(const void* data, size_t size, const juce::String& name, const juce::String& path = {})
{
    if (data == nullptr || size < 16) return nullptr;
    juce::FlacAudioFormat flac;
    auto* mis = new juce::MemoryInputStream(data, size, false);
    std::unique_ptr<juce::AudioFormatReader> r(flac.createReaderFor(mis, true));
    if (r == nullptr) return nullptr;
    if (r->lengthInSamples <= 0 || (double) r->lengthInSamples / juce::jmax(1.0, r->sampleRate) > kMaxSampleSeconds || r->numChannels > 2) return nullptr;
    juce::AudioBuffer<float> b((int) r->numChannels, (int) r->lengthInSamples);
    if (! r->read(&b, 0, (int) r->lengthInSamples, 0, true, b.getNumChannels() > 1)) return nullptr;
    return makeSample(b, r->sampleRate, name, path);
}

inline bool writeWavFile(const juce::AudioBuffer<float>& b, double rate, const juce::File& f, int bits = 24)
{
    if (b.getNumSamples() <= 0) return false;
    f.getParentDirectory().createDirectory();
    juce::TemporaryFile tmp(f);
    {
        juce::WavAudioFormat wav;
        auto fos = std::make_unique<juce::FileOutputStream>(tmp.getFile());
        if (! fos->openedOk()) return false;
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(fos.get(), rate, (unsigned) b.getNumChannels(), bits, {}, 0));
        if (w == nullptr) return false;
        fos.release();
        juce::AudioBuffer<float> c(b);
        for (int ch = 0; ch < c.getNumChannels(); ++ch) juce::FloatVectorOperations::clip(c.getWritePointer(ch), c.getReadPointer(ch), -1.f, 1.f, c.getNumSamples());
        if (! w->writeFromAudioSampleBuffer(c, 0, c.getNumSamples())) return false;
    }
    return tmp.overwriteTargetFileWithTemporary();
}
} // namespace rave
