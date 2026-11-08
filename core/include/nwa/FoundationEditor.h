#pragma once
#include "FoundationProcessor.h"
#include "LookAndFeel.h"

namespace nwa
{
// Minimaler Editor des Fundaments: Gain-Regler, Preset-Leiste, Host-Anzeige.
// Weist ausdruecklich darauf hin, dass die eigentlichen Funktionen noch nicht implementiert sind.
class FoundationEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FoundationEditor(FoundationProcessor&);
    ~FoundationEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresets(const juce::String& select = {});
    void setStatus(const juce::String&, bool warn = false);
    void savePreset();

    FoundationProcessor& proc;
    LookAndFeel laf;
    juce::Slider gain;
    juce::Label gainLabel, hostLabel, statusLabel;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "Save..." }, deleteButton { "Delete" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt;
    float peak = 0.f;
    bool statusWarn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoundationEditor)
};
} // namespace nwa
