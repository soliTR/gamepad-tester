#!/bin/sh
# Packs a portable release archive from a build made with -DGAMEPAD_TESTER_BUNDLED_SDL=ON.
#
# Usage: scripts/package.sh <version> <arch> [build directory]
# Result: dist/gamepad-tester-<version>-linux-<arch>.tar.gz
#
# File order, owners and timestamps are fixed (taken from the last commit), so the
# same build produces a byte-identical archive.
set -eu

version=$1
arch=$2
build=${3:-build}
name=gamepad-tester-$version-linux-$arch
stage=dist/$name

if [ ! -f "$build/_deps/sdl3-src/LICENSE.txt" ]; then
    echo "error: $build was not configured with -DGAMEPAD_TESTER_BUNDLED_SDL=ON" >&2
    exit 1
fi

rm -rf "$stage" "dist/$name.tar.gz"
mkdir -p "$stage"
cp "$build/gamepad-tester" "$stage/"
strip "$stage/gamepad-tester"
cp README.md LICENSE data/gamepad-tester.desktop "$stage/"
cp "$build/_deps/sdl3-src/LICENSE.txt" "$stage/LICENSE.SDL3.txt"

epoch=${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}
tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$epoch" -C dist -cf - "$name" | gzip -n -9 > "dist/$name.tar.gz"
rm -rf "$stage"
echo "dist/$name.tar.gz"
