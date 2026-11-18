#!/bin/sh
# Installs the packages needed to build gamepad-tester and prints the matching
# build command.
#
# Supported: Arch/CachyOS/Manjaro, Fedora, openSUSE Tumbleweed, Debian, Ubuntu
# and distributions based on them.
#
# On Debian/Ubuntu releases whose SDL3 is older than 3.4 (e.g. Ubuntu 24.04,
# Debian 13), the packages for building SDL3 from source are installed instead
# and the build uses -DGAMEPAD_TESTER_BUNDLED_SDL=ON. Pass --bundled-sdl to
# force that mode on Debian/Ubuntu.
set -eu

mode=auto
if [ "${1:-}" = "--bundled-sdl" ]; then
    mode=bundled
fi

sudo=""
if [ "$(id -u)" -ne 0 ]; then
    sudo=sudo
fi
# Ask no questions in CI; on a desktop the package manager asks as usual
yes_flag=""
if [ "${CI:-}" = "true" ]; then
    yes_flag=y
fi

# shellcheck source=/dev/null
. /etc/os-release
family=" ${ID} ${ID_LIKE:-} "

apt_bundled_packages="build-essential cmake ninja-build pkg-config ca-certificates \
libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
libwayland-dev libdecor-0-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
libudev-dev libusb-1.0-0-dev libdbus-1-dev"

case "$family" in
*" arch "*)
    if [ -n "$yes_flag" ]; then
        # fresh CI containers have no package database yet
        $sudo pacman -Syu --noconfirm
    fi
    $sudo pacman -S --needed ${yes_flag:+--noconfirm} base-devel cmake sdl3 libusb
    ;;
*" fedora "*)
    $sudo dnf install ${yes_flag:+-y} gcc cmake SDL3-devel libusb1-devel
    ;;
*" suse "* | *" opensuse "*)
    $sudo zypper ${yes_flag:+--non-interactive} install gcc cmake SDL3-devel libusb-1_0-devel
    ;;
*" debian "* | *" ubuntu "*)
    $sudo apt-get update
    if [ "$mode" = "auto" ]; then
        candidate=$(apt-cache policy libsdl3-dev | sed -n 's/^ *Candidate: //p')
        if [ -n "$candidate" ] && [ "$candidate" != "(none)" ] && dpkg --compare-versions "$candidate" ge 3.4; then
            mode=system
        else
            echo "SDL3 >= 3.4 is not packaged here (found: ${candidate:-none}); SDL3 will be built from source."
            mode=bundled
        fi
    fi
    if [ "$mode" = "bundled" ]; then
        # shellcheck disable=SC2086
        $sudo apt-get install ${yes_flag:+-y} --no-install-recommends $apt_bundled_packages
    else
        $sudo apt-get install ${yes_flag:+-y} --no-install-recommends build-essential cmake pkg-config libsdl3-dev libusb-1.0-0-dev
    fi
    ;;
*)
    echo "Unsupported distribution: ${PRETTY_NAME:-$ID}. See the README for the required packages." >&2
    exit 1
    ;;
esac

echo
echo "Dependencies installed. Build with:"
if [ "$mode" = "bundled" ]; then
    echo "  cmake -B build -DGAMEPAD_TESTER_BUNDLED_SDL=ON"
else
    echo "  cmake -B build"
fi
echo "  cmake --build build"
