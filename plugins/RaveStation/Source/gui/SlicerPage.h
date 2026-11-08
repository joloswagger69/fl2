#pragma once
#include "PageBase.h"
#include "../engine/KitSynth.h"

namespace rave::gui
{
class ChipStrip : public juce::Component
{
public:
    explicit ChipStrip(RaveProcessor& p) : proc(p) {}
    int selected = 0;
    std::function<void(int)> onSelect, onPreview;
    static constexpr int cw = 50, ch = 38;
    juce::Rectangle<int> chipRect(int i) const { const int per = juce::jmax(1, getWidth() / cw); return { (i % per) * cw, (i / per) * ch, cw - 4, ch - 4 }; }
    void paint(juce::Graphics& g) override
    {
        const juce::ScopedLock sl(proc.modelLock);
        const int n = (int) proc.arrangement.size();
        for (int i = 0; i < n; ++i)
        {
            const auto& o = proc.arrangement[(size_t) i];
            const auto r = chipRect(i).toFloat();
            const bool noPad = i >= kPads;
            g.setColour(i == selected ? col::accent.darker(0.35f) : col::panel.brighter(0.1f)); g.fillRoundedRectangle(r, 5.f);
            g.setColour(i == selected ? col::accent : col::outline); g.drawRoundedRectangle(r, 5.f, i == selected ? 2.f : 1.f);
            g.setColour(noPad ? col::textDim : col::text); g.setFont(juce::FontOptions(14.f, juce::Font::bold));
            g.drawText(juce::String(o.slice + 1), r.toNearestInt().withTrimmedBottom(12), juce::Justification::centred);
            juce::String tag;
            if (o.reverse) tag << "R";
            if (std::abs(o.semis) > 0.01f) tag << "P";
            if (std::abs(o.stretch - 1.f) > 0.01f) tag << "S";
            if (o.stutter > 1) tag << "x" << o.stutter;
            g.setColour(kRollC); g.setFont(juce::FontOptions(10.5f)); g.drawText(tag, r.toNearestInt().withTrimmedTop(20), juce::Justification::centred);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int n = (int) proc.arrangement.size();
        for (int i = 0; i < n; ++i) if (chipRect(i).contains(e.getPosition())) { selected = i; if (onSelect) onSelect(i); if (e.getNumberOfClicks() > 1 && onPreview) onPreview(i); repaint(); return; }
    }
private:
    RaveProcessor& proc;
};

class SlicerPage : public PageBase
{
public:
    explicit SlicerPage(Ctx& c) : PageBase(c), chips(c.proc)
    {
        addAndMakeVisible(wave); wave.setMode(WaveView::Mode::markers); wave.setEmptyText("no break loaded - use 'Load break...' or 'Demo break'");
        wave.onAddMarker = [this](float p) { edit([&] { proc.sliceModel.addMarker(snapZero(p)); }); };
        wave.onMoveMarker = [this](int i, float p) { edit([&] { proc.sliceModel.moveMarker(i, snapZero(p)); }, false); };
        wave.onRemoveMarker = [this](int i) { edit([&] { proc.sliceModel.removeMarker(i); }); };
        wave.onSelectMarker = [this](int i) { proc.previewSlice(i); markerSel = i; lastRev = -1; };
        wave.onLoopEndChanged = [this](float p) { const juce::ScopedLock sl(proc.modelLock); proc.sliceLoopSamples = p >= 0.999f || ! proc.sliceModel.source ? 0 : (int) (p * proc.sliceModel.source->length()); };

        makeButton(bLoad, "Load break...", [this] { chooseBreak(); });
        makeButton(bDemo, "Demo break", [this] { proc.setBreak(kit::makeDemoBreak(nullptr, 170.0), true); afterModelChange(); ctx.status("Synthetic demo break (170 BPM, 2 bars) loaded", false); });
        makeButton(bDetect, "Detect transients", [this] { proc.detectSlices((float) sens.getValue()); afterModelChange(); status(); });
        makeButton(bEqual, "Slice equally", [this] { edit([&] { proc.sliceModel.sliceEqual(equalBox.getSelectedId()); }); });
        makeButton(bPlayBreak, "Play break", [this] { proc.previewSample(proc.sliceModel.source); });
        makeButton(bStop, "Stop", [this] { proc.stopPreview(); });
        sens.setSliderStyle(juce::Slider::LinearHorizontal); sens.setRange(0.0, 1.0, 0.01); sens.setValue(0.6); sens.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
        sens.setColour(juce::Slider::trackColourId, col::accent); sens.setColour(juce::Slider::backgroundColourId, col::outline); addAndMakeVisible(sens);
        styleLabel(sensLabel); sensLabel.setText("Sensitivity", juce::dontSendNotification); addAndMakeVisible(sensLabel);
        for (int n : { 2, 4, 8, 12, 16, 32 }) equalBox.addItem(juce::String(n) + " slices", n);
        equalBox.setSelectedId(16, juce::dontSendNotification); addAndMakeVisible(equalBox);
        for (int n : { 16, 32, 64 }) loopBox.addItem(juce::String(n) + " steps", n);
        loopBox.setSelectedId(32, juce::dontSendNotification); addAndMakeVisible(loopBox);
        styleLabel(loopLabel); loopLabel.setText("Loop grid", juce::dontSendNotification); addAndMakeVisible(loopLabel);
        styleLabel(statusLabel, 13.f, col::text); addAndMakeVisible(statusLabel);
        styleLabel(hintLabel, 12.f); addAndMakeVisible(hintLabel);
        hintLabel.setText("Click the waveform = add marker (snaps to zero crossing)  |  drag marker = move  |  right-click marker = remove  |  drag the dashed 'loop end' line to cut off the tail", juce::dontSendNotification);

        addAndMakeVisible(chips);
        chips.onSelect = [this](int) { loadChip(); };
        chips.onPreview = [this](int i) { previewChip(i); };
        for (auto* s : { &cSemis, &cStretch, &cGain }) { s->setSliderStyle(juce::Slider::LinearHorizontal); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20); s->setColour(juce::Slider::trackColourId, col::accent); s->setColour(juce::Slider::backgroundColourId, col::outline); addAndMakeVisible(*s); }
        cSemis.setRange(-24, 24, 0.5); cSemis.setDoubleClickReturnValue(true, 0); cStretch.setRange(0.5, 2.0, 0.01); cStretch.setDoubleClickReturnValue(true, 1); cGain.setRange(0.0, 1.5, 0.01); cGain.setDoubleClickReturnValue(true, 1);
        for (int n : { 1, 2, 3, 4, 6, 8 }) cStutter.addItem(n == 1 ? "no stutter" : juce::String(n) + " repeats", n);
        addAndMakeVisible(cStutter);
        cRev.setButtonText("Reverse"); cRev.setClickingTogglesState(true); cRev.setColour(juce::TextButton::buttonOnColourId, col::accent.darker(0.3f)); addAndMakeVisible(cRev);
        for (auto* l : { &lSemis, &lStretch, &lGain, &lChip }) { styleLabel(*l); addAndMakeVisible(*l); }
        lChip.setColour(juce::Label::textColourId, col::text);
        lSemis.setText("Pitch", juce::dontSendNotification); lStretch.setText("Stretch", juce::dontSendNotification); lGain.setText("Gain", juce::dontSendNotification);
        auto apply = [this] { if (! loading) applyChip(); };
        cSemis.onValueChange = apply; cStretch.onValueChange = apply; cGain.onValueChange = apply; cStutter.onChange = apply; cRev.onClick = apply;
        makeButton(bLeft, "<", [this] { moveChip(-1); }); makeButton(bRight, ">", [this] { moveChip(1); });
        makeButton(bDup, "Duplicate", [this] { dupChip(); }); makeButton(bDel, "Remove", [this] { delChip(); });
        makeButton(bPrev, "Play slice", [this] { previewChip(chips.selected); });
        makeButton(bShuffle, "Shuffle", [this] { shuffle(); }); makeButton(bResetOrder, "Reset order", [this] { resetArrangement(); chips.repaint(); });
        makeButton(bReverseOrder, "Reverse order", [this] { const juce::ScopedLock sl(proc.modelLock); std::reverse(proc.arrangement.begin(), proc.arrangement.end()); chips.repaint(); });
        makeButton(bPlayArr, "Play arrangement", [this] { proc.previewSample(proc.renderArranged("arrangement")); });
        makeButton(bSlicesToPads, "Slices > pads", [this] { slicesToPads(); });
        makeButton(bArrToPads, "Arrangement > pads", [this] { arrangementToPads(); });
        makeButton(bArrToPad, "Arrangement > one pad", [this] { arrangementToOnePad(); });
        makeButton(bToPattern, "Slice pattern > current slot", [this] { slicesToPattern(); });
        for (int i = 0; i < kPads; ++i) { padBox.addItem("pad " + juce::String(i + 1), i + 1); }
        padBox.setSelectedId(1, juce::dontSendNotification); addAndMakeVisible(padBox);
        styleLabel(padLabel); padLabel.setText("target / first pad", juce::dontSendNotification); addAndMakeVisible(padLabel);
        loadChip();
    }

    void onShown() override { lastRev = -1; refresh(); }
    void refresh() override
    {
        {
            const juce::ScopedLock sl(proc.modelLock);
            const int n = proc.sliceModel.numSlices();
            bool bad = (int) proc.arrangement.size() != n && (proc.arrangement.empty() || true);
            for (auto& o : proc.arrangement) if (o.slice >= n) bad = true;
            if (n > 0 && (proc.arrangement.empty() || bad)) { if (proc.arrangement.empty() || (int) proc.arrangement.size() != n || bad) { if (proc.arrangement.empty() || std::any_of(proc.arrangement.begin(), proc.arrangement.end(), [n](const slicer::SliceOp& o) { return o.slice >= n; })) resetArrangementLocked(); } }
        }
        const int rev = proc.editRevision.load();
        if (rev == lastRev) return;
        lastRev = rev;
        const juce::ScopedLock sl(proc.modelLock);
        wave.setSample(proc.sliceModel.source);
        std::vector<float> m;
        if (proc.sliceModel.source) for (int v : proc.sliceModel.markers) m.push_back((float) v / (float) proc.sliceModel.source->length());
        wave.setMarkers(m, markerSel);
        wave.setLoopEnd(proc.sliceModel.source && proc.sliceLoopSamples > 0 ? (float) proc.sliceLoopSamples / (float) proc.sliceModel.source->length() : 0.f);
        status(); chips.repaint(); loadChip();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 8);
        auto row = r.removeFromTop(30);
        bLoad.setBounds(row.removeFromLeft(110).reduced(2, 0)); bDemo.setBounds(row.removeFromLeft(100).reduced(2, 0)); row.removeFromLeft(10);
        sensLabel.setBounds(row.removeFromLeft(74)); sens.setBounds(row.removeFromLeft(200)); bDetect.setBounds(row.removeFromLeft(130).reduced(2, 0)); row.removeFromLeft(10);
        equalBox.setBounds(row.removeFromLeft(100).reduced(0, 2)); bEqual.setBounds(row.removeFromLeft(110).reduced(2, 0));
        bStop.setBounds(row.removeFromRight(60).reduced(2, 0)); bPlayBreak.setBounds(row.removeFromRight(100).reduced(2, 0));
        r.removeFromTop(6);
        statusLabel.setBounds(r.removeFromTop(20));
        wave.setBounds(r.removeFromTop(210));
        hintLabel.setBounds(r.removeFromTop(20));
        r.removeFromTop(4);
        auto arrRow = r.removeFromTop(28);
        for (auto* b : { &bShuffle, &bResetOrder, &bReverseOrder, &bPlayArr }) b->setBounds(arrRow.removeFromLeft(118).reduced(2, 0));
        r.removeFromTop(4);
        chips.setBounds(r.removeFromTop(ChipStrip::ch * 2 + 4));
        r.removeFromTop(4);
        auto cr = r.removeFromTop(28);
        lChip.setBounds(cr.removeFromLeft(150)); cRev.setBounds(cr.removeFromLeft(74).reduced(2, 1));
        lSemis.setBounds(cr.removeFromLeft(42)); cSemis.setBounds(cr.removeFromLeft(150)); lStretch.setBounds(cr.removeFromLeft(52)); cStretch.setBounds(cr.removeFromLeft(140));
        lGain.setBounds(cr.removeFromLeft(36)); cGain.setBounds(cr.removeFromLeft(120)); cStutter.setBounds(cr.removeFromLeft(110).reduced(4, 2));
        auto cr2 = r.removeFromTop(30);
        bLeft.setBounds(cr2.removeFromLeft(40).reduced(2, 1)); bRight.setBounds(cr2.removeFromLeft(40).reduced(2, 1));
        bDup.setBounds(cr2.removeFromLeft(90).reduced(2, 1)); bDel.setBounds(cr2.removeFromLeft(90).reduced(2, 1)); bPrev.setBounds(cr2.removeFromLeft(100).reduced(2, 1));
        r.removeFromTop(6);
        auto out = r.removeFromTop(32);
        padLabel.setBounds(out.removeFromLeft(112)); padBox.setBounds(out.removeFromLeft(80).reduced(0, 2)); out.removeFromLeft(8);
        bSlicesToPads.setBounds(out.removeFromLeft(118).reduced(2, 1)); bArrToPads.setBounds(out.removeFromLeft(150).reduced(2, 1)); bArrToPad.setBounds(out.removeFromLeft(170).reduced(2, 1)); out.removeFromLeft(10);
        loopLabel.setBounds(out.removeFromLeft(70)); loopBox.setBounds(out.removeFromLeft(96).reduced(0, 2)); bToPattern.setBounds(out.removeFromLeft(210).reduced(2, 1));
    }

    // Test-Zugriff
    WaveView wave;
    ChipStrip chips;
    void testPressSlicesToPads() { slicesToPads(); }
    void testSetFirstPad(int p) { padBox.setSelectedId(p + 1, juce::dontSendNotification); }

private:
    template <typename F> void edit(F f, bool full = true)
    {
        { const juce::ScopedLock sl(proc.modelLock); f(); if (full) resetArrangementLocked(); }
        ++proc.editRevision; if (full) afterModelChange();
    }
    void afterModelChange() { markerSel = -1; { const juce::ScopedLock sl(proc.modelLock); resetArrangementLocked(); } lastRev = -1; chips.selected = 0; refresh(); }
    void resetArrangement() { const juce::ScopedLock sl(proc.modelLock); resetArrangementLocked(); ++proc.editRevision; }
    void resetArrangementLocked()
    {
        proc.arrangement.clear();
        for (int i = 0; i < proc.sliceModel.numSlices(); ++i) { slicer::SliceOp o; o.slice = i; proc.arrangement.push_back(o); }
    }
    int snapZero(float pos01) const
    {
        const juce::ScopedLock sl(proc.modelLock);
        if (! proc.sliceModel.source) return 0;
        const auto& s = *proc.sliceModel.source; const int n = s.length();
        int p = juce::jlimit(1, n - 2, (int) (pos01 * n));
        const float* d = s.data.getReadPointer(0);
        for (int k = 0; k < 64; ++k)
            for (int sgn : { -1, 1 }) { const int q = p + sgn * k; if (q > 0 && q < n && (d[q - 1] <= 0.f) != (d[q] <= 0.f)) return q; }
        return p;
    }
    void status()
    {
        const juce::ScopedLock sl(proc.modelLock);
        const int n = proc.sliceModel.numSlices();
        if (! proc.sliceModel.source) { statusLabel.setText("No break loaded.", juce::dontSendNotification); return; }
        auto& s = *proc.sliceModel.source;
        juce::String t = s.name + "   " + juce::String(s.seconds(), 2) + " s   " + juce::String(n) + " slices";
        if (n > kPads) t << "   (only the first " << kPads << " slices can be placed on pads)";
        statusLabel.setColour(juce::Label::textColourId, n > kPads ? col::warn : col::text);
        statusLabel.setText(t, juce::dontSendNotification);
    }
    void chooseBreak()
    {
        chooser = std::make_unique<juce::FileChooser>("Load break / loop", juce::File::getSpecialLocation(juce::File::userMusicDirectory), wildcard());
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc)
        {
            const auto f = fc.getResult(); if (! f.existsAsFile()) return;
            juce::String err;
            if (proc.loadBreakFile(f, &err)) { afterModelChange(); ctx.status("Break loaded: " + f.getFileName() + " - " + juce::String(proc.sliceModel.numSlices()) + " slices found", false); }
            else ctx.status(err.isNotEmpty() ? err : "Cannot load " + f.getFileName(), true);
        });
    }
    void loadChip()
    {
        loading = true;
        const juce::ScopedLock sl(proc.modelLock);
        const bool ok = chips.selected >= 0 && chips.selected < (int) proc.arrangement.size();
        if (ok)
        {
            const auto o = proc.arrangement[(size_t) chips.selected];
            lChip.setText("Chip " + juce::String(chips.selected + 1) + " = slice " + juce::String(o.slice + 1), juce::dontSendNotification);
            cSemis.setValue(o.semis, juce::dontSendNotification); cStretch.setValue(o.stretch, juce::dontSendNotification); cGain.setValue(o.gain, juce::dontSendNotification);
            cStutter.setSelectedId(o.stutter, juce::dontSendNotification); cRev.setToggleState(o.reverse, juce::dontSendNotification);
        }
        else lChip.setText("(no slice)", juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &cSemis, &cStretch, &cGain, &cStutter, &cRev, &bLeft, &bRight, &bDup, &bDel, &bPrev }) c->setEnabled(ok);
        loading = false;
    }
    void applyChip()
    {
        { const juce::ScopedLock sl(proc.modelLock);
          if (chips.selected < 0 || chips.selected >= (int) proc.arrangement.size()) return;
          auto& o = proc.arrangement[(size_t) chips.selected];
          o.semis = (float) cSemis.getValue(); o.stretch = (float) cStretch.getValue(); o.gain = (float) cGain.getValue(); o.stutter = juce::jmax(1, cStutter.getSelectedId()); o.reverse = cRev.getToggleState(); }
        chips.repaint();
    }
    void previewChip(int i)
    {
        SamplePtr s;
        { const juce::ScopedLock sl(proc.modelLock);
          if (i < 0 || i >= (int) proc.arrangement.size() || ! proc.sliceModel.source) return;
          const auto op = proc.arrangement[(size_t) i];
          auto b = slicer::processSlice(*proc.sliceModel.source, proc.sliceModel.startOf(op.slice), proc.sliceModel.endOf(op.slice), op);
          s = makeSample(b, proc.sliceModel.source->rate, "chip"); }
        proc.previewSample(s);
    }
    void moveChip(int d)
    {
        { const juce::ScopedLock sl(proc.modelLock);
          const int i = chips.selected, j = i + d;
          if (i < 0 || j < 0 || i >= (int) proc.arrangement.size() || j >= (int) proc.arrangement.size()) return;
          std::swap(proc.arrangement[(size_t) i], proc.arrangement[(size_t) j]); chips.selected = j; }
        chips.repaint();
    }
    void dupChip() { { const juce::ScopedLock sl(proc.modelLock); if (chips.selected < 0 || chips.selected >= (int) proc.arrangement.size() || proc.arrangement.size() >= 128) return; proc.arrangement.insert(proc.arrangement.begin() + chips.selected + 1, proc.arrangement[(size_t) chips.selected]); ++chips.selected; } chips.repaint(); }
    void delChip() { { const juce::ScopedLock sl(proc.modelLock); if (proc.arrangement.size() <= 1 || chips.selected < 0 || chips.selected >= (int) proc.arrangement.size()) return; proc.arrangement.erase(proc.arrangement.begin() + chips.selected); chips.selected = juce::jmin(chips.selected, (int) proc.arrangement.size() - 1); } chips.repaint(); loadChip(); }
    void shuffle()
    {
        static int round = 0; ++round;
        const juce::ScopedLock sl(proc.modelLock);
        auto ops = slicer::shuffledOps(proc.sliceModel.numSlices(), (int) std::lround(proc.getRaw("seed")) * 7 + round);
        for (size_t i = 0; i < ops.size() && i < proc.arrangement.size(); ++i) { auto keep = proc.arrangement[(size_t) ops[i].slice < proc.arrangement.size() ? (size_t) ops[i].slice : 0]; (void) keep; }
        proc.arrangement = ops; chips.repaint();
    }
    void slicesToPads()
    {
        const int n = proc.distributeSlicesToPads(padBox.getSelectedId() - 1);
        if (n == 0) { ctx.status("No slices to distribute", true); return; }
        const int total = proc.sliceModel.numSlices();
        ctx.status(juce::String(n) + " slices placed on pads " + juce::String(padBox.getSelectedId()) + "-" + juce::String(padBox.getSelectedId() + n - 1) + (total > n ? " (" + juce::String(total - n) + " did not fit)" : juce::String()), total > n);
    }
    void arrangementToPads()
    {
        std::vector<std::pair<int, SamplePtr>> parts;
        { const juce::ScopedLock sl(proc.modelLock);
          if (! proc.sliceModel.source) { ctx.status("No break loaded", true); return; }
          int pad = padBox.getSelectedId() - 1;
          for (auto& op : proc.arrangement)
          {
              if (pad >= kPads) break;
              auto b = slicer::processSlice(*proc.sliceModel.source, proc.sliceModel.startOf(op.slice), proc.sliceModel.endOf(op.slice), op);
              parts.emplace_back(pad++, makeSample(b, proc.sliceModel.source->rate, "Arr " + juce::String(op.slice + 1)));
          } }
        int n = 0; for (auto& p : parts) { proc.setParam(padParamId(p.first, pStart), 0.f); proc.setParam(padParamId(p.first, pEnd), 1.f); n += proc.setPadSample(p.first, p.second) ? 1 : 0; }
        ctx.status(juce::String(n) + " arranged slices placed on pads", n == 0);
    }
    void arrangementToOnePad()
    {
        auto s = proc.renderArranged(proc.sliceModel.source ? proc.sliceModel.source->name + " (arranged)" : juce::String("arranged"));
        if (! s) { ctx.status("Nothing to render", true); return; }
        const int pad = padBox.getSelectedId() - 1;
        const bool ok = proc.setPadSample(pad, s);
        ctx.status(ok ? "Arranged break (" + juce::String(s->seconds(), 2) + " s) placed on pad " + juce::String(pad + 1) : "Audio engine busy - try again", ! ok);
    }
    void slicesToPattern()
    {
        const int slot = juce::jlimit(0, kUserPatterns - 1, (int) std::lround(proc.getRaw("pattern")));
        if (! proc.sliceModel.source) { ctx.status("No break loaded", true); return; }
        proc.pushUndo(slot);
        proc.slicesToPatternSlot(slot, padBox.getSelectedId() - 1, loopBox.getSelectedId());
        ctx.status("Slice pattern written to pattern " + juce::String(slot + 1) + " (slices must be on pads " + juce::String(padBox.getSelectedId()) + "+ - use 'Slices > pads')", false);
    }

    juce::TextButton bLoad, bDemo, bDetect, bEqual, bPlayBreak, bStop, bShuffle, bResetOrder, bReverseOrder, bPlayArr, bSlicesToPads, bArrToPads, bArrToPad, bToPattern, bLeft, bRight, bDup, bDel, bPrev;
    juce::Slider sens, cSemis, cStretch, cGain; juce::ComboBox equalBox, loopBox, cStutter, padBox; juce::TextButton cRev;
    juce::Label sensLabel, loopLabel, statusLabel, hintLabel, lSemis, lStretch, lGain, lChip, padLabel;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastRev = -1, markerSel = -1; bool loading = false;
};
} // namespace rave::gui
