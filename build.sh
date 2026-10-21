#!/usr/bin/env bash
# Build the small client and server with vcpkg dependencies.
set -euo pipefail
cd "$(dirname "$0")"

preset=linux-release
clean=0
for arg in "$@"; do
    case "$arg" in
        --debug) preset=linux-debug ;;
        --clean) clean=1 ;;
        -h|--help)
            echo "Usage: ./build.sh [--debug] [--clean] [--help]"
            exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

for tool in git cmake ninja; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }
done
if command -v pkg-config >/dev/null && ! pkg-config --exists x11 xrandr xcursor xi gl libudev; then
    cat >&2 <<'PACKAGES'
SFML needs the X11, OpenGL and udev development headers.
Debian/Ubuntu: sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libgl1-mesa-dev libudev-dev autoconf automake libtool
Fedora: sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel mesa-libGL-devel systemd-devel autoconf automake libtool
PACKAGES
    exit 1
fi

if [[ -z ${VCPKG_ROOT:-} ]]; then
    export VCPKG_ROOT="$PWD/vcpkg"
    if [[ ! -d "$VCPKG_ROOT/.git" ]]; then
        git clone --filter=tree:0 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    fi
    if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
        "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    fi
fi
if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "VCPKG_ROOT must point to a bootstrapped vcpkg checkout." >&2
    exit 1
fi

if [[ $clean == 1 ]]; then
    rm -rf "build/client-$preset"
fi
cmake --preset "$preset"
cmake --build --preset "$preset"
for binary in r-type_client r-type_server; do
    cp "build/client-$preset/bin/$binary" "./$binary"
done
echo "Run ./r-type_server, then ./r-type_client"
