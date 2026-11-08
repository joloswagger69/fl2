#pragma once
#include "PageBase.h"

namespace rave::gui
{
class PadsPage : public PageBase, public juce::FileDragAndDropTarget
{
public:
    explicit PadsPage(Ctx& c) : PageBase(c),
        sVol("Volume"), sPan("Pan"), sPitch("Pitch"), sAtk("Attack"), sDec("Decay"), sStart("Start"), sEnd("End"),
        sLp("LP cutoff"), sHp("HP cutoff"), sRes("Resonance"), sDrv("Drive"), sCrush("Bitcrush"), sStretch("Time stretch"), sLayer("Layer level"), sGlitch("Glitch"),
        cStutter("Stutter", juce::StringArray::fromTokens(kStutterNames, "|", "")), cRole("Role (generator)", juce::StringArray::fromTokens(kRoleNames, "|", "")),
        tRev("Reverse"), tMute("Mute"), tSolo("Solo"), tKeep("Pitch keeps length")
    {
        for (int i = 0; i < kPads; ++i) { tiles[(size_t) i] = std::make_unique<Tile>(*this, i); addAndMakeVisible(*tiles[(size_t) i]); }
        for (auto* s : { &sVol, &sPan, &sPitch, &sAtk, &sDec, &sStart, &sEnd, &sLp, &sHp, &sRes, &sDrv, &sCrush, &sStretch, &sLayer, &sGlitch }) addAndMakeVisible(*s);
        for (auto* cb : { &cStutter, &cRole }) addAndMakeVisible(*cb);
        for (auto* t : { &tRev, &tMute, &tSolo, &tKeep }) addAndMakeVisible(*t);
        sVol.setSuffix(" dB"); sPitch.setSuffix(" st"); sAtk.setSuffix(" s"); sDec.setSuffix(" s"); sLp.setSuffix(" Hz"); sHp.setSuffix(" Hz"); sStretch.setSuffix(" x");
        sPan.slider.setNumDecimalPlacesToDisplay(2); sStart.slider.setNumDecimalPlacesToDisplay(3); sEnd.slider.setNumDecimalPlacesToDisplay(3);
        sLp.slider.setNumDecimalPlacesToDisplay(0); sHp.slider.setNumDecimalPlacesToDisplay(0);

        addAndMakeVisible(wave); wave.setMode(WaveView::Mode::region); wave.setEmptyText("empty pad - load a sample or drop an audio file here");
        wave.onRegionDragStart = [this] { gesture(true); };
        wave.onRegionChanged = [this](float s, float e) { setRaw(padParamId(sel, pStart), s); setRaw(padParamId(sel, pEnd), e); };
        wave.onRegionDragEnd = [this] { gesture(false); };

        title.setFont(juce::FontOptions(17.f, juce::Font::bold)); title.setColour(juce::Label::textColourId, col::text); addAndMakeVisible(title);
        styleLabel(info); addAndMakeVisible(info);
        styleLabel(kitLabel, 13.f); kitLabel.setText("Kit", juce::dontSendNotification); addAndMakeVisible(kitLabel);

        makeButton(bLoad, "Load sample...", [this] { chooseSample(false); });
        makeButton(bClear, "Clear", [this] { proc.clearPad(sel); ctx.status("Pad " + juce::String(sel + 1) + " cleared", false); });
        makeButton(bPlay, "Play (with settings)", [this] { proc.engine.auditionPad(sel, 1.f); });
        makeButton(bRaw, "Play (raw)", [this] { proc.previewSample(proc.padSample(sel)); });
        makeButton(bBounce, "Resample (bake)", [this] { bake(); });
        makeButton(bExport, "Export WAV...", [this] { exportPad(); });
        makeButton(bLayer, "Load layer...", [this] { chooseSample(true); });
        makeButton(bLayerClear, "Clear layer", [this] { proc.setPadLayer(sel, nullptr); });
        makeButton(bKitLoad, "Load", [this] { loadKit(); });
        makeButton(bKitSave, "Save kit...", [this] { saveKit(); });
        makeButton(bKitDefault, "Default kit", [this] { proc.loadDefaultKit(); ctx.status("Default synthesized kit loaded (pad settings unchanged)", false); });
        addAndMakeVisible(kitBox);
        bBounce.setTooltip("Renders the pad with all its settings into a new sample and resets the settings");
        select(0);
        refreshKits();
    }

    // Auswahl (von aussen und per Klick)
    void select(int pad)
    {
        sel = juce::jlimit(0, kPads - 1, pad); ctx.selectedPad = sel;
        auto id = [&](int p) { return padParamId(sel, p); };
        auto& a = proc.apvts;
        sVol.attach(a, id(pVol)); sPan.attach(a, id(pPan)); sPitch.attach(a, id(pPitch)); sAtk.attach(a, id(pAtk)); sDec.attach(a, id(pDec));
        sStart.attach(a, id(pStart)); sEnd.attach(a, id(pEnd)); sLp.attach(a, id(pLp)); sHp.attach(a, id(pHp)); sRes.attach(a, id(pRes));
        sDrv.attach(a, id(pDrv)); sCrush.attach(a, id(pCrush)); sStretch.attach(a, id(pStretch)); sLayer.attach(a, id(pLayer)); sGlitch.attach(a, id(pGlitch));
        cStutter.attach(a, id(pStutter)); cRole.attach(a, id(pRole));
        tRev.attach(a, id(pRev)); tMute.attach(a, id(pMute)); tSolo.attach(a, id(pSolo)); tKeep.attach(a, id(pKeep));
        lastRev = -1; refresh();
        for (auto& t : tiles) t->repaint();
    }
    int selected() const { return sel; }

    void refresh() override
    {
        const int rev = proc.editRevision.load();
        auto s = proc.padSample(sel);
        if (rev != lastRev || s != shown)
        {
            lastRev = rev; shown = s;
            wave.setSample(s);
            title.setText("PAD " + juce::String(sel + 1) + (s ? "  -  " + s->name : "  -  (empty)"), juce::dontSendNotification);
            info.setText(s ? juce::String(s->seconds(), 2) + " s   " + juce::String(s->rate / 1000.0, 1) + " kHz   " + (s->channels() > 1 ? "stereo" : "mono")
                              + (proc.padLayer(sel) ? "   + layer: " + proc.padLayer(sel)->name : juce::String())
                           : juce::String("no sample"), juce::dontSendNotification);
            for (auto& t : tiles) t->repaint();
        }
        if (! draggingRegion) wave.setRegion(proc.getRaw(padParamId(sel, pStart)), proc.getRaw(padParamId(sel, pEnd)));
        for (auto& t : tiles) t->tick();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 12);
        auto left = r.removeFromLeft(560);
        r.removeFromLeft(18);
        const int tw = 134, th = 96, gap = 8;
        for (int i = 0; i < kPads; ++i)
            tiles[(size_t) i]->setBounds(left.getX() + (i % 4) * (tw + gap), left.getY() + (i / 4) * (th + gap), tw, th);
        auto kit = left.removeFromBottom(34); kit.removeFromBottom(0);
        auto kr = kit; kitLabel.setBounds(kr.removeFromLeft(30)); bKitDefault.setBounds(kr.removeFromRight(100).reduced(0, 2)); kr.removeFromRight(6);
        bKitSave.setBounds(kr.removeFromRight(100).reduced(0, 2)); kr.removeFromRight(6); bKitLoad.setBounds(kr.removeFromRight(56).reduced(0, 2)); kr.removeFromRight(6);
        kitBox.setBounds(kr.reduced(0, 2));
        hint = juce::Rectangle<int>(left.getX(), left.getBottom() - 78, left.getWidth(), 40);

        // Rechts: Editor des gewaehlten Pads
        title.setBounds(r.removeFromTop(26)); info.setBounds(r.removeFromTop(20));
        wave.setBounds(r.removeFromTop(112)); r.removeFromTop(8);
        auto br = r.removeFromTop(28);
        for (auto* b : { &bLoad, &bClear, &bPlay, &bRaw, &bBounce, &bExport }) { b->setBounds(br.removeFromLeft(juce::jmax(60, b == &bBounce || b == &bPlay ? 118 : 92)).reduced(2, 0)); }
        r.removeFromTop(4);
        auto lr = r.removeFromTop(28); bLayer.setBounds(lr.removeFromLeft(110).reduced(2, 0)); bLayerClear.setBounds(lr.removeFromLeft(96).reduced(2, 0));
        r.removeFromTop(6);
        auto c1 = r.removeFromLeft(r.getWidth() / 2 - 6); r.removeFromLeft(12); auto c2 = r;
        const int rowH = 27;
        for (auto* s : { &sVol, &sPan, &sPitch, &sAtk, &sDec, &sStart, &sEnd }) s->setBounds(c1.removeFromTop(rowH));
        for (auto* s : { &sLp, &sHp, &sRes, &sDrv, &sCrush, &sStretch, &sLayer, &sGlitch }) s->setBounds(c2.removeFromTop(rowH));
        cStutter.setBounds(c1.removeFromTop(rowH)); cRole.setBounds(c1.removeFromTop(rowH)); c1.removeFromTop(6);
        auto tr = c1.removeFromTop(28);
        for (auto* t : { &tRev, &tMute, &tSolo }) t->setBounds(tr.removeFromLeft(70).reduced(2, 0));
        tKeep.setBounds(c1.removeFromTop(28).removeFromLeft(220).reduced(2, 0));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(col::textDim); g.setFont(juce::FontOptions(12.f));
        g.drawFittedText("Click a pad to select and audition it. Drag audio files (WAV/AIFF/FLAC/OGG) onto a pad to load them. Right-click a pad for mute / solo / clear.",
                         hint, juce::Justification::topLeft, 3);
        if (dropPad >= 0) { g.setColour(col::accent); g.drawRect(tiles[(size_t) dropPad]->getBounds().expanded(2), 2); }
    }

    // --- Drag & Drop
    bool isInterestedInFileDrag(const juce::StringArray& files) override { for (auto& f : files) if (wildcard().containsIgnoreCase(juce::File(f).getFileExtension().toLowerCase()) ) return true; return false; }
    void fileDragMove(const juce::StringArray&, int x, int y) override { const int p = padAt(x, y); if (p != dropPad) { dropPad = p; repaint(); } }
    void fileDragExit(const juce::StringArray&) override { dropPad = -1; repaint(); }
    void filesDropped(const juce::StringArray& files, int x, int y) override
    {
        const int target = padAt(x, y); dropPad = -1; repaint();
        int pad = target >= 0 ? target : sel, n = 0;
        for (auto& f : files)
        {
            if (pad >= kPads) break;
            juce::String err;
            if (auto s = loadSampleFile(juce::File(f), &err)) { if (proc.setPadSample(pad, s)) { ++n; ++pad; } }
            else { ctx.status(err.isNotEmpty() ? err : "Cannot load " + juce::File(f).getFileName(), true); }
        }
        if (n > 0) { ctx.status(juce::String(n) + " sample(s) loaded", false); select(target >= 0 ? target : sel); }
    }

    class Tile : public juce::Component
    {
    public:
        Tile(PadsPage& o, int i) : owner(o), index(i) {}
        void tick()
        {
            const int h = owner.proc.engine.padHits[(size_t) index].load(std::memory_order_relaxed);
            if (h != lastHits) { lastHits = h; flash = 1.f; repaint(); }
            else if (flash > 0.f) { flash = flash < 0.06f ? 0.f : flash * 0.78f; repaint(); }
        }
        void paint(juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced(1.f);
            const bool selected = owner.sel == index;
            const auto s = owner.proc.padSample(index);
            const bool muted = owner.proc.getRaw(padParamId(index, pMute)) > 0.5f, solo = owner.proc.getRaw(padParamId(index, pSolo)) > 0.5f;
            g.setColour(col::panel.interpolatedWith(kHit, flash * 0.7f)); g.fillRoundedRectangle(r, 6.f);
            g.setColour(selected ? col::accent : col::outline); g.drawRoundedRectangle(r, 6.f, selected ? 2.5f : 1.f);
            g.setColour(muted ? col::textDim.withAlpha(0.6f) : col::text);
            g.setFont(juce::FontOptions(22.f, juce::Font::bold)); g.drawText(juce::String(index + 1), r.reduced(8.f, 4.f).toNearestInt(), juce::Justification::topLeft);
            g.setFont(juce::FontOptions(12.5f));
            g.drawFittedText(s ? s->name : juce::String("(empty)"), r.reduced(8.f, 0.f).toNearestInt().withTrimmedTop(34).withTrimmedBottom(18), juce::Justification::centredLeft, 2);
            g.setColour(col::textDim); g.setFont(juce::FontOptions(11.f));
            const int role = (int) std::lround(owner.proc.getRaw(padParamId(index, pRole)));
            g.drawText(juce::StringArray::fromTokens(kRoleNames, "|", "")[juce::jlimit(0, 8, role)] + (s ? "  " + juce::String(s->seconds(), 2) + " s" : juce::String()),
                       r.reduced(8.f, 4.f).toNearestInt(), juce::Justification::bottomLeft);
            if (muted) { g.setColour(kMute); g.setFont(juce::FontOptions(13.f, juce::Font::bold)); g.drawText("M", r.toNearestInt().reduced(8, 6), juce::Justification::topRight); }
            if (solo) { g.setColour(kSolo); g.setFont(juce::FontOptions(13.f, juce::Font::bold)); g.drawText("S", r.toNearestInt().reduced(24, 6), juce::Justification::topRight); }
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            owner.select(index);
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m;
                const bool mu = owner.proc.getRaw(padParamId(index, pMute)) > 0.5f, so = owner.proc.getRaw(padParamId(index, pSolo)) > 0.5f;
                m.addItem(1, mu ? "Unmute" : "Mute"); m.addItem(2, so ? "Unsolo" : "Solo"); m.addSeparator();
                m.addItem(3, "Load sample..."); m.addItem(4, "Clear pad"); m.addItem(5, "Play raw sample");
                m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this), [this](int r)
                {
                    switch (r)
                    {
                        case 1: owner.proc.setParam(padParamId(index, pMute), owner.proc.getRaw(padParamId(index, pMute)) > 0.5f ? 0.f : 1.f); break;
                        case 2: owner.proc.setParam(padParamId(index, pSolo), owner.proc.getRaw(padParamId(index, pSolo)) > 0.5f ? 0.f : 1.f); break;
                        case 3: owner.chooseSample(false); break;
                        case 4: owner.proc.clearPad(index); break;
                        case 5: owner.proc.previewSample(owner.proc.padSample(index)); break;
                        default: break;
                    }
                    repaint();
                });
                return;
            }
            // Anschlagstaerke aus der vertikalen Position (oben = laut), wie bei Pads ueblich
            const float vel = juce::jlimit(0.25f, 1.f, 1.f - (float) e.y / (float) juce::jmax(1, getHeight()) * 0.75f);
            owner.proc.engine.auditionPad(index, vel);
        }
    private:
        PadsPage& owner; int index; int lastHits = 0; float flash = 0.f;
    };

    // Test-Zugriff
    Tile& tile(int i) { return *tiles[(size_t) i]; }

private:
    int padAt(int x, int y) const { for (int i = 0; i < kPads; ++i) if (tiles[(size_t) i]->getBounds().contains(x, y)) return i; return -1; }
    void setRaw(const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(v)); }
    void gesture(bool begin)
    {
        draggingRegion = begin;
        for (int p : { (int) pStart, (int) pEnd }) if (auto* prm = proc.apvts.getParameter(padParamId(sel, p))) { if (begin) prm->beginChangeGesture(); else prm->endChangeGesture(); }
    }
    void chooseSample(bool layer)
    {
        chooser = std::make_unique<juce::FileChooser>(layer ? "Load layer sample" : "Load sample", juce::File::getSpecialLocation(juce::File::userMusicDirectory), wildcard());
        const int pad = sel;
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, layer, pad](const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            juce::String err;
            auto s = loadSampleFile(f, &err);
            if (! s) { ctx.status(err.isNotEmpty() ? err : "Cannot load " + f.getFileName(), true); return; }
            const bool ok = layer ? proc.setPadLayer(pad, s) : proc.setPadSample(pad, s);
            ctx.status(ok ? "Loaded " + f.getFileName() + " to pad " + juce::String(pad + 1) : "Audio engine busy - try again", ! ok);
        });
    }
    void bake()
    {
        if (! proc.padSample(sel)) { ctx.status("Pad is empty", true); return; }
        ctx.status(proc.resamplePad(sel) ? "Pad " + juce::String(sel + 1) + " resampled - its settings are now baked into the sample and reset" : "Resample failed", false);
    }
    void exportPad()
    {
        if (! proc.padSample(sel)) { ctx.status("Pad is empty", true); return; }
        chooser = std::make_unique<juce::FileChooser>("Export pad as WAV (24 bit, with all pad settings)", exportDir().getChildFile("pad" + juce::String(sel + 1) + ".wav"), "*.wav");
        const int pad = sel;
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this, pad](const juce::FileChooser& fc)
        {
            auto f = fc.getResult(); if (f == juce::File()) return;
            juce::String err; const bool ok = proc.exportPadWav(pad, f.withFileExtension("wav"), true, &err);
            ctx.status(ok ? "Exported " + f.getFileName() : err, ! ok);
        });
    }
    static juce::File exportDir() { auto d = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("NEOWAVE_AUDIO").getChildFile("RAVE-STATION").getChildFile("Exports"); d.createDirectory(); return d; }
    void refreshKits() { kitBox.clear(); const auto kits = proc.listKits(); for (int i = 0; i < kits.size(); ++i) kitBox.addItem(kits[i], i + 1); if (kits.isEmpty()) kitBox.setTextWhenNothingSelected("(no saved kits)"); else kitBox.setSelectedId(1, juce::dontSendNotification); }
    void loadKit()
    {
        if (kitBox.getText().isEmpty()) { ctx.status("No kit selected", true); return; }
        juce::String err; const bool ok = proc.loadKit(kitBox.getText(), &err);
        ctx.status(ok ? "Kit '" + kitBox.getText() + "' loaded" : "Kit not loaded: " + err, ! ok);
    }
    void saveKit()
    {
        ctx.prompt("Save kit as", kitBox.getText().isNotEmpty() ? kitBox.getText() : "My Kit", [this](juce::String name)
        {
            juce::String err; const bool ok = proc.saveKit(name, &err);
            ctx.status(ok ? "Kit saved to " + proc.kitsDirectory().getChildFile(name).getFullPathName() : "Kit not saved: " + err, ! ok);
            refreshKits(); kitBox.setText(name, juce::dontSendNotification);
        });
    }

    std::array<std::unique_ptr<Tile>, kPads> tiles;
    ParamSlider sVol, sPan, sPitch, sAtk, sDec, sStart, sEnd, sLp, sHp, sRes, sDrv, sCrush, sStretch, sLayer, sGlitch;
    ParamCombo cStutter, cRole;
    ParamToggle tRev, tMute, tSolo, tKeep;
    WaveView wave; juce::Label title, info, kitLabel;
    juce::TextButton bLoad, bClear, bPlay, bRaw, bBounce, bExport, bLayer, bLayerClear, bKitLoad, bKitSave, bKitDefault;
    juce::ComboBox kitBox;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> hint;
    int sel = 0, lastRev = -1, dropPad = -1; bool draggingRegion = false;
    rave::SamplePtr shown;
};
} // namespace rave::gui
