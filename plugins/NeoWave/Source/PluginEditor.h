#pragma once
#include "PluginProcessor.h"
#include "GUI/NeoLookAndFeel.h"
#include "GUI/Controls.h"
#include "GUI/Visuals.h"
#include "GUI/PresetBrowser.h"

// Anzeige der aktuellen CC-Zuordnungen (ADVANCED-Seite)
class MidiMapView : public juce::Component
{
public:
    explicit MidiMapView(NeoProcessor& p) : proc(p) {}
    void paint(juce::Graphics& g) override;
private:
    NeoProcessor& proc;
};

class ClickLabel : public juce::Label
{
public:
    std::function<void()> onClick;
    void mouseDown(const juce::MouseEvent&) override { if (onClick) onClick(); }
};

class NeoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NeoEditor(NeoProcessor& p);
    ~NeoEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    enum { kPages = 7 };
    void timerCallback() override;
    void showPage(int i);
    void buildBackground();

    ctl::Knob*    K(int pid, const char* label = nullptr);
    ctl::Choice*  C(int pid, const char* label = nullptr);
    ctl::Panel*   Pn(int page, const char* title);
    void build();
    void layoutPages();

    NeoProcessor& proc;
    NeoLookAndFeel laf;

    // Kopfzeile
    ClickLabel presetLabel;
    juce::TextButton bPrev { "<" }, bNext { ">" }, bAB { "A" }, bUndo { "Undo" }, bRedo { "Redo" }, bRand { "Rand" }, bSmart { "Smart" };
    juce::TextButton tabs[kPages];
    juce::Component pages[kPages];
    int currentPage = 0;

    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<ctl::Panel> panels;
    std::vector<ctl::Knob*> knobs;
    std::vector<ctl::Panel*> pagePanels[kPages];

    // Visuals
    Sculpture sculpture;
    ScopeView scopeMain, scopeOsc, scopeFilter, scopeFx;
    SpectrumView specMain, specOsc, specFilter, specFx;
    std::unique_ptr<EnvView> envAmpMain, envAmpFilter, envFilt, envMod;
    std::unique_ptr<StepEditor> stepEditor;
    std::unique_ptr<PresetBrowser> browser;
    std::unique_ptr<MidiMapView> midiMap;
    ctl::Hint hintA, hintB, hintFilter, hintTools, hintMix;
    juce::TextButton bPanic { "Panic (All Notes Off)" }, bClearMap { "Clear all MIDI mappings" }, bInit { "Init Patch" };
    ctl::Choice *chWaveA = nullptr, *chTableA = nullptr, *chWaveB = nullptr, *chTableB = nullptr;
    std::vector<std::unique_ptr<ctl::MatrixRow>> matrix;

    juce::Image background;
    juce::String lastPreset, lastCat;
    int lastLearn = -2, lastEngA = -1, lastEngB = -1, lastFilter = -1;
    float visLevel = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeoEditor)
};
