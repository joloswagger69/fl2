#pragma once
#include "Widgets.h"
#include "../RaveProcessor.h"

namespace rave::gui
{
struct Ctx
{
    RaveProcessor& proc;
    std::function<void(const juce::String&, bool)> status;                                              // Meldung, isWarning
    std::function<void(const juce::String& title, const juce::String& initial, std::function<void(juce::String)>)> prompt;
    std::function<void(int pad)> selectPad;                                                              // fuer Seiten-uebergreifende Auswahl
    int selectedPad = 0;
    int stepClipboardValid = 0;
};

class PageBase : public juce::Component
{
public:
    explicit PageBase(Ctx& c) : ctx(c), proc(c.proc) {}
    virtual void refresh() {}                  // ~20 Hz, nur leichte Arbeit
    virtual void onShown() {}
protected:
    Ctx& ctx; RaveProcessor& proc;
    juce::TextButton* makeButton(juce::TextButton& b, const juce::String& text, std::function<void()> cb)
    {
        b.setButtonText(text); b.onClick = std::move(cb); addAndMakeVisible(b); return &b;
    }
    static juce::String wildcard()
    {
        static juce::String w = []() { juce::AudioFormatManager m; m.registerBasicFormats(); return m.getWildcardForAllFormats(); }();
        return w;
    }
};
} // namespace rave::gui
