#pragma once
#include <nwa/FoundationProcessor.h>

// Stabile Identitaet - nach dem ersten Release NICHT mehr aendern.
inline nwa::PluginInfo makePluginInfo()
{
    nwa::PluginInfo i;
    i.id = "nwa.melodystudio";
    i.name = "MELODY//STUDIO";
    i.instrument = true;
    i.phase = 3;
    i.summary = "chord, melody, bass, arpeggio and acid generators, Make a Track and MIDI file export.";
    return i;
}
