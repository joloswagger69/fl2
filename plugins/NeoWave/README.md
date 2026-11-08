# NEO//WAVE – Y2K Synthesizer (VST3)

Polyphoner Software-Synthesizer als **VST3-Plugin** für FL Studio (Windows und macOS), geschrieben in C++17 mit JUCE 8 und CMake.
Frutiger-Aero-/Y2K-Oberfläche, 108 Factory-Presets, Modulationsmatrix, Effektkette, Preset-Browser.

> **Ehrlicher Hinweis vorweg**
> Das Plugin wurde unter Linux kompiliert und mit einem eigenen Offline-Testwerkzeug geprüft (siehe „Tests“).
> **Es wurde nicht in FL Studio getestet, nicht mit pluginval validiert und nicht abgehört** – ich kann kein Audio hören.
> Die Preset-Lautstärken sind nach RMS und Spitzenpegel kalibriert, nicht nach Gehör. Ob etwas *gut klingt*, entscheidest du.
> Plane einen ersten Durchlauf ein, in dem du Presets anspielst und gegebenenfalls Parameter oder Presets nachschärfst.

---

## 1. Installation

### Variante A: fertig bauen lassen (empfohlen, kein Compiler nötig)
1. Lege ein neues GitHub-Repository an und lade den Inhalt dieses Ordners hoch (inklusive `.github/`).
2. Öffne im Repository **Actions → Build NEO//WAVE** und warte, bis der Lauf grün ist (ca. 10–20 Minuten, JUCE wird automatisch geladen).
3. Lade unter *Artifacts* `NEOWAVE-VST3-Windows` bzw. `NEOWAVE-VST3-macOS` herunter und entpacke es.
4. Kopiere den Ordner/Bundle **`NEOWAVE.vst3`** an den passenden Ort:
   - **Windows:** `C:\Program Files\Common Files\VST3\`
   - **macOS:** `/Library/Audio/Plug-Ins/VST3/` (oder `~/Library/Audio/Plug-Ins/VST3/`)
5. **FL Studio:** *Options → Manage plugins → Find installed plugins* (Haken bei „Verify plugins“ ist optional), danach in der Plugin-Liste „NEOWAVE“ als Instrument auswählen oder per Plugin-Datenbank (*Add → More plugins*) einfügen.
6. macOS: Das Bundle ist nicht notarisiert. Falls Gatekeeper es blockiert: `xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/NEOWAVE.vst3`

### Variante B: selbst bauen
Voraussetzungen: CMake ≥ 3.22, ein C++17-Compiler (Visual Studio 2022 bzw. Xcode/clang), Internetzugang (JUCE 8.0.4 wird per `FetchContent` geladen).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release          # macOS universal: -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target NeoWave_VST3 --parallel
# Ergebnis: build/NeoWave_artefacts/Release/VST3/NEOWAVE.vst3
```
Zusätzlich existiert ein Standalone-Target (`NeoWave_Standalone`) zum Ausprobieren ohne DAW.

### Lizenzhinweis
JUCE steht unter AGPLv3 bzw. kommerzieller Lizenz. Für private Nutzung ist das unproblematisch; wer das gebaute Plugin weitergibt oder verkauft, muss die JUCE-Lizenzbedingungen erfüllen.

---

## 2. Klangerzeugung

Zwei Oszillatoren (A, B) plus Sub, Noise, Ring- und Cross-FM, danach ein Filter, Verstärker-Hüllkurve und die Effektkette.

| Engine | Morph | Color |
|---|---|---|
| **Analog** (PolyBLEP: Saw, Square, Triangle, Sine) | Saw: Oktav-Mix · Square: Pulsbreite · Triangle: Skew · Sine: Fold | Drive |
| **Wavetable** (bandbegrenzt, 8 Tabellen × 16 Frames) | Frame-Scan | Phase-Bend |
| **FM** (2 Operatoren) | Modulationsindex | Frequenzverhältnis |
| **Phase Distortion** (CZ-Stil) | Verzerrung | resonanter Sweep |
| **Granular** (Wolke aus Hann-Grains über die Wavetable) | Frame-Position | kürzere Grains + Streuung |

Weitere Quellen: **Sub** (Sinus/Rechteck, −1/−2 Oktaven), **Noise** (White, Pink, Crackle, Dust), **Ring Mod**, **Cross FM**, **Hard Sync** (B → A), **Unison bis 16 Stimmen** mit Detune und Stereo-Spread.
Wavetables: Chrome Saw, Aqua Glass, Vox Machine, Neon Bell, Pulse Drift, Cathedral, Harmonic Rise, Static Bloom (per inverser FFT berechnet).

**Filter (10):** LP 12, LP 24, HP, BP, Notch, Ladder, Acid, Formant, Comb, Diode – mit Cutoff, Resonanz, Drive, Key-Tracking und Filter-Hüllkurve.

**Stimmen:** Poly (bis 16 Stimmen), Mono, Legato, Glide, Pitch-Bend-Bereich, Voice-Steal (Smart/Oldest/Lowest/Highest), Sustain-Pedal.

## 3. Modulation
- **Matrix mit 8 Slots**, Quellen: LFO 1–3, Mod-Env, Step-Sequencer, Velocity, Mod-Wheel, Aftertouch, Keytrack, Random, Macro 1–4, Amp-Env, Filter-Env.
  Ziele: Pitch, A/B-Pitch, A/B-Morph, A/B-Color, Sub, Noise, Cutoff, Resonanz, Filter-Drive, Unison-Detune, Amp, Cross-FM, Ring, A/B-Level.
- **3 LFOs** (Sine, Triangle, Saw, Square, S&H, Smooth Random; frei laufend oder Key-Retrigger), **Mod-Hüllkurve**, **16-Step-Modulator** (Länge 2–16, Smooth, frei oder an Host-Tempo gekoppelt).
- **4 Macros** (frei in der Matrix zuweisbar), **MIDI-Learn** für jeden Regler.

## 4. Effekte (feste Reihenfolge)
Saturation (Analog, Distortion, Wavefolder, Bitcrush) → Mod-FX (Chorus, Dimension, Flanger, Phaser) → Frequency Shifter → Delay (Stereo, Ping Pong, Tape) → Reverb (Hall, Shimmer) → Multiband → Transient Shaper → Compressor → Stereo-Width → Master mit DC-Blocker und Soft-Limiter.

## 5. Bedienung
- **Sieben Seiten:** MAIN, OSCILLATORS, FILTER, MODULATION, EFFECTS, PRESETS, ADVANCED.
- **Regler:** Ziehen = ändern, Doppelklick = Standardwert, Rechtsklick = *MIDI Learn / Clear MIDI mapping / Reset to default*. Ein grüner Punkt am Regler zeigt eine CC-Zuordnung.
- **Kopfleiste:** Preset-Name (Klick öffnet den Browser), `<` `>` Preset vor/zurück, **A/B**-Vergleich, **Undo/Redo**, **Rand** (voller Zufall), **Smart** (musikalisch sinnvoller Zufall nach Archetyp: Bass, Lead, Pad, Pluck, Textur).
- **Preset-Browser:** Kategorien, Favoriten (★), Suche, Prev/Next, *Save As…* (Benutzerpresets als `.nwpreset` unter `Dokumente/NEOWAVE/Presets`), Löschen, Init-Patch.
- **ADVANCED:** Voice-Modus, Steal, Pitch-Bend, Max-Stimmen, Liste aller MIDI-CC-Zuordnungen, *Panic*.
- Visualisierung: animierte Glas-Skulptur (reagiert auf Pegel, LFOs, Morph, Color, Unison-Anzahl), Oszilloskop, Spektrum-Analyzer, Hüllkurven- und Step-Anzeigen.
- Alle 133 Parameter sind Host-automatisierbar (FL Studio: Rechtsklick auf Regler → *Create automation clip*).

## 6. Presets
108 Presets, jedes von Hand als Parametersatz definiert (`Source/Presets.cpp`) und auf einheitliche Lautstärke kalibriert:

Init 1 · Y2K Future 10 · Jungle / DnB 10 · Techno 10 · Trance / Progressive 9 · Bass 10 · Leads 10 · Pads / Atmospheres 11 · Plucks / Bells 10 · Keys / Arps 8 · FX / Experimental 11 · Ambient / IDM 8.

## 7. Architektur
```
Source/
  Params.h            Parameter als X-Makro: Enum, Tabelle und Host-Layout kommen aus derselben Liste
  PluginProcessor.*   Stimmenverwaltung, MIDI, Chunk-Verarbeitung (32 Samples), State, A/B, Randomize, Presets laden/speichern
  Presets.*           Factory-Bibliothek (Builder-DSL), Favoriten, Benutzerpreset-Ordner; PresetLevels.inc = kalibrierte Pegel
  DSP/                Tables (Wavetables), DspCore (Hüllkurven, LFO, Filter), Voice (Oszillatoren + Stimme), Effects (Effektkette)
  GUI/                LookAndFeel, Controls (Knob, Choice, Panel, Matrix-Zeile), Visuals, PresetBrowser
  PluginEditor.*      Kopfleiste, sieben Seiten, 30-Hz-Timer für die Anzeigen
Tests/
  RenderTest.cpp      Offline-Test und Pegel-Kalibrierung
  EditorShot.cpp      rendert alle Editor-Seiten offscreen als PNG
```
Echtzeit-Disziplin: Der Audio-Thread alloziert nichts (alle Puffer in `prepare`), Parameter werden als Momentaufnahme pro 32-Sample-Chunk gelesen, Visualisierungsdaten laufen lock-free (Ringpuffer/Atomics). Ein NaN-Wächter fängt ungültige Samples ab und setzt die Effekte zurück (Zähler im ADVANCED-Tab).

## 8. Tests
```bash
cmake --build build --target NeoWaveTest
./build/NeoWaveTest_artefacts/Release/NeoWaveTest                 # alle Presets: NaN, Pegel, Spektrum-Schwerpunkt, CPU
./build/NeoWaveTest_artefacts/Release/NeoWaveTest --state         # Speichern/Laden-Roundtrip
./build/NeoWaveTest_artefacts/Release/NeoWaveTest --random --repeat 100   # Zufalls-Stresstest
./build/NeoWaveTest_artefacts/Release/NeoWaveTest --calibrate     # Pegel neu messen -> Source/PresetLevels.inc (danach neu bauen)
```
Stand beim Ausliefern: 108 Presets, 0 Fehler, schlechtester Preset ≈ 7 % eines Kerns, State-Roundtrip ohne Abweichung, 100 Zufallsläufe ohne ungültige Samples. Der GitHub-Workflow führt diese Tests ebenfalls aus.
`NeoWaveShot <ordner> [presetIndex]` erzeugt PNGs aller Seiten (unter Linux z. B. mit `xvfb-run`).

---

## 9. Bekannte Grenzen (bitte lesen)
- **Nicht in FL Studio / nicht mit pluginval geprüft**, nicht abgehört (siehe oben). Windows- und macOS-Builds laufen erst über die GitHub-Action; ich konnte nur Linux bauen.
- **Kein Import eigener Wavetables**, keine frei zeichenbaren Hüllkurven oder LFO-Kurven.
- Step-Modulator und LFOs laufen frei; der Step-Modulator kann in Notenwerten zum Host-Tempo laufen, ist aber **nicht taktgenau eingerastet**. LFOs haben keinen Tempo-Sync.
- **Kein Oversampling**: Wavefolder, Bitcrush und harte Distortion können bei hohen Tönen hörbar aliasen. Der Ladder-Filter ist bei etwa 0,2 × Samplerate begrenzt.
- Granular ist eine bewusst schlanke 4-Grain-Wolke, kein Sampler-Granulator.
- Der Multiband-Kompressor hat drei Bänder (Trennung bei 200 Hz und 2,5 kHz, Linkwitz-Riley), wird aber über einen einzigen Regler bedient; Schwellen, Verhältnis und Trennfrequenzen sind nicht einzeln einstellbar.
- Die Effektreihenfolge ist fest. Die Oberfläche hat feste Größe (1180 × 720).
- Einige Presets sitzen am oberen Ende des Master-Reglers, weil sie von Natur aus leise sind (z. B. *Digital Dreams*, *Slow Glass*), andere (*Peak Time Stab*, *Metallic Growl*) laufen hörbar in den Limiter. Das ist Geschmackssache – Master-Regler anpassen oder neu kalibrieren.
- Parameter-IDs sind versioniert (Version 1). Spätere Umbenennungen würden alte Projekte brechen; beim Erweitern neue Parameter ans Ende der Liste in `Params.h` setzen.
