#pragma once
// Pattern-Daten: Plain-Struktur zum Bearbeiten (GUI, Generator, Export) und eine Bank aus Atomics,
// die der Audio-Thread ohne Sperren lesen kann, waehrend die GUI schreibt.
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cstdint>

namespace rave
{
inline constexpr int kPads = 16;
inline constexpr int kMaxSteps = 64;
inline constexpr int kUserPatterns = 16;     // A..P
inline constexpr int kVariants = 4;          // EVOLVE-Varianten
inline constexpr int kPatternSlots = kUserPatterns + kVariants;

enum StepFlags : uint8_t { kGhost = 1, kRoll = 2 };

struct Step
{
    uint8_t vel = 0;       // 0 = aus, 1..127
    uint8_t prob = 100;    // 0..100 %
    uint8_t ratchet = 1;   // 1..8 Teilungen
    uint8_t flags = 0;
    int8_t  pitch = 0;     // Halbtoene, -24..24
    bool on() const noexcept { return vel > 0; }
    bool operator==(const Step& o) const noexcept { return vel == o.vel && prob == o.prob && ratchet == o.ratchet && flags == o.flags && pitch == o.pitch; }
    void sanitize() noexcept
    {
        vel = (uint8_t) juce::jlimit(0, 127, (int) vel); prob = (uint8_t) juce::jlimit(0, 100, (int) prob);
        ratchet = (uint8_t) juce::jlimit(1, 8, (int) ratchet); flags &= 3; pitch = (int8_t) juce::jlimit(-24, 24, (int) pitch);
    }
};

struct PlainPattern
{
    Step s[kPads][kMaxSteps];
    int length = 16;

    void clear() { for (auto& row : s) for (auto& st : row) st = Step(); }
    int hits() const { int n = 0; for (int p = 0; p < kPads; ++p) for (int i = 0; i < length; ++i) n += s[p][i].on(); return n; }
    int hits(int pad) const { int n = 0; for (int i = 0; i < length; ++i) n += s[pad][i].on(); return n; }
    bool operator==(const PlainPattern& o) const
    {
        if (length != o.length) return false;
        for (int p = 0; p < kPads; ++p) for (int i = 0; i < kMaxSteps; ++i) if (! (s[p][i] == o.s[p][i])) return false;
        return true;
    }
};

class PatternBank
{
public:
    PatternBank() { for (auto& p : slots) p.length = 16; }

    Step get(int slot, int pad, int step) const noexcept
    {
        const auto& c = cell(slot, pad, step);
        Step s; s.vel = c.vel; s.prob = c.prob; s.ratchet = c.ratchet; s.flags = c.flags; s.pitch = (int8_t) c.pitch.load();
        return s;
    }
    void set(int slot, int pad, int step, Step s) noexcept
    {
        s.sanitize();
        auto& c = cell(slot, pad, step);
        c.vel = s.vel; c.prob = s.prob; c.ratchet = s.ratchet; c.flags = s.flags; c.pitch = s.pitch;
    }
    int length(int slot) const noexcept { return juce::jlimit(1, kMaxSteps, slots[(size_t) clampSlot(slot)].length.load()); }
    void setLength(int slot, int n) noexcept { slots[(size_t) clampSlot(slot)].length = juce::jlimit(1, kMaxSteps, n); }

    void load(int slot, PlainPattern& out) const
    {
        out.length = length(slot);
        for (int p = 0; p < kPads; ++p) for (int i = 0; i < kMaxSteps; ++i) out.s[p][i] = get(slot, p, i);
    }
    void store(int slot, const PlainPattern& in)
    {
        for (int p = 0; p < kPads; ++p) for (int i = 0; i < kMaxSteps; ++i) set(slot, p, i, in.s[p][i]);
        setLength(slot, in.length);
    }
    void clear(int slot) { PlainPattern p; p.length = length(slot); store(slot, p); }

    // Kompakte Textform: "L=16;pad,step,vel,prob,ratchet,flags,pitch;..."
    juce::String toString(int slot) const
    {
        juce::String t = "L=" + juce::String(length(slot));
        for (int p = 0; p < kPads; ++p)
            for (int i = 0; i < kMaxSteps; ++i)
            {
                const auto s = get(slot, p, i);
                if (! s.on()) continue;
                t << ';' << p << ',' << i << ',' << (int) s.vel << ',' << (int) s.prob << ',' << (int) s.ratchet << ',' << (int) s.flags << ',' << (int) s.pitch;
            }
        return t;
    }
    bool fromString(int slot, const juce::String& text)
    {
        PlainPattern pat;
        auto parts = juce::StringArray::fromTokens(text, ";", "");
        if (parts.isEmpty() || ! parts[0].startsWith("L=")) return false;
        pat.length = juce::jlimit(1, kMaxSteps, parts[0].substring(2).getIntValue());
        for (int k = 1; k < parts.size(); ++k)
        {
            auto f = juce::StringArray::fromTokens(parts[k], ",", "");
            if (f.size() != 7) return false;
            const int pad = f[0].getIntValue(), st = f[1].getIntValue();
            if (pad < 0 || pad >= kPads || st < 0 || st >= kMaxSteps) return false;
            Step s; s.vel = (uint8_t) f[2].getIntValue(); s.prob = (uint8_t) f[3].getIntValue(); s.ratchet = (uint8_t) f[4].getIntValue();
            s.flags = (uint8_t) f[5].getIntValue(); s.pitch = (int8_t) f[6].getIntValue(); s.sanitize();
            pat.s[pad][st] = s;
        }
        store(slot, pat);
        return true;
    }

private:
    static int clampSlot(int s) noexcept { return juce::jlimit(0, kPatternSlots - 1, s); }
    struct Cell { std::atomic<uint8_t> vel { 0 }, prob { 100 }, ratchet { 1 }, flags { 0 }; std::atomic<int> pitch { 0 }; };
    struct Slot { std::array<Cell, (size_t) kPads * kMaxSteps> cells; std::atomic<int> length { 16 }; };
    const Cell& cell(int slot, int pad, int step) const noexcept
    {
        return slots[(size_t) clampSlot(slot)].cells[(size_t) (juce::jlimit(0, kPads - 1, pad) * kMaxSteps + juce::jlimit(0, kMaxSteps - 1, step))];
    }
    Cell& cell(int slot, int pad, int step) noexcept
    {
        return slots[(size_t) clampSlot(slot)].cells[(size_t) (juce::jlimit(0, kPads - 1, pad) * kMaxSteps + juce::jlimit(0, kMaxSteps - 1, step))];
    }
    std::array<Slot, (size_t) kPatternSlots> slots;
};
} // namespace rave
