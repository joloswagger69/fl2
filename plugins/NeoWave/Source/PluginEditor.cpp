#include "PluginEditor.h"

using ctl::Panel;

// ------------------------------------------------------------------------------------------------
void MidiMapView::paint(juce::Graphics& g)
{
    using namespace nwc;
    g.setFont(juce::Font(juce::FontOptions(13.f)));
    int y = 0, n = 0;
    for (int cc = 0; cc < 128; ++cc)
    {
        const int p = proc.ccMap[(size_t) cc].load();
        if (p < 0 || p >= P::kCount) continue;
        ++n;
        if (y > getHeight() - 24) continue;
        auto r = juce::Rectangle<int>(0, y, getWidth(), 22);
        g.setColour(aqua.withAlpha(0.12f));
        g.fillRoundedRectangle(r.toFloat().reduced(0.f, 1.f), 5.f);
        g.setColour(neon);
        g.drawText("CC " + juce::String(cc), r.withTrimmedLeft(8).withWidth(64), juce::Justification::centredLeft);
        g.setColour(text);
        g.drawText(juce::String(pdef(p).name) + "  (" + pdef(p).id + ")", r.withTrimmedLeft(76), juce::Justification::centredLeft);
        y += 24;
    }
    if (n == 0)
    {
        g.setColour(textDim);
        g.drawFittedText("No mappings yet.\n\nRight-click any knob and choose \"MIDI Learn\", then move a controller on your keyboard.",
                         getLocalBounds().reduced(10), juce::Justification::centred, 6);
    }
}

// ------------------------------------------------------------------------------------------------
NeoEditor::NeoEditor(NeoProcessor& p)
    : juce::AudioProcessorEditor(&p), proc(p),
      scopeMain(p), scopeOsc(p), scopeFilter(p), scopeFx(p),
      specMain(p), specOsc(p), specFilter(p), specFx(p)
{
    setLookAndFeel(&laf);
    setSize(1180, 720);
    setResizable(false, false);

    build();

    for (auto& pg : pages) addChildComponent(pg);

    for (int i = 0; i < kPages; ++i)
    {
        static const char* names[kPages] = { "MAIN", "OSCILLATORS", "FILTER", "MODULATION", "EFFECTS", "PRESETS", "ADVANCED" };
        tabs[i].setButtonText(names[i]);
        tabs[i].setClickingTogglesState(true);
        tabs[i].setRadioGroupId(100);
        tabs[i].onClick = [this, i] { showPage(i); };
        addAndMakeVisible(tabs[i]);
    }

    presetLabel.setJustificationType(juce::Justification::centred);
    presetLabel.setFont(juce::Font(juce::FontOptions(20.f, juce::Font::bold)));
    presetLabel.setColour(juce::Label::textColourId, nwc::text);
    presetLabel.onClick = [this] { showPage(5); };
    addAndMakeVisible(presetLabel);

    bPrev.onClick = [this] { if (browser) browser->step(-1); };
    bNext.onClick = [this] { if (browser) browser->step(+1); };
    bAB.onClick = [this] { proc.toggleAB(); bAB.setButtonText(proc.abActive == 0 ? "A" : "B"); };
    bUndo.onClick = [this] { proc.undo.undo(); };
    bRedo.onClick = [this] { proc.undo.redo(); };
    bRand.onClick = [this] { proc.randomize(false); };
    bSmart.onClick = [this] { proc.randomize(true); };
    for (auto* b : { &bPrev, &bNext, &bAB, &bUndo, &bRedo, &bRand, &bSmart }) addAndMakeVisible(*b);

    showPage(0);
    startTimerHz(30);
    timerCallback();
}

NeoEditor::~NeoEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ------------------------------------------------------------------------------------------------
ctl::Knob* NeoEditor::K(int pid, const char* label)
{
    auto* k = new ctl::Knob(proc, pid, label != nullptr ? juce::String(label) : juce::String());
    owned.add(k);
    knobs.push_back(k);
    return k;
}
ctl::Choice* NeoEditor::C(int pid, const char* label)
{
    auto* c = new ctl::Choice(proc, pid, label != nullptr ? juce::String(label) : juce::String());
    owned.add(c);
    return c;
}
Panel* NeoEditor::Pn(int page, const char* title)
{
    auto* p = new Panel(title);
    panels.add(p);
    pages[page].addAndMakeVisible(p);
    return p;
}

void NeoEditor::build()
{
    auto kn = [](ctl::Knob* k, float w = 1.f) { return Panel::knob(k, w); };
    auto ch = [](ctl::Choice* c, float w = 1.f) { return Panel::choice(c, w); };

    envAmpMain   = std::make_unique<EnvView>(proc, P::ampA, P::ampD, P::ampS, P::ampR);
    envAmpFilter = std::make_unique<EnvView>(proc, P::ampA, P::ampD, P::ampS, P::ampR);
    envFilt      = std::make_unique<EnvView>(proc, P::fltA, P::fltD, P::fltS, P::fltR);
    envMod       = std::make_unique<EnvView>(proc, P::modEnvA, P::modEnvD, P::modEnvS, P::modEnvR);
    stepEditor   = std::make_unique<StepEditor>(proc);
    browser      = std::make_unique<PresetBrowser>(proc);
    midiMap      = std::make_unique<MidiMapView>(proc);

    // ================================================================ MAIN
    {
        auto& pg = pages[0];
        pg.addAndMakeVisible(sculpture);  sculpture.setBounds(0, 0, 440, 380);
        pg.addAndMakeVisible(scopeMain);  scopeMain.setBounds(0, 390, 215, 218);
        pg.addAndMakeVisible(specMain);   specMain.setBounds(225, 390, 215, 218);

        auto* pm = Pn(0, "MACROS");
        pm->addRow({ kn(K(P::macro1)), kn(K(P::macro2)), kn(K(P::macro3)), kn(K(P::macro4)) });
        pm->setBounds(452, 0, 348, 146);

        auto* pv = Pn(0, "VOICE & OUTPUT");
        pv->addRow({ kn(K(P::master)), kn(K(P::glide)), kn(K(P::width)), ch(C(P::voiceMode), 1.2f) });
        pv->setBounds(808, 0, 348, 146);

        auto* po = Pn(0, "OSCILLATOR MIX");
        po->addRow({ kn(K(P::aLevel, "Osc A")), kn(K(P::bLevel, "Osc B")), kn(K(P::subLevel)), kn(K(P::noiseLevel)),
                     kn(K(P::uniVoices)), kn(K(P::uniDetune)), kn(K(P::uniSpread)) });
        po->setBounds(452, 154, 704, 146);

        auto* pf = Pn(0, "FILTER");
        pf->addRow({ ch(C(P::fType)) });
        pf->addRow({ kn(K(P::fCut)), kn(K(P::fRes)), kn(K(P::fDrive)), kn(K(P::fEnv, "Env Amt")) });
        pf->addRow({ kn(K(P::fltA, "F.Attack")), kn(K(P::fltD, "F.Decay")), kn(K(P::fltS, "F.Sustain")), kn(K(P::fltR, "F.Release")) });
        pf->setBounds(452, 308, 348, 300);

        auto* pa = Pn(0, "AMP ENVELOPE");
        pa->addRow({ Panel::fill(envAmpMain.get(), 144) });
        pa->addRow({ kn(K(P::ampA)), kn(K(P::ampD)), kn(K(P::ampS)), kn(K(P::ampR)) });
        pa->setBounds(808, 308, 348, 300);
    }

    // ================================================================ OSCILLATORS
    {
        auto& pg = pages[1];
        auto* a = Pn(1, "OSCILLATOR A");
        a->addRow({ ch(C(P::aEngine)) });
        a->addRow({ ch(chWaveA = C(P::aWave, "Wave (Analog)")), ch(chTableA = C(P::aTable, "Table (WT / Granular)")) });
        a->addRow({ kn(K(P::aMorph)), kn(K(P::aColor)), kn(K(P::aLevel)) });
        a->addRow({ kn(K(P::aOct)), kn(K(P::aSemi)), kn(K(P::aFine)) });
        a->addRow({ Panel::fill(&hintA, 40) });
        a->setBounds(0, 0, 380, 396);

        auto* b = Pn(1, "OSCILLATOR B");
        b->addRow({ ch(C(P::bEngine)), ch(C(P::bSync, "Sync (B -> A)")) });
        b->addRow({ ch(chWaveB = C(P::bWave, "Wave (Analog)")), ch(chTableB = C(P::bTable, "Table (WT / Granular)")) });
        b->addRow({ kn(K(P::bMorph)), kn(K(P::bColor)), kn(K(P::bLevel)) });
        b->addRow({ kn(K(P::bOct)), kn(K(P::bSemi)), kn(K(P::bFine)) });
        b->addRow({ Panel::fill(&hintB, 40) });
        b->setBounds(388, 0, 380, 396);

        pg.addAndMakeVisible(scopeOsc); scopeOsc.setBounds(776, 0, 380, 194);
        pg.addAndMakeVisible(specOsc);  specOsc.setBounds(776, 202, 380, 194);

        auto* m = Pn(1, "SUB  /  NOISE  /  RING  /  CROSS FM  /  UNISON");
        m->addRow({ ch(C(P::subWave)), ch(C(P::subOct)), ch(C(P::noiseType)), Panel::fill(&hintMix, 40, 3.f) });
        m->addRow({ kn(K(P::subLevel)), kn(K(P::noiseLevel)), kn(K(P::ringMod)), kn(K(P::crossFm)),
                    kn(K(P::uniVoices)), kn(K(P::uniDetune)), kn(K(P::uniSpread)) });
        m->setBounds(0, 404, 1156, 194);
        hintMix.maxLines = 3;
        hintMix.setText("Sub: sine or square, one or two octaves down.  Ring: blends in the A x B product.  "
                        "Cross FM: oscillators phase-modulate each other.  Unison stacks up to 16 detuned, panned voices per note.");
    }

    // ================================================================ FILTER
    {
        auto& pg = pages[2];
        auto* f = Pn(2, "FILTER");
        f->addRow({ ch(C(P::fType), 0.5f), Panel::fill(&hintFilter, 40, 2.5f) });
        f->addRow({ kn(K(P::fCut)), kn(K(P::fRes)), kn(K(P::fDrive)), kn(K(P::fKey)), kn(K(P::fEnv, "Env Amt")) });
        f->setBounds(0, 0, 560, 190);

        auto* fe = Pn(2, "FILTER ENVELOPE");
        fe->addRow({ Panel::fill(envFilt.get(), 90) });
        fe->addRow({ kn(K(P::fltA)), kn(K(P::fltD)), kn(K(P::fltS)), kn(K(P::fltR)), kn(K(P::fEnv, "Env Amt")) });
        fe->setBounds(568, 0, 588, 244);

        auto* ae = Pn(2, "AMP ENVELOPE");
        ae->addRow({ Panel::fill(envAmpFilter.get(), 90) });
        ae->addRow({ kn(K(P::ampA)), kn(K(P::ampD)), kn(K(P::ampS)), kn(K(P::ampR)) });
        ae->setBounds(0, 252, 560, 244);

        pg.addAndMakeVisible(specFilter);  specFilter.setBounds(568, 252, 588, 356);
        pg.addAndMakeVisible(scopeFilter); scopeFilter.setBounds(0, 504, 560, 104);
    }

    // ================================================================ MODULATION
    {
        for (int i = 0; i < 3; ++i)
        {
            auto* l = Pn(3, ("LFO " + juce::String(i + 1)).toRawUTF8());
            l->addRow({ ch(C(P::lfo1Shape + 3 * i)), ch(C(P::lfo1Key + 3 * i)) });
            l->addRow({ kn(K(P::lfo1Rate + 3 * i)) });
            l->setBounds(i * 216, 0, 208, 194);
        }
        auto* st = Pn(3, "STEP MODULATOR");
        st->addRow({ Panel::fill(stepEditor.get(), 100) });
        st->addRow({ kn(K(P::stepRate)), ch(C(P::stepSync), 1.6f), kn(K(P::stepLen)), kn(K(P::stepSmooth)) });
        st->setBounds(0, 202, 640, 250);

        auto* me = Pn(3, "MOD ENVELOPE");
        me->addRow({ Panel::fill(envMod.get(), 90, 2.f), kn(K(P::modEnvA)), kn(K(P::modEnvD)), kn(K(P::modEnvS)), kn(K(P::modEnvR)) });
        me->setBounds(0, 460, 640, 146);

        auto* mx = Pn(3, "MODULATION MATRIX");
        for (int i = 0; i < 8; ++i)
        {
            matrix.push_back(std::make_unique<ctl::MatrixRow>(proc, P::mmSrc1 + 3 * i, P::mmDst1 + 3 * i, P::mmAmt1 + 3 * i));
            mx->addRow({ Panel::fill(matrix.back().get(), 46) });
        }
        mx->setBounds(648, 0, 508, 440);

        auto* mc = Pn(3, "MACROS");
        mc->addRow({ kn(K(P::macro1)), kn(K(P::macro2)), kn(K(P::macro3)), kn(K(P::macro4)) });
        mc->setBounds(648, 448, 508, 146);
    }

    // ================================================================ EFFECTS
    {
        auto& pg = pages[4];
        auto* s = Pn(4, "SATURATION / DISTORTION");
        s->addRow({ ch(C(P::satType)) });
        s->addRow({ kn(K(P::satAmt)), kn(K(P::satMix)) });
        s->setBounds(0, 0, 280, 194);

        auto* m = Pn(4, "MODULATION FX");
        m->addRow({ ch(C(P::mfType)) });
        m->addRow({ kn(K(P::mfRate)), kn(K(P::mfDepth)), kn(K(P::mfFb)), kn(K(P::mfMix)) });
        m->setBounds(288, 0, 420, 194);

        auto* fs = Pn(4, "FREQUENCY SHIFTER");
        fs->addRow({ kn(K(P::fsShift)), kn(K(P::fsMix)) });
        fs->setBounds(716, 0, 240, 194);

        auto* w = Pn(4, "STEREO");
        w->addRow({ kn(K(P::width)) });
        w->setBounds(964, 0, 192, 194);

        auto* d = Pn(4, "DELAY");
        d->addRow({ ch(C(P::dlType)) });
        d->addRow({ kn(K(P::dlTime)), kn(K(P::dlFb)), kn(K(P::dlTone)), kn(K(P::dlMix)) });
        d->setBounds(0, 202, 440, 194);

        auto* r = Pn(4, "REVERB");
        r->addRow({ ch(C(P::rvType)) });
        r->addRow({ kn(K(P::rvSize)), kn(K(P::rvDamp)), kn(K(P::rvMix)) });
        r->setBounds(448, 202, 370, 194);

        auto* dy = Pn(4, "DYNAMICS");
        dy->addRow({ kn(K(P::mbAmt)), kn(K(P::tsAmt)), kn(K(P::cmpAmt)) });
        dy->setBounds(826, 202, 330, 194);

        pg.addAndMakeVisible(specFx);  specFx.setBounds(0, 404, 570, 204);
        pg.addAndMakeVisible(scopeFx); scopeFx.setBounds(578, 404, 578, 204);
    }

    // ================================================================ PRESETS
    {
        pages[5].addAndMakeVisible(*browser);
        browser->setBounds(0, 0, 1156, 608);
    }

    // ================================================================ ADVANCED
    {
        auto* v = Pn(6, "VOICES");
        v->addRow({ ch(C(P::voiceMode)), ch(C(P::stealMode, "Voice Steal")) });
        v->addRow({ kn(K(P::glide)), kn(K(P::bendRange, "Pitch Bend")), kn(K(P::polyphony, "Max Voices")) });
        v->setBounds(0, 0, 520, 194);

        auto* ms = Pn(6, "OUTPUT");
        ms->addRow({ kn(K(P::master)), kn(K(P::width)) });
        ms->setBounds(528, 0, 300, 194);

        bClearMap.onClick = [this] { proc.clearMidiMap(); midiMap->repaint(); for (auto* k : knobs) k->repaint(); };
        auto* mm = Pn(6, "MIDI CC MAP");
        mm->addRow({ Panel::fill(midiMap.get(), 480) });
        mm->addRow({ Panel::fill(&bClearMap, 34) });
        mm->setBounds(836, 0, 320, 608);

        bPanic.onClick = [this] { proc.panic(); };
        bInit.onClick = [this] { proc.setAllToDefault(true); proc.presetName = "Init"; proc.presetCategory = "Init"; };
        hintTools.maxLines = 14;
        auto* t = Pn(6, "TOOLS & STATUS");
        t->addRow({ Panel::fill(&bPanic, 34), Panel::fill(&bInit, 34) });
        t->addRow({ Panel::fill(&hintTools, 250) });
        t->setBounds(0, 202, 828, 406);
    }
}

// ------------------------------------------------------------------------------------------------
void NeoEditor::showPage(int i)
{
    currentPage = juce::jlimit(0, kPages - 1, i);
    for (int k = 0; k < kPages; ++k)
    {
        pages[k].setVisible(k == currentPage);
        tabs[k].setToggleState(k == currentPage, juce::dontSendNotification);
    }
    if (currentPage == 5 && browser) browser->refreshInfo();
}

void NeoEditor::buildBackground()
{
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    background = juce::Image(juce::Image::RGB, W, H, true);
    juce::Graphics g(background);
    using namespace nwc;

    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff010c1e), 0.f, 0.f, juce::Colour(0xff04305c), (float) W * 0.6f, (float) H, false));
    g.fillAll();
    g.setGradientFill(juce::ColourGradient(aqua.withAlpha(0.20f), (float) W * 0.12f, (float) H * 0.28f, aqua.withAlpha(0.f), (float) W * 0.12f + 560.f, (float) H * 0.28f, true));
    g.fillAll();
    g.setGradientFill(juce::ColourGradient(turq.withAlpha(0.16f), (float) W * 0.9f, (float) H * 0.9f, turq.withAlpha(0.f), (float) W * 0.9f - 520.f, (float) H * 0.9f, true));
    g.fillAll();

    // Lichtbalken
    for (int i = 0; i < 4; ++i)
    {
        juce::Path beam;
        const float x = (float) W * (0.1f + 0.25f * (float) i);
        beam.addQuadrilateral(x, 0.f, x + 120.f, 0.f, x - 240.f, (float) H, x - 420.f, (float) H);
        g.setColour(juce::Colours::white.withAlpha(0.022f));
        g.fillPath(beam);
    }
    // Raster
    g.setColour(aqua.withAlpha(0.035f));
    for (int x = 0; x < W; x += 40) g.drawVerticalLine(x, 0.f, (float) H);
    for (int y = 0; y < H; y += 40) g.drawHorizontalLine(y, 0.f, (float) W);
    // Glas-Blasen
    juce::Random r(1999);
    for (int i = 0; i < 14; ++i)
    {
        const float rad = 20.f + r.nextFloat() * 110.f, cx = r.nextFloat() * (float) W, cy = r.nextFloat() * (float) H;
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f), cx - rad * 0.4f, cy - rad * 0.5f, aqua.withAlpha(0.01f), cx + rad, cy + rad, true));
        g.fillEllipse(cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.f);
    }
    // Kopfleiste
    drawGlass(g, { 8.f, 8.f, (float) W - 16.f, 60.f }, 16.f, 1.f);
    drawGlass(g, { 324.f, 16.f, 402.f, 44.f }, 12.f, 1.f);
}

void NeoEditor::paint(juce::Graphics& g)
{
    using namespace nwc;
    g.drawImageAt(background, 0, 0);

    auto tw = [](const juce::Font& f, const juce::String& s) {
        juce::GlyphArrangement ga; ga.addLineOfText(f, s, 0.f, 0.f);
        return ga.getBoundingBox(0, -1, true).getWidth();
    };
    juce::Font logo(juce::FontOptions(34.f, juce::Font::bold));
    g.setFont(logo);
    float x = 26.f;
    const float y = 12.f, h = 40.f;
    auto word = [&](const juce::String& s, juce::Colour c) {
        const float w = tw(logo, s) + 2.f;
        g.setColour(aqua.withAlpha(0.28f));
        g.drawText(s, juce::Rectangle<float>(x + 1.f, y + 1.f, w, h), juce::Justification::centredLeft);
        g.setColour(c);
        g.drawText(s, juce::Rectangle<float>(x, y, w, h), juce::Justification::centredLeft);
        x += w;
    };
    word("NEO", juce::Colours::white);
    word("//", aqua);
    word("WAVE", turq);
    g.setColour(textDim);
    g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    g.drawText("Y2K SYNTHESIZER  -  FUTURE AQUA EDITION", 28, 48, 290, 14, juce::Justification::centredLeft);

    // Voice-/Pegelanzeige
    g.setColour(textDim);
    g.setFont(juce::Font(juce::FontOptions(11.f, juce::Font::bold)));
    g.drawText("VOICES  " + juce::String(proc.activeVoices.load()), 1098, 18, 66, 14, juce::Justification::centredLeft);
    auto bar = juce::Rectangle<float>(1098.f, 38.f, 66.f, 9.f);
    g.setColour(juce::Colour(0xff02101f));
    g.fillRoundedRectangle(bar, 4.f);
    const float lv = juce::jlimit(0.f, 1.f, visLevel * 1.4f);
    g.setGradientFill(juce::ColourGradient(aqua, bar.getX(), 0.f, lv > 0.85f ? juce::Colours::orangered : neon, bar.getRight(), 0.f, false));
    g.fillRoundedRectangle(bar.withWidth(juce::jmax(0.f, bar.getWidth() * lv)), 4.f);
    g.setColour(aqua.withAlpha(0.5f));
    g.drawRoundedRectangle(bar, 4.f, 1.f);

    if (proc.learnParam.load() >= 0)
    {
        g.setColour(neon);
        g.setFont(juce::Font(juce::FontOptions(12.f, juce::Font::bold)));
        g.drawText("MIDI LEARN: move a controller...", 770, 0, 320, 14, juce::Justification::centredRight);
    }
}

void NeoEditor::resized()
{
    buildBackground();
    presetLabel.setBounds(334, 18, 382, 40);
    bPrev.setBounds(292, 21, 34, 34);
    bNext.setBounds(724, 21, 34, 34);
    bAB.setBounds(776, 21, 40, 34);
    bUndo.setBounds(822, 21, 56, 34);
    bRedo.setBounds(882, 21, 56, 34);
    bRand.setBounds(942, 21, 56, 34);
    bSmart.setBounds(1002, 21, 64, 34);
    for (int i = 0; i < kPages; ++i) tabs[i].setBounds(12 + i * 136, 74, 130, 28);
    for (auto& pg : pages) pg.setBounds(12, 108, 1156, 608);
}

// ------------------------------------------------------------------------------------------------
void NeoEditor::timerCallback()
{
    if (proc.presetName != lastPreset || proc.presetCategory != lastCat)
    {
        lastPreset = proc.presetName; lastCat = proc.presetCategory;
        presetLabel.setText(lastPreset, juce::dontSendNotification);
        if (currentPage == 5 && browser) browser->refreshInfo();
    }
    {
        const auto t = proc.abActive == 0 ? "A" : "B";
        if (bAB.getButtonText() != t) bAB.setButtonText(t);
    }

    const int lp = proc.learnParam.load();
    if (lp != lastLearn)
    {
        lastLearn = lp;
        for (auto* k : knobs) if (k->isShowing()) k->repaint();
        if (midiMap) midiMap->repaint();
    }

    visLevel += (proc.level.load() - visLevel) * 0.35f;
    repaint(juce::Rectangle<int>(760, 0, 420, 70));

    // Skulptur
    Sculpture::State s;
    s.level = proc.level.load();
    for (int i = 0; i < 3; ++i) s.lfo[i] = proc.visLfo[i].load();
    s.step = proc.visStep.load();
    s.morph = proc.getParamReal(P::aMorph);
    s.color = proc.getParamReal(P::aColor);
    s.detune = proc.getParamReal(P::uniDetune);
    s.voices = (int) std::lround(proc.getParamReal(P::uniVoices));
    s.active = proc.activeVoices.load();
    s.cutoff = proc.param(P::fCut)->getValue();

    switch (currentPage)
    {
        case 0:
            sculpture.update(s, 1.f / 30.f); sculpture.repaint();
            scopeMain.repaint(); specMain.update(); specMain.repaint();
            break;
        case 1:
        {
            scopeOsc.repaint(); specOsc.update(); specOsc.repaint();
            const int ea = (int) std::lround(proc.getParamReal(P::aEngine)), eb = (int) std::lround(proc.getParamReal(P::bEngine));
            if (ea != lastEngA || eb != lastEngB)
            {
                lastEngA = ea; lastEngB = eb;
                auto hint = [](int e) -> juce::String {
                    switch (e)
                    {
                        case 0: return "Analog (PolyBLEP): Morph/Color depend on the wave - Saw: octave mix, Square: pulse width, Triangle: skew, Sine: fold. Color adds drive.";
                        case 1: return "Wavetable (band-limited): Morph scans the 16 frames of the table, Color bends the phase.";
                        case 2: return "2-operator FM: Morph = modulation index, Color = carrier/modulator ratio.";
                        case 3: return "Phase distortion (CZ style): Morph = distortion amount, Color = resonant sweep.";
                        default: return "Granular: a cloud of overlapping grains reads the table. Morph = frame position, Color = shorter grains + more scatter.";
                    }
                };
                hintA.setText(hint(ea)); hintB.setText(hint(eb));
                auto en = [](ctl::Choice* c, bool on) { if (c->isEnabled() != on) { c->setEnabled(on); c->setAlpha(on ? 1.f : 0.4f); } };
                en(chWaveA, ea == 0); en(chTableA, ea == 1 || ea == 4);
                en(chWaveB, eb == 0); en(chTableB, eb == 1 || eb == 4);
            }
            break;
        }
        case 2:
        {
            scopeFilter.repaint(); specFilter.update(); specFilter.repaint();
            const int ft = (int) std::lround(proc.getParamReal(P::fType));
            if (ft != lastFilter)
            {
                lastFilter = ft;
                static const char* t[] = {
                    "State-variable low-pass, 12 dB/oct.",
                    "Two cascaded state-variable stages, 24 dB/oct low-pass.",
                    "State-variable high-pass.",
                    "State-variable band-pass.",
                    "State-variable notch.",
                    "4-pole ladder with output feedback and saturation.",
                    "3-pole (18 dB) acid filter: high-passed feedback, asymmetric saturation.",
                    "Three parallel band-pass formants.",
                    "Comb filter (delay-line resonator).",
                    "Diode-style ladder: saturation in every stage." };
                hintFilter.setText(t[juce::jlimit(0, 9, ft)]);
            }
            break;
        }
        case 3:
            stepEditor->repaint();
            break;
        case 4:
            scopeFx.repaint(); specFx.update(); specFx.repaint();
            break;
        case 6:
            hintTools.setText("Active voices: " + juce::String(proc.activeVoices.load()) + " / 16      Invalid-sample guard events: "
                              + juce::String(proc.nanEvents.load())
                              + "\n\nMIDI: pitch bend, mod wheel (CC1), aftertouch, sustain pedal (CC64) and all-notes-off are handled. "
                                "Any knob can be mapped to a CC via right-click > MIDI Learn; mappings are stored with the project.\n\n"
                                "Presets: user presets are saved as .nwpreset files in Documents/NEOWAVE/Presets.\n\n"
                                "Panic stops all voices and clears reverb/delay tails.");
            if (midiMap) midiMap->repaint();
            break;
        default: break;
    }
    envAmpMain->repaint(); envAmpFilter->repaint(); envFilt->repaint(); envMod->repaint();
}
