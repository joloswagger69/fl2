#pragma once
// Einheitliche, nuechterne Basis fuer alle NEO//WAVE-AUDIO-Plugins.
#include <juce_gui_basics/juce_gui_basics.h>

namespace nwa
{
namespace colours
{
    inline const juce::Colour background { 0xff15181d }, panel { 0xff1e2329 }, outline { 0xff343c46 },
                              text { 0xffe6eaef }, textDim { 0xff8d98a5 }, accent { 0xff33c6e0 }, warn { 0xffe0a233 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle,
                          juce::Slider&) override;
};
} // namespace nwa
