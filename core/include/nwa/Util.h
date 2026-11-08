#pragma once
// Kleine, allokationsfreie Hilfsfunktionen (audio-thread-tauglich).
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

#ifndef NWA_VERSION_STRING
 #define NWA_VERSION_STRING "0.0.0"
#endif

namespace nwa
{
inline constexpr const char* kVersion = NWA_VERSION_STRING;

inline float dbToGain(float db) noexcept   { return std::pow(10.f, db * 0.05f); }
inline float gainToDb(float g) noexcept    { return 20.f * std::log10(std::max(g, 1.0e-9f)); }

// Ersetzt NaN/Inf durch 0. Gibt true zurueck, wenn etwas ersetzt wurde.
inline bool sanitize(float* data, int n) noexcept
{
    bool bad = false;
    for (int i = 0; i < n; ++i)
        if (! std::isfinite(data[i])) { data[i] = 0.f; bad = true; }
    return bad;
}
inline bool sanitize(juce::AudioBuffer<float>& b) noexcept
{
    bool bad = false;
    for (int c = 0; c < b.getNumChannels(); ++c) bad |= sanitize(b.getWritePointer(c), b.getNumSamples());
    return bad;
}

inline float midiToHz(int note) noexcept { return 440.f * std::pow(2.f, ((float) note - 69.f) / 12.f); }
} // namespace nwa
