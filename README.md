# NEO//WAVE AUDIO

Produktfamilie aus vier eigenständigen VST3-Plugins für FL Studio (Windows x64 zuerst, macOS später), gebaut mit C++17, JUCE 8.0.4 und CMake.

| Plugin | Rolle | Stand |
|---|---|---|
| **RAVE//STATION** (`RAVE-STATION.vst3`) | Drum Machine, Break Slicer, Breakbeat Generator | Phase 2 – **Funktionsumfang vorhanden, unter Linux getestet, Windows/FL offen** (siehe `plugins/RaveStation/README.md`) |
| **MELODY//STUDIO** (`MELODY-STUDIO.vst3`) | Akkord-, Melodie-, Bass- und Acid-Generator | Phase 3 – **Funktionsumfang vorhanden, unter Linux getestet, Windows/FL offen** (siehe `plugins/MelodyStudio/README.md`) |
| **SPACE//FX** (`SPACE-FX.vst3`) | Delay + Reverb | Phase 4 – **nur Fundament**, keine Funktionen |
| **NEO//WAVE** (`NEOWAVE.vst3`) | Synthesizer | Funktionsumfang vorhanden (siehe `plugins/NeoWave/README.md`), Abgleich mit der Spezifikation in Phase 5 |

## Stand Phase 1 (Fundament; weiterhin gültig für SPACE//FX)

**Vorhanden und automatisch getestet (Linux):**
- Monorepo-Struktur, ein Build für alle Plugins, gemeinsames Fundament `core/` (State, Presets, Hilfsfunktionen, Basis-GUI, Plugin-Grundgerüst).
- Die drei neuen Plugins sind **ehrlich gekennzeichnete Fundament-Builds**. Sie haben nur Funktionen, die tatsächlich arbeiten: ein Gain-Regler, bei Instrumenten ein Sinus-Testton auf MIDI-Noten, bei SPACE-FX Durchleitung mit Gain, State-Speichern/-Laden, Benutzerpresets, Anzeige von Host-Tempo und Transport. Der Editor sagt das ausdrücklich.
- Eindeutige Identität je Plugin (Plugin-Code, Name, VST3-Klassen-ID); CMake bricht bei Doppelvergabe ab.
- `FoundationTest` (1397 Prüfungen): Mono/Stereo, 6 Samplerates (22,05–192 kHz), 10 Puffergrössen (1–4096), Null-Samples, NaN/Inf-Eingang, MIDI-Müll, kaputte/fremde/manipulierte States, Presets inkl. bösartiger Dateinamen (`../..`, `CON`, 500 Zeichen), Editor mehrfach öffnen/schliessen.
- Alle vier VST3-Bundles bauen unter Linux (GCC 13) und haben verschiedene Klassen-IDs.
- GitHub-Actions-Workflow für Windows x64 (MSVC), Linux-Referenz, Release-Job (zip pro Plugin) bei Tags `v*`.

**NICHT geprüft – bitte beachten:**
- **Kein Windows-Build lief bisher.** Der Workflow ist geschrieben, aber nie ausgeführt. MSVC kann Fehler zeigen, die GCC nicht zeigt. Erst der erste grüne Lauf belegt, dass der Windows-Build funktioniert.
- **Nicht in FL Studio getestet** (Scan, Laden, Tempo, Automation, Projekt-Wiederherstellung). Dafür gibt es `docs/FL_STUDIO_TESTLISTE.md`.
- Nicht mit pluginval validiert.
- Klang wurde nie gehört; das Fundament erzeugt ohnehin nur einen Testton.
- **Nicht vorhanden:** Sämtliche Funktionen von SPACE//FX (Phase 4). RAVE//STATION: siehe eigenes README.
- NEO//WAVE nutzt `core/` noch nicht; es ist ein unverändert eingehängtes Plugin.

Plan, Architektur und Risiken: `docs/PLAN.md`.

## Installation (Windows)
1. Repository auf GitHub hochladen, unter *Actions → Build* den Lauf abwarten (ca. 15–30 Minuten, JUCE wird automatisch geladen).
2. Pro Plugin das Artefakt `…-win64-vst3` laden und entpacken. Es enthält einen Ordner `NAME.vst3`.
3. Die `.vst3`-Ordner nach `C:\Program Files\Common Files\VST3\` kopieren.
4. FL Studio: *Options → Manage plugins → Find installed plugins*.
5. Bei Tags `v0.1.0` usw. erzeugt der Release-Job zusätzlich ZIP-Dateien unter *Releases*. Ein Tag muss zur Version in `CMakeLists.txt` passen.

## Selbst bauen
Voraussetzungen: CMake ≥ 3.22, Visual Studio 2022 oder neuer (Desktop-C++) bzw. GCC/Clang, Git, Internet (JUCE 8.0.4 per `FetchContent`).

```bash
cmake -B build -A x64                                      # Windows (nimmt die installierte Visual-Studio-Version)
cmake -B build -DCMAKE_BUILD_TYPE=Release                  # Linux/macOS
cmake --build build --config Release --parallel --target RaveStation_VST3 MelodyStudio_VST3 SpaceFX_VST3 NeoWave_VST3
cmake --build build --config Release --parallel --target FoundationTest NeoWaveTest RaveTest RaveProcessorTest
ctest --test-dir build -C Release --output-on-failure
```
Ergebnis: `build/plugins/<Plugin>/<Plugin>_artefacts/Release/VST3/<NAME>.vst3`.
Mit lokalem JUCE-Checkout (offline): `-DNWA_JUCE_SOURCE_DIR=/pfad/zu/JUCE`.
Unter Linux brauchen die Tests/Editor-Schnappschüsse ein X-Display (`xvfb-run`).

## Abhängigkeiten
| Abhängigkeit | Version | Zweck |
|---|---|---|
| JUCE | 8.0.4 (per FetchContent) | Plugin-Framework, GUI, VST3-Wrapper |
| CMake | ≥ 3.22 | Build |
| C++17-Compiler | MSVC 2022 / GCC 13 / Clang | |
| Linux zusätzlich | ALSA, X11, Freetype, Fontconfig, GL (siehe Workflow) | JUCE-Systembibliotheken |

Keine weiteren externen Bibliotheken. Lizenz: JUCE steht unter AGPLv3 bzw. kommerzieller Lizenz; wer die gebauten Plugins weitergibt, muss das beachten.

## Struktur
```
core/        gemeinsames Fundament (INTERFACE-Bibliothek nwa_core, wird in jedes Plugin einkompiliert)
plugins/     RaveStation/  MelodyStudio/  SpaceFX/  NeoWave/   (je eigene CMake-Datei und eigene IDs)
tests/       FoundationTest
docs/        PLAN.md, FL_STUDIO_TESTLISTE.md
```

## Identität (nach erstem Release eingefroren)
| Plugin | Datei | Plugin-Code | Hersteller | State-ID |
|---|---|---|---|---|
| RAVE//STATION | RAVE-STATION.vst3 | Rvs1 | Neow | nwa.ravestation |
| MELODY//STUDIO | MELODY-STUDIO.vst3 | Mds1 | Neow | nwa.melodystudio |
| SPACE//FX | SPACE-FX.vst3 | Spf1 | Neow | nwa.spacefx |
| NEO//WAVE | NEOWAVE.vst3 | Nwv1 | Neow | (eigenes Format) |

Das „//“ aus den Anzeigenamen ist in Dateinamen nicht erlaubt, deshalb Bindestrich im Dateinamen.
