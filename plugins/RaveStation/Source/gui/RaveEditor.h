#pragma once
#include "../RaveProcessor.h"

namespace rave::gui
{
juce::AudioProcessorEditor* createRaveEditor(RaveProcessor& p);
}
