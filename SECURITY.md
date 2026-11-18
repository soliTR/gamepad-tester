# Security policy

## Supported versions

Only the latest release receives fixes.

## Reporting a vulnerability

Please do not open a public issue. Use GitHub's private reporting instead: open the
[Security tab](https://github.com/soliTR/gamepad-tester/security) of this repository and click
**Report a vulnerability**. You will get an answer as soon as possible.

## Verifying a release

Every release archive can be checked in two ways:

1. **Integrity:** `sha256sum --check --ignore-missing SHA256SUMS` in the folder with the downloaded files.
2. **Origin:** `gh attestation verify <archive> --repo soliTR/gamepad-tester` with the
   [GitHub CLI](https://cli.github.com). This confirms the archive was built by this repository's
   release workflow on GitHub from a specific commit. The output names that commit, so you can read
   the exact source code the program was built from.

If either check fails, do not run the file, and please report it as described above.
