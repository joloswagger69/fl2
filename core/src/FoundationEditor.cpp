#include <nwa/FoundationEditor.h>

namespace nwa
{
FoundationEditor::FoundationEditor(FoundationProcessor& p) : juce::AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&laf);
    setSize(560, 380);

    gain.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
    gain.setTextValueSuffix(" dB");
    addAndMakeVisible(gain);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "gain", gain);

    gainLabel.setText(proc.info.instrument ? "Test tone gain" : "Output gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setColour(juce::Label::textColourId, colours::textDim);
    addAndMakeVisible(gainLabel);

    hostLabel.setFont(juce::Font(juce::FontOptions(14.f)));
    hostLabel.setColour(juce::Label::textColourId, colours::text);
    addAndMakeVisible(hostLabel);

    statusLabel.setFont(juce::Font(juce::FontOptions(13.f)));
    statusLabel.setColour(juce::Label::textColourId, colours::textDim);
    addAndMakeVisible(statusLabel);

    presetBox.setTextWhenNothingSelected("User presets");
    presetBox.onChange = [this] {
        const auto name = presetBox.getText();
        if (name.isEmpty()) return;
        switch (proc.presets.load(name))
        {
            case PresetManager::Status::ok:          setStatus("Loaded preset \"" + name + "\""); break;
            case PresetManager::Status::notFound:    setStatus("Preset file not found", true); refreshPresets(); break;
            case PresetManager::Status::wrongPlugin: setStatus("That preset belongs to a different plugin", true); break;
            case PresetManager::Status::invalidName:
            case PresetManager::Status::ioError:
            case PresetManager::Status::corrupt:     setStatus("Preset file is damaged", true); break;
        }
    };
    addAndMakeVisible(presetBox);

    saveButton.onClick = [this] { savePreset(); };
    deleteButton.onClick = [this] {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && proc.presets.remove(name)) { setStatus("Deleted \"" + name + "\""); refreshPresets(); }
    };
    addAndMakeVisible(saveButton);
    addAndMakeVisible(deleteButton);

    refreshPresets();
    setStatus("Foundation build - the sound engine arrives in phase " + juce::String(proc.info.phase));
    startTimerHz(10);
    timerCallback();
}

FoundationEditor::~FoundationEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void FoundationEditor::setStatus(const juce::String& s, bool warn)
{
    statusWarn = warn;
    statusLabel.setColour(juce::Label::textColourId, warn ? colours::warn : colours::textDim);
    statusLabel.setText(s, juce::dontSendNotification);
}

void FoundationEditor::refreshPresets(const juce::String& select)
{
    presetBox.clear(juce::dontSendNotification);
    const auto names = proc.presets.list();
    for (int i = 0; i < names.size(); ++i) presetBox.addItem(names[i], i + 1);
    const auto sel = select.isNotEmpty() ? select : proc.presets.currentName;
    const int idx = names.indexOf(sel);
    if (idx >= 0) presetBox.setSelectedId(idx + 1, juce::dontSendNotification);
}

void FoundationEditor::savePreset()
{
    auto* w = new juce::AlertWindow("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", proc.presets.currentName, "Name");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<FoundationEditor> sp(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([sp, w](int r) {
        std::unique_ptr<juce::AlertWindow> own(w);
        if (r != 1 || sp == nullptr) return;
        const auto name = own->getTextEditorContents("name");
        switch (sp->proc.presets.save(name))
        {
            case PresetManager::Status::ok:          sp->setStatus("Saved preset \"" + sp->proc.presets.currentName + "\""); sp->refreshPresets(); break;
            case PresetManager::Status::invalidName: sp->setStatus("That name cannot be used as a file name", true); break;
            case PresetManager::Status::ioError:
            case PresetManager::Status::notFound:
            case PresetManager::Status::corrupt:
            case PresetManager::Status::wrongPlugin: sp->setStatus("Could not write the preset file", true); break;
        }
    }), false);
}

void FoundationEditor::timerCallback()
{
    juce::String t;
    if (proc.hostHasTempo.load()) t << "Host tempo: " << juce::String(proc.hostBpm.load(), 2) << " BPM";
    else t << "Host tempo: not provided";
    t << "   |   Transport: " << (proc.hostPlaying.load() ? "playing" : "stopped");
    hostLabel.setText(t, juce::dontSendNotification);

    const float p = proc.outPeak.load();
    peak = p > peak ? p : peak * 0.85f;
    repaint(getLocalBounds().removeFromBottom(46));
}

void FoundationEditor::paint(juce::Graphics& g)
{
    using namespace colours;
    g.fillAll(background);

    g.setColour(text);
    g.setFont(juce::Font(juce::FontOptions(28.f, juce::Font::bold)));
    g.drawText(proc.info.name, 24, 16, getWidth() - 48, 34, juce::Justification::centredLeft);

    g.setColour(textDim);
    g.setFont(juce::Font(juce::FontOptions(13.f)));
    g.drawText("NEO//WAVE AUDIO  -  v" + juce::String(kVersion) + "  -  "
                   + juce::String(proc.info.instrument ? "instrument" : "effect"),
               24, 50, getWidth() - 48, 18, juce::Justification::centredLeft);

    // Hinweis-Kasten
    auto box = juce::Rectangle<float>(24.f, 84.f, (float) getWidth() - 48.f, 96.f);
    g.setColour(panel); g.fillRoundedRectangle(box, 8.f);
    g.setColour(outline); g.drawRoundedRectangle(box, 8.f, 1.f);
    g.setColour(warn);
    g.setFont(juce::Font(juce::FontOptions(13.f, juce::Font::bold)));
    g.drawText("FOUNDATION BUILD - FEATURES NOT IMPLEMENTED YET", box.reduced(14.f, 8.f).removeFromTop(18.f), juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(juce::Font(juce::FontOptions(13.f)));
    g.drawFittedText("Planned for phase " + juce::String(proc.info.phase) + ": " + proc.info.summary
                         + (proc.info.instrument ? "\nWorking now: sine test tone on incoming MIDI notes, gain, state, presets, host tempo."
                                                 : "\nWorking now: stereo/mono pass-through with gain, state, presets, host tempo."),
                     box.reduced(14.f, 30.f).toNearestInt(), juce::Justification::topLeft, 5);

    // Pegelanzeige
    auto bar = juce::Rectangle<float>(24.f, (float) getHeight() - 36.f, (float) getWidth() - 48.f, 8.f);
    g.setColour(panel); g.fillRoundedRectangle(bar, 4.f);
    const float lv = juce::jlimit(0.f, 1.f, (juce::Decibels::gainToDecibels(peak, -60.f) + 60.f) / 60.f);
    g.setColour(peak > 1.f ? warn : accent);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * lv), 4.f);
    g.setColour(textDim);
    g.setFont(juce::Font(juce::FontOptions(11.f)));
    g.drawText("OUTPUT PEAK", 24, getHeight() - 24, 120, 14, juce::Justification::centredLeft);
}

void FoundationEditor::resized()
{
    auto b = getLocalBounds().reduced(24);
    b.removeFromTop(166);                       // Kopf + Hinweis
    auto left = b.removeFromLeft(130);
    gainLabel.setBounds(left.removeFromTop(18));
    gain.setBounds(left.removeFromTop(120));

    b.removeFromLeft(16);
    hostLabel.setBounds(b.removeFromTop(24));
    b.removeFromTop(8);
    auto row = b.removeFromTop(28);
    deleteButton.setBounds(row.removeFromRight(70));
    row.removeFromRight(6);
    saveButton.setBounds(row.removeFromRight(80));
    row.removeFromRight(6);
    presetBox.setBounds(row);
    b.removeFromTop(8);
    statusLabel.setBounds(b.removeFromTop(22));
}
} // namespace nwa
