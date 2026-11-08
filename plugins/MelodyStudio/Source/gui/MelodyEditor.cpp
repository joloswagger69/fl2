#include "MelodyEditor.h"
#include "MelodyWidgets.h"

namespace mel::gui
{
using juce::StringArray;
static StringArray items(const char* s) { return StringArray::fromTokens(s, "|", ""); }

struct Ctx
{
    MelodyProcessor& proc;
    std::function<void(const juce::String&, bool)> status;
};

// ------------------------------------------------------------------------------------------------ Notenansicht (Piano-Roll-Anzeige)
class NoteView : public juce::Component
{
public:
    NoteView(MelodyProcessor& p, int part, bool compact_ = false) : proc(p), partIdx(part), compact(compact_) { setOpaque(false); }
    void poll()
    {
        const int rev = proc.revision.load(); const double pt = proc.playTicks.load();
        if (rev != lastRev || std::abs(pt - lastPlay) > 1.0 || (pt < 0) != (lastPlay < 0)) { lastRev = rev; lastPlay = pt; repaint(); }
    }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(col::background.darker(0.3f)); g.fillRoundedRectangle(r, 4.f);
        g.setColour(col::outline); g.drawRoundedRectangle(r.reduced(0.5f), 4.f, 1.f);
        auto snap = proc.current();
        if (! snap) return;
        const auto& pr = snap->proj; const auto& notes = pr.part(partIdx);
        const int total = pr.ctx.totalTicks();
        const float chordStrip = compact ? 0.f : 16.f;
        auto area = r.reduced(2.f); area.removeFromTop(chordStrip);
        const float w = area.getWidth(), W = (float) total;
        // Takt- und Zaehlzeitlinien
        const int beat = kPpq * 4 / pr.ctx.sigDen;
        for (int t = 0; t < total; t += beat)
        {
            const bool bar = (t % pr.ctx.ticksPerBar()) == 0;
            g.setColour(bar ? col::outline.brighter(0.5f) : col::outline.withAlpha(0.55f));
            const float x = area.getX() + w * (float) t / W; g.drawVerticalLine((int) x, r.getY() + 1, r.getBottom() - 1);
        }
        // Akkordnamen
        if (! compact)
        {
            g.setFont(juce::FontOptions(12.f)); g.setColour(col::textDim);
            for (auto& c : pr.prog) g.drawText(c.chord.name, (int) (area.getX() + w * (float) c.tick / W) + 4, (int) r.getY() + 1, 120, 15, juce::Justification::left);
        }
        if (notes.empty())
        {
            g.setColour(col::textDim); g.setFont(juce::FontOptions(compact ? 12.f : 14.f));
            g.drawText("empty - press Generate", area.toNearestInt(), juce::Justification::centred);
        }
        else
        {
            int lo = 127, hi = 0; for (auto& n : notes) { lo = std::min(lo, n.pitch); hi = std::max(hi, n.pitch); }
            const int span = std::max(12, hi - lo + 3); lo -= (span - (hi - lo)) / 2;
            const float rowH = area.getHeight() / (float) span;
            const auto c = kPartCol[partIdx];
            for (auto& n : notes)
            {
                const float x = area.getX() + w * (float) n.tick / W, wd = std::max(2.f, w * (float) n.len / W - 1.f);
                const float y = area.getBottom() - rowH * (float) (n.pitch - lo + 1);
                g.setColour(c.withAlpha(0.35f + 0.65f * (float) n.vel / 127.f));
                g.fillRoundedRectangle(x, y + 0.5f, wd, std::max(2.f, rowH - 1.f), 1.5f);
                if (n.vel >= 112) { g.setColour(juce::Colours::white.withAlpha(0.8f)); g.drawRoundedRectangle(x, y + 0.5f, wd, std::max(2.f, rowH - 1.f), 1.5f, 1.f); }
            }
        }
        const double pt = proc.playTicks.load();
        if (pt >= 0.0) { g.setColour(kPlayhead); const float x = area.getX() + w * (float) (pt / W); g.drawLine(x, r.getY(), x, r.getBottom(), 1.5f); }
    }
private:
    MelodyProcessor& proc; int partIdx; bool compact; int lastRev = -1; double lastPlay = -2;
};

// ------------------------------------------------------------------------------------------------ Regler-Gruppe
class Section : public juce::Component
{
public:
    explicit Section(MelodyProcessor& p, int cols_ = 2) : proc(p), cols(cols_) {}
    ParamSlider& slider(const juce::String& name, const juce::String& id, std::function<juce::String(double)> fmt = {})
    {
        auto s = std::make_unique<ParamSlider>(name, 150); s->attach(proc.apvts, id);
        if (fmt) { s->slider.textFromValueFunction = std::move(fmt); s->slider.updateText(); }
        addAndMakeVisible(*s); auto& ref = *s; comps.push_back(std::move(s)); return ref;
    }
    void combo(const juce::String& name, const juce::String& id, const char* list)
    {
        auto c = std::make_unique<ParamCombo>(name, items(list), 150); c->attach(proc.apvts, id); addAndMakeVisible(*c); comps.push_back(std::move(c));
    }
    void toggle(const juce::String& name, const juce::String& id)
    {
        auto t = std::make_unique<ParamToggle>(name); t->attach(proc.apvts, id); addAndMakeVisible(*t); comps.push_back(std::move(t));
    }
    int rows() const { return ((int) comps.size() + cols - 1) / cols; }
    void resized() override
    {
        const int colW = getWidth() / cols; int i = 0;
        for (auto& c : comps) { const int cx = i / rows(), cy = i % rows(); c->setBounds(cx * colW + 4, cy * 31, colW - 12, 28); ++i; }
    }
private:
    MelodyProcessor& proc; int cols; std::vector<std::unique_ptr<juce::Component>> comps;
};

static juce::String seedText(double v) { return juce::String((int) v); }
static juce::String signedInt(double v) { const int i = (int) std::lround(v); return i > 0 ? "+" + juce::String(i) : juce::String(i); }

// ------------------------------------------------------------------------------------------------ Seite fuer 1-2 Teile
class PartBlock : public juce::Component
{
public:
    PartBlock(Ctx& c, int part_, const juce::String& title, bool hasVariation, bool compactView = false)
        : ctx(c), part(part_), view(c.proc, part_, compactView), mute("Mute"), compactLabel(compactView)
    {
        label.setText(title, juce::dontSendNotification); label.setFont(juce::FontOptions(16.f, juce::Font::bold)); label.setColour(juce::Label::textColourId, kPartCol[part]);
        label.setInterceptsMouseClicks(false, false); addAndMakeVisible(label);
        mk(bGen, "Generate", [this] { go(false); }); mk(bIdea, "New idea", [this] { go(true); });
        if (hasVariation) mk(bVar, "Variation", [this] { ctx.proc.variation(part, 0.35f); ctx.status(juce::String(kPartNames[part]) + ": variation created (Undo restores the previous version)", false); });
        mk(bClear, "Clear", [this] { ctx.proc.clearPart(part); ctx.status(juce::String(kPartNames[part]) + " cleared", false); });
        mute.attach(ctx.proc.apvts, "mute_" + juce::String(part)); mute.setTooltip("Silence this part in the preview synth. Does not affect MIDI export."); addAndMakeVisible(mute);
        addAndMakeVisible(view);
        bGen.setTooltip("Rebuild this part with the current settings and seed (other parts stay as they are)");
        bIdea.setTooltip("Same, with a new random seed");
    }
    void resized() override
    {
        auto r = getLocalBounds(); auto top = r.removeFromTop(30);
        label.setBounds(top.removeFromLeft(compactLabel ? 110 : 100));
        for (auto* b : { &bGen, &bIdea, &bVar, &bClear }) if (b->isVisible()) b->setBounds(top.removeFromLeft(88).reduced(2, 2));
        mute.setBounds(top.removeFromRight(64).reduced(2, 2));
        view.setBounds(r.reduced(0, 2));
    }
    void poll() { view.poll(); }
    int part;
private:
    void mk(juce::TextButton& b, const char* t, std::function<void()> cb) { b.setButtonText(t); b.onClick = std::move(cb); addAndMakeVisible(b); }
    void go(bool newSeed)
    {
        const bool forced = ctx.proc.isStale() || ctx.proc.current() == nullptr;
        ctx.proc.regenerate(part, newSeed);
        ctx.status(forced ? "Key / scale / length changed: all parts were rebuilt so they fit together" : juce::String(kPartNames[part]) + (newSeed ? ": new idea" : ": generated") + "  (Undo available)", false);
    }
    Ctx& ctx; NoteView view; juce::Label label; juce::TextButton bGen, bIdea, bVar, bClear; ParamToggle mute; bool compactLabel;
};

class PartPage : public juce::Component
{
public:
    PartPage(Ctx& c) : ctx(c) {}
    void addBlock(int part, const juce::String& title, bool var, std::unique_ptr<Section> sec)
    {
        auto b = std::make_unique<PartBlock>(ctx, part, title, var); addAndMakeVisible(*b); blocks.push_back(std::move(b));
        addAndMakeVisible(*sec); secs.push_back(std::move(sec));
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 8); const int n = (int) blocks.size();
        if (n == 1)
        {
            blocks[0]->setBounds(r.removeFromTop(300)); r.removeFromTop(10);
            secs[0]->setBounds(r.removeFromTop(31 * secs[0]->rows()));
        }
        else
        {
            const int colW = r.getWidth() / n;
            for (int i = 0; i < n; ++i)
            {
                auto col_ = r.removeFromLeft(colW).reduced(i == 0 ? 0 : 6, 0).withTrimmedRight(i == 0 ? 6 : 0);
                blocks[(size_t) i]->setBounds(col_.removeFromTop(240)); col_.removeFromTop(10);
                secs[(size_t) i]->setBounds(col_.removeFromTop(31 * secs[(size_t) i]->rows()));
            }
        }
    }
    void poll() { for (auto& b : blocks) b->poll(); }
private:
    Ctx& ctx; std::vector<std::unique_ptr<PartBlock>> blocks; std::vector<std::unique_ptr<Section>> secs;
};

// ------------------------------------------------------------------------------------------------ Make a Track
class TrackPage : public juce::Component
{
public:
    explicit TrackPage(Ctx& c) : ctx(c), genre("Genre", items(kGenreNames), 60), acid("Include Acid//Flow"), tempo("Set genre tempo")
    {
        genre.attach(c.proc.apvts, "genre"); acid.attach(c.proc.apvts, "t_acid"); tempo.attach(c.proc.apvts, "t_tempo");
        addAndMakeVisible(genre); addAndMakeVisible(acid); addAndMakeVisible(tempo);
        bMake.setButtonText("Make a track"); bMake.onClick = [this] { ctx.proc.makeTrack(false); ctx.status("Track generated from the genre setup (Undo available)", false); };
        bIdea.setButtonText("New idea"); bIdea.onClick = [this] { ctx.proc.makeTrack(true); ctx.status("New track idea generated (Undo available)", false); };
        bMake.setTooltip("Writes the genre preset (tempo, swing, all part settings) into the controls and rebuilds all parts with the track seed");
        addAndMakeVisible(bMake); addAndMakeVisible(bIdea);
        info.setFont(juce::FontOptions(12.f)); info.setColour(juce::Label::textColourId, col::textDim); info.setInterceptsMouseClicks(false, false);
        info.setText("Pick genre and mood (mood is in the header), then press Make a track. The genre writes tempo, swing and the settings of every part, "
                     "so you can fine-tune each part on its own tab afterwards. Key, scale, length and time signature come from the header and are not changed.", juce::dontSendNotification);
        addAndMakeVisible(info);
        for (int p = 0; p < 5; ++p)
        {
            rows.push_back(std::make_unique<PartBlock>(c, p, kPartNames[p], p != 0, true)); addAndMakeVisible(*rows.back());
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 8);
        auto row = r.removeFromTop(32);
        genre.setBounds(row.removeFromLeft(300)); row.removeFromLeft(10); acid.setBounds(row.removeFromLeft(170).reduced(0, 2)); tempo.setBounds(row.removeFromLeft(140).reduced(2, 2));
        row.removeFromLeft(10); bMake.setBounds(row.removeFromLeft(130).reduced(2, 1)); bIdea.setBounds(row.removeFromLeft(110).reduced(2, 1));
        info.setBounds(r.removeFromTop(48)); r.removeFromTop(4);
        const int h = r.getHeight() / 5;
        for (auto& b : rows) b->setBounds(r.removeFromTop(h).reduced(0, 2));
    }
    void poll() { for (auto& b : rows) b->poll(); }
private:
    Ctx& ctx; ParamCombo genre; ParamToggle acid, tempo; juce::TextButton bMake, bIdea; juce::Label info; std::vector<std::unique_ptr<PartBlock>> rows;
};

// ------------------------------------------------------------------------------------------------ Export
class ExportPage : public juce::Component
{
public:
    explicit ExportPage(Ctx& c) : ctx(c)
    {
        for (int p = 0; p < 5; ++p)
        {
            auto b = std::make_unique<juce::TextButton>(juce::String("Export ") + kPartNames[p] + " (.mid)"); b->onClick = [this, p] { doPart(p); };
            addAndMakeVisible(*b); partBtns.push_back(std::move(b));
        }
        mk(bSep, "Export all parts as separate .mid files", [this] { doSeparate(); });
        mk(bAll, "Export all parts as ONE .mid (one track per part)", [this] { doCombined(); });
        mk(bOpen, "Open exports folder", [this] { ctx.proc.exportDirectory().revealToUser(); });
        text.setFont(juce::FontOptions(12.f)); text.setColour(juce::Label::textColourId, col::textDim); text.setInterceptsMouseClicks(false, false);
        text.setText("MIDI files: Type 1, 480 PPQ, tempo and time signature included, length = loop length. One file per part, or one file with a track per part.\n"
                     "In FL Studio: drag the .mid file from the folder into the Piano roll or Playlist. Notes are exported exactly as shown (swing is already part of the notes).\n"
                     "Acid slides are exported as overlapping notes (legato), not as FL slide flags. Velocity >= 112 marks accents. Real-time MIDI output to other plugins is not available in this version.\n"
                     "Muting a part in the preview does not change what is exported. Empty parts are skipped.", juce::dontSendNotification);
        addAndMakeVisible(text);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 10);
        for (auto& b : partBtns) b->setBounds(r.removeFromTop(34).removeFromLeft(360).reduced(0, 2));
        r.removeFromTop(10);
        for (auto* b : { &bSep, &bAll, &bOpen }) b->setBounds(r.removeFromTop(34).removeFromLeft(360).reduced(0, 2));
        r.removeFromTop(12); text.setBounds(r.removeFromTop(130));
    }
    bool testSeparate(const juce::File& d) { return ctx.proc.exportSeparate(d, "test") > 0; }
private:
    void mk(juce::TextButton& b, const char* t, std::function<void()> cb) { b.setButtonText(t); b.onClick = std::move(cb); addAndMakeVisible(b); }
    juce::String baseName() const
    {
        auto snap = ctx.proc.current(); if (! snap) return "MELODY-STUDIO";
        return "MELODY-STUDIO_" + items(kKeyNames)[snap->proj.ctx.key] + "_" + items(kScaleNames)[snap->proj.ctx.scale].replace(" ", "") + "_" + juce::String(juce::roundToInt(ctx.proc.getRaw("bpm"))) + "bpm";
    }
    void ask(const juce::String& title, const juce::String& name, std::function<void(juce::File)> cb)
    {
        chooser = std::make_unique<juce::FileChooser>(title, ctx.proc.exportDirectory().getChildFile(name), "*.mid");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [cb](const juce::FileChooser& fc) { auto f = fc.getResult(); if (f != juce::File()) cb(f); });
    }
    void doPart(int p)
    {
        if (ctx.proc.current() == nullptr || ctx.proc.current()->proj.part(p).empty()) { ctx.status(juce::String(kPartNames[p]) + " is empty - nothing to export", true); return; }
        ask(juce::String("Export ") + kPartNames[p], baseName() + "_" + kPartNames[p] + ".mid", [this, p](juce::File f)
        {
            f = f.withFileExtension("mid"); juce::String err; const bool ok = ctx.proc.exportPart(p, f, &err);
            ctx.status(ok ? "MIDI written: " + f.getFullPathName() : "MIDI export failed: " + err, ! ok);
        });
    }
    void doSeparate()
    {
        if (ctx.proc.current() == nullptr) return;
        auto d = ctx.proc.exportDirectory().getChildFile(baseName() + "_" + juce::Time::getCurrentTime().formatted("%H%M%S")); d.createDirectory();
        juce::String err; const int n = ctx.proc.exportSeparate(d, baseName(), &err);
        ctx.status(n > 0 ? juce::String(n) + " MIDI files written to " + d.getFullPathName() : (err.isNotEmpty() ? "MIDI export failed: " + err : "Nothing to export"), n <= 0);
        if (n > 0) d.revealToUser();
    }
    void doCombined()
    {
        ask("Export combined MIDI", baseName() + ".mid", [this](juce::File f)
        {
            f = f.withFileExtension("mid"); juce::String err; const bool ok = ctx.proc.exportCombined(f, &err);
            ctx.status(ok ? "MIDI written: " + f.getFullPathName() : "MIDI export failed: " + err, ! ok);
        });
    }
    Ctx& ctx; std::vector<std::unique_ptr<juce::TextButton>> partBtns; juce::TextButton bSep, bAll, bOpen; juce::Label text; std::unique_ptr<juce::FileChooser> chooser;
};

// ------------------------------------------------------------------------------------------------ Editor
class MelodyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MelodyEditor(MelodyProcessor& p)
        : juce::AudioProcessorEditor(p), proc(p), ctx { p, [this](const juce::String& m, bool w) { setStatus(m, w); } },
          sync("Host sync"), play("Play"), bpm("BPM", 40), vol("Volume", 56), swing("Swing", 50),
          key("Key", items(kKeyNames), 36), scale("Scale", items(kScaleNames), 42), bars("Length", items("1 bar|2 bars|4 bars|8 bars|16 bars"), 50),
          sig("Time", items("4/4|3/4|5/4|6/8"), 36), mood("Mood", items(kMoodNames), 40),
          chordsPage(ctx), melodyPage(ctx), bassPage(ctx), acidPage(ctx), trackPage(ctx), exportPage(ctx)
    {
        setLookAndFeel(&laf); setSize(1180, 800); setResizable(false, false);
        title.setText("MELODY//STUDIO", juce::dontSendNotification); title.setFont(juce::FontOptions(22.f, juce::Font::bold)); title.setColour(juce::Label::textColourId, col::accent); addAndMakeVisible(title);
        brand.setText("NEO//WAVE AUDIO", juce::dontSendNotification); styleLabel(brand, 11.f); addAndMakeVisible(brand);
        addAndMakeVisible(presetBox); presetBox.setTextWhenNothingSelected("Presets..."); presetBox.onChange = [this] { loadPreset(); };
        bSave.setButtonText("Save"); bSave.onClick = [this] { ask("Save preset as", proc.presets.currentName.isNotEmpty() ? proc.presets.currentName : "My Idea", [this](juce::String n) { savePreset(n); }); }; addAndMakeVisible(bSave);
        bDel.setButtonText("Delete"); bDel.onClick = [this] { deletePreset(); }; addAndMakeVisible(bDel);
        bUndo.setButtonText("Undo"); bUndo.onClick = [this] { const bool ok = proc.undo(); setStatus(ok ? "Undone" : "Nothing to undo", ! ok); }; bUndo.setTooltip("Undo the last generate / variation / clear / track"); addAndMakeVisible(bUndo);
        styleLabel(statusLabel, 13.f, col::text); addAndMakeVisible(statusLabel);
        styleLabel(staleLabel, 12.f, col::warn); addAndMakeVisible(staleLabel);

        sync.attach(proc.apvts, "sync"); play.attach(proc.apvts, "play"); bpm.attach(proc.apvts, "bpm"); vol.attach(proc.apvts, "vol"); swing.attach(proc.apvts, "swing");
        key.attach(proc.apvts, "key"); scale.attach(proc.apvts, "scale"); bars.attach(proc.apvts, "bars"); sig.attach(proc.apvts, "sig"); mood.attach(proc.apvts, "mood");
        for (auto* c : std::initializer_list<juce::Component*> { &sync, &play, &bpm, &vol, &swing, &key, &scale, &bars, &sig, &mood }) addAndMakeVisible(*c);
        sync.setTooltip("On: preview follows the host (FL) transport and tempo. Off: internal BPM and the Play button.");
        swing.slider.setTooltip("Applied when a part is generated (baked into the notes), not live");

        // Seiten
        {
            auto s = std::make_unique<Section>(proc);
            s->slider("Complexity", "c_cplx"); s->slider("Max extension", "c_ext", [](double v) { return items(kExtNames)[juce::jlimit(0, 4, (int) v)]; });
            s->combo("Voicing", "c_voicing", kVoicingNames); s->slider("Inversion", "c_inv", seedText); s->combo("Chords per bar", "c_perbar", kPerBarNames); s->combo("Rhythm", "c_rhythm", kChordRhythmNames);
            s->toggle("Voice leading", "c_vl"); s->toggle("Allow sus chords", "c_sus"); s->toggle("Allow dim / aug", "c_dimaug"); s->slider("Seed", "c_seed", seedText);
            chordsPage.addBlock(0, "CHORDS", false, std::move(s));
        }
        {
            auto s = std::make_unique<Section>(proc, 1);
            s->combo("Character", "m_char", kMelCharNames); s->slider("Density", "m_dens"); s->slider("Rhythm complexity", "m_rcplx"); s->slider("Repetition", "m_rep"); s->slider("Variation", "m_var");
            s->slider("Max leap (semitones)", "m_leap", seedText); s->slider("Octave", "m_oct", signedInt); s->toggle("Follow chords", "m_follow"); s->slider("Seed", "m_seed", seedText);
            melodyPage.addBlock(1, "MELODY", true, std::move(s));
            auto a = std::make_unique<Section>(proc, 1);
            a->combo("Pattern", "a_pat", kArpPatternNames); a->combo("Rate", "a_rate", kArpRateNames); a->slider("Gate", "a_gate"); a->slider("Octaves", "a_oct", seedText); a->slider("Octave shift", "a_shift", signedInt); a->slider("Seed", "a_seed", seedText);
            melodyPage.addBlock(3, "ARP", true, std::move(a));
        }
        {
            auto s = std::make_unique<Section>(proc);
            s->combo("Style", "b_style", kBassStyleNames); s->slider("Density", "b_dens"); s->slider("Octave", "b_oct", seedText); s->toggle("Approach notes", "b_appr"); s->slider("Seed", "b_seed", seedText);
            bassPage.addBlock(2, "BASS", true, std::move(s));
        }
        {
            auto s = std::make_unique<Section>(proc);
            s->combo("Character", "x_char", kAcidCharNames); s->slider("Density", "x_dens"); s->slider("Slide", "x_slide"); s->slider("Accent", "x_acc"); s->slider("Tie", "x_tie");
            s->slider("Octave jumps", "x_oct"); s->slider("Note probability", "x_prob"); s->slider("Gate length", "x_gate"); s->slider("Pattern variation", "x_var");
            s->slider("Pitch variation", "x_pmod"); s->combo("Steps", "x_steps", "16|32"); s->slider("Base octave", "x_octb", seedText); s->slider("Seed", "x_seed", seedText);
            acidPage.addBlock(4, "ACID//FLOW", true, std::move(s));
        }

        const char* names[] = { "CHORDS", "MELODY", "BASS", "ACID", "MAKE A TRACK", "EXPORT" };
        for (int i = 0; i < 6; ++i)
        {
            auto& b = tabBtn[(size_t) i]; b.setButtonText(names[i]); b.setClickingTogglesState(true); b.setRadioGroupId(77);
            b.setColour(juce::TextButton::buttonOnColourId, col::accent.darker(0.3f)); b.onClick = [this, i] { showTab(i); }; addAndMakeVisible(b);
        }
        pageComps = { &chordsPage, &melodyPage, &bassPage, &acidPage, &trackPage, &exportPage };
        for (auto* c : pageComps) addChildComponent(*c);
        resized(); refreshPresets(); showTab(0);
        setStatus("Ready - a first idea is already generated. Press Play (or start FL) to hear it.", false);
        startTimerHz(20);
    }
    ~MelodyEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(col::background); g.setColour(col::panel); g.fillRect(0, 0, getWidth(), 118);
        g.setColour(col::outline); g.drawHorizontalLine(118, 0.f, (float) getWidth());
    }
    void resized() override
    {
        auto r = getLocalBounds(); auto top = r.removeFromTop(118).reduced(14, 8);
        auto l1 = top.removeFromTop(34);
        title.setBounds(l1.removeFromLeft(200)); brand.setBounds(l1.removeFromLeft(110)); l1.removeFromLeft(10);
        presetBox.setBounds(l1.removeFromLeft(240).reduced(0, 3)); bSave.setBounds(l1.removeFromLeft(60).reduced(3, 3)); bDel.setBounds(l1.removeFromLeft(64).reduced(3, 3)); bUndo.setBounds(l1.removeFromLeft(64).reduced(3, 3));
        statusLabel.setBounds(l1.reduced(12, 0));
        top.removeFromTop(6);
        auto l2 = top.removeFromTop(28);
        sync.setBounds(l2.removeFromLeft(96).reduced(2, 0)); play.setBounds(l2.removeFromLeft(70).reduced(2, 0)); l2.removeFromLeft(8);
        bpm.setBounds(l2.removeFromLeft(300)); l2.removeFromLeft(8); vol.setBounds(l2.removeFromLeft(280)); l2.removeFromLeft(8); swing.setBounds(l2.removeFromLeft(240));
        top.removeFromTop(4);
        auto l3 = top.removeFromTop(28);
        key.setBounds(l3.removeFromLeft(110)); l3.removeFromLeft(6); scale.setBounds(l3.removeFromLeft(210)); l3.removeFromLeft(6); bars.setBounds(l3.removeFromLeft(170)); l3.removeFromLeft(6);
        sig.setBounds(l3.removeFromLeft(110)); l3.removeFromLeft(6); mood.setBounds(l3.removeFromLeft(190)); l3.removeFromLeft(10); staleLabel.setBounds(l3);
        auto tabs = r.removeFromTop(34).reduced(14, 3);
        for (auto& b : tabBtn) b.setBounds(tabs.removeFromLeft(140).reduced(2, 0));
        r.removeFromTop(2);
        for (auto* c : pageComps) if (c != nullptr) c->setBounds(r);
    }
    void showTabForTest(int i) { showTab(i); }

private:
    void timerCallback() override
    {
        switch (tab) { case 0: chordsPage.poll(); break; case 1: melodyPage.poll(); break; case 2: bassPage.poll(); break; case 3: acidPage.poll(); break; case 4: trackPage.poll(); break; default: break; }
        const bool stale = proc.isStale();
        staleLabel.setText(stale ? "Key / scale / length changed - press Generate to rebuild all parts" : juce::String(), juce::dontSendNotification);
        if (statusAge > 0 && --statusAge == 0) statusLabel.setText({}, juce::dontSendNotification);
    }
    void showTab(int i)
    {
        tab = i;
        for (size_t k = 0; k < pageComps.size(); ++k) pageComps[k]->setVisible((int) k == i);
        for (size_t k = 0; k < tabBtn.size(); ++k) tabBtn[k].setToggleState((int) k == i, juce::dontSendNotification);
    }
    void setStatus(const juce::String& m, bool warn)
    {
        statusLabel.setColour(juce::Label::textColourId, warn ? col::warn : col::text); statusLabel.setText(m, juce::dontSendNotification); statusAge = 20 * (warn ? 12 : 7);
    }
    void ask(const juce::String& heading, const juce::String& initial, std::function<void(juce::String)> cb)
    {
        auto* w = new juce::AlertWindow(heading, "Name:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", initial);
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([w, cb = std::move(cb)](int res) { if (res == 1) cb(w->getTextEditorContents("name")); }), true);
    }
    void refreshPresets()
    {
        presetBox.clear(juce::dontSendNotification); const auto names = proc.presets.list();
        for (int i = 0; i < names.size(); ++i) presetBox.addItem(names[i], i + 1);
        presetBox.setText(proc.presets.currentName, juce::dontSendNotification);
    }
    void savePreset(const juce::String& raw)
    {
        const auto name = nwa::PresetManager::sanitizeName(raw);
        if (name.isEmpty()) { setStatus("Invalid preset name", true); return; }
        const auto st = proc.presets.save(name, proc.makeExtra());
        setStatus(st == nwa::PresetManager::Status::ok ? "Preset saved: " + name + "  (includes the generated notes)" : "Could not save preset", st != nwa::PresetManager::Status::ok);
        if (st == nwa::PresetManager::Status::ok) proc.presets.currentName = name;
        refreshPresets();
    }
    void loadPreset()
    {
        const auto name = presetBox.getText(); if (name.isEmpty()) return;
        juce::ValueTree extra; const auto st = proc.presets.load(name, &extra);
        if (st == nwa::PresetManager::Status::ok) { proc.applyExtra(extra); setStatus("Preset loaded: " + name, false); }
        else setStatus("Could not load preset '" + name + "'", true);
    }
    void deletePreset()
    {
        const auto name = presetBox.getText(); if (name.isEmpty()) return;
        auto* w = new juce::AlertWindow("Delete preset", "Delete '" + name + "' permanently?", juce::MessageBoxIconType::QuestionIcon, this);
        w->addButton("Delete", 1); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, name](int res)
        { if (res == 1) { proc.presets.remove(name); if (proc.presets.currentName == name) proc.presets.currentName = {}; refreshPresets(); setStatus("Preset deleted", false); } }), true);
    }

    MelodyProcessor& proc; Ctx ctx; nwa::LookAndFeel laf;
    juce::Label title, brand, statusLabel, staleLabel; juce::ComboBox presetBox; juce::TextButton bSave, bDel, bUndo;
    ParamToggle sync, play; ParamSlider bpm, vol, swing; ParamCombo key, scale, bars, sig, mood;
    std::array<juce::TextButton, 6> tabBtn;
    PartPage chordsPage, melodyPage, bassPage, acidPage; TrackPage trackPage; ExportPage exportPage;
    std::array<juce::Component*, 6> pageComps {};
    int tab = 0, statusAge = 0;
};

juce::AudioProcessorEditor* createMelodyEditor(MelodyProcessor& p) { return new MelodyEditor(p); }
} // namespace mel::gui
