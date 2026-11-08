#pragma once
// Standard-MIDI-File-Export (Typ 1, 480 Ticks pro Viertel): Tempo, Taktart, Spurnamen, korrekte Notenlaengen und Velocity.
#include <juce_audio_basics/juce_audio_basics.h>
#include "Project.h"

namespace mel::midi
{
struct TrackSpec { const Part* notes; int channel; juce::String name; };

inline juce::MidiMessageSequence noteTrack(const TrackSpec& t, int lengthTicks, bool withMeta, double bpm, int sigNum, int sigDen)
{
    juce::MidiMessageSequence seq;
    seq.addEvent(juce::MidiMessage::textMetaEvent(3, t.name), 0.0);
    if (withMeta)
    {
        seq.addEvent(juce::MidiMessage::tempoMetaEvent((int) std::lround(60000000.0 / std::clamp(bpm, 20.0, 400.0))), 0.0);
        seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(sigNum, sigDen), 0.0);
    }
    struct E { int tick; bool on; int pitch; int vel; };
    std::vector<E> ev;
    for (auto& n : *t.notes)
    {
        if (n.len < 1 || n.pitch < 0 || n.pitch > 127) continue;
        ev.push_back({ n.tick, true, n.pitch, std::clamp(n.vel, 1, 127) });
        ev.push_back({ n.tick + n.len, false, n.pitch, 0 });
    }
    std::stable_sort(ev.begin(), ev.end(), [](const E& a, const E& b) { if (a.tick != b.tick) return a.tick < b.tick; return a.on < b.on; });   // Offs vor Ons
    const int ch = std::clamp(t.channel, 1, 16);
    for (auto& e : ev) seq.addEvent(e.on ? juce::MidiMessage::noteOn(ch, e.pitch, (juce::uint8) e.vel) : juce::MidiMessage::noteOff(ch, e.pitch), (double) e.tick);
    seq.addEvent(juce::MidiMessage::endOfTrack(), (double) std::max(lengthTicks, ev.empty() ? 0 : ev.back().tick));
    return seq;
}

// Schreibt eine Datei mit dem Tempo-/Taktart-Track und den angegebenen Spuren. Rueckgabe false + Fehlertext bei Problemen.
inline bool writeFile(const juce::File& file, const std::vector<TrackSpec>& tracks, double bpm, int sigNum, int sigDen, int lengthTicks, juce::String* error = nullptr)
{
    if (tracks.empty()) { if (error) *error = "Nothing to export (the part is empty)"; return false; }
    juce::MidiFile mf; mf.setTicksPerQuarterNote(kPpq);
    if (tracks.size() == 1) mf.addTrack(noteTrack(tracks[0], lengthTicks, true, bpm, sigNum, sigDen));
    else
    {
        juce::MidiMessageSequence conductor;
        conductor.addEvent(juce::MidiMessage::textMetaEvent(3, "MELODY//STUDIO"), 0.0);
        conductor.addEvent(juce::MidiMessage::tempoMetaEvent((int) std::lround(60000000.0 / std::clamp(bpm, 20.0, 400.0))), 0.0);
        conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(sigNum, sigDen), 0.0);
        conductor.addEvent(juce::MidiMessage::endOfTrack(), (double) lengthTicks);
        mf.addTrack(conductor);
        for (auto& t : tracks) mf.addTrack(noteTrack(t, lengthTicks, false, bpm, sigNum, sigDen));
    }
    juce::MemoryOutputStream mos;
    if (! mf.writeTo(mos, 1)) { if (error) *error = "Could not build the MIDI data"; return false; }
    auto parent = file.getParentDirectory();
    if (! parent.exists()) parent.createDirectory();
    juce::TemporaryFile tmp(file);                                  // atomar: erst temporaer schreiben, dann ersetzen
    if (! tmp.getFile().replaceWithData(mos.getData(), mos.getDataSize())) { if (error) *error = "Could not write " + file.getFullPathName(); return false; }
    if (! tmp.overwriteTargetFileWithTemporary()) { if (error) *error = "Could not replace " + file.getFullPathName(); return false; }
    return true;
}
} // namespace mel::midi
