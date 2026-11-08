// Rendert den Editor offscreen in PNG-Dateien und bedient ihn ueber seine Buttons (gleicher Code-Pfad wie ein Klick).
// Aufruf: MelodyShot <ausgabeordner>
#include "../Source/MelodyProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

static void pump(mel::MelodyProcessor& proc, int ms)
{
    juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi;
    for (int k = 0; k < ms / 15; ++k) { buf.clear(); proc.processBlock(buf, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil(15); }
}
static bool visibleChain(const juce::Component* c) { for (; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent()) if (! c->isVisible()) return false; return true; }
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

    mel::MelodyProcessor proc; proc.setPlayConfigDetails(0, 2, 44100.0, 512); proc.prepareToPlay(44100.0, 512);
    proc.setParam("sync", 0.f); proc.setParam("play", 1.f);
    proc.setParam("t_acid", 1.f); proc.makeTrack(false);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    if (! ed) { std::printf("no editor\n"); return 1; }
    pump(proc, 300);

    const char* names[] = { "CHORDS", "MELODY", "BASS", "ACID", "MAKE A TRACK", "EXPORT" };
    for (int page = 0; page < 6; ++page)
    {
        for (int i = 0; i < ed->getNumChildComponents(); ++i)
            if (auto* b = dynamic_cast<juce::TextButton*>(ed->getChildComponent(i))) if (b->getRadioGroupId() == 77 && b->getButtonText() == names[page]) b->triggerClick();
        pump(proc, 600);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png; juce::FileOutputStream fo(out.getChildFile("page" + juce::String(page) + ".png")); fo.setPosition(0); fo.truncate(); png.writeImageToStream(img, fo);
    }
    auto click = [&](const char* tab, const char* text) -> bool
    {
        if (auto* t = findButton(*ed, tab)) t->triggerClick();
        pump(proc, 60);
        auto* b = findButton(*ed, text); if (b == nullptr) return false;
        b->triggerClick(); pump(proc, 60); return true;
    };
    auto same = [](const mel::Part& a, const mel::Part& b) { if (a.size() != b.size()) return false; for (size_t i = 0; i < a.size(); ++i) if (a[i].tick != b[i].tick || a[i].pitch != b[i].pitch || a[i].len != b[i].len) return false; return true; };
    {
        auto s0 = proc.current();
        // Generate / New idea wirken nur auf den eigenen Teil
        CHECK(click("MELODY", "New idea"));          // erster sichtbarer "New idea" gehoert zur Melody
        auto s1 = proc.current();
        CHECK(! same(s0->proj.melody, s1->proj.melody)); CHECK(same(s0->proj.chords, s1->proj.chords)); CHECK(same(s0->proj.bass, s1->proj.bass)); CHECK(same(s0->proj.arp, s1->proj.arp));
        CHECK(proc.canUndo());
        CHECK(click("MELODY", "Variation")); CHECK(! same(s1->proj.melody, proc.current()->proj.melody));
        // Header-Undo
        CHECK(click("MELODY", "Undo")); CHECK(same(s1->proj.melody, proc.current()->proj.melody));
        CHECK(click("BASS", "Clear")); CHECK(proc.current()->proj.bass.empty());
        CHECK(click("BASS", "Generate")); CHECK(! proc.current()->proj.bass.empty());
        CHECK(click("ACID", "Clear")); CHECK(proc.current()->proj.acid.empty());
        CHECK(click("ACID", "New idea")); CHECK(! proc.current()->proj.acid.empty());
        CHECK(click("CHORDS", "New idea")); CHECK(! proc.current()->proj.chords.empty());
        // Make a track
        const auto seed = proc.getRaw("t_seed");
        CHECK(click("MAKE A TRACK", "New idea")); CHECK(proc.getRaw("t_seed") >= 0.f); (void) seed;
        CHECK(click("MAKE A TRACK", "Make a track"));
        // Export-Buttons
        CHECK(findButton(*ed, "Export all parts as ONE .mid (one track per part)") == nullptr);   // nur auf der Export-Seite sichtbar

    }
    // Mute-Schalter wirken auf den Parameter
    {
        if (auto* t = findButton(*ed, "CHORDS")) t->triggerClick();
        pump(proc, 60);
        auto* m = findButton(*ed, "Mute"); CHECK(m != nullptr);
        if (m) { m->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("mute_0") > 0.5f); m->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("mute_0") < 0.5f); }
    }
    ed.reset();
    std::printf(failures == 0 ? "ok (GUI-Checks bestanden)\n" : "GUI-Checks: %d Fehler\n", failures);
    return failures == 0 ? 0 : 1;
}
