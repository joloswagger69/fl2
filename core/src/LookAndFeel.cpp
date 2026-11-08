#include <nwa/LookAndFeel.h>

namespace nwa
{
LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, outline);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.darker(0.4f));
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    using namespace colours;
    auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(6.f);
    const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc(c.x, c.y, r - 3.f, r - 3.f, 0.f, a0, a1, true);
    g.setColour(outline);
    g.strokePath(track, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc(c.x, c.y, r - 3.f, r - 3.f, 0.f, a0, angle, true);
    g.setColour(accent);
    g.strokePath(val, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(panel.brighter(0.15f));
    g.fillEllipse(c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);
    juce::Path p;
    p.addRoundedRectangle(-1.5f, -r * 0.58f, 3.f, r * 0.34f, 1.5f);
    g.setColour(text);
    g.fillPath(p, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
}
} // namespace nwa
