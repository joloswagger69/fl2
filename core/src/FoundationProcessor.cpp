#include <nwa/FoundationProcessor.h>
#include <nwa/FoundationEditor.h>

namespace nwa
{
juce::AudioProcessor::BusesProperties FoundationProcessor::makeBuses(bool instrument)
{
    BusesProperties b;
    if (! instrument) b = b.withInput("Input", juce::AudioChannelSet::stereo(), true);
    return b.withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

juce::AudioProcessorValueTreeState::ParameterLayout FoundationProcessor::createLayout(bool instrument)
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "gain", 1 }, instrument ? "Test Tone Gain" : "Output Gain",
        juce::NormalisableRange<float>(-60.f, 12.f, 0.1f), instrument ? -18.f : 0.f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    return l;
}

FoundationProcessor::FoundationProcessor(const PluginInfo& i)
    : AudioProcessor(makeBuses(i.instrument)),
      info(i),
      apvts(*this, nullptr, "NWA_PARAMS", createLayout(i.instrument)),
      presets(apvts, i.id, i.name.replace("//", "-"), i.stateVersion)
{
    gainDb = apvts.getRawParameterValue("gain");
}

bool FoundationProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (info.instrument) return l.getMainInputChannelSet().isDisabled() || l.inputBuses.isEmpty();
    return l.getMainInputChannelSet() == out;
}

void FoundationProcessor::prepareToPlay(double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    gain.reset(sr, 0.02);
    gain.setCurrentAndTargetValue(dbToGain(gainDb->load()));
    note = -1; env = target = phase = 0.f;
}

void FoundationProcessor::handleMidi(const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())  { note = m.getNoteNumber(); inc = midiToHz(note) / (float) sr; vel = m.getFloatVelocity(); target = 1.f; }
    else if (m.isNoteOff() && m.getNoteNumber() == note) { target = 0.f; }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { target = 0.f; }
}

void FoundationProcessor::renderTone(juce::AudioBuffer<float>& b, int start, int end) noexcept
{
    const int nch = b.getNumChannels();
    const float step = 1.f / (float) (0.005 * sr);   // 5 ms Rampe -> klickfrei
    for (int i = start; i < end; ++i)
    {
        env += juce::jlimit(-step, step, target - env);
        float s = 0.f;
        if (env > 0.f)
        {
            s = std::sin(juce::MathConstants<float>::twoPi * phase) * env * (0.25f + 0.75f * vel);
            phase += inc; if (phase >= 1.f) phase -= 1.f;
        }
        s *= gain.getNextValue();
        for (int c = 0; c < nch; ++c) b.getWritePointer(c)[i] = s;
    }
}

void FoundationProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();

    // Host-Informationen
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { hostBpm = *bpm; hostHasTempo = true; } else hostHasTempo = false;
        }

    if (n <= 0 || nch <= 0) { midi.clear(); return; }
    gain.setTargetValue(dbToGain(gainDb->load()));

    bool bad = false;
    if (info.instrument)
    {
        buffer.clear();
        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit(pos, n, meta.samplePosition);
            renderTone(buffer, pos, at);
            pos = at;
            handleMidi(meta.getMessage());
        }
        renderTone(buffer, pos, n);
    }
    else
    {
        bad |= sanitize(buffer);   // NaN/Inf vom Host nie durchreichen
        for (int i = 0; i < n; ++i)
        {
            const float g = gain.getNextValue();
            for (int c = 0; c < nch; ++c) buffer.getWritePointer(c)[i] *= g;
        }
    }
    bad |= sanitize(buffer);
    if (bad) ++sanitizedBlocks;

    float pk = 0.f;
    for (int c = 0; c < nch; ++c) pk = juce::jmax(pk, buffer.getMagnitude(c, 0, n));
    outPeak = pk;
    midi.clear();
}

juce::AudioProcessorEditor* FoundationProcessor::createEditor() { return new FoundationEditor(*this); }

void FoundationProcessor::getStateInformation(juce::MemoryBlock& dest) { state::save(apvts, info.id, info.stateVersion, dest); }

void FoundationProcessor::setStateInformation(const void* data, int size)
{
    // Ungueltige/fremde Daten lassen den aktuellen Zustand unveraendert.
    state::load(apvts, info.id, info.stateVersion, data, size);
}
} // namespace nwa
