#pragma once
#include "PageBase.h"

namespace rave::gui
{
// kleine Pattern-Vorschau (Zeilen = Pads, Spalten = Schritte)
class Thumb : public juce::Component
{
public:
    Thumb(RaveProcessor& p, int s) : proc(p), slot(s) {}
    bool selected = false;
    std::function<void()> onClick;
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(col::panel); g.fillRoundedRectangle(r, 4.f);
        g.setColour(selected ? col::accent : col::outline); g.drawRoundedRectangle(r.reduced(0.5f), 4.f, selected ? 2.f : 1.f);
        const int len = juce::jmax(1, proc.bank.length(slot));
        auto in = r.reduced(6.f);
        const float cw = in.getWidth() / (float) len, rh = in.getHeight() / (float) kPads;
        for (int pad = 0; pad < kPads; ++pad)
            for (int st = 0; st < len; ++st)
            {
                const Step s = proc.bank.get(slot, pad, st);
                if (! s.on()) continue;
                g.setColour((s.flags & rave::kGhost) ? rave::gui::kGhost : kHit);
                g.fillRect(in.getX() + st * cw, in.getY() + pad * rh, juce::jmax(1.f, cw - 0.5f), juce::jmax(1.f, rh - 0.5f));
            }
    }
    void mouseDown(const juce::MouseEvent&) override { if (onClick) onClick(); }
private:
    RaveProcessor& proc; int slot;
};

class GeneratorPage : public PageBase
{
public:
    explicit GeneratorPage(Ctx& c) : PageBase(c),
        genre("Genre", juce::StringArray::fromTokens(kGenreNames, "|", "")),
        sig("Time signature", juce::StringArray::fromTokens(kTimeSigNames, "|", "")),
        div("Step grid", juce::StringArray::fromTokens(kStepDivNames, "|", "")),
        intens("Intensity"), cplx("Complexity"), dens("Density"), ghost("Ghost notes"), var("Variation"), rnd("Randomness"), gswing("Swing (0=genre)"), tempo("Tempo (0=genre)"), seed("Seed", 92),
        evAmount("EVOLVE amount")
    {
        for (auto* p : std::initializer_list<juce::Component*> { &genre, &sig, &div, &intens, &cplx, &dens, &ghost, &var, &rnd, &gswing, &tempo, &seed, &evAmount }) addAndMakeVisible(*p);
        genre.attach(proc.apvts, "genre"); sig.attach(proc.apvts, "sig"); div.attach(proc.apvts, "div");
        intens.attach(proc.apvts, "g_int"); cplx.attach(proc.apvts, "g_cplx"); dens.attach(proc.apvts, "g_dens"); ghost.attach(proc.apvts, "g_ghost");
        var.attach(proc.apvts, "g_var"); rnd.attach(proc.apvts, "g_rand"); gswing.attach(proc.apvts, "g_swing"); tempo.attach(proc.apvts, "g_tempo"); seed.attach(proc.apvts, "seed");
        evAmount.slider.setRange(0.05, 1.0, 0.01); evAmount.slider.setValue(0.4);

        lenBox.addItem("16 steps", 16); lenBox.addItem("32 steps", 32); lenBox.addItem("48 steps", 48); lenBox.addItem("64 steps", 64);
        lenBox.setSelectedId(32, juce::dontSendNotification); addAndMakeVisible(lenBox);
        styleLabel(lenLabel); lenLabel.setText("Length", juce::dontSendNotification); addAndMakeVisible(lenLabel);
        tApply.setButtonText("Set BPM + swing"); tApply.setClickingTogglesState(true); tApply.setToggleState(true, juce::dontSendNotification);
        tApply.setColour(juce::TextButton::buttonOnColourId, col::accent.darker(0.3f)); addAndMakeVisible(tApply);

        makeButton(bDice, "Dice (new seed)", [this] { proc.setParam("seed", (float) (1 + (juce::Random::getSystemRandom().nextInt(9999)))); });
        makeButton(bGen, "Generate into current pattern", [this] { generate(); });
        makeButton(bUndo, "Undo", [this] { ctx.status(proc.popUndo() ? "Undone" : "Nothing to undo", false); });
        makeButton(bEvolve, "EVOLVE: make 4 variants", [this] { evolve(); });
        makeButton(bDiscard, "Discard variants", [this] { proc.discardVariants(); selVariant = -1; ctx.status("Variants discarded - original pattern untouched", false); });
        makeButton(bAccept, "Accept selected variant", [this] { accept(); });
        makeButton(bBackOrig, "Back to original", [this] { backToOriginal(); });
        styleLabel(note, 12.f); addAndMakeVisible(note);
        note.setText("Generate OVERWRITES the current pattern (use Undo). EVOLVE never touches the original: variants live in slots V1-V4; click one to hear it, 'Accept' copies it into the original's slot.", juce::dontSendNotification);
        styleLabel(vLabel, 13.f, col::text); addAndMakeVisible(vLabel);
        for (int i = 0; i < kVariants; ++i)
        {
            thumbs[(size_t) i] = std::make_unique<Thumb>(proc, kUserPatterns + i);
            thumbs[(size_t) i]->onClick = [this, i] { selVariant = i; proc.setParam("pattern", (float) (kUserPatterns + i)); refresh(); };
            addAndMakeVisible(*thumbs[(size_t) i]);
        }
        origThumb = std::make_unique<Thumb>(proc, 0); addAndMakeVisible(*origThumb);
        origThumb->onClick = [this] { backToOriginal(); };
        styleLabel(origLabel); addAndMakeVisible(origLabel);
    }
    ~GeneratorPage() override = default;

    void onShown() override { refresh(); }
    void refresh() override
    {
        const int src = proc.variantSource();
        const int cur = juce::jlimit(0, kPatternSlots - 1, (int) std::lround(proc.getRaw("pattern")));
        const bool have = src >= 0;
        for (int i = 0; i < kVariants; ++i) { thumbs[(size_t) i]->selected = cur == kUserPatterns + i; thumbs[(size_t) i]->setVisible(have); thumbs[(size_t) i]->repaint(); }
        if (cur >= kUserPatterns) selVariant = cur - kUserPatterns;
        bAccept.setEnabled(have && selVariant >= 0); bDiscard.setEnabled(have); bBackOrig.setEnabled(have);
        bUndo.setEnabled(proc.canUndo());
        vLabel.setText(have ? "Variants of pattern " + juce::String(src + 1) + " (original unchanged)" : "No variants yet - press 'EVOLVE: make 4 variants'", juce::dontSendNotification);
        origThumb->setVisible(have);
        if (have) { origLabel.setText("Original: pattern " + juce::String(src + 1), juce::dontSendNotification); }
        else origLabel.setText({}, juce::dontSendNotification);
        origSlotShown(src);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced(14, 8);
        auto left = r.removeFromLeft(430);
        for (auto* p : std::initializer_list<juce::Component*> { &genre, &sig, &div }) p->setBounds(left.removeFromTop(28));
        left.removeFromTop(4);
        for (auto* p : std::initializer_list<juce::Component*> { &intens, &cplx, &dens, &ghost, &var, &rnd, &gswing, &tempo, &seed }) p->setBounds(left.removeFromTop(28));
        left.removeFromTop(6);
        auto row = left.removeFromTop(28); lenLabel.setBounds(row.removeFromLeft(60)); lenBox.setBounds(row.removeFromLeft(110).reduced(0, 2)); tApply.setBounds(row.removeFromLeft(150).reduced(4, 1));
        left.removeFromTop(6);
        auto row2 = left.removeFromTop(32); bDice.setBounds(row2.removeFromLeft(130).reduced(2, 1)); bGen.setBounds(row2.removeFromLeft(230).reduced(2, 1)); bUndo.setBounds(row2.removeFromLeft(60).reduced(2, 1));
        r.removeFromLeft(20);
        note.setBounds(r.removeFromTop(48));
        evAmount.setBounds(r.removeFromTop(28));
        auto row3 = r.removeFromTop(32); bEvolve.setBounds(row3.removeFromLeft(210).reduced(2, 1)); bDiscard.setBounds(row3.removeFromLeft(140).reduced(2, 1));
        r.removeFromTop(10);
        vLabel.setBounds(r.removeFromTop(22));
        const int tw = juce::jmin(260, (r.getWidth() - 12) / 2), th = 120;
        auto grid = r.removeFromTop(th * 2 + 8);
        for (int i = 0; i < kVariants; ++i) thumbs[(size_t) i]->setBounds((i % 2) * (tw + 12) + grid.getX(), grid.getY() + (i / 2) * (th + 8), tw, th);
        r.removeFromTop(6);
        auto row4 = r.removeFromTop(32); bAccept.setBounds(row4.removeFromLeft(190).reduced(2, 1)); bBackOrig.setBounds(row4.removeFromLeft(150).reduced(2, 1));
        r.removeFromTop(6);
        origLabel.setBounds(r.removeFromTop(20));
        origThumb->setBounds(r.removeFromTop(th).withWidth(tw));
    }

private:
    void origSlotShown(int src)
    {
        if (src != shownSrc) { shownSrc = src; origThumb.reset(); origThumb = std::make_unique<Thumb>(proc, juce::jmax(0, src)); origThumb->onClick = [this] { backToOriginal(); }; addAndMakeVisible(*origThumb); resized(); }
        origThumb->repaint();
    }
    int curUserSlot() const { return juce::jlimit(0, kUserPatterns - 1, (int) std::lround(proc.getRaw("pattern"))); }
    void generate()
    {
        int slot = (int) std::lround(proc.getRaw("pattern"));
        if (slot >= kUserPatterns) { ctx.status("Select a normal pattern (1-16) first - variants cannot be overwritten", true); return; }
        const int len = lenBox.getSelectedId();
        proc.pushUndo(slot);
        proc.bank.setLength(slot, len);
        const auto res = proc.generatePattern(slot, tApply.getToggleState());
        ctx.status("Generated " + juce::String(kGenreNamesArr()[proc.getRaw("genre") > 0 ? (int) std::lround(proc.getRaw("genre")) : 0]) + " pattern (" + juce::String(res.bars) + " bar" + (res.bars == 1 ? "" : "s") + ") into pattern " + juce::String(slot + 1) + " - Undo available", false);
    }
    static juce::StringArray kGenreNamesArr() { return juce::StringArray::fromTokens(kGenreNames, "|", ""); }
    void evolve()
    {
        int slot = (int) std::lround(proc.getRaw("pattern"));
        if (slot >= kUserPatterns) slot = juce::jmax(0, proc.variantSource());
        proc.evolveFrom(slot, (float) evAmount.slider.getValue());
        selVariant = -1; proc.setParam("pattern", (float) slot);
        ctx.status("4 variants created from pattern " + juce::String(slot + 1) + " - click one to hear it. The original is unchanged.", false);
    }
    void accept()
    {
        const int src = proc.variantSource();
        if (src < 0 || selVariant < 0) return;
        proc.pushUndo(src);
        proc.acceptVariant(selVariant, src);
        proc.setParam("pattern", (float) src);
        ctx.status("Variant V" + juce::String(selVariant + 1) + " copied into pattern " + juce::String(src + 1) + " (Undo available)", false);
    }
    void backToOriginal() { const int src = proc.variantSource(); if (src >= 0) proc.setParam("pattern", (float) src); }

    ParamCombo genre, sig, div; ParamSlider intens, cplx, dens, ghost, var, rnd, gswing, tempo, seed, evAmount;
    juce::ComboBox lenBox; juce::Label lenLabel, note, vLabel, origLabel; juce::TextButton tApply, bDice, bGen, bUndo, bEvolve, bDiscard, bAccept, bBackOrig;
    std::array<std::unique_ptr<Thumb>, kVariants> thumbs; std::unique_ptr<Thumb> origThumb;
    int selVariant = -1, shownSrc = -2;
};
} // namespace rave::gui
