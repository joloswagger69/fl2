#include "RaveEditor.h"
#include "PadsPage.h"
#include "SequencerPage.h"
#include "SlicerPage.h"
#include "GeneratorPage.h"
#include "ExportPage.h"

namespace rave::gui
{
class RaveEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RaveEditor(RaveProcessor& p)
        : juce::AudioProcessorEditor(p), proc(p),
          ctx { p, [this](const juce::String& m, bool w) { setStatus(m, w); },
                [this](const juce::String& t, const juce::String& i, std::function<void(juce::String)> cb) { askText(t, i, std::move(cb)); }, {}, 0, 0 },
          sync("Host sync"), play("Play"), bpm("BPM", 40), swing("Swing", 50), humT("Hum. time", 74), humV("Hum. vel.", 74), master("Master", 52),
          sig("Time sig.", juce::StringArray::fromTokens(kTimeSigNames, "|", ""), 66), div("Grid", juce::StringArray::fromTokens(kStepDivNames, "|", ""), 34),
          pads(ctx), seq(ctx), slicer(ctx), gen(ctx), exp(ctx)
    {
        setLookAndFeel(&laf);
        setSize(1180, 800);
        setResizable(false, false);

        title.setText("RAVE//STATION", juce::dontSendNotification); title.setFont(juce::FontOptions(22.f, juce::Font::bold)); title.setColour(juce::Label::textColourId, col::accent); addAndMakeVisible(title);
        brand.setText("NEO//WAVE AUDIO", juce::dontSendNotification); styleLabel(brand, 11.f); addAndMakeVisible(brand);
        addAndMakeVisible(presetBox); presetBox.setTextWhenNothingSelected("Presets..."); presetBox.onChange = [this] { loadPreset(); };
        bSave.setButtonText("Save"); bSave.onClick = [this] { ask("Save preset as", proc.presets.currentName.isNotEmpty() ? proc.presets.currentName : "My Preset", [this](juce::String n) { savePreset(n); }); }; addAndMakeVisible(bSave);
        bDel.setButtonText("Delete"); bDel.onClick = [this] { deletePreset(); }; addAndMakeVisible(bDel);
        styleLabel(statusLabel, 13.f, col::text); addAndMakeVisible(statusLabel);

        sync.attach(proc.apvts, "sync"); play.attach(proc.apvts, "play");
        bpm.attach(proc.apvts, "bpm"); swing.attach(proc.apvts, "swing"); humT.attach(proc.apvts, "humt"); humV.attach(proc.apvts, "humv"); master.attach(proc.apvts, "master");
        sig.attach(proc.apvts, "sig"); div.attach(proc.apvts, "div");
        for (auto* c : std::initializer_list<juce::Component*> { &sync, &play, &bpm, &swing, &humT, &humV, &master, &sig, &div }) addAndMakeVisible(*c);
        sync.setTooltip("On: follow the host (FL) tempo and transport. Off: use the internal BPM and the Play button.");

        const char* names[] = { "PADS", "SEQUENCER", "SLICER", "GENERATOR", "EXPORT" };
        for (int i = 0; i < 5; ++i)
        {
            auto& b = tabBtn[(size_t) i]; b.setButtonText(names[i]); b.setClickingTogglesState(true); b.setRadioGroupId(77);
            b.setColour(juce::TextButton::buttonOnColourId, col::accent.darker(0.3f)); b.onClick = [this, i] { showTab(i); }; addAndMakeVisible(b);
        }
        pageComps = { &pads, &seq, &slicer, &gen, &exp };
        for (auto* c : pageComps) addChildComponent(*c);
        seq.goGenerator = [this] { showTab(3); };
        ctx.selectPad = [this](int pad) { pads.select(pad); };
        resized();

        refreshPresets();
        showTab(0);
        setStatus("Ready.", false);
        startTimerHz(20);
    }
    ~RaveEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(col::background);
        g.setColour(col::panel); g.fillRect(0, 0, getWidth(), 118);
        g.setColour(col::outline); g.drawHorizontalLine(118, 0.f, (float) getWidth());
    }
    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(118).reduced(14, 8);
        auto l1 = top.removeFromTop(34);
        title.setBounds(l1.removeFromLeft(190)); brand.setBounds(l1.removeFromLeft(110));
        l1.removeFromLeft(10); presetBox.setBounds(l1.removeFromLeft(260).reduced(0, 3)); bSave.setBounds(l1.removeFromLeft(64).reduced(3, 3)); bDel.setBounds(l1.removeFromLeft(64).reduced(3, 3));
        statusLabel.setBounds(l1.reduced(12, 0));
        top.removeFromTop(6);
        auto l2 = top.removeFromTop(28);
        sync.setBounds(l2.removeFromLeft(96).reduced(2, 0)); play.setBounds(l2.removeFromLeft(70).reduced(2, 0)); l2.removeFromLeft(8);
        bpm.setBounds(l2.removeFromLeft(280)); l2.removeFromLeft(8); swing.setBounds(l2.removeFromLeft(220)); l2.removeFromLeft(8); master.setBounds(l2.removeFromLeft(240));
        top.removeFromTop(4);
        auto l3 = top.removeFromTop(28);
        sig.setBounds(l3.removeFromLeft(190)); l3.removeFromLeft(8); div.setBounds(l3.removeFromLeft(140)); l3.removeFromLeft(8);
        humT.setBounds(l3.removeFromLeft(240)); l3.removeFromLeft(8); humV.setBounds(l3.removeFromLeft(240));

        auto tabs = r.removeFromTop(34).reduced(14, 3);
        for (auto& b : tabBtn) b.setBounds(tabs.removeFromLeft(130).reduced(2, 0));
        r.removeFromTop(2);
        for (auto* c : pageComps) if (c != nullptr) c->setBounds(r);
    }

    // Test-Zugriff
    PadsPage& padsPage() { return pads; }
    SequencerPage& seqPage() { return seq; }
    SlicerPage& slicerPage() { return slicer; }
    void showTabForTest(int i) { showTab(i); }

private:
    void timerCallback() override
    {
        if (auto* p = pageComps[(size_t) tab]) static_cast<PageBase*>(p)->refresh();
        if (statusAge > 0 && --statusAge == 0) { statusLabel.setText({}, juce::dontSendNotification); }
    }
    void showTab(int i)
    {
        tab = i;
        for (size_t k = 0; k < pageComps.size(); ++k) pageComps[k]->setVisible((int) k == i);
        for (size_t k = 0; k < tabBtn.size(); ++k) tabBtn[k].setToggleState((int) k == i, juce::dontSendNotification);
        static_cast<PageBase*>(pageComps[(size_t) i])->onShown();
    }
    void setStatus(const juce::String& m, bool warn)
    {
        statusLabel.setColour(juce::Label::textColourId, warn ? col::warn : col::text);
        statusLabel.setText(m, juce::dontSendNotification); statusAge = 20 * (warn ? 12 : 7);
    }
    void ask(const juce::String& heading, const juce::String& initial, std::function<void(juce::String)> cb) { askText(heading, initial, std::move(cb)); }
    void askText(const juce::String& heading, const juce::String& initial, std::function<void(juce::String)> cb)
    {
        auto* w = new juce::AlertWindow(heading, "Name:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", initial);
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([w, cb = std::move(cb)](int res) { if (res == 1) cb(w->getTextEditorContents("name")); }), true);
    }
    void refreshPresets()
    {
        presetBox.clear(juce::dontSendNotification);
        const auto names = proc.presets.list();
        for (int i = 0; i < names.size(); ++i) presetBox.addItem(names[i], i + 1);
        presetBox.setText(proc.presets.currentName, juce::dontSendNotification);
    }
    void savePreset(const juce::String& raw)
    {
        const auto name = nwa::PresetManager::sanitizeName(raw);
        if (name.isEmpty()) { setStatus("Invalid preset name", true); return; }
        const auto st = proc.presets.save(name, proc.makeExtra());
        setStatus(st == nwa::PresetManager::Status::ok ? "Preset saved: " + name + "  (includes patterns, samples and slicer)" : "Could not save preset", st != nwa::PresetManager::Status::ok);
        if (st == nwa::PresetManager::Status::ok) proc.presets.currentName = name;
        refreshPresets();
    }
    void loadPreset()
    {
        const auto name = presetBox.getText();
        if (name.isEmpty()) return;
        juce::ValueTree extra;
        const auto st = proc.presets.load(name, &extra);
        if (st == nwa::PresetManager::Status::ok)
        {
            proc.applyExtra(extra); ++proc.editRevision;
            setStatus("Preset loaded: " + name, false);
        }
        else setStatus("Could not load preset '" + name + "'", true);
    }
    void deletePreset()
    {
        const auto name = presetBox.getText();
        if (name.isEmpty()) return;
        auto* w = new juce::AlertWindow("Delete preset", "Delete '" + name + "' permanently?", juce::MessageBoxIconType::QuestionIcon, this);
        w->addButton("Delete", 1); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, name, w](int res)
        { juce::ignoreUnused(w); if (res == 1) { proc.presets.remove(name); if (proc.presets.currentName == name) proc.presets.currentName = {}; refreshPresets(); setStatus("Preset deleted", false); } }), true);
    }

    RaveProcessor& proc;
    Ctx ctx;
    nwa::LookAndFeel laf;
    juce::Label title, brand, statusLabel; juce::ComboBox presetBox; juce::TextButton bSave, bDel;
    ParamToggle sync, play; ParamSlider bpm, swing, humT, humV, master; ParamCombo sig, div;
    std::array<juce::TextButton, 5> tabBtn;
    PadsPage pads; SequencerPage seq; SlicerPage slicer; GeneratorPage gen; ExportPage exp;
    std::array<juce::Component*, 5> pageComps {};
    int tab = 0, statusAge = 0;
};

juce::AudioProcessorEditor* createRaveEditor(RaveProcessor& p) { return new RaveEditor(p); }
} // namespace rave::gui
