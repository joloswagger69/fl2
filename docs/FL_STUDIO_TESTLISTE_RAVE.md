# FL-Studio-Testliste – RAVE//STATION (Phase 2)

Nicht von mir geprüft (kein Windows/FL). Bitte der Reihe nach abhaken, Abweichungen mit Schritt-Nummer melden.

## Laden und Grundlagen
- [ ] Plugin laden, Editor öffnet mit 5 Tabs (PADS, SEQUENCER, SLICER, GENERATOR, EXPORT). Schliessen/öffnen, Plugin entfernen: kein Absturz.
- [ ] Piano Roll: Noten C1–D#2 (36–51) spielen die 16 Pads. Klang vorhanden, kein Knacken, kein Hängenbleiben.
- [ ] *Host sync* an: FL-Play startet den Sequencer, Stopp stoppt ihn. Tempo ändern während der Wiedergabe: Sequencer folgt, Takt bleibt synchron (Kick auf 1 liegt auf dem FL-Metronom-Klick).
- [ ] *Host sync* aus: *Play* und BPM-Regler arbeiten intern.
- [ ] Loop in FL/Position springen: Sequencer springt mit.

## Pads und Samples
- [ ] Eigenes WAV per Datei-Dialog und per Drag & Drop auf ein Pad laden. Wellenform erscheint, Pad spielt.
- [ ] Start/Ende in der Wellenform ziehen; Volume, Pan, Pitch, Attack, Decay, Reverse hörbar.
- [ ] Filter, Drive, Bitcrush, Stutter, Glitch, Layer jeweils einmal hörbar.
- [ ] Time-Stretch/„Pitch keeps length“: Ergebnis hörbar (Transienten werden weich – bekannt).
- [ ] Kit speichern, anderes Sample laden, Kit wieder laden.

## Sequencer
- [ ] Klick, Ziehen (Velocity), Malen über leere Zellen, Rechtsklick-Menü (Ghost, Roll, Wahrscheinlichkeit, Ratchet).
- [ ] Länge 16/32/64, Taktarten wechseln, Swing und Humanize hörbar.
- [ ] Pattern 1–16 umschalten (Wechsel am Pattern-Ende), Copy/Paste/Clear/Undo.

## Slicer
- [ ] Echten Drum-Break (Amen o. ä.) laden, *Detect transients*: Marker sitzen auf den Schlägen (sonst Empfindlichkeit/Marker nachziehen).
- [ ] Slices anhören (Klick auf Marker), Arrangement umordnen, *Play arrangement*.
- [ ] *Slices > pads*, danach *Slice pattern > current slot*: Pattern spielt den Break nach.

## Generator / EVOLVE
- [ ] Je Genre einmal generieren, Dice mehrfach: Ergebnisse unterscheiden sich, klingen musikalisch nachvollziehbar. Gleicher Seed = gleiches Pattern.
- [ ] Undo stellt das alte Pattern wieder her.
- [ ] EVOLVE: 4 Varianten, jede anhören, Original ist unverändert, *Accept* übernimmt, *Discard* verwirft.

## Export
- [ ] MIDI exportieren, in FL in die Playlist/Piano Roll ziehen: Noten und Länge stimmen.
- [ ] Pattern-WAV (1 und 4 Loops, mit/ohne nahtlos) in FL laden: Länge = ganze Takte, Loop ohne Klick.
- [ ] Stems, Slices, einzelnes Pad: Dateien vorhanden und abspielbar.

## Speichern
- [ ] Projekt speichern, FL schliessen, öffnen: Patterns, Samples, Slicer-Zustand und alle Regler sind wiederhergestellt (Samples spielen).
- [ ] Preset speichern/laden/löschen.
- [ ] Parameter-Automation (z. B. Pad-Volume, Swing) per *Create automation clip*.
- [ ] Puffergrössen 64/256/1024 und Samplerates 44,1/48/96 kHz: kein Knacken, Timing stabil.
- [ ] Mehrere Instanzen gleichzeitig; CPU-Last bei 64 Stimmen im Rahmen.
