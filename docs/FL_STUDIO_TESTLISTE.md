# FL-Studio-Testliste (Windows) – Phase 1

Diese Punkte konnte ich **nicht** selbst prüfen (kein Windows, kein FL Studio in meiner Umgebung).
Bitte nach dem ersten erfolgreichen GitHub-Actions-Lauf der Reihe nach abhaken und Abweichungen melden.

## Installation und Scan
- [ ] Artefakte der vier Plugins von GitHub Actions laden und entpacken (jeweils ein Ordner `NAME.vst3`).
- [ ] Die vier `.vst3`-Ordner nach `C:\Program Files\Common Files\VST3\` kopieren (Administratorrechte nötig).
- [ ] FL Studio: *Options → Manage plugins → Find installed plugins* (mit „Verify plugins“).
- [ ] Alle vier erscheinen mit eigenem Namen (RAVE-STATION, MELODY-STUDIO, SPACE-FX, NEOWAVE), keiner überschreibt einen anderen.
- [ ] RAVE-STATION, MELODY-STUDIO, NEOWAVE sind als Instrument, SPACE-FX als Effekt einsortiert.

## Laden und Verhalten (je Plugin)
- [ ] Plugin laden, Editor öffnen, Editor schliessen und erneut öffnen, Plugin entfernen: kein Absturz.
- [ ] Instrument: MIDI-Noten (Piano Roll) erzeugen den Test-Sinuston (nur Foundation-Plugins). Effekt: Audio läuft durch, Gain wirkt.
- [ ] Host-Tempo im Editor entspricht dem FL-Tempo; Transport zeigt „playing“/„stopped“ passend zu Play/Stop. (Foundation-Plugins zeigen das an.)
- [ ] Tempo während der Wiedergabe ändern: Anzeige folgt.
- [ ] Gain automatisieren (Rechtsklick auf Regler → *Create automation clip*), Wiedergabe: Regler bewegt sich, kein Knacken.
- [ ] Verschiedene Puffergrössen (FL: *Options → Audio settings*, z. B. 64/256/1024/2048 Samples) und Samplerates (44,1/48/96 kHz).
- [ ] Mono-Spur / Stereo-Spur (bei SPACE-FX Mixer-Insert).

## Speichern
- [ ] Regler verstellen, Projekt speichern, FL schliessen, Projekt öffnen: Werte sind wiederhergestellt.
- [ ] Benutzerpreset speichern (Save…), anderen Wert einstellen, Preset über die Auswahlliste laden: Wert wird wiederhergestellt.
- [ ] Presets liegen unter `Dokumente\NEOWAVE_AUDIO\<Plugin>\Presets` (NEOWAVE: `Dokumente\NEOWAVE\Presets`).

Ergebnisse bitte als Liste zurückmelden. Fehlgeschlagene Punkte haben Vorrang vor Phase 2.
