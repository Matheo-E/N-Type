#!/usr/bin/env bash
# Builds N-Type and copies r-type_server and r-type_client to the repository root.
#
# Uses $VCPKG_ROOT when it is set; otherwise fetches vcpkg once into ./vcpkg.
# Run ./build.sh --help for the options.

set -euo pipefail
cd "$(dirname "$0")"

usage() {
    cat <<'EOF'
USAGE: ./build.sh [options]
  --debug        debug build (default: release)
  --server-only  build only the server and the tests (no SFML needed)
  --clean        delete the previous build of the chosen configuration first
  -h, --help     show this help
EOF
}

die() {
    echo "build.sh: $*" >&2
    exit 1
}

preset=linux-release
server_only=0
clean=0
for arg in "$@"; do
    case "$arg" in
        --debug) preset=linux-debug ;;
        --server-only) server_only=1 ;;
        --clean) clean=1 ;;
        -h | --help) usage; exit 0 ;;
        *) usage >&2; die "unknown option '$arg'" ;;
    esac
done
# The server-only preset is a debug build without the client.
if [[ $server_only == 1 ]]; then
    preset=linux-server
fi

for tool in git cmake ninja; do
    command -v "$tool" > /dev/null || die "'$tool' is required but not installed"
done
command -v c++ > /dev/null || command -v g++ > /dev/null || command -v clang++ > /dev/null ||
    die "a C++ compiler is required (GCC 14+ or Clang 18+)"

# SFML needs a few system headers on Linux. Catch the most common setup
# problem here instead of deep inside the dependency build.
if [[ $server_only == 0 ]] && command -v pkg-config > /dev/null &&
    ! pkg-config --exists x11 xrandr xcursor xi gl libudev 2> /dev/null; then
    cat >&2 <<'EOF'
build.sh: the X11/OpenGL/udev development headers SFML needs are missing.
  Debian/Ubuntu: sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
                   libgl1-mesa-dev libudev-dev libasound2-dev autoconf automake libtool
  Fedora:        sudo dnf install libX11-devel libXrandr-devel libXcursor-devel libXi-devel \
                   mesa-libGL-devel systemd-devel alsa-lib-devel autoconf automake libtool
Or build without the client: ./build.sh --server-only
EOF
    exit 1
fi

if [[ -n ${VCPKG_ROOT:-} ]]; then
    [[ -x $VCPKG_ROOT/vcpkg ]] || die "VCPKG_ROOT=$VCPKG_ROOT is not a bootstrapped vcpkg"
else
    VCPKG_ROOT=$PWD/vcpkg
    if [[ ! -d $VCPKG_ROOT/.git ]]; then
        echo "-- Fetching vcpkg into ./vcpkg (first build only)"
        # Commits only; vcpkg downloads the files it needs on demand.
        git clone --filter=tree:0 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    fi
    if [[ ! -x $VCPKG_ROOT/vcpkg ]]; then
        "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    fi
fi
export VCPKG_ROOT

if [[ $clean == 1 ]]; then
    rm -rf "build/$preset"
fi

echo "-- Building with preset $preset (the first build also compiles the dependencies)"
cmake --preset "$preset"
cmake --build --preset "$preset"

for binary in r-type_server r-type_client; do
    rm -f "$binary"
    if [[ -f build/$preset/bin/$binary ]]; then
        cp "build/$preset/bin/$binary" .
    fi
done

echo
echo "Done. Run from the repository root:"
echo "  ./r-type_server -p 4242"
if [[ -f r-type_client ]]; then
    echo "  ./r-type_client -h 127.0.0.1 -p 4242"
fi
