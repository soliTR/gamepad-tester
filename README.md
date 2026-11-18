# gamepad-tester

Graphical Linux program for testing gamepads. Written in C with SDL3 (3.4 or newer), built with CMake.

Made for the PS5 DualSense, 8BitDo controllers and the Nintendo Switch 2 Pro Controller (USB only).

## Features

| Tab | Content |
| --- | --- |
| 1 Overview | Name, type, IDs, connection, battery, available features, event log |
| 2 Buttons | Controller graphic with live display and a checklist that ticks off every pressed button |
| 3 Sticks | Both sticks live with trail and raw values, analog triggers |
| 4 Drift | Rest test (5 s, traffic-light rating), magnified center view, circle test with roundness error |
| 5 Rumble | Left/right motor individually, strength slider, alternating test, ramp, trigger rumble if available |
| 6 Gyro | Live values from gyro and accelerometer, 3D view, history graph, gyro rest test with bias measurement |
| 7 Extras | Touchpad, LED color, player LED, battery |

## Installation

### 1. Dependencies

You need a C compiler, CMake and SDL3 3.4 or newer. On Arch Linux and CachyOS:

```bash
sudo pacman -S --needed base-devel cmake sdl3 libusb steam-devices
```

- SDL needs `libusb` for the Switch 2 Pro Controller.
- `steam-devices` (repository `multilib`) contains udev rules; without them rumble, gyro and LED don't work on many controllers. Reconnect your controllers once after installing it.

Other distributions need the equivalent packages. If only an older SDL3 version is available there, build SDL3 from source.

### 2. Build

In the project folder:

```bash
cmake -B build
cmake --build build
```

You can already run the program without installing it:

```bash
./build/gamepad-tester
```

### 3. Install

For all users (to `/usr/local`):

```bash
sudo cmake --install build
```

Or just for your own user (to `~/.local`, no root needed):

```bash
cmake --install build --prefix ~/.local
```

This installs the program (`bin/gamepad-tester`) and an application menu entry ("Gamepad-Tester"). Start it with `gamepad-tester` or from the menu. When installing to `~/.local`, `~/.local/bin` must be in your `PATH`. Most distributions already do this; otherwise, in fish:

```bash
fish_add_path ~/.local/bin
```

### Uninstall

CMake records the installed files in `build/install_manifest.txt`:

```bash
xargs rm -v < build/install_manifest.txt
```

If you installed with `sudo`, run this with `sudo` as well.

## Usage

- `1` to `7` or left/right arrow: switch tabs
- `N`: switch between multiple controllers
- `R`: reset the test in the current tab
- `Esc`: quit
- Buttons are clicked with the mouse

Controllers can be plugged in and out at any time.

## Stick drift rating

The rest test measures for 5 seconds while both sticks are released. It rates the largest deflection during that time:

- up to 3 %: OK
- up to 7 %: slight drift, usually absorbed by the deadzone in games
- above: drift

The circle test shows whether the stick reaches the edge in every direction (a minimum below 90 % is suspicious) and how even its range of motion is.

## Troubleshooting

- **Rumble, gyro or LED missing:** Usually access to `/dev/hidraw*` is missing. On Arch/CachyOS the `steam-devices` package provides suitable udev rules. Reconnect the controller afterwards.
- **Steam is running:** Steam Input may grab the controller. Quit Steam.
- **Switch 2 Pro Controller:** SDL supports it only over USB (via libusb), not over Bluetooth.
- **8BitDo:** Gyro and rumble depend on the mode (X-Input, D-Input, Switch). Switch mode usually has both.

## Development

### Structure

```
src/
  main.c        startup, main loop, header and footer
  ui.c/.h       colors, drawing, text (8x8 bitmap font), buttons
  pads.c/.h     connected controllers, selection, hotplug
  padinfo.c/.h  names of types, buttons and triggers; capabilities
  input.c/.h    stick and trigger values per frame
  trail.c/.h    fading trail for sticks and touchpad
  eventlog.c/.h event log
  quat.h        quaternions for the 3D view
  devtools.c/.h virtual controller and screenshots
  tabs/
    tabs.h      common interface of all tabs
    info.c  buttons.c  sticks.c  drift.c  rumble.c  gyro.c  extras.c
```

Each tab is a `Tab` object with `reset`, `update`, `draw` and `reset_key` (see `tabs/tabs.h`).
`update` runs for all tabs every frame, so that e.g. rumble keeps going when you switch tabs.
`draw` runs only for the visible tab. A new tab needs its own file and an entry in the `tabs[]` list in `main.c`.

### Testing without hardware

```bash
GAMEPAD_TESTER_VIRTUAL=1 ./build/gamepad-tester            # virtual controller
GAMEPAD_TESTER_VIRTUAL=1 GAMEPAD_TESTER_TAB=3 GAMEPAD_TESTER_SHOT=screenshot.bmp ./build/gamepad-tester
```

`GAMEPAD_TESTER_SHOT` saves a screenshot after 30 frames and quits.

## License

MIT, see [LICENSE](LICENSE).

Uses [SDL3](https://libsdl.org) (zlib license). Product and brand names such as DualSense, Nintendo Switch or 8BitDo belong to their respective owners and are only used to describe which controllers are supported.
