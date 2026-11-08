# RAVE//STATION (Phase 2)

VST3-Instrument für FL Studio: Drum Machine + Step-Sequencer + Break-Slicer + Breakbeat-Generator + Sample-Morph + Export.

**Wichtig:** RAVE//STATION hat eine **eigene Sample-Engine und einen eigenen Sequencer**. Es greift **nicht** auf Audio-Clips in der FL-Playlist zu und verändert sie nicht. Samples lädst du ins Plugin (Datei-Dialog oder Drag & Drop auf ein Pad / in den Slicer).

## Was es kann (alles in dieser Fassung vorhanden)
- **Pads:** 16 Pads, WAV/AIFF/FLAC/OGG, pro Pad Lautstärke, Pan, Pitch, Attack, Decay, Start/Ende (auch per Ziehen in der Wellenform), Reverse, Mute, Solo, Tiefpass/Hochpass/Resonanz, Drive, Bitcrush, Time-Stretch, Layer (zweites Sample), Stutter, Glitch, Generator-Rolle. 64 Stimmen, älteste wird gestohlen. MIDI-Noten 36–51 (C1–D#2) spielen die Pads live. Kits speichern/laden (Ordner unter `Dokumente\NEOWAVE_AUDIO\RAVE-STATION\Kits`).
- **Sequencer:** 16 Pattern-Plätze (1–16) + 4 EVOLVE-Varianten (V1–V4). 4–64 Schritte, Raster 1/8, 1/16, 1/32, Taktarten 2/4 bis 12/8, Swing, Humanize (Timing/Velocity), pro Schritt Velocity, Wahrscheinlichkeit, Ratchet (1–8), Roll, Ghost-Note, Pitch. Step- und Pattern-Copy/Paste, Undo (40 Schritte). Tempo folgt FL (Host Sync) oder intern (BPM + Play).
- **Slicer:** Break laden (oder synthetischen Demo-Break), Transienten-Erkennung mit Empfindlichkeit, Marker setzen/verschieben/löschen (rastet auf Nulldurchgang), gleichmässige Teilung, Loop-Ende, Arrangement der Slices (Reihenfolge, Reverse, Pitch, Stretch, Stutter, Duplizieren, Shuffle). Slices → Pads, Arrangement → Pads / ein Pad, Slice-Pattern ins aktuelle Pattern schreiben.
- **Generator:** 8 Genres (Jungle, Drum & Bass, Breakbeat, UK Garage, Oldschool Rave, Hardcore, Techno, Experimental) mit Intensität, Komplexität, Dichte, Ghost-Notes, Variation, Zufälligkeit, Swing, Tempo, Seed (reproduzierbar). **Überschreibt das aktuelle Pattern** (Undo vorhanden).
- **EVOLVE:** erzeugt 4 Varianten des Patterns in V1–V4. Das Original bleibt unverändert. Varianten anhören, eine übernehmen oder alle verwerfen.
- **Export:** MIDI (importierbar in FL), Pattern als WAV (1–8 Loops, optional nahtlos), Stems (eine WAV pro Pad), Slices als einzelne WAVs, einzelnes Pad (mit Einstellungen). Presets speichern auch Patterns, Samples und Slicer-Zustand.

## Bedienung in Kürze
Klick auf Zelle = Schritt an/aus · vertikal ziehen = Velocity · Rechtsklick auf Schritt = Ghost/Roll/Wahrscheinlichkeit/Ratchet/Kopieren · Rechtsklick auf Zeilenname = Zeilenwerkzeuge · Slicer: Klick in Wellenform = Marker, Rechtsklick auf Marker = löschen, gestrichelte Linie = Loop-Ende.

## Technische Eckdaten
- 372 Host-Parameter (pro Pad 22 + Globale); State-ID `nwa.ravestation`, State-Version 1, Plugin-Code `Rvs1`.
- Audio-Thread ohne Locks/Allokation; Samples kommen über eine lock-freie Mailbox, Stretch/Pitch-Rendering läuft in einem Worker-Thread.
- Der Default-Kit ist **synthetisch erzeugt** (keine Aufnahmen, keine Urheberrechtsfragen), klingt entsprechend einfach.

## Getestet (automatisch, Linux, GCC 13)
`ctest`: `rave_engine` (Stimmen, blockunabhängiges Timing, Swing, Ratchet, Wahrscheinlichkeit, Mute/Solo, Pattern-Wechsel, MIDI-Trigger, Polyphonie-Stresstest, Slicer-Genauigkeit auf synthetischem Break, Generator-Musikalität, EVOLVE), `rave_processor` (State/Presets, Kits, Worker, MIDI-Round-Trip, WAV/Stems/Slices, Hörbarkeit und Reproduzierbarkeit jedes Morph-Parameters), `rave_gui` (alle 5 Seiten offscreen gerendert + Button-Checks: Generieren, EVOLVE lässt Original unverändert, Undo, Slicer, Kit).

## NICHT geprüft / bekannte Grenzen – bitte lesen
- **Klang nie gehört.** Alle Aussagen zu „klingt gut“ fehlen bewusst. Timing, Pegel und Reproduzierbarkeit sind gemessen, der Höreindruck nicht.
- **Kein Windows-Build, nicht in FL Studio getestet** (Phase 2). Siehe `docs/FL_STUDIO_TESTLISTE_RAVE.md`. Nicht mit pluginval validiert.
- **GUI nur per Rendering und Button-Auslösung geprüft**, nicht mit echter Maus (Drag, Rechtsklick-Menüs, Datei-Dialoge, Drag & Drop) und nicht in FL.
- **Time-Stretch (WSOLA) verschmiert Transienten** und trifft den Faktor nur ungefähr (gemessen ≈1,75× bei Wunsch 2×, per Schwellwert). Für Drums eher ein Effekt als ein sauberer Stretch.
- **Slicer-Erkennung nur an einem synthetischen Break validiert**; bei echten Breaks sind Marker oft nachzubessern. Nur die ersten 16 Slices passen auf Pads.
- **Choke-Gruppen** existieren in Engine und Parametern, haben aber **keinen Regler im Editor** (nur über die Host-Parameterliste).
- **MIDI-Export:** Step-Pitch und Humanize werden nicht geschrieben; Schritte mit Wahrscheinlichkeit < 100 % werden als normale Noten geschrieben, ausser die Option „roll step probability“ ist aktiv.
- **Projektgrösse:** Samples liegen im Plugin-State (24-Bit-FLAC, Base64). Der Default-Kit macht ca. 564 KB; grosse Breaks vergrössern FL-Projekte entsprechend.
- Kein Drag-out von MIDI/Audio aus dem Plugin in FL (nicht implementiert) – Export in Datei, dann in FL ziehen.
- Nicht in dieser Phase: Sampler-Zeitlupe/Granular, mehr als 16 Pads, Mehrkanal-Ausgänge (ein Stereo-Ausgang).
