#!/usr/bin/env bash
# Clean build helper.
#
# Usage: ./build.sh {release|debug|asan} [extra cmake args...]
#
# Wipes the mode's build directory and reconfigures + builds from scratch:
#   release -> build/       (CMAKE_BUILD_TYPE=RelWithDebInfo)
#   debug   -> build_debug/ (CMAKE_BUILD_TYPE=Debug)
#   asan    -> build_asan/  (CMAKE_BUILD_TYPE=Debug + ASan/UBSan)
#
# Extra args are forwarded to the cmake configure step, e.g.:
#   ./build.sh debug -DCMAKE_CXX_COMPILER=clang++

set -euo pipefail

usage() {
    echo "Usage: $0 {release|debug|asan} [extra cmake args...]" >&2
    exit 1
}

[ $# -ge 1 ] || usage
MODE="$1"
shift

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

JOBS="${JOBS:-$(nproc)}"

case "$MODE" in
    release)
        BUILD_DIR="build"
        CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=RelWithDebInfo)
        ;;
    debug)
        BUILD_DIR="build_debug"
        CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Debug)
        ;;
    asan)
        BUILD_DIR="build_asan"
        CMAKE_ARGS=(
            -DCMAKE_BUILD_TYPE=Debug
            "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
            "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
        )
        ;;
    *)
        usage
        ;;
esac

echo "==> [$MODE] Cleaning $BUILD_DIR"
rm -rf "$BUILD_DIR"

echo "==> [$MODE] Configuring in $BUILD_DIR"
cmake -B "$BUILD_DIR" "${CMAKE_ARGS[@]}" "$@"

echo "==> [$MODE] Building with $JOBS jobs"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> [$MODE] Done: $BUILD_DIR/familiar"
