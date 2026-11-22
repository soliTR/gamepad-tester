# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [semantic versioning](https://semver.org).

## [Unreleased]

- Overview tab: polling rate of the controller and its jitter, measured from the input event timestamps of the last second, with the standard rate it matches (125 to 8000 Hz).

## [1.0.0] - 2026-11-18

First release.

- Seven test tabs: overview, buttons with checklist, sticks, stick drift (rest test and circle test), rumble, gyro with 3D view and gyro rest test, extras (touchpad, LED color, player LED, battery).
- Tested with the PS5 DualSense, an 8BitDo Ultimate 2 and the Nintendo Switch 2 Pro Controller (USB).
- Hotplug and switching between several connected controllers.
- Portable release archives for x86_64 and aarch64 with SDL 3.4.20 built in, SHA-256 checksums and signed build provenance.
- `--version` and `--help` options.

[1.0.0]: https://github.com/soliTR/gamepad-tester/releases/tag/v1.0.0
