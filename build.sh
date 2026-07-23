#!/usr/bin/env bash
# Build helper.
#
# Usage: ./build.sh {release|debug|asan} [--clean] [extra cmake args...]
#
# Configures (if needed) + incrementally builds the mode's build directory:
#   release -> build/       (CMAKE_BUILD_TYPE=RelWithDebInfo)
#   debug   -> build_debug/ (CMAKE_BUILD_TYPE=Debug)
#   asan    -> build_asan/  (CMAKE_BUILD_TYPE=Debug + ASan/UBSan)
#
# Pass --clean to wipe the mode's build directory first and reconfigure
# from scratch (use when switching compilers/flags, or if the build is
# in a weird state).
#
# Extra args are forwarded to the cmake configure step, e.g.:
#   ./build.sh debug --clean -DCMAKE_CXX_COMPILER=clang++
#
# Also (re)points ./compile_commands.json at the mode just built, for
# clangd/ccls.

set -euo pipefail

usage() {
    echo "Usage: $0 {release|debug|asan} [--clean] [extra cmake args...]" >&2
    exit 1
}

[ $# -ge 1 ] || usage
MODE="$1"
shift

CLEAN=0
if [ "${1:-}" = "--clean" ]; then
    CLEAN=1
    shift
fi

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

if [ "$CLEAN" = "1" ]; then
    echo "==> [$MODE] Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo "==> [$MODE] Configuring in $BUILD_DIR"
cmake -B "$BUILD_DIR" "${CMAKE_ARGS[@]}" "$@"

ln -sf "$BUILD_DIR/compile_commands.json" compile_commands.json

echo "==> [$MODE] Building with $JOBS jobs"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> [$MODE] Done: $BUILD_DIR/familiar"
