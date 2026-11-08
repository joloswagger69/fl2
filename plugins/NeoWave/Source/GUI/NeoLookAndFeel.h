#pragma once
// Y2K / Frutiger-Aero Look: Glas, Chrom, Aqua-Verlaeufe, plastische Regler
#include <juce_gui_basics/juce_gui_basics.h>

namespace nwc
{
const juce::Colour deep    { 0xff031126 };
const juce::Colour navy    { 0xff06284d };
const juce::Colour ocean   { 0xff0b4a7e };
const juce::Colour aqua    { 0xff3de6ff };
const juce::Colour cyan    { 0xff00c8e8 };
const juce::Colour turq    { 0xff19e6c1 };
const juce::Colour silver  { 0xffdfe8f2 };
const juce::Colour text    { 0xffeaf6ff };
const juce::Colour textDim { 0xff8fb8d6 };
const juce::Colour neon    { 0xffb6ff3d };
}

inline void drawGlass(juce::Graphics& g, juce::Rectangle<float> r, float corner, float alpha = 1.f)
{
    using namespace nwc;
    g.setGradientFill(juce::ColourGradient(ocean.withAlpha(0.55f * alpha), r.getX(), r.getY(),
                                           deep.withAlpha(0.70f * alpha), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, corner);
    // oberer Glanz
    auto top = r.reduced(1.5f).withHeight(r.getHeight() * 0.45f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.20f * alpha), top.getX(), top.getY(),
                                           juce::Colours::white.withAlpha(0.02f * alpha), top.getX(), top.getBottom(), false));
    g.fillRoundedRectangle(top, corner - 1.f);
    g.setColour(aqua.withAlpha(0.35f * alpha));
    g.drawRoundedRectangle(r, corner, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.12f * alpha));
    g.drawRoundedRectangle(r.reduced(1.5f), corner - 1.f, 1.f);
}

class NeoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NeoLookAndFeel()
    {
        using namespace nwc;
        setColour(juce::Slider::textBoxTextColourId, text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, text);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::PopupMenu::backgroundColourId, navy);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, cyan.darker(0.3f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId, deep.withAlpha(0.7f));
        setColour(juce::TextEditor::textColourId, text);
        setColour(juce::TextEditor::outlineColourId, aqua.withAlpha(0.4f));
        setColour(juce::TextEditor::focusedOutlineColourId, aqua);
        setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ScrollBar::thumbColourId, aqua.withAlpha(0.5f));
        setColour(juce::AlertWindow::backgroundColourId, navy);
        setColour(juce::AlertWindow::textColourId, text);
    }

    juce::Font getLabelFont(juce::Label& l) override { return juce::Font(juce::FontOptions(l.getFont().getHeight())); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::Font(juce::FontOptions(13.f)); }
    juce::Font getPopupMenuFont() override { return juce::Font(juce::FontOptions(14.f)); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        using namespace nwc;
        auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(3.f);
        const float d = juce::jmin(b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;
        const float ang = a0 + pos * (a1 - a0);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const bool en = s.isEnabled();

        // Spur
        juce::Path track;
        track.addCentredArc(c.x, c.y, r, r, 0.f, a0, a1, true);
        g.setColour(juce::Colour(0xff02101f).withAlpha(0.8f));
        g.strokePath(track, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Wert-Bogen mit Glow
        juce::Path val;
        const float from = bipolar ? a0 + (a1 - a0) * 0.5f : a0;
        val.addCentredArc(c.x, c.y, r, r, 0.f, juce::jmin(from, ang), juce::jmax(from, ang), true);
        g.setColour(aqua.withAlpha(en ? 0.22f : 0.08f));
        g.strokePath(val, juce::PathStrokeType(8.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(en ? aqua : textDim);
        g.strokePath(val, juce::PathStrokeType(3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Chrom-Ring
        const float rb = r - 5.f;
        g.setGradientFill(juce::ColourGradient(juce::Colours::white, c.x, c.y - rb, juce::Colour(0xff3b4f66), c.x, c.y + rb, false));
        g.fillEllipse(c.x - rb, c.y - rb, rb * 2, rb * 2);
        // Glas-Kuppel
        const float rd = rb - 2.5f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff9af2ff), c.x - rd * 0.5f, c.y - rd * 0.6f,
                                               juce::Colour(0xff0a3a66), c.x + rd * 0.6f, c.y + rd * 0.9f, true));
        g.fillEllipse(c.x - rd, c.y - rd, rd * 2, rd * 2);
        // Glanzlicht
        juce::Path hl;
        hl.addEllipse(c.x - rd * 0.72f, c.y - rd * 0.92f, rd * 1.44f, rd * 0.85f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.75f), c.x, c.y - rd * 0.9f,
                                               juce::Colours::white.withAlpha(0.0f), c.x, c.y - rd * 0.1f, false));
        g.fillPath(hl);
        // Zeiger
        const float ca = std::cos(ang - juce::MathConstants<float>::halfPi), sa = std::sin(ang - juce::MathConstants<float>::halfPi);
        g.setColour(juce::Colour(0xff02101f));
        g.drawLine(c.x + ca * rd * 0.25f, c.y + sa * rd * 0.25f, c.x + ca * rd * 0.88f, c.y + sa * rd * 0.88f, 3.2f);
        g.setColour(juce::Colours::white);
        g.drawLine(c.x + ca * rd * 0.30f, c.y + sa * rd * 0.30f, c.x + ca * rd * 0.88f, c.y + sa * rd * 0.88f, 1.8f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                          juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace nwc;
        const float cy = (float) y + (float) h * 0.5f;
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        g.setColour(juce::Colour(0xff02101f).withAlpha(0.8f));
        g.fillRoundedRectangle((float) x, cy - 3.f, (float) w, 6.f, 3.f);
        const float mid = (float) x + (float) w * 0.5f;
        const float from = bipolar ? mid : (float) x;
        g.setGradientFill(juce::ColourGradient(aqua, from, cy, turq, pos, cy, false));
        g.fillRoundedRectangle(juce::jmin(from, pos), cy - 3.f, std::abs(pos - from), 6.f, 3.f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white, pos, cy - 7.f, juce::Colour(0xff3b4f66), pos, cy + 7.f, false));
        g.fillEllipse(pos - 7.f, cy - 7.f, 14.f, 14.f);
        g.setColour(aqua.withAlpha(0.8f));
        g.fillEllipse(pos - 4.f, cy - 4.f, 8.f, 8.f);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override
    {
        using namespace nwc;
        auto r = juce::Rectangle<float>(0.f, 0.f, (float) w, (float) h).reduced(1.f);
        g.setGradientFill(juce::ColourGradient(down ? cyan.darker(0.4f) : juce::Colour(0xff2f78ad), 0, r.getY(),
                                               juce::Colour(0xff0a2b4d), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 7.f);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRoundedRectangle(r.reduced(2.f).withHeight(r.getHeight() * 0.45f), 5.f);
        g.setColour(aqua.withAlpha(0.6f));
        g.drawRoundedRectangle(r, 7.f, 1.1f);
        juce::Path arrow;
        const float ax = (float) w - 14.f, ay = (float) h * 0.5f;
        arrow.addTriangle(ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
        g.setColour(juce::Colours::white);
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        using namespace nwc;
        auto r = b.getLocalBounds().toFloat().reduced(1.f);
        const bool on = b.getToggleState();
        const auto top = on ? turq : juce::Colour(0xff3a86bd);
        const auto bot = on ? juce::Colour(0xff0a6f86) : juce::Colour(0xff0a2b4d);
        g.setGradientFill(juce::ColourGradient((down ? top.darker(0.3f) : (hover ? top.brighter(0.2f) : top)), 0, r.getY(), bot, 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.fillRoundedRectangle(r.reduced(3.f, 2.f).withHeight(r.getHeight() * 0.42f), r.getHeight() * 0.25f);
        g.setColour(aqua.withAlpha(on ? 0.9f : 0.5f));
        g.drawRoundedRectangle(r, r.getHeight() * 0.5f, 1.1f);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(13.f, juce::Font::bold)); }

    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override
    {
        g.fillAll(nwc::navy);
        g.setColour(nwc::aqua.withAlpha(0.5f));
        g.drawRect(0, 0, w, h, 1);
    }
};
