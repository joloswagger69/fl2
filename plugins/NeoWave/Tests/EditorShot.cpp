// Rendert den Editor offscreen (ohne Fenster) in PNG-Dateien - zum Pruefen des Layouts.
// Aufruf: NeoWaveShot <ausgabeordner> [presetIndex]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File out = juce::File::getCurrentWorkingDirectory();
    if (argc > 1) out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.createDirectory();
    const int preset = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;

    NeoProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    proc.loadFactoryPreset(preset);

    // ein paar Bloecke rendern, damit Scope/Spektrum/Skulptur Daten haben
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int n : { 48, 55, 64 }) midi.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8) 100), 0);
    for (int i = 0; i < 40; ++i) { buf.clear(); proc.processBlock(buf, midi); midi.clear(); }

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    for (int page = 0; page < 7; ++page)
    {
        // Seite per Tab-Klick umschalten
        for (int i = 0; i < ed->getNumChildComponents(); ++i)
            if (auto* b = dynamic_cast<juce::TextButton*>(ed->getChildComponent(i)))
                if (b->getRadioGroupId() == 100 && b->getButtonText() == juce::StringArray { "MAIN", "OSCILLATORS", "FILTER", "MODULATION", "EFFECTS", "PRESETS", "ADVANCED" }[page])
                    b->triggerClick();
        for (int k = 0; k < 20; ++k)
        {
            buf.clear(); proc.processBlock(buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(15);
        }
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream fo(out.getChildFile("page" + juce::String(page) + ".png"));
        fo.setPosition(0); fo.truncate();
        png.writeImageToStream(img, fo);
    }
    ed.reset();
    std::printf("ok\n");
    return 0;
}
