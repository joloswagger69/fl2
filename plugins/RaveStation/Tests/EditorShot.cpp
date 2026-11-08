// Rendert den Editor offscreen (ohne Fenster) in PNG-Dateien - zum Pruefen des Layouts.
// Aufruf: RaveShot <ausgabeordner>
#include "../Source/RaveProcessor.h"
#include "../Source/engine/KitSynth.h"
#include <juce_gui_basics/juce_gui_basics.h>

static void pump(rave::RaveProcessor& proc, int ms)
{
    juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi;
    for (int k = 0; k < ms / 15; ++k) { buf.clear(); proc.processBlock(buf, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil(15); }
}

static bool visibleChain(const juce::Component* c)   // isShowing() braucht ein Fenster; offscreen pruefen wir die Sichtbarkeitskette
{
    for (; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent()) if (! c->isVisible()) return false;
    return true;
}
static juce::TextButton* findButton(juce::Component& c, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::TextButton*>(&c)) if (b->getButtonText() == text && visibleChain(b)) return b;
    for (auto* ch : c.getChildren()) if (auto* r = findButton(*ch, text)) return r;
    return nullptr;
}
static int failures = 0;
#define CHECK(c) do { if (! (c)) { ++failures; std::printf("FEHLER: %s (Zeile %d)\n", #c, __LINE__); } } while (0)

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File out = juce::File::getCurrentWorkingDirectory();
    if (argc > 1) out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.createDirectory();

    rave::RaveProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    proc.loadDefaultKit();
    proc.setParam("seed", 7.f);
    proc.generatePattern(0, true);
    proc.setParam("play", 1.f); proc.setParam("sync", 0.f);
    proc.setBreak(rave::kit::makeDemoBreak(nullptr, 170.0), true);
    proc.detectSlices(0.6f);
    proc.distributeSlicesToPads(0);
    proc.evolveFrom(0, 0.5f);

    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    if (! ed) { std::printf("no editor\n"); return 1; }
    const char* names[] = { "PADS", "SEQUENCER", "SLICER", "GENERATOR", "EXPORT" };
    for (int page = 0; page < 5; ++page)
    {
        for (int i = 0; i < ed->getNumChildComponents(); ++i)
            if (auto* b = dynamic_cast<juce::TextButton*>(ed->getChildComponent(i)))
                if (b->getRadioGroupId() == 77 && b->getButtonText() == names[page]) b->triggerClick();
        pump(proc, 400);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        juce::FileOutputStream fo(out.getChildFile("page" + juce::String(page) + ".png"));
        fo.setPosition(0); fo.truncate();
        png.writeImageToStream(img, fo);
    }
    // ---- einfache Bedienungs-Tests ueber Buttons (kein Maus-Ersatz, aber derselbe Code-Pfad wie ein Klick) ----
    auto click = [&](const char* tab, const char* text) -> bool
    {
        if (auto* t = findButton(*ed, tab)) t->triggerClick();
        pump(proc, 100);
        auto* b = findButton(*ed, text);
        if (b == nullptr) return false;
        b->triggerClick(); pump(proc, 100); return true;
    };
    {
        proc.discardVariants(); proc.setParam("pattern", 0.f);
        CHECK(click("GENERATOR", "Dice (new seed)"));
        const float seed0 = proc.getRaw("seed");
        CHECK(click("GENERATOR", "Dice (new seed)")); CHECK(proc.getRaw("seed") > 0.f); (void) seed0;
        CHECK(click("GENERATOR", "Generate into current pattern"));
        const auto before = proc.bank.toString(0);
        CHECK(proc.canUndo());
        CHECK(click("GENERATOR", "EVOLVE: make 4 variants"));
        CHECK(proc.variantSource() == 0);
        CHECK(before == proc.bank.toString(0));                      // Original bleibt unveraendert
        CHECK(click("GENERATOR", "Discard variants"));
                CHECK(click("SEQUENCER", "Clear pattern"));
        CHECK(proc.bank.toString(0) != before);
        CHECK(click("SEQUENCER", "Undo"));
        CHECK(proc.bank.toString(0) == before);
        CHECK(click("SLICER", "Demo break"));
        CHECK(proc.sliceModel.numSlices() >= 10);
        CHECK(click("SLICER", "Slice equally"));
        CHECK(proc.sliceModel.numSlices() == 16);
        CHECK(click("SLICER", "Detect transients"));
        CHECK(click("SLICER", "Slices > pads"));
        CHECK(proc.padSample(0) != nullptr);
        CHECK(click("PADS", "Default kit"));
        pump(proc, 200);
    }
    ed.reset();
    std::printf(failures == 0 ? "ok (GUI-Checks bestanden)\n" : "GUI-Checks: %d Fehler\n", failures);
    return failures == 0 ? 0 : 1;
}
