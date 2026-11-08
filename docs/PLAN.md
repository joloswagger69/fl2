# Entwicklungsplan NEO//WAVE AUDIO

Stand: Phase 1 (technisches Fundament). Dieses Dokument wird pro Phase fortgeschrieben.

## 1. Analyse des Ausgangszustands

Vorhanden war genau ein Projekt: **NEO//WAVE** (Synthesizer, JUCE 8.0.4, CMake, VST3 + Standalone, 108 Presets, 133 Parameter, eigenes Offline-Testwerkzeug, Linux-Build getestet). Es ist ein eigenständiges Projekt mit eigenem `FetchContent`; es teilt noch keinen Code mit anderen Plugins.
Die anderen drei Plugins (RAVE-STATION, MELODY-STUDIO, SPACE-FX) existierten nicht.

Konsequenz für die Reihenfolge: Die Master-Vorgabe sieht NEO//WAVE als letztes Plugin vor (Phase 5). Es ist aber schon weit fortgeschritten. Es wird deshalb **unverändert als viertes Plugin eingehängt** und erst in Phase 5 an das gemeinsame Fundament angeglichen und gegen die Spezifikation geprüft (siehe Abschnitt 5, „Abweichungen NEO//WAVE“).

## 2. Architektur

Monorepo mit einem Build, mehreren eigenständigen Plugins:

```
CMakeLists.txt             JUCE 8.0.4 (FetchContent), Compiler-Flags, Registrierung der Plugins
core/                      gemeinsames Fundament (INTERFACE-Bibliothek `nwa_core`, Quellen werden pro Plugin mitkompiliert)
  include/nwa/             Versionsinfo, dB-/Sanitize-Helfer, State-Format, Preset-Manager, LookAndFeel,
                           FoundationProcessor/-Editor (Plugin-Grundgerüst mit Testfunktionen)
  cmake/NwaPlugin.cmake    Prüft Eindeutigkeit von Plugin-Code und Produktname
plugins/
  RaveStation/ MelodyStudio/ SpaceFX/   je eigener Ordner, eigene CMake-Datei, eigene IDs
  NeoWave/                              bestehendes Projekt
tests/FoundationTest.cpp   automatische Tests des Fundaments
docs/                      Plan, Architektur
```

Warum eine INTERFACE-Bibliothek statt einer statischen Bibliothek: JUCE-Module werden pro Zielprojekt kompiliert. Eine statische Bibliothek, die JUCE-Module enthält, führt zu doppelten Modul-Objekten (ODR-Risiko). Mit einer INTERFACE-Bibliothek wird der gemeinsame Quellcode in jedes Plugin einkompiliert; die Plugins bleiben vollständig eigenständig (keine gemeinsamen DLLs, nichts zu installieren ausser der VST3-Datei).

### Gemeinsame Bausteine (core)
| Baustein | Aufgabe |
|---|---|
| `StateIO` | Versioniertes Zustandsformat (XML in `copyXmlToBinary`). Prüft Plugin-ID und Version, lehnt fremde/kaputte Daten ab, ohne den aktuellen Zustand zu zerstören. |
| `PresetManager` | Benutzerpresets als Dateien (`Dokumente/NEOWAVE_AUDIO/<Plugin>/Presets`), atomares Schreiben, Namensbereinigung (Windows-reservierte Namen, Pfadzeichen), nur im Message-Thread. |
| `Sanitize` / `Db` | NaN/Inf-Schutz, dB-Umrechnung. |
| `FoundationProcessor` | Plugin-Grundgerüst: Bus-Layouts (Mono/Stereo), Parameter-Layout, State, Host-Tempo/Transport-Auslesung, Prüffunktion (Gain bzw. Testton). |
| `FoundationEditor` | Minimaler Editor mit Preset-Leiste und Host-Anzeige, ehrlich als Fundament gekennzeichnet. |
| `LookAndFeel` | Einheitliche, nüchterne Basis für alle Plugin-GUIs. |

## 3. Plugin-Identität (Konflikte vermeiden)

| Plugin | Produktname (Dateiname) | Plugin-Code | Hersteller-Code | Typ |
|---|---|---|---|---|
| RAVE//STATION | `RAVE-STATION.vst3` | `Rvs1` | `Neow` | Instrument |
| MELODY//STUDIO | `MELODY-STUDIO.vst3` | `Mds1` | `Neow` | Instrument (MIDI-Generator) |
| SPACE//FX | `SPACE-FX.vst3` | `Spf1` | `Neow` | Effekt |
| NEO//WAVE | `NEOWAVE.vst3` | `Nwv1` | `Neow` | Instrument |

Das „//“ im Anzeigenamen ist in Dateinamen nicht erlaubt, deshalb heissen die Dateien mit Bindestrich. Die VST3-Klassen-IDs werden von JUCE aus Hersteller-Code, Plugin-Code und Name abgeleitet und sind damit pro Plugin verschieden. CMake bricht ab, wenn zwei Plugins denselben Code oder Produktnamen bekommen.
**Wichtig:** Plugin-Code und Parameter-IDs sind nach dem ersten Release eingefroren, sonst laden alte FL-Studio-Projekte nicht mehr korrekt.

## 4. Technische Risiken (früh erkannt)

| Risiko | Einschätzung | Gegenmassnahme |
|---|---|---|
| **Windows/FL Studio nicht lokal testbar** | Hoch. Die Entwicklungsumgebung ist Linux. | Windows-Build läuft ausschliesslich in GitHub Actions (MSVC, x64). Foundation-Test läuft dort ebenfalls. FL-Studio-Tests muss der Nutzer anhand einer Checkliste durchführen (README). Nichts davon wird als getestet ausgegeben. |
| MSVC-Compilerfehler, die GCC nicht zeigt | Mittel | `/utf-8`, `/permissive-`, gleiche Warnstufe; der erste CI-Lauf entscheidet. |
| Sample-Import/Streaming (RAVE-STATION) | Mittel | Dateien im Message-Thread dekodieren, fertig im Speicher an den Audio-Thread übergeben (lock-free Austausch). Ab Phase 2. |
| Transientenerkennung | Mittel | Spectral-Flux mit adaptiver Schwelle, an echtem Material zu prüfen. Ich habe hier keine echten Breaks; Tests mit synthetischen Signalen. Ab Phase 2. |
| MIDI-Export-Kompatibilität (MELODY-STUDIO) | Niedrig | Standard-MIDI-File Typ 1 über `juce::MidiFile`; Import in FL muss der Nutzer prüfen. |
| Real-time-MIDI-Ausgabe in FL Studio | Hoch/unklar | Wird nicht umgesetzt, solange nicht in FL getestet. Datei-Export hat Vorrang (Vorgabe). |
| Reverb-Qualität (SPACE-FX) | Mittel | FDN-Architektur mit Diffusion; Klangqualität ohne Gehör nicht beurteilbar. Es werden nur messbare Eigenschaften geprüft (Stabilität, Abklingzeit, Pegel). |
| Host-Tempo-Sync | Niedrig | `AudioPlayHead` ist bereits im Fundament angebunden und in der GUI sichtbar, damit der Nutzer in FL sofort prüfen kann. |
| Lizenz | Mittel | JUCE steht unter AGPLv3 bzw. kommerzieller Lizenz. |

## 5. Phasen

| Phase | Inhalt | Status |
|---|---|---|
| 1 | Fundament: Struktur, CMake, `core`, Plugin-Grundgerüste, Tests, Windows-CI | **dieses Release** |
| 2 | RAVE//STATION | offen |
| 3 | MELODY//STUDIO | offen |
| 4 | SPACE//FX | offen |
| 5 | NEO//WAVE angleichen und gegen Spezifikation prüfen | offen (Plugin existiert) |
| 6 | Qualitätssicherung | offen |

### Abweichungen NEO//WAVE gegenüber der Master-Spezifikation (für Phase 5)
Das bestehende Plugin deckt Engines, Oszillatoren, Filter, Modulation, Effekte und Presets weitgehend ab, aber nicht alles wörtlich:
- **Effekte:** Es gibt keinen schaltbaren EQ und keine einzeln ein-/ausschaltbaren Effekte (jeder Effekt hat Mischung/Menge, bei 0 ist er praktisch aus).
- **Noise Engine** ist als Noise-Quelle vorhanden, nicht als eigene Engine.
- **Presets:** 108 vorhanden; Kategorien weichen von der Liste ab (Acid und Rave fehlen als eigene Kategorien, dafür Y2K, Keys/Arps, Ambient/IDM).
- Design: das Plugin ist deutlich verspielter als die Spezifikation verlangt (Animationen); Skalierbarkeit der GUI fehlt (feste Grösse).
- Tooltips fehlen.

## 6. Definition of Done pro Phase
Kompiliert ohne Fehler (Linux und Windows-CI) · automatische Tests grün · jede sichtbare Funktion hat eine echte Wirkung · alle ungetesteten Punkte sind im README ausdrücklich genannt.
