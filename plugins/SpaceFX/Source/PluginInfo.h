#pragma once
#include <nwa/FoundationProcessor.h>

// Stabile Identitaet - nach dem ersten Release NICHT mehr aendern.
inline nwa::PluginInfo makePluginInfo()
{
    nwa::PluginInfo i;
    i.id = "nwa.spacefx";
    i.name = "SPACE//FX";
    i.instrument = false;
    i.phase = 4;
    i.summary = "delay and reverb engines, routing, freeze, ducking and presets.";
    return i;
}
