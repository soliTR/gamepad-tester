#!/bin/sh
# Starts the program without a screen, once per tab, with the virtual controller
# attached, and checks that every run exits cleanly and saves a screenshot.
#
# Usage: scripts/smoke-test.sh [path/to/gamepad-tester] [output directory]
set -eu

binary=${1:-build/gamepad-tester}
out=${2:-build/smoke-test}
mkdir -p "$out"

"$binary" --version

export SDL_VIDEO_DRIVER=offscreen
export SDL_RENDER_DRIVER=software
export GAMEPAD_TESTER_VIRTUAL=1

for tab in 0 1 2 3 4 5 6; do
    shot="$out/tab$tab.bmp"
    rm -f "$shot"
    GAMEPAD_TESTER_TAB=$tab GAMEPAD_TESTER_SHOT=$shot timeout 30 "$binary" >/dev/null
    if [ ! -s "$shot" ]; then
        echo "FAIL: tab $tab produced no screenshot" >&2
        exit 1
    fi
    echo "ok: tab $tab"
done
echo "Smoke test passed"
