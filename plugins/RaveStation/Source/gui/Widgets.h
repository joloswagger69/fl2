#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <nwa/LookAndFeel.h>
#include "../engine/Sample.h"

namespace rave::gui
{
namespace col = nwa::colours;
inline const juce::Colour kHit { 0xff33c6e0 }, kGhost { 0xff8a7bd8 }, kRollC { 0xffe0a233 }, kMute { 0xffd9534f }, kSolo { 0xffe0c233 }, kPlayhead { 0x55ffffff };

inline void styleLabel(juce::Label& l, float size = 13.f, juce::Colour c = col::textDim, juce::Justification j = juce::Justification::centredLeft)
{
    l.setFont(juce::FontOptions(size)); l.setColour(juce::Label::textColourId, c); l.setJustificationType(j); l.setInterceptsMouseClicks(false, false); l.setMinimumHorizontalScale(1.f);
}

// Regler mit Beschriftung, neu an einen Parameter bindbar (fuer "ausgewaehltes Pad")
class ParamSlider : public juce::Component
{
public:
    explicit ParamSlider(const juce::String& name, int labelW = 92) : labelWidth(labelW)
    {
        label.setText(name, juce::dontSendNotification); styleLabel(label);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
        slider.setColour(juce::Slider::trackColourId, col::accent); slider.setColour(juce::Slider::thumbColourId, col::text);
        slider.setColour(juce::Slider::backgroundColourId, col::outline);
        addAndMakeVisible(label); addAndMakeVisible(slider);
    }
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        att.reset();
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, id, slider);
        slider.textFromValueFunction = [](double v) { return juce::String(v, std::abs(v) >= 100.0 ? 1 : 2); };   // kurze, lesbare Anzeige
        if (auto* prm = apvts.getParameter(id)) if (prm->getLabel().isNotEmpty()) slider.setTextValueSuffix(" " + prm->getLabel());
        slider.updateText();
        slider.setDoubleClickReturnValue(true, apvts.getParameter(id) ? apvts.getParameter(id)->convertFrom0to1(apvts.getParameter(id)->getDefaultValue()) : 0.0);
    }
    void detach() { att.reset(); }
    void setSuffix(const juce::String& s) { slider.setTextValueSuffix(s); }
    void resized() override { auto r = getLocalBounds(); label.setBounds(r.removeFromLeft(labelWidth)); slider.setBounds(r); }
    juce::Slider slider;
private:
    juce::Label label; int labelWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ParamToggle : public juce::TextButton
{
public:
    explicit ParamToggle(const juce::String& name) : juce::TextButton(name)
    {
        setClickingTogglesState(true);
        setColour(juce::TextButton::buttonOnColourId, col::accent.darker(0.3f)); setColour(juce::TextButton::textColourOnId, col::text);
    }
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& id) { att.reset(); att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, id, *this); }
    void detach() { att.reset(); }
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class ParamCombo : public juce::Component
{
public:
    ParamCombo(const juce::String& name, const juce::StringArray& items, int labelW = 92) : labelWidth(labelW)
    {
        label.setText(name, juce::dontSendNotification); styleLabel(label);
        combo.addItemList(items, 1);
        addAndMakeVisible(label); addAndMakeVisible(combo);
    }
    void attach(juce::AudioProcessorValueTreeState& apvts, const juce::String& id) { att.reset(); att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, id, combo); }
    void detach() { att.reset(); }
    void resized() override { auto r = getLocalBounds(); label.setBounds(r.removeFromLeft(labelWidth)); combo.setBounds(r.reduced(0, 1)); }
    juce::ComboBox combo;
private:
    juce::Label label; int labelWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Wellenform mit Peaks-Cache. Modi: Anzeige, Marker (Slicer), Bereich (Pad Start/Ende)
class WaveView : public juce::Component
{
public:
    enum class Mode { display, markers, region };
    WaveView() { setOpaque(false); }

    void setSample(const rave::SamplePtr& s)
    {
        if (s == sample) return;
        sample = s; peaks.clear();
        if (s && s->length() > 1)
        {
            const int cols = 2048, n = s->length();
            peaks.resize((size_t) cols);
            for (int c = 0; c < cols; ++c)
            {
                const int a = (int) ((long long) c * n / cols), b = juce::jmax(a + 1, (int) ((long long) (c + 1) * n / cols));
                float lo = 0.f, hi = 0.f;
                for (int ch = 0; ch < s->channels(); ++ch)
                {
                    const float* d = s->data.getReadPointer(ch);
                    for (int i = a; i < b && i < n; ++i) { lo = juce::jmin(lo, d[i]); hi = juce::jmax(hi, d[i]); }
                }
                peaks[(size_t) c] = { lo, hi };
            }
        }
        repaint();
    }
    void setMode(Mode m) { mode = m; }
    void setMarkers(std::vector<float> m, int sel = -1) { markers = std::move(m); selected = sel; repaint(); }
    void setLoopEnd(float pos01) { loopEnd = pos01; repaint(); }
    void setRegion(float s, float e) { regionStart = s; regionEnd = e; repaint(); }
    void setPlayhead(float p) { if (std::abs(p - playhead) > 1e-4f) { playhead = p; repaint(); } }
    void setEmptyText(const juce::String& t) { emptyText = t; }

    std::function<void(float)> onAddMarker;
    std::function<void(int)> onRemoveMarker, onSelectMarker;
    std::function<void(int, float)> onMoveMarker;
    std::function<void(float)> onLoopEndChanged;
    std::function<void(float, float)> onRegionChanged;   // Start/Ende 0..1 (waehrend des Ziehens)
    std::function<void()> onRegionDragEnd;
    std::function<void()> onRegionDragStart;

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(col::background.darker(0.3f)); g.fillRoundedRectangle(r, 4.f);
        g.setColour(col::outline); g.drawRoundedRectangle(r.reduced(0.5f), 4.f, 1.f);
        if (peaks.empty()) { g.setColour(col::textDim); g.setFont(juce::FontOptions(14.f)); g.drawText(emptyText, getLocalBounds(), juce::Justification::centred); return; }
        const float mid = r.getCentreY(), amp = r.getHeight() * 0.46f;
        const int w = getWidth();
        if (mode == Mode::region)
        {
            g.setColour(col::accent.withAlpha(0.10f));
            g.fillRect(juce::Rectangle<float>(regionStart * w, r.getY(), juce::jmax(1.f, (regionEnd - regionStart) * w), r.getHeight()));
        }
        juce::Path p;
        for (int x = 0; x < w; ++x)
        {
            const auto pk = peaks[(size_t) juce::jlimit(0, (int) peaks.size() - 1, x * (int) peaks.size() / juce::jmax(1, w))];
            p.addLineSegment(juce::Line<float>((float) x + 0.5f, mid - pk.second * amp - 0.5f, (float) x + 0.5f, mid - pk.first * amp + 0.5f), 0.6f);
        }
        const bool dim = mode == Mode::region;
        g.setColour(col::accent.withAlpha(dim ? 0.85f : 0.9f)); g.fillPath(p);
        g.setColour(col::outline.brighter(0.2f)); g.drawHorizontalLine((int) mid, 0.f, (float) w);
        if (mode == Mode::region)
        {
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.fillRect(0.f, r.getY(), regionStart * w, r.getHeight()); g.fillRect(regionEnd * w, r.getY(), w - regionEnd * w, r.getHeight());
            g.setColour(col::warn);
            g.drawLine(regionStart * w, r.getY(), regionStart * w, r.getBottom(), 2.f); g.drawLine(regionEnd * w, r.getY(), regionEnd * w, r.getBottom(), 2.f);
        }
        if (mode == Mode::markers)
        {
            for (size_t i = 0; i < markers.size(); ++i)
            {
                const float x = markers[i] * w;
                g.setColour(i == (size_t) selected ? col::warn : col::text.withAlpha(0.8f));
                g.drawLine(x, r.getY(), x, r.getBottom(), i == (size_t) selected ? 2.f : 1.2f);
                g.setFont(juce::FontOptions(11.f)); g.setColour(col::text);
                g.drawText(juce::String((int) i + 1), (int) x + 3, 2, 24, 14, juce::Justification::left);
            }
            if (loopEnd > 0.f && loopEnd < 0.999f)
            {
                g.setColour(kRollC); const float x = loopEnd * w;
                const float dash[] = { 5.f, 4.f };
                g.drawDashedLine(juce::Line<float>(x, r.getY(), x, r.getBottom()), dash, 2, 2.f);
                g.setFont(juce::FontOptions(11.f)); g.drawText("loop end", (int) x - 52, (int) r.getBottom() - 16, 50, 14, juce::Justification::right);
            }
        }
        if (playhead >= 0.f) { g.setColour(kPlayhead); g.drawLine(playhead * w, r.getY(), playhead * w, r.getBottom(), 1.5f); }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (peaks.empty()) return;
        const float pos = juce::jlimit(0.f, 1.f, (float) e.x / (float) juce::jmax(1, getWidth()));
        dragKind = 0; dragIndex = -1;
        if (mode == Mode::region)
        {
            dragKind = std::abs(pos - regionStart) < std::abs(pos - regionEnd) ? 1 : 2;
            if (onRegionDragStart) onRegionDragStart();
            mouseDrag(e);
            return;
        }
        if (mode != Mode::markers) return;
        const int hit = markerAt(e.x);
        if (e.mods.isPopupMenu()) { if (hit > 0 && onRemoveMarker) onRemoveMarker(hit); return; }
        if (loopEnd > 0.f && std::abs(e.x - loopEnd * getWidth()) < 6) { dragKind = 4; return; }
        if (hit >= 0) { dragKind = 3; dragIndex = hit; if (onSelectMarker) onSelectMarker(hit); }
        else if (onAddMarker) onAddMarker(pos);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float pos = juce::jlimit(0.f, 1.f, (float) e.x / (float) juce::jmax(1, getWidth()));
        if (dragKind == 1 || dragKind == 2)
        {
            if (dragKind == 1) regionStart = juce::jmin(pos, regionEnd - 0.002f); else regionEnd = juce::jmax(pos, regionStart + 0.002f);
            if (onRegionChanged) onRegionChanged(regionStart, regionEnd);
            repaint();
        }
        else if (dragKind == 3 && dragIndex > 0 && onMoveMarker) onMoveMarker(dragIndex, pos);
        else if (dragKind == 4) { loopEnd = juce::jlimit(0.05f, 1.f, pos); if (onLoopEndChanged) onLoopEndChanged(loopEnd); repaint(); }
    }
    void mouseUp(const juce::MouseEvent&) override { if ((dragKind == 1 || dragKind == 2) && onRegionDragEnd) onRegionDragEnd(); dragKind = 0; }

    int markerAt(int x) const
    {
        int best = -1; float bd = 7.f;
        for (size_t i = 0; i < markers.size(); ++i) { const float d = std::abs(markers[i] * getWidth() - (float) x); if (d < bd) { bd = d; best = (int) i; } }
        return best;
    }
private:
    rave::SamplePtr sample;
    std::vector<std::pair<float, float>> peaks;
    std::vector<float> markers; int selected = -1;
    float loopEnd = 0.f, regionStart = 0.f, regionEnd = 1.f, playhead = -1.f;
    Mode mode = Mode::display; int dragKind = 0, dragIndex = -1;
    juce::String emptyText { "no sample" };
};
} // namespace rave::gui
