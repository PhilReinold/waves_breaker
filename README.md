# Waves Breaker – automatischer Vocal-Fader (Vintage-Optik)

Zieht den Pegel einer Gesangsspur automatisch auf einen Zielwert, wie ein Mixer, der den Fader von Hand nachzieht.
Format: Audio Unit (für Logic) und VST3. Eigenentwicklung, nicht von Waves Audio.

## Fertig bauen lassen (ohne Xcode, über GitHub)

1. Jeder Push baut automatisch (Reiter *Actions* -> *Build Waves Breaker*), manuell über *Run workflow*.
2. Der Workflow testet zuerst den DSP-Kern, baut dann AU + VST3 und prüft die Audio Unit mit `auval` (dem Test, den auch Logic nutzt).
3. Nach ca. 5-10 Minuten unter *Artifacts* `WavesBreaker-macOS` laden und entpacken.
4. `Waves Breaker.component` nach `~/Library/Audio/Plug-Ins/Components/`, `Waves Breaker.vst3` nach `~/Library/Audio/Plug-Ins/VST3/`.
5. Falls macOS blockt (Datei ist nicht notarisiert): `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Waves Breaker.component"`
6. Logic: Plug-in-Verwaltung -> Neu durchsuchen. Zu finden unter Audio-Units -> Philip Jin Plugins -> Waves Breaker.

Lokal bauen (Mac mit Xcode + CMake): `cmake -B build -G Xcode && cmake --build build --config Release`.
Optionaler Check: `auval -v aufx Wbrk Phil`

## Bedienung

| Regler | Funktion |
|---|---|
| Target | Ziel-Pegel der Stimme (0 bis -42 dB); Marken am Meter zeigen das Zielband |
| Range | Zielband; innerhalb davon kein Eingriff |
| Max Boost / Max Cut | Fader-Grenzen oben / unten (Range Max / Min) |
| Vocal Sens | Schwelle: darunter gilt es nicht als Gesang, der Fader hält |
| Music Sens | -12..+12 dB; mit Sidechain folgt das Ziel dem Pegel der Musik |
| Idle | Fader-Ruheposition, wenn kein Gesang aktiv ist |
| Speed | Reaktionsgeschwindigkeit (langsam .. schnell) |
| Output | Ausgangspegel |
| Lookahead | 12 ms Vorausschau (Latenz wird an Logic gemeldet); aus = 0 ms Latenz |
| Breath | Atmer und Zischlaute zählen nicht als Gesang |
| Automation OFF / READ / WRITE | wie beim Vocal Rider, siehe unten |
| Lampen | VOCAL = Gesang erkannt, MUSIC = Sidechain greift ein, CLIP = Ausgang über 0 dBFS |

Sidechain in Logic: Vocal-Spur -> Plug-in-Fenster oben rechts "Side Chain" -> Bus/Spur mit der Musik wählen, dann Music Sens aufdrehen.

## Automation (Fader-Fahrten in Logic schreiben)

1. Logic: Spur-Automation auf **Latch** stellen, Parameter **Waves Breaker -> Rider Fader** wählen.
2. Im Plugin **WRITE** drücken, Song abspielen. Die Fader-Fahrten landen als Automationskurve in Logic (funktioniert auch bei geschlossenem Plugin-Fenster).
3. Danach Logic auf **Read** und im Plugin **READ** drücken. Jetzt spielt der Fader die Kurve ab; einzelne Stellen kannst du in Logic korrigieren oder im READ-Modus den Fader von Hand ziehen (Logic auf Touch/Latch).
4. **OFF** = live automatisch fahren, nichts schreiben.

Hinweis: Durch das Abtasten im Message-Thread kann die geschriebene Kurve ein paar Millisekunden versetzt sein, wie beim Original.

Presets: Auswahl im Kopf des Plugins (< >) und im Logic-Preset-Menü.

## Technik

- Pegelmessung gehörrichtig (K-Weighting nach ITU-R BS.1770, wie bei Lautheitsmessern), 40 ms RMS, 100-Hz-Hochpass gegen Rumpeln/Plopps.
- Zielbereich als echte Totzone: Liegt die Stimme im Band, bleibt der Fader stehen; sonst fährt er nur bis an den Bandrand (wenig Pumpen).
- Gate mit 3 dB Hysterese (kein Flattern an der Schwelle), Fader hält in Pausen und kehrt nach ~1 s zur Idle-Position.
- Music Sens: Ziel folgt der Musik-Lautheit relativ zu ihrem Langzeitmittel (Refrain lauter -> Stimme mit).
- Regelung alle 16 Samples, Gain pro Sample interpoliert: keine Knackser, ca. 0,4 % CPU eines Kerns (Stereo, 96 kHz).

## Sicherheit

Das Plugin liest oder schreibt keine Dateien und nutzt kein Netzwerk. Der Signalweg ist gegen NaN/Inf gesichert, der Ausgang ist hart auf +12 dBFS begrenzt. Trotzdem beim ersten Test Monitore leise lassen.

## Test des DSP-Kerns (ohne JUCE)

    g++ -std=c++17 -O2 -ISource Tests/dsp_test.cpp -o dsp_test && ./dsp_test
