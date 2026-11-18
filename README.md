# gamepad-tester

Grafisches Linux-Programm zum Testen von Gamepads. Geschrieben in C mit SDL3 (ab 3.4), gebaut mit CMake.

Gedacht für PS5 DualSense, 8BitDo-Controller und den Nintendo Switch 2 Pro Controller (nur per USB).

## Funktionen

| Tab | Inhalt |
| --- | --- |
| 1 Übersicht | Name, Typ, IDs, Verbindung, Akku, vorhandene Funktionen, Ereignis-Log |
| 2 Tasten | Controller-Grafik mit Live-Anzeige und eine Checkliste, die jede gedrückte Taste abhakt |
| 3 Sticks | Beide Sticks live mit Spur und Rohwerten, Trigger analog |
| 4 Stickdrift | Ruhetest (5 s, Ampel-Bewertung), vergrößerte Mittelansicht, Kreistest mit Rundheitsfehler |
| 5 Vibration | Linker/rechter Motor einzeln, Stärke-Regler, Wechsel-Test, Rampe, Trigger-Vibration falls vorhanden |
| 6 Gyro | Live-Werte von Gyro und Beschleunigungssensor, 3D-Ansicht, Verlaufsgraph, Gyro-Ruhetest mit Bias-Messung |
| 7 Extras | Touchpad, LED-Farbe, Spieler-LED, Akku |

## Bauen und starten

```bash
cmake -B build
cmake --build build
./build/gamepad-tester
```

## Bedienung

- `1` bis `7` oder Pfeil links/rechts: Tab wechseln
- `N`: zwischen mehreren Controllern wechseln
- `R`: Test im aktuellen Tab zurücksetzen
- `Esc`: beenden
- Schaltflächen per Maus

Controller können jederzeit an- und abgesteckt werden.

## Bewertung Stickdrift

Der Ruhetest misst 5 Sekunden lang, während beide Sticks losgelassen sind. Bewertet wird die größte Auslenkung in dieser Zeit:

- bis 3 %: OK
- bis 7 %: leichter Drift, in Spielen meist durch die Deadzone abgefangen
- darüber: Drift

Der Kreistest zeigt, ob der Stick den Rand in allen Richtungen erreicht (Minimum unter 90 % ist auffällig) und wie gleichmäßig der Ausschlag ist.

## Fehlerbehebung

- **Vibration, Gyro oder LED fehlen:** Meist fehlen Zugriffsrechte auf `/dev/hidraw*`. Unter Arch/CachyOS liefert das Paket `steam-devices` passende udev-Regeln. Danach den Controller neu anstecken.
- **Steam läuft:** Steam Input kann den Controller belegen. Steam dann beenden.
- **Switch 2 Pro Controller:** SDL unterstützt ihn nur per USB (über libusb), nicht per Bluetooth.
- **8BitDo:** Gyro und Vibration hängen vom Modus ab (X-Input, D-Input, Switch). Im Switch-Modus sind meist beide verfügbar.

## Entwicklung

Zum Testen ohne Hardware:

```bash
GAMEPAD_TESTER_VIRTUAL=1 ./build/gamepad-tester            # virtueller Controller
GAMEPAD_TESTER_VIRTUAL=1 GAMEPAD_TESTER_TAB=3 GAMEPAD_TESTER_SHOT=bild.bmp ./build/gamepad-tester
```

`GAMEPAD_TESTER_SHOT` speichert nach 30 Frames einen Screenshot und beendet das Programm.
