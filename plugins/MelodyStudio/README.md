# MELODY//STUDIO (Phase 3)

VST3-Instrument für FL Studio: erzeugt **Akkorde, Melodien, Arpeggios, Bass-Linien und Acid-Pattern** und exportiert sie als **MIDI**. Der eingebaute Synth dient nur zum **Vorhören** der Ideen (einfacher Klang); die eigentliche Musik entsteht, wenn du die MIDI-Dateien in FL in dein Instrument ziehst.

**Wichtig:** Es gibt **keine Echtzeit-MIDI-Ausgabe** an andere Plugins (das habe ich nicht getestet und deshalb nicht eingebaut). Der Weg ist: erzeugen → Export-Tab → .mid in FL ziehen.

## Schnellstart
Beim Laden ist bereits eine erste Idee da (A-Moll, 4 Takte, Akkorde + Melodie + Bass + Arp). *Play* drücken oder *Host sync* an lassen und in FL abspielen. Alternativ: Tab **MAKE A TRACK** → Genre wählen → *Make a track*.

## Was es kann
- **CHORDS:** 7 Moods (Happy, Uplifting, Energetic, Dreamy, Melancholic, Dark, Tense), Komplexität, max. Erweiterung (Dreiklang bis 13), Voicing (Close, Drop 2, Open, Spread), Umkehrung, Voice Leading, Akkorde pro Takt, 4 Rhythmen, Sus, dim/aug. Akkorde bleiben in der Tonart (mit gezielten Ausnahmen wie Zwischendominanten ab hoher Komplexität).
- **MELODY:** 5 Charaktere (Melody, Lead, Hook, Bass melody, Atmospheric), Dichte, rhythmische Komplexität, Wiederholung, Variation, maximale Sprungweite, Oktave, „folgt den Akkorden“. Betonte Zählzeiten und Phrasenenden landen auf Akkordtönen (gemessen: 100 % in 960 Testläufen). **ARP** (5 Muster, Rate 1/4–1/8-Triole, Gate, Oktaven) ebenfalls auf dieser Seite.
- **BASS:** 7 Stile (Root, 8tel, Offbeat, Synkopen, Walking, Oktav-Bounce, Rolling 16tel), Dichte, Oktave, Annäherungstöne.
- **ACID:** 5 Charaktere, Dichte, Slide, Accent, Tie, Oktavsprünge, Notenwahrscheinlichkeit, Gate, Pattern-Variation, Tonhöhen-Variation, 16 oder 32 Schritte, Basisoktave.
- **MAKE A TRACK:** 10 Genres (House, Techno, Trance, Synthwave, Pop, Hip-Hop, Ambient, Drum & Bass, Garage, Rave) × Mood. Das Genre schreibt Tempo, Swing und die Einstellungen aller Teile in die Regler und erzeugt alles aus einem Seed (reproduzierbar). Acid optional.
- **Pro Teil:** *Generate* (gleiche Einstellungen, gleicher Seed), *New idea* (neuer Seed), *Variation* (leichte Änderung des vorhandenen Teils), *Clear*, *Mute* (nur Vorhören). Andere Teile bleiben dabei unverändert. **Undo** im Kopfbereich.
- **Tonart / Skala (12) / Länge (1–16 Takte) / Taktart (4/4, 3/4, 5/4, 6/8) / Mood / BPM / Swing** gelten für alle Teile. Ändert man Tonart, Skala, Länge oder Taktart, erscheint ein Hinweis; beim nächsten *Generate* werden **alle** Teile neu erzeugt, damit sie zusammenpassen.
- **EXPORT:** jeder Teil einzeln als .mid, alle Teile getrennt (ein Ordner) oder eine .mid-Datei mit einer Spur je Teil. Type 1, 480 PPQ, Tempo und Taktart eingetragen, Länge = Loop-Länge. Presets speichern die erzeugten Noten mit.

## Technische Eckdaten
- State-ID `nwa.melodystudio`, State-Version 1, Plugin-Code `Mds1`. Deterministisch: gleicher Seed + gleiche Einstellungen = gleiche Noten.
- Erzeugung läuft im GUI-Thread; der Audio-Thread liest einen unveränderlichen Schnappschuss (keine Locks, keine Allokation). Die letzten 24 Stände bleiben am Leben.
- Der Vorhör-Synth folgt dem FL-Transport (Host sync) oder läuft intern (Play). Pegel wird weich begrenzt.

## Getestet (automatisch, Linux, GCC 13)
`ctest`: `melody_music` (Theorie, Akkordnamen, Voice Leading, Melodie-Regeln und Regler-Wirkung, Bass, Arp, Acid, alle Genres/Moods, Event-Liste), `melody_processor` (erste Idee, unabhängige Neuerzeugung, Undo, Tonart-Änderung, alle Genres × Moods, alle Taktarten/Längen, MIDI-Export per `juce::MidiFile` zurückgelesen: Tempo, Taktart, Notenzahl, Tonhöhe, Anfangszeit, Velocity; State-Round-Trip inkl. Noten, beschädigter State, manipulierte Noten, Audio endlich/begrenzt/nicht still, Mute, Blockgrössen-Unabhängigkeit, 300 Fuzz-Runden), `melody_gui` (alle 6 Seiten offscreen gerendert + Button-Checks: New idea wirkt nur auf den eigenen Teil, Variation, Undo, Clear, Make a track, Mute). Test-Prozesse laufen auch mit 512 KB Stack (Windows-Limit 1 MB).

## NICHT geprüft / bekannte Grenzen – bitte lesen
- **Klang nie gehört.** „Musikalisch sinnvoll“ ist hier nur als Regelwerk getestet (Akkordtöne auf betonten Zählzeiten, Tonart, Stimmführung, Dichte usw.), nicht per Gehör. Ob eine Idee gut klingt, entscheidest du.
- **Kein Windows-Build, nicht in FL Studio getestet.** Siehe `docs/FL_STUDIO_TESTLISTE_MELODY.md`. Nicht mit pluginval validiert.
- **GUI nur per Rendering und Button-Auslösung geprüft**, nicht mit echter Maus und nicht in FL. Datei-Dialoge im Export-Tab sind ungetestet.
- **Keine Echtzeit-MIDI-Ausgabe**, kein Drag-out der Noten aus dem Plugin (Export in Datei, dann in FL ziehen).
- **Acid-Slides** werden als überlappende Noten (legato) exportiert, **nicht** als FL-Slide-Flags. „Pitch variation“ ist eine chromatische Färbung einzelner Noten, **kein** Pitch-Bend. Wahrscheinlichkeit und Swing werden beim **Erzeugen** gewürfelt/eingerechnet, nicht live.
- **Akkorde neu erzeugen passt andere Teile nicht an:** Melodie, Bass und Arp bleiben stehen und können dann gegen die neuen Akkorde laufen – dann Teile einzeln neu erzeugen oder *Make a track*.
- **Vorhör-Synth ist einfach** (je Teil ein Saw/Puls-Klang mit Filter). Er ersetzt kein Instrument. Startet man FL mitten im Loop, klingen bereits begonnene Noten erst ab ihrem nächsten Einsatz.
- Nicht in dieser Phase: Audio-Export (WAV), Chord-Packs/Presets mitgelieferter Progressionen, eigene Akkord-Eingabe, Noten-Editor.
