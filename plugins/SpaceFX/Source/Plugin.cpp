#include "PluginInfo.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new nwa::FoundationProcessor(makePluginInfo());
}
