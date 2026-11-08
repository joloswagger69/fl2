#pragma once
#include "PageBase.h"

namespace rave::gui
{
class ExportPage : public PageBase
{
public:
    explicit ExportPage(Ctx& c) : PageBase(c)
    {
        for (int n : { 1, 2, 4, 8 }) loops.addItem(juce::String(n) + (n == 1 ? " loop" : " loops"), n);
        loops.setSelectedId(1, juce::dontSendNotification);
        for (auto s : { "44100 Hz", "48000 Hz", "96000 Hz" }) rate.addItem(s, rate.getNumItems() + 1);
        rate.setSelectedId(1, juce::dontSendNotification);
        for (auto* cb : { &loops, &rate }) addAndMakeVisible(*cb);
        tRoll.setButtonText("MIDI: roll step probability once (random, seed-based)"); tRoll.setClickingTogglesState(true);
        tWrap.setButtonText("WAV: seamless loop (fold tail back to start)"); tWrap.setClickingTogglesState(true); tWrap.setToggleState(true, juce::dontSendNotification);
        for (auto* t : { &tRoll, &tWrap }) { t->setColour(juce::TextButton::buttonOnColourId, col::accent.darker(0.3f)); addAndMakeVisible(*t); }
        styleLabel(lLoops); lLoops.setText("Length", juce::dontSendNotification); styleLabel(lRate); lRate.setText("Sample rate", juce::dontSendNotification);
        addAndMakeVisible(lLoops); addAndMakeVisible(lRate);
        makeButton(bMidi, "Export pattern as MIDI (.mid)", [this] { doMidi(); });
        makeButton(bWav, "Export pattern as WAV", [this] { doWav(); });
        makeButton(bStems, "Export stems (one WAV per pad)", [this] { doStems(); });
        makeButton(bSlices, "Export slices of the loaded break", [this] { doSlices(); });
        makeButton(bPad, "Export selected pad (processed)", [this] { doPad(); });
        makeButton(bOpen, "Open exports folder", [this] { exportDir().revealToUser(); });
        styleLabel(text, 12.f); addAndMakeVisible(text);
        text.setText("Exports go where you choose (WAV/MIDI) or to Documents/NEOWAVE_AUDIO/RAVE-STATION/Exports (stems, slices).\n"
                     "MIDI: pad n = note 35+n (pad 1 = C1 = note 36), 1 file = current pattern, length from the pattern. Pitch of steps and humanize are NOT written to the MIDI file.\n"
                     "WAV: rendered offline with the same engine as playback, including swing, ratchets, rolls, probability and the pad processing.", juce::dontSendNotification);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 10);
        auto row = r.removeFromTop(28);
        lLoops.setBounds(row.removeFromLeft(60)); loops.setBounds(row.removeFromLeft(110).reduced(0, 2)); row.removeFromLeft(20);
        lRate.setBounds(row.removeFromLeft(90)); rate.setBounds(row.removeFromLeft(110).reduced(0, 2));
        r.removeFromTop(6);
        tRoll.setBounds(r.removeFromTop(28).removeFromLeft(430));
        tWrap.setBounds(r.removeFromTop(28).removeFromLeft(430));
        r.removeFromTop(12);
        for (auto* b : { &bMidi, &bWav, &bStems, &bSlices, &bPad, &bOpen }) { b->setBounds(r.removeFromTop(34).removeFromLeft(330).reduced(0, 2)); }
        r.removeFromTop(12);
        text.setBounds(r.removeFromTop(90));
    }
    // Test-Zugriff (ohne Dialog)
    bool testMidi(const juce::File& f) { return proc.exportMidi(slot(), f, loops.getSelectedId(), tRoll.getToggleState()); }
private:
    int slot() const { return juce::jlimit(0, kPatternSlots - 1, (int) std::lround(proc.getRaw("pattern"))); }
    double sampleRate() const { return std::array<double, 3> { 44100.0, 48000.0, 96000.0 }[(size_t) juce::jlimit(0, 2, rate.getSelectedId() - 1)]; }
    juce::File exportDir() const { auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEOWAVE_AUDIO").getChildFile("RAVE-STATION").getChildFile("Exports"); d.createDirectory(); return d; }
    void ask(const juce::String& title, const juce::String& name, const juce::String& pattern, std::function<void(juce::File)> cb)
    {
        chooser = std::make_unique<juce::FileChooser>(title, exportDir().getChildFile(name), pattern);
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [cb](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) cb(f); });
    }
    void doMidi()
    {
        ask("Export MIDI", "pattern" + juce::String(slot() + 1) + ".mid", "*.mid", [this](juce::File f)
        {
            f = f.withFileExtension("mid"); juce::String err;
            const bool ok = proc.exportMidi(slot(), f, loops.getSelectedId(), tRoll.getToggleState(), &err);
            ctx.status(ok ? "MIDI written: " + f.getFullPathName() + " (drag it into the FL piano roll / playlist)" : "MIDI export failed: " + err, ! ok);
        });
    }
    void doWav()
    {
        ask("Export WAV", "pattern" + juce::String(slot() + 1) + ".wav", "*.wav", [this](juce::File f)
        {
            f = f.withFileExtension("wav"); juce::String err;
            const bool ok = proc.exportPatternWav(slot(), f, loops.getSelectedId(), tWrap.getToggleState(), sampleRate(), &err);
            ctx.status(ok ? "WAV written: " + f.getFullPathName() : "WAV export failed: " + err, ! ok);
        });
    }
    void doStems()
    {
        auto d = exportDir().getChildFile("Stems_pattern" + juce::String(slot() + 1) + "_" + juce::Time::getCurrentTime().formatted("%H%M%S"));
        d.createDirectory();
        const int n = proc.exportStems(slot(), d, loops.getSelectedId(), tWrap.getToggleState(), sampleRate());
        ctx.status(n > 0 ? juce::String(n) + " stems written to " + d.getFullPathName() : "Nothing to export (pattern empty?)", n == 0);
        if (n > 0) d.revealToUser();
    }
    void doSlices()
    {
        auto d = exportDir().getChildFile("Slices_" + juce::Time::getCurrentTime().formatted("%H%M%S")); d.createDirectory();
        juce::String err; const int n = proc.exportSlices(d, "slice", &err);
        ctx.status(n > 0 ? juce::String(n) + " slices written to " + d.getFullPathName() : (err.isNotEmpty() ? err : "No slices - load a break on the SLICER page"), n <= 0);
        if (n > 0) d.revealToUser();
    }
    void doPad()
    {
        const int pad = ctx.selectedPad;
        ask("Export pad " + juce::String(pad + 1), "pad" + juce::String(pad + 1) + ".wav", "*.wav", [this, pad](juce::File f)
        {
            f = f.withFileExtension("wav"); juce::String err;
            const bool ok = proc.exportPadWav(pad, f, true, &err);
            ctx.status(ok ? "Pad written: " + f.getFullPathName() : "Pad export failed: " + err, ! ok);
        });
    }
    juce::ComboBox loops, rate; juce::TextButton tRoll, tWrap, bMidi, bWav, bStems, bSlices, bPad, bOpen; juce::Label lLoops, lRate, text;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace rave::gui
