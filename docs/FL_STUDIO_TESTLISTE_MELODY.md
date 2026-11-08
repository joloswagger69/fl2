# FL-Studio-Testliste – MELODY//STUDIO (Phase 3)

Nicht von mir geprüft (kein Windows/FL). Bitte der Reihe nach abhaken, Abweichungen mit Schritt-Nummer melden.

## Laden und Grundlagen
- [ ] Plugin laden, Editor öffnet mit 6 Tabs (CHORDS, MELODY, BASS, ACID, MAKE A TRACK, EXPORT). Schliessen/öffnen, Plugin entfernen: kein Absturz.
- [ ] Beim Laden sind auf CHORDS/MELODY/BASS bereits Noten zu sehen (erste Idee).
- [ ] *Host sync* an: FL-Play startet die Vorschau, Stopp stoppt (Ausklingen ist ok). Tempo in FL ändern: Vorschau folgt. Loop/Position springen: kein Hängenbleiben von Tönen.
- [ ] *Host sync* aus: *Play* und BPM-Regler arbeiten intern. Volume-Regler wirkt.
- [ ] Vorschau ist hörbar, kein Knacken, kein Übersteuern. (Klang ist bewusst einfach.)

## Erzeugen
- [ ] CHORDS: *New idea* mehrmals – Akkorde wechseln, passen zur Tonart. Mood wechseln, *Generate*: hörbar anderer Charakter. Voicing/Inversion/Complexity ändern, *Generate*.
- [ ] MELODY: Charaktere durchprobieren, Dichte/Wiederholung/Sprungweite wirken. *Variation* verändert nur leicht. ARP: Muster/Rate/Gate/Oktaven.
- [ ] BASS: 7 Stile, Oktave, Annäherungstöne. ACID: *New idea*, Slide/Accent/Tie/Probability hörbar bzw. in der Anzeige (helle Umrandung = Accent).
- [ ] *New idea* in einem Teil verändert die anderen Teile nicht. *Undo* (Kopfbereich) stellt den vorherigen Stand wieder her.
- [ ] Tonart/Skala/Länge/Taktart ändern: gelber Hinweis erscheint; *Generate* erzeugt alles passend neu.
- [ ] *Mute* je Teil schaltet die Vorschau stumm.
- [ ] Taktarten 3/4, 5/4, 6/8 und Längen 1/8/16 Takte: Anzeige und Wiedergabe laufen im Loop sauber.

## Make a Track
- [ ] Je Genre einmal *Make a track* (mit und ohne Acid): Tempo und Swing ändern sich, alle Teile werden gefüllt. *New idea* gibt ein anderes Ergebnis. Gleicher Seed (Preset laden) = gleiche Noten.

## Export
- [ ] Jeden Teil als .mid exportieren und in FL ins Piano Roll ziehen: Noten, Länge, Tempo stimmen.
- [ ] *All parts as ONE .mid*: in FL importieren – eine Spur je Teil.
- [ ] *All parts as separate files*: Ordner enthält eine Datei je nicht-leerem Teil.
- [ ] Acid-Slides: in FL als überlappende Noten (kein Slide-Flag – bekannt).

## Speichern
- [ ] Preset speichern/laden (Noten sind enthalten). Projekt speichern, FL schliessen, öffnen: Noten und Regler sind wiederhergestellt, Vorschau klingt wie vorher.
