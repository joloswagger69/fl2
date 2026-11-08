#pragma once
// Bedienelemente: Knob (mit MIDI-Learn-Kontextmenü), Auswahlfeld, Glas-Panel mit Zeilenlayout.
#include "../PluginProcessor.h"
#include "NeoLookAndFeel.h"

namespace ctl
{
using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

// ---- Wertanzeige je nach Parameter -------------------------------------------------
inline juce::String formatValue(int pid, double v)
{
    using namespace P;
    switch (pid)
    {
        case fCut:  return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " kHz" : juce::String((int) std::round(v)) + " Hz";
        case ampA: case ampD: case ampR: case fltA: case fltD: case fltR:
        case modEnvA: case modEnvD: case modEnvR: case glide: case dlTime:
            return v < 1.0 ? juce::String((int) std::round(v * 1000.0)) + " ms" : juce::String(v, 2) + " s";
        case lfo1Rate: case lfo2Rate: case lfo3Rate: case stepRate: case mfRate:
            return juce::String(v, 2) + " Hz";
        case fsShift: return (v >= 0 ? "+" : "") + juce::String((int) std::round(v)) + " Hz";
        case aFine: case bFine: return (v >= 0 ? "+" : "") + juce::String((int) std::round(v)) + " ct";
        case aOct: case bOct: case aSemi: case bSemi: return (v > 0 ? "+" : "") + juce::String((int) std::round(v));
        case bendRange: return juce::String((int) std::round(v)) + " st";
        case polyphony: case uniVoices: case stepLen: return juce::String((int) std::round(v));
        default: break;
    }
    const auto& d = pdef(pid);
    if (d.lo < 0.f) return (v >= 0 ? "+" : "") + juce::String((int) std::round(v * 100.0)) + "%";
    return juce::String((int) std::round(v * 100.0)) + "%";
}

inline double parseValue(int pid, const juce::String& t)
{
    double v = t.retainCharacters("0123456789.-+").getDoubleValue();
    using namespace P;
    switch (pid)
    {
        case fCut: if (t.containsIgnoreCase("k")) v *= 1000.0; return v;
        case ampA: case ampD: case ampR: case fltA: case fltD: case fltR:
        case modEnvA: case modEnvD: case modEnvR: case glide: case dlTime:
            return t.containsIgnoreCase("ms") ? v / 1000.0 : v;
        case lfo1Rate: case lfo2Rate: case lfo3Rate: case stepRate: case mfRate: case fsShift:
        case aFine: case bFine: case aOct: case bOct: case aSemi: case bSemi:
        case bendRange: case polyphony: case uniVoices: case stepLen:
            return v;
        default: return v / 100.0;
    }
}

// Slider mit Rechtsklick-Menü
class LearnSlider : public juce::Slider
{
public:
    std::function<void()> onContext;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onContext) { onContext(); return; }
        juce::Slider::mouseDown(e);
    }
};

inline void showParamMenu(NeoProcessor& proc, int pid, juce::Component* owner)
{
    juce::PopupMenu m;
    m.addItem(1, "MIDI Learn  (then move a controller)");
    m.addItem(2, "Clear MIDI mapping");
    m.addSeparator();
    m.addItem(3, "Reset to default");
    juce::Component::SafePointer<juce::Component> sp(owner);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(owner), [&proc, pid, sp](int r) {
        if (r == 1) proc.learnParam = pid;
        else if (r == 2) { for (auto& c : proc.ccMap) if (c.load() == pid) c = -1; }
        else if (r == 3) proc.setParamReal(pid, pdef(pid).def);
        if (sp != nullptr) sp->repaint();
    });
}

// ---- Knob ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    static constexpr int kW = 84, kH = 102;

    Knob(NeoProcessor& p, int paramId, juce::String label = {}) : proc(p), pid(paramId)
    {
        name.setText(label.isEmpty() ? juce::String(pdef(pid).name) : label, juce::dontSendNotification);
        name.setJustificationType(juce::Justification::centred);
        name.setFont(juce::Font(juce::FontOptions(12.f, juce::Font::bold)));
        name.setColour(juce::Label::textColourId, nwc::textDim);
        name.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(name);

        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 16);
        slider.setMouseDragSensitivity(180);
        slider.setPopupDisplayEnabled(false, false, nullptr);
        slider.onContext = [this] { showParamMenu(proc, pid, this); };
        addAndMakeVisible(slider);
        att = std::make_unique<SliderAtt>(proc.apvts, pdef(pid).id, slider);
        const int id = pid;
        slider.textFromValueFunction = [id](double v) { return formatValue(id, v); };
        slider.valueFromTextFunction = [id](const juce::String& t) { return parseValue(id, t); };
        slider.updateText();
        setSize(kW, kH);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        name.setBounds(b.removeFromTop(15));
        slider.setBounds(b);
    }
    void paintOverChildren(juce::Graphics& g) override
    {
        if (proc.learnParam.load() == pid)
        {
            g.setColour(nwc::neon);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.f), 8.f, 2.f);
        }
        else
        {
            for (auto& c : proc.ccMap)
                if (c.load() == pid) { g.setColour(nwc::neon); g.fillEllipse(6.f, 6.f, 5.f, 5.f); break; }
        }
    }
    int paramId() const { return pid; }
    LearnSlider slider;
private:
    NeoProcessor& proc;
    int pid;
    juce::Label name;
    std::unique_ptr<SliderAtt> att;
};

// ---- Auswahlfeld ----------------------------------------------------------------------
class Choice : public juce::Component
{
public:
    static constexpr int kH = 44;
    Choice(NeoProcessor& p, int paramId, juce::String label = {}) : pid(paramId)
    {
        name.setText(label.isEmpty() ? juce::String(pdef(pid).name) : label, juce::dontSendNotification);
        name.setFont(juce::Font(juce::FontOptions(12.f, juce::Font::bold)));
        name.setColour(juce::Label::textColourId, nwc::textDim);
        name.setJustificationType(juce::Justification::centredLeft);
        name.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(name);
        combo.addItemList(juce::StringArray::fromTokens(pdef(pid).choices, "|", ""), 1);
        addAndMakeVisible(combo);
        att = std::make_unique<ComboAtt>(p.apvts, pdef(pid).id, combo);
        setSize(120, kH);
    }
    void resized() override
    {
        auto b = getLocalBounds();
        name.setBounds(b.removeFromTop(15).reduced(4, 0));
        combo.setBounds(b.reduced(2, 1));
    }
    juce::ComboBox combo;
private:
    int pid;
    juce::Label name;
    std::unique_ptr<ComboAtt> att;
};

// ---- Panel ----------------------------------------------------------------------------
class Panel : public juce::Component
{
public:
    struct Cell { juce::Component* c; float weight; int h; bool fixedKnob; };
    explicit Panel(juce::String t) : title(std::move(t)) {}

    void addRow(std::vector<Cell> r) { rows.push_back(std::move(r)); for (auto& c : rows.back()) addAndMakeVisible(c.c); }
    // Hilfsfunktionen
    static Cell knob(Knob* k, float w = 1.f) { return { k, w, Knob::kH, true }; }
    static Cell fill(juce::Component* c, int h, float w = 1.f) { return { c, w, h, false }; }
    static Cell choice(Choice* c, float w = 1.f) { return { c, w, Choice::kH, false }; }

    int preferredHeight() const
    {
        int h = 34;
        for (auto& r : rows) { int m = 0; for (auto& c : r) m = juce::jmax(m, c.h); h += m + 4; }
        return h + 6;
    }
    void paint(juce::Graphics& g) override
    {
        using namespace nwc;
        auto b = getLocalBounds().toFloat().reduced(1.f);
        drawGlass(g, b, 14.f, 0.9f);
        g.setColour(aqua);
        g.setFont(juce::Font(juce::FontOptions(13.f, juce::Font::bold)));
        g.drawText(title, 16, 7, getWidth() - 32, 18, juce::Justification::centredLeft);
        g.setColour(aqua.withAlpha(0.25f));
        g.fillRect(16.f, 27.f, (float) getWidth() - 32.f, 1.f);
    }
    void resized() override
    {
        int y = 32;
        const int inner = getWidth() - 20;
        for (auto& r : rows)
        {
            float sum = 0; int rowH = 0;
            for (auto& c : r) { sum += c.weight; rowH = juce::jmax(rowH, c.h); }
            float x = 10.f;
            for (auto& c : r)
            {
                const float cw = (float) inner * c.weight / sum;
                if (c.fixedKnob) c.c->setBounds((int) (x + (cw - Knob::kW) * 0.5f), y, Knob::kW, Knob::kH);
                else c.c->setBounds((int) x + 3, y + (rowH - c.h) / 2, (int) cw - 6, c.h);
                x += cw;
            }
            y += rowH + 4;
        }
    }
private:
    juce::String title;
    std::vector<std::vector<Cell>> rows;
};

// ---- einfacher Text-Hinweis ---------------------------------------------------------------
class Hint : public juce::Component
{
public:
    int maxLines = 3;
    void setText(const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        g.setColour(nwc::textDim);
        g.setFont(juce::Font(juce::FontOptions(12.f)));
        g.drawFittedText(text, getLocalBounds().reduced(6, 2), juce::Justification::centredLeft, maxLines);
    }
private:
    juce::String text;
};

// ---- Matrix-Zeile: Quelle -> Ziel, Menge ---------------------------------------------------
class MatrixRow : public juce::Component
{
public:
    MatrixRow(NeoProcessor& p, int srcId, int dstId, int amtId)
        : src(p, srcId, "Source"), dst(p, dstId, "Destination"), amtPid(amtId)
    {
        addAndMakeVisible(src); addAndMakeVisible(dst);
        amt.setSliderStyle(juce::Slider::LinearHorizontal);
        amt.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
        amt.onContext = [this, &p] { showParamMenu(p, amtPid, this); };
        addAndMakeVisible(amt);
        att = std::make_unique<SliderAtt>(p.apvts, pdef(amtId).id, amt);
        amt.textFromValueFunction = [id = amtId](double v) { return formatValue(id, v); };
        amt.valueFromTextFunction = [id = amtId](const juce::String& t) { return parseValue(id, t); };
        amt.updateText();
        setSize(10, 52);
    }
    void resized() override
    {
        auto b = getLocalBounds();
        const int w = b.getWidth();
        src.setBounds(b.removeFromLeft(w / 3).reduced(0, 0));
        dst.setBounds(b.removeFromLeft(w / 3));
        amt.setBounds(b.reduced(4, 14));
    }
    Choice src, dst;
    LearnSlider amt;
private:
    int amtPid;
    std::unique_ptr<SliderAtt> att;
};
} // namespace ctl
