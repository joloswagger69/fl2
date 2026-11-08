#pragma once
#include "../MelodyProcessor.h"

namespace mel::gui
{
juce::AudioProcessorEditor* createMelodyEditor(MelodyProcessor& p);
}
