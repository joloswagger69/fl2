# Changelog

## 0.3.0 – Phase 3: MELODY//STUDIO
- MELODY//STUDIO ist jetzt ein vollwertiger Musik-Ideengenerator mit MIDI-Export und einfachem Vorhör-Synth (kein Klangerzeuger für die Produktion).
- Chords (7 Moods, Komplexität, Erweiterungen bis 13, Voicings, Umkehrungen, Voice Leading, Sus/dim/aug, 4 Rhythmen), Melody (5 Charaktere, Dichte, Rhythmik, Wiederholung, Variation, Sprungweite), Arp, Bass (7 Stile), Acid//Flow (5 Charaktere, Slide, Accent, Tie, Wahrscheinlichkeit, 16/32 Schritte).
- Make a Track: 10 Genres, schreibt Tempo, Swing und alle Teil-Einstellungen und erzeugt alles aus einem Seed.
- Jeder Teil lässt sich einzeln neu erzeugen (Generate / New idea / Variation / Clear), Undo, Tonart/Skala/Länge/Taktart werden für alle Teile gemeinsam gehalten.
- Export: einzelne Teile oder alles als .mid (getrennt oder eine Datei mit einer Spur je Teil). Echtzeit-MIDI-Ausgabe an andere Plugins ist **nicht** enthalten.
- Tests: `MelodyTest` (Musiklogik), `MelodyProcessorTest` (Erzeugung, Undo, MIDI-Round-Trip, State, Audio, Fuzz), `MelodyShot` (GUI-Rendering + Button-Checks, nur Linux/xvfb).
- Bekannt: noch nie unter Windows gebaut, nicht in FL Studio getestet, Klang nie gehört (siehe `plugins/MelodyStudio/README.md`).

## 0.2.0 – Phase 2: RAVE//STATION
- RAVE//STATION ist jetzt ein vollwertiges Instrument mit eigener Sample-Engine und internem Sequencer (kein Zugriff auf Playlist-Audio-Clips von FL).
- 16 Pads (WAV/AIFF/FLAC/OGG, Drag & Drop), Pad-Einstellungen, Layer, Kits speichern/laden, mitgelieferter synthetischer Default-Kit.
- Step-Sequencer (16 Muster, 4–64 Schritte, 8 Taktarten, Swing, Humanize, Velocity/Wahrscheinlichkeit/Ratchet/Roll/Ghost/Pitch pro Schritt, Copy/Paste, Undo).
- Break-Slicer (Transienten-Erkennung, manuelle Marker, Arrangement mit Reverse/Pitch/Stretch/Stutter, Slices -> Pads, Slice-Pattern).
- Breakbeat-Generator (8 Genres, Parameter, Seed) und EVOLVE (4 Varianten, Original bleibt unverändert, Variante übernehmen).
- Sample-Morph pro Pad (Reverse, Pitch, Stretch, Filter, Drive, Bitcrush, Stutter, Glitch, Layer, Resample).
- Export: MIDI, Pattern-WAV, Stems, Slices, einzelne Pads.
- Tests: `RaveTest` (Engine), `RaveProcessorTest` (State/Presets/Export/Morph/Generator), `RaveShot` (GUI-Rendering + Button-Checks, nur Linux/xvfb).
- Bekannt: noch nie unter Windows gebaut und nicht in FL Studio getestet (siehe `plugins/RaveStation/README.md`).

## 0.1.0 – Phase 1: technisches Fundament
- Monorepo mit gemeinsamer Core-Bibliothek (`core/`), vier Plugin-Ordnern und eigenem Testwerkzeug.
- Neu: RAVE-STATION, MELODY-STUDIO, SPACE-FX als **Fundament-Builds** (nur Testfunktionen, noch keine Plugin-Features).
- NEO//WAVE als viertes Plugin eingehängt (bestehender Funktionsumfang, unverändert).
- Eindeutige Plugin-Codes und Namen, per CMake erzwungen.
- Windows-x64-CI (MSVC), Linux-Referenz-Build, Release-Job für Tags `v*`.
