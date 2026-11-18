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

## Installation

### 1. Abhängigkeiten

Benötigt werden ein C-Compiler, CMake und SDL3 ab Version 3.4. Unter Arch Linux und CachyOS:

```bash
sudo pacman -S --needed base-devel cmake sdl3 libusb steam-devices
```

- `libusb` braucht SDL für den Switch 2 Pro Controller.
- `steam-devices` (Repository `multilib`) enthält udev-Regeln, ohne die Vibration, Gyro und LED bei vielen Controllern nicht funktionieren. Nach der Installation die Controller einmal neu anstecken.

Andere Distributionen brauchen die entsprechenden Pakete. Ist dort nur eine ältere SDL3-Version verfügbar, muss SDL3 aus dem Quellcode gebaut werden.

### 2. Bauen

Im Projektordner:

```bash
cmake -B build
cmake --build build
```

Das Programm lässt sich jetzt schon ohne Installation starten:

```bash
./build/gamepad-tester
```

### 3. Installieren

Für alle Benutzer (nach `/usr/local`):

```bash
sudo cmake --install build
```

Oder nur für den eigenen Benutzer (nach `~/.local`, ohne Root-Rechte):

```bash
cmake --install build --prefix ~/.local
```

Installiert werden das Programm (`bin/gamepad-tester`) und ein Eintrag im Anwendungsmenü („Gamepad-Tester“). Danach startet es mit `gamepad-tester` oder über das Menü. Bei der Installation nach `~/.local` muss `~/.local/bin` im `PATH` stehen. Bei den meisten Distributionen ist das schon der Fall, sonst in fish:

```bash
fish_add_path ~/.local/bin
```

### Deinstallieren

CMake merkt sich die installierten Dateien in `build/install_manifest.txt`:

```bash
xargs rm -v < build/install_manifest.txt
```

Wurde mit `sudo` installiert, den Befehl ebenfalls mit `sudo` ausführen.

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

### Aufbau

```
src/
  main.c        Start, Hauptschleife, Kopf- und Fußzeile
  ui.c/.h       Farben, Zeichnen, Text (inkl. Umlaute), Schaltflächen
  pads.c/.h     verbundene Controller, Auswahl, An-/Abstecken
  padinfo.c/.h  Namen von Typen, Tasten und Triggern; Fähigkeiten
  input.c/.h    Stick- und Triggerwerte pro Frame
  trail.c/.h    Leuchtspur für Sticks und Touchpad
  eventlog.c/.h Ereignis-Log
  quat.h        Quaternionen für die 3D-Ansicht
  devtools.c/.h virtueller Controller und Screenshots
  tabs/
    tabs.h      gemeinsame Schnittstelle aller Tabs
    info.c  buttons.c  sticks.c  drift.c  rumble.c  gyro.c  extras.c
```

Jeder Tab ist ein `Tab`-Objekt mit `reset`, `update`, `draw` und `reset_key` (siehe `tabs/tabs.h`).
`update` läuft für alle Tabs in jedem Frame, damit z. B. eine Vibration weiterläuft, wenn man den Tab wechselt.
`draw` läuft nur für den sichtbaren Tab. Ein neuer Tab braucht eine eigene Datei und einen Eintrag in der Liste `tabs[]` in `main.c`.

### Testen ohne Hardware

```bash
GAMEPAD_TESTER_VIRTUAL=1 ./build/gamepad-tester            # virtueller Controller
GAMEPAD_TESTER_VIRTUAL=1 GAMEPAD_TESTER_TAB=3 GAMEPAD_TESTER_SHOT=bild.bmp ./build/gamepad-tester
```

`GAMEPAD_TESTER_SHOT` speichert nach 30 Frames einen Screenshot und beendet das Programm.
