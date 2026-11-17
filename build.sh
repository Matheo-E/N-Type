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
missing=()
for tool in pkg-config autoconf autoreconf autoheader aclocal automake libtoolize; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
# autoconf-archive provides macros, not an executable. ALSA needs these
# even when all the Autotools commands are already installed.
if command -v aclocal >/dev/null; then
    IFS=: read -r -a aclocal_dirs <<< "${ACLOCAL_PATH:-}:$(aclocal --print-ac-dir)"
    archive_found=0
    for dir in "${aclocal_dirs[@]}"; do
        if [[ -n "$dir" && -f "$dir/ax_check_compile_flag.m4" ]]; then
            archive_found=1
            break
        fi
    done
    [[ $archive_found == 1 ]] || missing+=("autoconf-archive")
fi
if command -v pkg-config >/dev/null && ! pkg-config --exists x11 xrandr xcursor xi gl glu libudev; then
    missing+=("X11/OpenGL/GLU/udev development headers")
fi
if (( ${#missing[@]} > 0 )); then
    printf 'Missing build dependencies: %s\n' "${missing[*]}" >&2
    cat >&2 <<'PACKAGES'
SFML graphics and audio (ALSA) need these system dependencies.
Debian/Ubuntu/Pop!_OS: sudo apt install pkg-config libx11-dev libxrandr-dev libxcursor-dev libxi-dev libgl1-mesa-dev libglu1-mesa-dev libudev-dev autoconf autoconf-archive automake libtool
Fedora: sudo dnf install pkgconf-pkg-config libX11-devel libXrandr-devel libXcursor-devel libXi-devel mesa-libGL-devel mesa-libGLU-devel systemd-devel autoconf autoconf-archive automake libtool
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
for binary in r-type_client r-type_server ntype_sandbox; do
    cp "build/client-$preset/bin/$binary" "./$binary"
done
echo "Run ./r-type_server, then ./r-type_client (or ./ntype_sandbox to try the engine alone)"
