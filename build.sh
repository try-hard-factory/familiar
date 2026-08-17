#!/usr/bin/env bash
# Build helper.
#
# Usage: ./build.sh {release|debug|asan|coverage} [--clean] [extra cmake args...]
#
# Configures (if needed) + incrementally builds the mode's build directory:
#   release  -> build/          (CMAKE_BUILD_TYPE=RelWithDebInfo)
#   debug    -> build_debug/    (CMAKE_BUILD_TYPE=Debug)
#   asan     -> build_asan/     (CMAKE_BUILD_TYPE=Debug + ASan/UBSan)
#   coverage -> build_coverage/ (CMAKE_BUILD_TYPE=Debug + --coverage
#                instrumentation, forced onto GCC/gcov regardless of the
#                other modes' default compiler (/usr/bin/c++, clang on
#                this machine) - lcov/genhtml need a gcov build of the
#                matching GCC version to parse .gcno/.gcda reliably, and
#                pairing it with clang's own gcov-compatible output is
#                not something this script tries to support.
#                The GoogleTest suite is linked into `familiar` in every
#                mode, not just this one (see src/CMakeLists.txt +
#                main.cpp's "-t" handling) - run it with
#                `build_coverage/familiar -t`. See ./coverage.sh for the
#                full build+run+lcov+genhtml pipeline.
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
    echo "Usage: $0 {release|debug|asan|coverage} [--clean] [extra cmake args...]" >&2
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

# Picks the newest gcc/g++ major-version pair that has BOTH a gcc-N and
# g++-N binary present - not just the unversioned `gcc`/`g++` on PATH,
# because on a machine with several GCC versions installed side by side
# the unversioned name isn't guaranteed to be the newest one, and it
# matters here: confirmed on this machine that the unversioned `g++`
# resolves to g++-13, whose own bundled libstdc++.so predates the
# CXXABI_1.3.15 symbol the system's current libstdc++6/ICU expect,
# breaking the final link with "undefined reference to
# __cxa_call_terminate@CXXABI_1.3.15" - g++-15's bundled libstdc++.so
# does export it (verified via readelf), i.e. libstdc++-13-dev and the
# libstdc++6 runtime have drifted apart on this box while libstdc++-15-dev
# still tracks it. Picking the newest installed pair sidesteps that
# instead of hardcoding a version number that would go stale. Falls back
# to plain gcc/g++ if no versioned pair exists at all.
#
# Also used by ./coverage.sh (duplicated there) for picking the matching
# gcov binary at report-generation time.
find_newest_gcc_pair() {
    local best_ver=-1 best_gxx="" best_gcc=""
    local gxx ver gcc_candidate
    for gxx in /usr/bin/g++-*; do
        [ -x "$gxx" ] || continue
        ver="${gxx##*g++-}"
        case "$ver" in
            ''|*[!0-9]*) continue ;;
        esac
        gcc_candidate="/usr/bin/gcc-$ver"
        [ -x "$gcc_candidate" ] || continue
        if [ "$ver" -gt "$best_ver" ]; then
            best_ver="$ver"
            best_gxx="$gxx"
            best_gcc="$gcc_candidate"
        fi
    done
    if [ -z "$best_gxx" ]; then
        command -v g++ >/dev/null 2>&1 && command -v gcc >/dev/null 2>&1 || return 1
        best_gxx="$(command -v g++)"
        best_gcc="$(command -v gcc)"
    fi
    echo "$best_gcc" "$best_gxx"
}

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
    coverage)
        BUILD_DIR="build_coverage"
        GCC_BIN="" GXX_BIN=""
        if ! read -r GCC_BIN GXX_BIN < <(find_newest_gcc_pair); then
            echo "error: no gcc/g++ pair found on PATH." >&2
            exit 1
        fi
        echo "==> [coverage] Using $GXX_BIN / $GCC_BIN"
        CMAKE_ARGS=(
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_C_COMPILER="$GCC_BIN"
            -DCMAKE_CXX_COMPILER="$GXX_BIN"
            "-DCMAKE_CXX_FLAGS=--coverage -O0 -g"
            "-DCMAKE_EXE_LINKER_FLAGS=--coverage"
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
