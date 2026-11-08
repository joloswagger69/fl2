#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <nwa/LookAndFeel.h>

namespace mel::gui
{
namespace col = nwa::colours;
// Farben der fuenf Teile: Chords, Melody, Bass, Arp, Acid
inline const juce::Colour kPartCol[5] = { juce::Colour(0xff33c6e0), juce::Colour(0xff8a7bd8), juce::Colour(0xffe0a233), juce::Colour(0xff5fcf80), juce::Colour(0xffe0605a) };
inline const juce::Colour kPlayhead { 0x88ffffff };

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

} // namespace mel::gui
