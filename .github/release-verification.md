
## Download and verify

Pick the archive for your CPU (`x86_64` for most PCs, `aarch64` for ARM). Requires glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 or newer).

Check that the archive is intact:

```bash
sha256sum --check --ignore-missing SHA256SUMS
```

Check that it was built by this repository's release workflow from the tagged source code (needs the [GitHub CLI](https://cli.github.com)):

```bash
gh attestation verify gamepad-tester-@VERSION@-linux-x86_64.tar.gz --repo soliTR/gamepad-tester
```

Offline, with the attached provenance file: add `--bundle gamepad-tester-@VERSION@.intoto.jsonl`.

Then unpack and run it:

```bash
tar xzf gamepad-tester-@VERSION@-linux-x86_64.tar.gz
./gamepad-tester-@VERSION@-linux-x86_64/gamepad-tester
```

See the [README](https://github.com/soliTR/gamepad-tester#readme) for installing it permanently and for the udev rules that rumble, gyro and LED need.
