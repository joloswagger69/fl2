#pragma once
// Visualisierungen: Oszilloskop, Spektrum, animierte Klang-Skulptur, Hüllkurven- und Step-Anzeige.
// Alle lesen nur lock-free Daten aus dem Processor und laufen auf dem GUI-Timer (30 Hz).
#include "../PluginProcessor.h"
#include "NeoLookAndFeel.h"

// ------------------------------------------------------------------ Oszilloskop
class ScopeView : public juce::Component
{
public:
    explicit ScopeView(NeoProcessor& p) : proc(p) { setOpaque(false); setInterceptsMouseClicks(false, false); }

    void paint(juce::Graphics& g) override
    {
        using namespace nwc;
        auto b = getLocalBounds().toFloat();
        drawGlass(g, b, 10.f, 0.9f);
        auto r = b.reduced(8.f, 10.f);

        g.setColour(aqua.withAlpha(0.10f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int) (r.getY() + r.getHeight() * i / 4.f), r.getX(), r.getRight());
        for (int i = 1; i < 8; ++i) g.drawVerticalLine((int) (r.getX() + r.getWidth() * i / 8.f), r.getY(), r.getBottom());

        const int N = NeoProcessor::kScope, mask = N - 1, shown = 1024;
        const int pos = proc.scopePos.load();
        // Trigger: aufsteigender Nulldurchgang in den letzten 2048 Samples
        int start = pos - shown - 1;
        for (int i = 0; i < N / 2 - shown; ++i)
        {
            const int idx = pos - shown - i;
            if (proc.scope[(size_t) ((idx - 1) & mask)] < 0.f && proc.scope[(size_t) (idx & mask)] >= 0.f) { start = idx; break; }
        }
        juce::Path path;
        for (int i = 0; i < shown; ++i)
        {
            const float v = juce::jlimit(-1.f, 1.f, proc.scope[(size_t) ((start + i) & mask)] * 1.6f);
            const float px = r.getX() + r.getWidth() * (float) i / (float) (shown - 1);
            const float py = r.getCentreY() - v * r.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath(px, py); else path.lineTo(px, py);
        }
        g.setColour(aqua.withAlpha(0.18f));
        g.strokePath(path, juce::PathStrokeType(5.f));
        g.setColour(aqua.withAlpha(0.35f));
        g.strokePath(path, juce::PathStrokeType(2.5f));
        g.setColour(juce::Colours::white);
        g.strokePath(path, juce::PathStrokeType(1.2f));
    }
private:
    NeoProcessor& proc;
};

// ------------------------------------------------------------------ Spektrum
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView(NeoProcessor& p) : proc(p), fft(11), window(2048)
    {
        setOpaque(false); setInterceptsMouseClicks(false, false);
        juce::dsp::WindowingFunction<float>::fillWindowingTables(window.data(), 2048, juce::dsp::WindowingFunction<float>::hann, false);
        bars.fill(0.f);
    }

    void update()
    {
        const int N = NeoProcessor::kScope, mask = N - 1, pos = proc.scopePos.load();
        std::fill(data.begin(), data.end(), 0.f);
        for (int i = 0; i < 2048; ++i) data[(size_t) i] = proc.scope[(size_t) ((pos - 2048 + i) & mask)] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform(data.data());
        for (int b = 0; b < kBars; ++b)
        {
            const float f0 = 30.f * std::pow(18000.f / 30.f, (float) b / (float) kBars);
            const float f1 = 30.f * std::pow(18000.f / 30.f, (float) (b + 1) / (float) kBars);
            const int k0 = juce::jlimit(1, 1023, (int) (f0 / 21.53f)), k1 = juce::jlimit(k0 + 1, 1024, (int) (f1 / 21.53f) + 1);
            float m = 0.f;
            for (int k = k0; k < k1; ++k) m = juce::jmax(m, data[(size_t) k]);
            const float db = juce::Decibels::gainToDecibels(m / 256.f, -90.f);
            const float target = juce::jlimit(0.f, 1.f, (db + 70.f) / 70.f);
            bars[(size_t) b] = target > bars[(size_t) b] ? target : bars[(size_t) b] * 0.86f + target * 0.14f;
        }
    }

    void paint(juce::Graphics& g) override
    {
        using namespace nwc;
        auto b = getLocalBounds().toFloat();
        drawGlass(g, b, 10.f, 0.9f);
        auto r = b.reduced(8.f, 8.f);
        const float bw = r.getWidth() / (float) kBars;
        for (int i = 0; i < kBars; ++i)
        {
            const float h = bars[(size_t) i] * r.getHeight();
            if (h < 1.f) continue;
            auto bar = juce::Rectangle<float>(r.getX() + bw * (float) i + 0.8f, r.getBottom() - h, bw - 1.6f, h);
            g.setGradientFill(juce::ColourGradient(aqua, bar.getX(), bar.getY(), juce::Colour(0xff0a5fa0).withAlpha(0.6f), bar.getX(), bar.getBottom(), false));
            g.fillRoundedRectangle(bar, 1.5f);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.fillRect(bar.getX(), bar.getY(), bar.getWidth(), 1.5f);
        }
    }
private:
    static constexpr int kBars = 56;
    NeoProcessor& proc;
    juce::dsp::FFT fft;
    std::vector<float> window;
    std::array<float, 4096> data {};
    std::array<float, kBars> bars {};
};

// ------------------------------------------------------------------ Klang-Skulptur
class Sculpture : public juce::Component
{
public:
    struct State { float level = 0, lfo[3] = {}, step = 0, morph = 0, color = 0, detune = 0, cutoff = 0.5f; int voices = 1, active = 0; };

    Sculpture()
    {
        setOpaque(false); setInterceptsMouseClicks(false, false);
        juce::Random r(7);
        for (auto& bu : bubbles) bu = { r.nextFloat(), r.nextFloat(), 0.01f + r.nextFloat() * 0.03f, 0.01f + r.nextFloat() * 0.04f, r.nextFloat() * 6.28f };
    }

    void update(const State& s, float dt)
    {
        st = s;
        time += dt;
        smooth += (s.level - smooth) * 0.25f;
        for (auto& b : bubbles) { b.y -= b.speed * dt; b.x += std::sin(time * 0.6f + b.phase) * 0.0006f; if (b.y < -0.05f) { b.y = 1.05f; } }
    }

    void paint(juce::Graphics& g) override
    {
        using namespace nwc;
        auto b = getLocalBounds().toFloat();
        const float W = b.getWidth(), H = b.getHeight();

        // Hintergrund: Digital-Blau mit Aqua-Aura
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0d4a82), b.getCentreX(), b.getCentreY(), deep, b.getX(), b.getY(), true));
        g.fillRoundedRectangle(b, 18.f);
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle(b, 18.f);
        g.reduceClipRegion(clip);

        // feines Raster
        g.setColour(aqua.withAlpha(0.06f));
        for (float x = 0; x < W; x += 28.f) g.drawVerticalLine((int) x, 0.f, H);
        for (float y = 0; y < H; y += 28.f) g.drawHorizontalLine((int) y, 0.f, W);

        // Blasen
        for (auto& bu : bubbles)
        {
            const float r = bu.r * H, cx = bu.x * W, cy = bu.y * H;
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.35f), cx - r * 0.4f, cy - r * 0.5f, aqua.withAlpha(0.04f), cx + r, cy + r, true));
            g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 0.8f);
        }

        const float cx = b.getCentreX(), cy = b.getCentreY();
        const float R = juce::jmin(W, H) * 0.27f * (1.f + 0.07f * smooth);

        // Boden-Reflexion
        g.setGradientFill(juce::ColourGradient(aqua.withAlpha(0.22f), cx, cy + R * 1.45f, aqua.withAlpha(0.f), cx + R * 1.5f, cy + R * 1.45f, true));
        g.fillEllipse(cx - R * 1.6f, cy + R * 1.25f, R * 3.2f, R * 0.4f);

        // Orbits
        struct Orb { float z, x, y, r; };
        std::vector<Orb> orbs;
        const int n = juce::jlimit(1, 16, st.voices);
        for (int ring = 0; ring < 3; ++ring)
        {
            const float tilt = 0.28f + 0.22f * (float) ring;
            const float rot = time * (0.15f + 0.05f * (float) ring) * (ring == 1 ? -1.f : 1.f) + (float) ring * 1.1f;
            const float rx = R * (1.5f + 0.18f * (float) ring + st.detune * 0.35f), ry = rx * tilt;
            juce::Path ringPath;
            ringPath.addEllipse(-rx, -ry, rx * 2, ry * 2);
            const auto tf = juce::AffineTransform::rotation(rot * 0.0f + (float) ring * 0.5f - 0.35f).translated(cx, cy);
            g.setColour(aqua.withAlpha(0.18f));
            g.strokePath(ringPath, juce::PathStrokeType(1.4f), tf);
            // Orbs auf den Ringen
            for (int i = ring; i < n; i += 3)
            {
                const float a = time * (0.5f + 0.2f * (float) ring) + (float) i * juce::MathConstants<float>::twoPi / (float) juce::jmax(1, n / 3);
                float ox = rx * std::cos(a), oy = ry * std::sin(a);
                const float z = std::sin(a);
                const float ang = (float) ring * 0.5f - 0.35f;
                const float tx = ox * std::cos(ang) - oy * std::sin(ang), ty = ox * std::sin(ang) + oy * std::cos(ang);
                orbs.push_back({ z, cx + tx, cy + ty, 4.f + 3.f * (z * 0.5f + 0.5f) + smooth * 4.f });
            }
        }
        auto drawOrb = [&](const Orb& o) {
            g.setGradientFill(juce::ColourGradient(juce::Colours::white, o.x - o.r * 0.4f, o.y - o.r * 0.5f, turq.darker(0.2f), o.x + o.r, o.y + o.r, true));
            g.fillEllipse(o.x - o.r, o.y - o.r, o.r * 2, o.r * 2);
            g.setColour(aqua.withAlpha(0.35f));
            g.drawEllipse(o.x - o.r - 2, o.y - o.r - 2, o.r * 2 + 4, o.r * 2 + 4, 1.f);
        };
        for (auto& o : orbs) if (o.z < 0.f) drawOrb(o);

        // Glaskugel
        g.setColour(aqua.withAlpha(0.14f + 0.12f * smooth));
        g.fillEllipse(cx - R * 1.25f, cy - R * 1.25f, R * 2.5f, R * 2.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xaa9ff3ff), cx - R * 0.5f, cy - R * 0.6f, juce::Colour(0x990a3a70), cx + R * 0.7f, cy + R * 0.9f, true));
        g.fillEllipse(cx - R, cy - R, R * 2, R * 2);

        // Organischer Klang-Kern (Blob)
        juce::Path blob;
        const int pts = 96;
        const float amp = 0.07f + 0.30f * smooth;
        for (int i = 0; i <= pts; ++i)
        {
            const float th = juce::MathConstants<float>::twoPi * (float) i / (float) pts;
            float rr = 1.f + amp * std::sin(2.f * th + time * 1.3f + st.lfo[0] * 2.f)
                           + amp * 0.7f * std::sin(3.f * th - time * 0.9f + st.lfo[1] * 2.f)
                           + (0.04f + st.morph * 0.12f) * std::sin(5.f * th + time * 2.1f)
                           + st.color * 0.08f * std::sin(9.f * th - time * 3.1f + st.lfo[2] * 3.f);
            rr *= 0.56f * R;
            const float px = cx + rr * std::cos(th), py = cy + rr * std::sin(th);
            if (i == 0) blob.startNewSubPath(px, py); else blob.lineTo(px, py);
        }
        blob.closeSubPath();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe9ffff), cx - R * 0.3f, cy - R * 0.4f,
                                               juce::Colour(0xff0aa0d8).interpolatedWith(turq, st.cutoff).withAlpha(0.85f), cx + R * 0.4f, cy + R * 0.5f, true));
        g.fillPath(blob);
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.strokePath(blob, juce::PathStrokeType(1.4f));

        // Glanzlichter der Kugel
        juce::Path hl; hl.addEllipse(cx - R * 0.78f, cy - R * 0.97f, R * 1.56f, R * 0.9f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.8f), cx, cy - R * 0.95f, juce::Colours::white.withAlpha(0.f), cx, cy - R * 0.15f, false));
        g.fillPath(hl);
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawEllipse(cx - R, cy - R, R * 2, R * 2, 1.6f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.0f), cx, cy + R * 0.2f, aqua.withAlpha(0.55f), cx, cy + R, false));
        juce::Path rimLow; rimLow.addCentredArc(cx, cy, R * 0.96f, R * 0.96f, 0.f, 1.2f, 1.95f, true);
        g.strokePath(rimLow, juce::PathStrokeType(3.f));

        for (auto& o : orbs) if (o.z >= 0.f) drawOrb(o);
        g.restoreState();

        // Glas-Rahmen
        g.setColour(aqua.withAlpha(0.5f));
        g.drawRoundedRectangle(b.reduced(0.7f), 18.f, 1.4f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.14f), 0, b.getY(), juce::Colours::white.withAlpha(0.f), 0, b.getY() + H * 0.4f, false));
        g.fillRoundedRectangle(b.reduced(2.f).withHeight(H * 0.4f), 16.f);
    }

private:
    struct Bubble { float x, y, r, speed, phase; };
    std::array<Bubble, 16> bubbles;
    State st;
    float time = 0.f, smooth = 0.f;
};

// ------------------------------------------------------------------ ADSR-Anzeige
class EnvView : public juce::Component
{
public:
    EnvView(NeoProcessor& p, int a, int d, int s, int r) : proc(p), ids { a, d, s, r } { setInterceptsMouseClicks(false, false); setOpaque(false); }

    void paint(juce::Graphics& g) override
    {
        using namespace nwc;
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff02101f).withAlpha(0.6f));
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(aqua.withAlpha(0.3f));
        g.drawRoundedRectangle(b, 6.f, 1.f);
        auto r = b.reduced(6.f, 6.f);
        const float a = proc.getParamReal(ids[0]), d = proc.getParamReal(ids[1]), s = proc.getParamReal(ids[2]), rel = proc.getParamReal(ids[3]);
        auto w = [](float t) { return std::pow(juce::jlimit(0.001f, 10.f, t), 0.4f); };
        const float wa = w(a), wd = w(d), wr = w(rel), ws = 0.9f;
        const float tot = wa + wd + ws + wr;
        const float x0 = r.getX(), x1 = x0 + r.getWidth() * wa / tot, x2 = x1 + r.getWidth() * wd / tot, x3 = x2 + r.getWidth() * ws / tot, x4 = r.getRight();
        const float yb = r.getBottom(), yt = r.getY(), ys = yb - s * r.getHeight();
        juce::Path p;
        p.startNewSubPath(x0, yb); p.lineTo(x1, yt);
        for (int i = 1; i <= 10; ++i) p.lineTo(x1 + (x2 - x1) * (float) i / 10.f, ys + (yt - ys) * std::exp(-4.f * (float) i / 10.f));
        p.lineTo(x3, ys);
        for (int i = 1; i <= 10; ++i) p.lineTo(x3 + (x4 - x3) * (float) i / 10.f, yb - (yb - ys) * std::exp(-4.f * (float) i / 10.f));
        juce::Path fill(p); fill.lineTo(x4, yb); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(aqua.withAlpha(0.35f), 0, yt, aqua.withAlpha(0.02f), 0, yb, false));
        g.fillPath(fill);
        g.setColour(aqua.withAlpha(0.3f)); g.strokePath(p, juce::PathStrokeType(4.f));
        g.setColour(juce::Colours::white); g.strokePath(p, juce::PathStrokeType(1.4f));
    }
private:
    NeoProcessor& proc;
    int ids[4];
};

// ------------------------------------------------------------------ Step-Modulator Editor
class StepEditor : public juce::Component
{
public:
    explicit StepEditor(NeoProcessor& p) : proc(p) {}

    void paint(juce::Graphics& g) override
    {
        using namespace nwc;
        auto b = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff02101f).withAlpha(0.6f));
        g.fillRoundedRectangle(b, 8.f);
        g.setColour(aqua.withAlpha(0.3f));
        g.drawRoundedRectangle(b, 8.f, 1.f);
        const int len = juce::jlimit(2, 16, (int) std::lround(proc.getParamReal(P::stepLen)));
        const int cur = proc.visStepIdx.load();
        const float cw = b.getWidth() / 16.f;
        const float mid = b.getCentreY();
        g.setColour(aqua.withAlpha(0.25f));
        g.drawHorizontalLine((int) mid, b.getX() + 4.f, b.getRight() - 4.f);
        for (int i = 0; i < 16; ++i)
        {
            const float v = proc.getParamReal(P::step1 + i);
            auto col = juce::Rectangle<float>(b.getX() + cw * (float) i + 3.f, b.getY() + 4.f, cw - 6.f, b.getHeight() - 8.f);
            const bool on = i < len;
            const float h = v * (col.getHeight() * 0.5f);
            juce::Rectangle<float> bar(col.getX(), h >= 0 ? mid - h : mid, col.getWidth(), std::abs(h));
            if (std::abs(h) < 1.5f) bar = { col.getX(), mid - 1.f, col.getWidth(), 2.f };
            g.setColour(juce::Colours::white.withAlpha(on ? 0.06f : 0.02f));
            g.fillRoundedRectangle(col, 3.f);
            g.setGradientFill(juce::ColourGradient(on ? (i == cur ? juce::Colours::white : aqua) : textDim.withAlpha(0.4f), bar.getX(), bar.getY(),
                                                   on ? turq.darker(0.3f) : textDim.withAlpha(0.2f), bar.getX(), bar.getBottom(), false));
            g.fillRoundedRectangle(bar, 2.f);
            if (on && i == cur) { g.setColour(aqua.withAlpha(0.5f)); g.drawRoundedRectangle(col, 3.f, 1.2f); }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override { begin(e); }
    void mouseDrag(const juce::MouseEvent& e) override { setFrom(e); }
    void mouseUp(const juce::MouseEvent&) override { if (active >= 0) { proc.param(P::step1 + active)->endChangeGesture(); active = -1; } }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit(0, 15, (int) (e.position.x / (getWidth() / 16.f)));
        proc.setParamReal(P::step1 + i, 0.f);
    }

private:
    void begin(const juce::MouseEvent& e) { active = -1; setFrom(e); }
    void setFrom(const juce::MouseEvent& e)
    {
        const int i = juce::jlimit(0, 15, (int) (e.position.x / (getWidth() / 16.f)));
        if (i != active)
        {
            if (active >= 0) proc.param(P::step1 + active)->endChangeGesture();
            active = i;
            proc.param(P::step1 + i)->beginChangeGesture();
        }
        float v = 1.f - 2.f * (e.position.y - 4.f) / (float) (getHeight() - 8);
        v = juce::jlimit(-1.f, 1.f, v);
        if (std::abs(v) < 0.06f) v = 0.f;
        proc.setParamReal(P::step1 + i, v);
        repaint();
    }
    NeoProcessor& proc;
    int active = -1;
};
