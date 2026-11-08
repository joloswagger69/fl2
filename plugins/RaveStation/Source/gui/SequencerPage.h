#pragma once
#include "PageBase.h"

namespace rave::gui
{
class StepGrid : public juce::Component
{
public:
    static constexpr int labelW = 190, rowH = 28;
    explicit StepGrid(Ctx& c) : ctx(c), proc(c.proc) {}

    int slot = 0;
    int selPad = 0, selStep = 0;
    std::function<void()> onSelectionChanged;
    int playStep = -1;

    int length() const { return proc.bank.length(slot); }
    double cellW() const { return juce::jmin(64.0, (double) (getWidth() - labelW - 6) / juce::jmax(1, length())); }
    juce::Rectangle<int> cellRect(int pad, int step) const { const double w = cellW(); return { labelW + (int) std::floor(step * w), pad * rowH, (int) std::floor((step + 1) * w) - (int) std::floor(step * w), rowH }; }
    int stepAt(int x) const { const int s = (int) std::floor((x - labelW) / cellW()); return s >= 0 && s < length() ? s : -1; }

    void paint(juce::Graphics& g) override
    {
        const int L = length();
        const int spb = proc.stepsPerBarNow(), spq = gen::stepsPerQuarter((int) std::lround(proc.getRaw("div")));
        for (int pad = 0; pad < kPads; ++pad)
        {
            auto row = juce::Rectangle<int>(0, pad * rowH, getWidth(), rowH);
            g.setColour(pad % 2 ? col::panel.darker(0.15f) : col::panel.darker(0.3f)); g.fillRect(row);
            const bool muted = proc.getRaw(padParamId(pad, pMute)) > 0.5f, solo = proc.getRaw(padParamId(pad, pSolo)) > 0.5f;
            const auto smp = proc.padSample(pad);
            g.setColour(pad == selPad ? col::accent : (muted ? col::textDim.withAlpha(0.5f) : col::text));
            g.setFont(juce::FontOptions(12.5f, pad == selPad ? juce::Font::bold : juce::Font::plain));
            g.drawText(juce::String(pad + 1).paddedLeft(' ', 2) + "  " + (smp ? smp->name : juce::String("(empty)")), 8, row.getY(), labelW - 34, rowH, juce::Justification::centredLeft, true);
            if (muted) { g.setColour(kMute); g.drawText("M", labelW - 28, row.getY(), 12, rowH, juce::Justification::centred); }
            if (solo) { g.setColour(kSolo); g.drawText("S", labelW - 16, row.getY(), 12, rowH, juce::Justification::centred); }
            for (int st = 0; st < L; ++st)
            {
                const auto cr = cellRect(pad, st).reduced(1, 2);
                const bool onBar = st % spb == 0, onBeat = st % spq == 0;
                g.setColour(onBar ? col::outline.brighter(0.35f) : (onBeat ? col::outline.brighter(0.1f) : col::outline.darker(0.3f)));
                g.fillRoundedRectangle(cr.toFloat(), 3.f);
                const Step s = proc.bank.get(slot, pad, st);
                if (s.on())
                {
                    const bool ghost = (s.flags & rave::kGhost) != 0, roll = (s.flags & rave::kRoll) != 0;
                    const auto c = ghost ? kGhost : (roll ? kRollC : kHit);
                    g.setColour(c.withAlpha(s.prob < 100 ? 0.35f : 0.28f)); g.fillRoundedRectangle(cr.toFloat(), 3.f);
                    const float h = cr.getHeight() * (float) s.vel / 127.f;
                    g.setColour(c.withAlpha(s.prob < 100 ? 0.65f : 1.f));
                    g.fillRoundedRectangle(cr.getX() * 1.f, cr.getBottom() - h, (float) cr.getWidth(), h, 3.f);
                    g.setColour(juce::Colours::black.withAlpha(0.75f)); g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
                    if (cr.getWidth() >= 22)
                    {
                        juce::String t; if (s.ratchet > 1) t << "x" << (int) s.ratchet; if (s.prob < 100) t << " " << (int) s.prob << "%"; if (s.pitch != 0) t << " " << (s.pitch > 0 ? "+" : "") << (int) s.pitch;
                        g.drawText(t.trim(), cr, juce::Justification::centred, false);
                    }
                    else if (s.ratchet > 1) { g.fillRect(cr.getCentreX() - 1, cr.getY() + 2, 2, 4); }
                }
                if (pad == selPad && st == selStep) { g.setColour(juce::Colours::white); g.drawRoundedRectangle(cr.toFloat(), 3.f, 1.6f); }
            }
        }
        if (playStep >= 0 && playStep < L)
        {
            g.setColour(kPlayhead.withAlpha(0.18f)); g.fillRect(juce::Rectangle<int>(cellRect(0, playStep).getX(), 0, cellRect(0, playStep).getWidth(), kPads * rowH));
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int pad = e.y / rowH;
        if (pad < 0 || pad >= kPads) return;
        dragging = false; paintOn = false; velEdit = false; moved = false;
        if (e.x < labelW)
        {
            select(pad, selStep);
            if (e.mods.isPopupMenu()) { rowMenu(pad); return; }
            proc.engine.auditionPad(pad, 0.9f); return;
        }
        const int st = stepAt(e.x);
        if (st < 0) return;
        if (e.mods.isPopupMenu()) { select(pad, st); stepMenu(pad, st); return; }
        const Step cur = proc.bank.get(slot, pad, st);
        proc.pushUndo(slot);
        select(pad, st);
        downY = e.y; downX = e.x; dragPad = pad; startVel = cur.vel;
        if (! cur.on()) { Step s; s.vel = (uint8_t) lastVel; proc.bank.set(slot, pad, st, s); paintOn = true; ++proc.editRevision; proc.engine.auditionPad(pad, s.vel / 127.f); notify(); }
        else { velEdit = true; wasOn = true; }
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragPad < 0) return;
        if (std::abs(e.y - downY) > 3 || std::abs(e.x - downX) > 3) moved = true;
        if (paintOn)
        {
            const int st = stepAt(e.x);
            if (st >= 0 && ! proc.bank.get(slot, dragPad, st).on()) { Step s; s.vel = (uint8_t) lastVel; proc.bank.set(slot, dragPad, st, s); ++proc.editRevision; repaint(); }
            return;
        }
        if (velEdit && moved)
        {
            Step s = proc.bank.get(slot, dragPad, selStep);
            if (! s.on()) return;
            s.vel = (uint8_t) juce::jlimit(1, 127, (int) std::lround(startVel + (downY - e.y) * 1.4f));
            lastVel = s.vel; proc.bank.set(slot, dragPad, selStep, s); ++proc.editRevision; notify(); repaint();
        }
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (velEdit && ! moved && dragPad >= 0)   // einfacher Klick auf aktiven Schritt: ausschalten
        {
            proc.bank.set(slot, dragPad, selStep, Step());
            ++proc.editRevision; notify(); repaint();
        }
        dragPad = -1; velEdit = false; paintOn = false;
    }

    void select(int pad, int step) { selPad = pad; selStep = step; if (onSelectionChanged) onSelectionChanged(); ctx.selectedPad = pad; repaint(); }

    // Test-/Skript-Zugriff: wie ein Klick auf die Zelle
    void clickCell(int pad, int step, bool popup = false)
    {
        const auto r = cellRect(pad, step);
        const juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(), r.getCentre().toFloat(), popup ? juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier) : juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                                 1.f, 0.f, 0.f, 0.f, 0.f, this, this, juce::Time::getCurrentTime(), r.getCentre().toFloat(), juce::Time::getCurrentTime(), 1, false);
        mouseDown(e); mouseUp(e);
    }

    Step clip; bool hasClip = false; std::vector<Step> rowClip; bool hasRowClip = false;
    int lastVel = 100;

private:
    void notify() { if (onSelectionChanged) onSelectionChanged(); }

    void stepMenu(int pad, int st)
    {
        const Step s = proc.bank.get(slot, pad, st);
        juce::PopupMenu m;
        m.addItem(1, "Ghost note", true, s.on() && (s.flags & rave::kGhost));
        m.addItem(2, "Roll (velocity ramp)", true, s.on() && (s.flags & rave::kRoll));
        juce::PopupMenu pm; for (int p : { 100, 75, 50, 25 }) pm.addItem(300 + p, juce::String(p) + " %", s.on(), s.on() && s.prob == p);
        m.addSubMenu("Probability", pm);
        juce::PopupMenu rm; for (int r : { 1, 2, 3, 4, 6, 8 }) rm.addItem(200 + r, r == 1 ? "off" : "x" + juce::String(r), s.on(), s.on() && s.ratchet == r);
        m.addSubMenu("Ratchet", rm);
        m.addSeparator(); m.addItem(3, "Copy step", s.on()); m.addItem(4, "Paste step", hasClip); m.addItem(5, "Clear step", s.on());
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(cellRect(pad, st))), [this, pad, st](int r)
        {
            if (r == 0) return;
            Step cur = proc.bank.get(slot, pad, st);
            proc.pushUndo(slot);
            if (r == 1 && cur.on()) cur.flags ^= rave::kGhost;
            else if (r == 2 && cur.on()) cur.flags ^= rave::kRoll;
            else if (r >= 301 && r <= 400 && cur.on()) cur.prob = (uint8_t) (r - 300);
            else if (r >= 201 && r <= 208 && cur.on()) cur.ratchet = (uint8_t) (r - 200);
            else if (r == 3) { clip = cur; hasClip = true; return; }
            else if (r == 4 && hasClip) cur = clip;
            else if (r == 5) cur = Step();
            proc.bank.set(slot, pad, st, cur); ++proc.editRevision; notify(); repaint();
        });
    }
    void rowMenu(int pad)
    {
        juce::PopupMenu m;
        m.addItem(1, "Copy row"); m.addItem(2, "Paste row", hasRowClip); m.addItem(3, "Clear row");
        m.addItem(4, "Shift left"); m.addItem(5, "Shift right"); m.addItem(6, "Repeat first half");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(juce::Rectangle<int>(0, pad * rowH, labelW, rowH))), [this, pad](int r)
        {
            if (r == 0) return;
            const int L = length();
            std::vector<Step> row((size_t) L); for (int i = 0; i < L; ++i) row[(size_t) i] = proc.bank.get(slot, pad, i);
            if (r == 1) { rowClip = row; hasRowClip = true; return; }
            proc.pushUndo(slot);
            std::vector<Step> out((size_t) L);
            if (r == 2 && hasRowClip) for (int i = 0; i < L; ++i) out[(size_t) i] = i < (int) rowClip.size() ? rowClip[(size_t) i] : Step();
            else if (r == 3) {}
            else if (r == 4) for (int i = 0; i < L; ++i) out[(size_t) i] = row[(size_t) ((i + 1) % L)];
            else if (r == 5) for (int i = 0; i < L; ++i) out[(size_t) ((i + 1) % L)] = row[(size_t) i];
            else if (r == 6) for (int i = 0; i < L; ++i) out[(size_t) i] = row[(size_t) (i % juce::jmax(1, L / 2))];
            else out = row;
            for (int i = 0; i < L; ++i) proc.bank.set(slot, pad, i, out[(size_t) i]);
            ++proc.editRevision; repaint();
        });
    }

    Ctx& ctx; RaveProcessor& proc;
    bool dragging = false, paintOn = false, velEdit = false, moved = false, wasOn = false;
    int downY = 0, downX = 0, dragPad = -1, startVel = 100;
};

class SequencerPage : public PageBase
{
public:
    explicit SequencerPage(Ctx& c) : PageBase(c), grid(c)
    {
        addAndMakeVisible(grid);
        for (int i = 0; i < kPatternSlots; ++i)
        {
            auto& b = slotBtn[(size_t) i];
            b.setButtonText(i < kUserPatterns ? juce::String(i + 1) : "V" + juce::String(i - kUserPatterns + 1));
            b.setClickingTogglesState(false);
            b.onClick = [this, i] { proc.setParam("pattern", (float) i); };
            addAndMakeVisible(b);
        }
        lenLabel.setText("Length", juce::dontSendNotification); styleLabel(lenLabel); addAndMakeVisible(lenLabel);
        for (int n : { 4, 8, 12, 16, 24, 32, 48, 64 }) lenBox.addItem(juce::String(n) + " steps", n);
        lenBox.onChange = [this] { if (! updating) { proc.pushUndo(grid.slot); proc.bank.setLength(grid.slot, lenBox.getSelectedId()); ++proc.editRevision; grid.repaint(); } };
        addAndMakeVisible(lenBox);
        makeButton(bCopy, "Copy pattern", [this] { proc.bank.load(grid.slot, patClip); hasPatClip = true; ctx.status("Pattern copied", false); });
        makeButton(bPaste, "Paste pattern", [this] { if (! hasPatClip) { ctx.status("Nothing copied", true); return; } proc.pushUndo(grid.slot); proc.bank.store(grid.slot, patClip); ++proc.editRevision; });
        makeButton(bClear, "Clear pattern", [this] { proc.pushUndo(grid.slot); proc.clearPattern(grid.slot); });
        makeButton(bUndo, "Undo", [this] { ctx.status(proc.popUndo() ? "Undone" : "Nothing to undo", false); });
        makeButton(bCopyStep, "Copy step", [this] { const Step s = proc.bank.get(grid.slot, grid.selPad, grid.selStep); if (s.on()) { grid.clip = s; grid.hasClip = true; } });
        makeButton(bPasteStep, "Paste step", [this] { if (grid.hasClip) { proc.pushUndo(grid.slot); proc.bank.set(grid.slot, grid.selPad, grid.selStep, grid.clip); ++proc.editRevision; } });
        makeButton(bEvolve, "EVOLVE >", [this] { if (goGenerator) goGenerator(); });
        bEvolve.setTooltip("Create four variations of this pattern (the original is never touched) - see the GENERATOR page");

        info.setText("Click = add/remove  |  drag up/down = velocity  |  drag across empty cells = paint  |  right-click = ghost / roll / probability / ratchet  |  right-click a row name = row tools", juce::dontSendNotification);
        styleLabel(info, 12.f); addAndMakeVisible(info);

        for (auto* s : { &sVel, &sProb, &sRatchet, &sPitch })
        {
            s->setSliderStyle(juce::Slider::LinearHorizontal); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
            s->setColour(juce::Slider::trackColourId, col::accent); s->setColour(juce::Slider::backgroundColourId, col::outline); addAndMakeVisible(*s);
        }
        sVel.setRange(1, 127, 1); sProb.setRange(1, 100, 1); sProb.setTextValueSuffix(" %"); sRatchet.setRange(1, 8, 1); sPitch.setRange(-12, 12, 1); sPitch.setTextValueSuffix(" st");
        for (auto* l : { &lVel, &lProb, &lRatchet, &lPitch, &lStep }) { styleLabel(*l); addAndMakeVisible(*l); }
        lVel.setText("Velocity", juce::dontSendNotification); lProb.setText("Probability", juce::dontSendNotification); lRatchet.setText("Ratchet", juce::dontSendNotification); lPitch.setText("Pitch", juce::dontSendNotification);
        lStep.setColour(juce::Label::textColourId, col::text);
        tGhost.setButtonText("Ghost"); tRoll.setButtonText("Roll");
        for (auto* t : { &tGhost, &tRoll }) { t->setClickingTogglesState(true); t->setColour(juce::TextButton::buttonOnColourId, kGhost.darker(0.2f)); addAndMakeVisible(*t); }
        tRoll.setColour(juce::TextButton::buttonOnColourId, kRollC.darker(0.2f));
        auto edit = [this] { if (! updating) applyInspector(); };
        sVel.onValueChange = edit; sProb.onValueChange = edit; sRatchet.onValueChange = edit; sPitch.onValueChange = edit;
        tGhost.onClick = edit; tRoll.onClick = edit;
        grid.onSelectionChanged = [this] { loadInspector(); };
        loadInspector();
    }

    std::function<void()> goGenerator;
    StepGrid grid;

    void onShown() override { lastRev = -1; refresh(); }
    void refresh() override
    {
        const int slot = juce::jlimit(0, kPatternSlots - 1, (int) std::lround(proc.getRaw("pattern")));
        const int rev = proc.editRevision.load();
        const int playing = proc.engine.uiStep.load(), playSlot = proc.engine.uiSlot.load();
        const int ps = playSlot == slot ? playing : -1;
        if (slot != grid.slot || rev != lastRev || ps != grid.playStep)
        {
            grid.slot = slot; grid.playStep = ps;
            const bool slotChanged = slot != lastSlot;
            lastSlot = slot;
            if (rev != lastRev || slotChanged)
            {
                updating = true; lenBox.setSelectedId(proc.bank.length(slot), juce::dontSendNotification);
                if (lenBox.getSelectedId() == 0) lenBox.setText(juce::String(proc.bank.length(slot)) + " steps", juce::dontSendNotification);
                updating = false;
                for (int i = 0; i < kPatternSlots; ++i)
                {
                    const bool has = proc.bank.toString(i).contains(";"), cur = i == slot;
                    slotBtn[(size_t) i].setColour(juce::TextButton::buttonColourId, cur ? col::accent.darker(0.35f) : (has ? col::panel.brighter(0.18f) : col::panel.darker(0.2f)));
                    slotBtn[(size_t) i].setColour(juce::TextButton::textColourOffId, cur ? col::text : (has ? col::text : col::textDim));
                }
                loadInspector();
            }
            lastRev = rev; grid.repaint();
        }
        bUndo.setEnabled(proc.canUndo());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 8);
        auto top = r.removeFromTop(30);
        for (int i = 0; i < kPatternSlots; ++i)
        {
            if (i == kUserPatterns) top.removeFromLeft(14);
            slotBtn[(size_t) i].setBounds(top.removeFromLeft(i < kUserPatterns ? 40 : 42).reduced(1, 0));
        }
        r.removeFromTop(6);
        auto ctl = r.removeFromTop(28);
        lenLabel.setBounds(ctl.removeFromLeft(52)); lenBox.setBounds(ctl.removeFromLeft(104).reduced(0, 1)); ctl.removeFromLeft(10);
        for (auto* b : { &bCopy, &bPaste, &bClear, &bUndo }) b->setBounds(ctl.removeFromLeft(100).reduced(2, 0));
        ctl.removeFromLeft(10);
        for (auto* b : { &bCopyStep, &bPasteStep }) b->setBounds(ctl.removeFromLeft(94).reduced(2, 0));
        bEvolve.setBounds(ctl.removeFromRight(110).reduced(2, 0));
        r.removeFromTop(6);
        grid.setBounds(r.removeFromTop(StepGrid::rowH * kPads));
        r.removeFromTop(6);
        info.setBounds(r.removeFromTop(18));
        auto insp = r.removeFromTop(30);
        lStep.setBounds(insp.removeFromLeft(210));
        auto place = [&](juce::Label& l, juce::Slider& s, int w) { l.setBounds(insp.removeFromLeft(72)); s.setBounds(insp.removeFromLeft(w)); insp.removeFromLeft(10); };
        place(lVel, sVel, 150); place(lProb, sProb, 130); place(lRatchet, sRatchet, 100); place(lPitch, sPitch, 120);
        tGhost.setBounds(insp.removeFromLeft(64).reduced(2, 2)); tRoll.setBounds(insp.removeFromLeft(64).reduced(2, 2));
    }

    // Test-Zugriff
    void setInspectorVelocity(int v) { sVel.setValue(v); }

private:
    void loadInspector()
    {
        updating = true;
        const Step s = proc.bank.get(grid.slot, grid.selPad, grid.selStep);
        const auto smp = proc.padSample(grid.selPad);
        lStep.setText("Step " + juce::String(grid.selStep + 1) + " - pad " + juce::String(grid.selPad + 1) + (smp ? " (" + smp->name + ")" : juce::String()) + (s.on() ? "" : "  [off]"), juce::dontSendNotification);
        sVel.setValue(s.on() ? s.vel : 100, juce::dontSendNotification); sProb.setValue(s.prob, juce::dontSendNotification); sRatchet.setValue(s.ratchet, juce::dontSendNotification); sPitch.setValue(s.pitch, juce::dontSendNotification);
        tGhost.setToggleState(s.on() && (s.flags & rave::kGhost), juce::dontSendNotification); tRoll.setToggleState(s.on() && (s.flags & rave::kRoll), juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &sVel, &sProb, &sRatchet, &sPitch, &tGhost, &tRoll }) c->setEnabled(s.on());
        updating = false;
    }
    void applyInspector()
    {
        Step s = proc.bank.get(grid.slot, grid.selPad, grid.selStep);
        if (! s.on()) return;
        s.vel = (uint8_t) sVel.getValue(); s.prob = (uint8_t) sProb.getValue(); s.ratchet = (uint8_t) sRatchet.getValue(); s.pitch = (int8_t) sPitch.getValue();
        s.flags = (uint8_t) ((tGhost.getToggleState() ? rave::kGhost : 0) | (tRoll.getToggleState() ? rave::kRoll : 0));
        proc.bank.set(grid.slot, grid.selPad, grid.selStep, s); grid.lastVel = s.vel; ++proc.editRevision; grid.repaint();
    }

    std::array<juce::TextButton, kPatternSlots> slotBtn;
    juce::Label lenLabel, info, lStep, lVel, lProb, lRatchet, lPitch;
    juce::ComboBox lenBox;
    juce::TextButton bCopy, bPaste, bClear, bUndo, bCopyStep, bPasteStep, bEvolve, tGhost, tRoll;
    juce::Slider sVel, sProb, sRatchet, sPitch;
    PlainPattern patClip; bool hasPatClip = false;
    bool updating = false; int lastRev = -1, lastSlot = -1;
};
} // namespace rave::gui
