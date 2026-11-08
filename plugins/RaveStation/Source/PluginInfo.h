#pragma once
#include <nwa/FoundationProcessor.h>

// Stabile Identitaet - nach dem ersten Release NICHT mehr aendern.
inline nwa::PluginInfo makePluginInfo()
{
    nwa::PluginInfo i;
    i.id = "nwa.ravestation";
    i.name = "RAVE//STATION";
    i.instrument = true;
    i.phase = 2;
    i.summary = "drum machine, step sequencer, break slicer, breakbeat generator, sample morph, EVOLVE and export.";
    return i;
}
