# Contributing

Thanks for your interest! Bug reports, controller reports, ideas and pull requests are all welcome.

## Reporting

- **Bugs and controller reports:** use the [issue forms](https://github.com/soliTR/gamepad-tester/issues/new/choose). The console output (start the program from a terminal) helps most.
- **Questions:** [Discussions](https://github.com/soliTR/gamepad-tester/discussions).
- **Security problems:** privately, see [SECURITY.md](SECURITY.md).

## Getting started

```bash
git clone https://github.com/soliTR/gamepad-tester.git
cd gamepad-tester
scripts/install-deps.sh
cmake -B build -DGAMEPAD_TESTER_WERROR=ON
cmake --build build
```

No controller at hand? The virtual controller drives every tab:

```bash
GAMEPAD_TESTER_VIRTUAL=1 ./build/gamepad-tester
scripts/smoke-test.sh      # headless run of all tabs, as in CI
```

The [Development section of the README](README.md#development) explains the code structure. Issues labeled [good first issue](https://github.com/soliTR/gamepad-tester/labels/good%20first%20issue) are a good place to start.

## Code style

- C11, 4 spaces, braces on the same line, `snake_case` names; match the surrounding code.
- English for code, comments, UI texts and commit messages.
- Keep each tab self-contained in `src/tabs/`; shared helpers go to `ui.c`, `padinfo.c` and friends.
- No new dependencies besides SDL3 without discussing it first.

## Pull requests

- One topic per pull request; reference the issue (`Fixes #12`).
- CI must pass: it builds and smoke-tests on Arch, Fedora, openSUSE, Ubuntu and Debian.
- If you change the UI, check that all texts still fit and attach a screenshot.

By contributing you agree that your contribution is licensed under the [MIT License](LICENSE) and that you follow the [Code of Conduct](CODE_OF_CONDUCT.md).
