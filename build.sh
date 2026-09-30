#!/usr/bin/env bash
# Build helper.
#
# Usage: ./build.sh [mode] [compiler] [generator] [jobs] [--clean]
#
# Every argument is optional and ORDER DOESN'T MATTER - each one is
# recognised by what it is, not by its position:
#
#   mode       release | debug | asan | coverage        (default: debug)
#   compiler   gcc | clang                              (default: whatever
#              /usr/bin/c++ points at - see switch-compiler.sh)
#   generator  ninja | make                             (default: ninja if
#              installed, else make)
#   jobs       number 1-99, e.g. 8                      (default: nproc)
#   --clean    wipe this build directory and reconfigure from scratch
#
# So all of these work:
#   ./build.sh
#   ./build.sh release ninja
#   ./build.sh debug clang ninja 8
#   ./build.sh 4 asan --clean
#
# Anything else is an error: Invalid flag <...>.
#
# Build directories, one per configuration so they never fight over a
# cached CMake config:
#   release  -> build/          (CMAKE_BUILD_TYPE=RelWithDebInfo)
#   debug    -> build_debug/    (CMAKE_BUILD_TYPE=Debug)
#   asan     -> build_asan/     (CMAKE_BUILD_TYPE=Debug + ASan/UBSan)
#   coverage -> build_coverage/ (CMAKE_BUILD_TYPE=Debug + --coverage
#                instrumentation, forced onto GCC/gcov regardless of the
#                other modes' default compiler - lcov/genhtml need a gcov
#                build of the matching GCC version to parse .gcno/.gcda
#                reliably, and pairing it with clang's own
#                gcov-compatible output is not something this script
#                tries to support.
#                The GoogleTest suite is linked into `familiar` in every
#                mode, not just this one (see src/CMakeLists.txt +
#                main.cpp's "-t" handling) - run it with
#                `build_coverage/familiar -t`. See ./coverage.sh for the
#                full build+run+lcov+genhtml pipeline.
#
# Naming an explicit compiler appends a suffix: `./build.sh debug clang`
# builds in build_debug_clang/, leaving build_debug/ untouched. That way
# both compilers' trees coexist and switching between them costs nothing
# - CMake caches the compiler in its own config and never re-checks it,
# so sharing one directory would force a full rebuild every switch.
# Without an explicit compiler the plain names above are used, so
# coverage.sh, CI and .gitignore keep pointing at the same places.
#
# Unlike switch-compiler.sh, choosing a compiler here affects ONLY this
# build - it doesn't touch update-alternatives and needs no sudo.
#
# Also (re)points ./compile_commands.json at whatever was built last, for
# clangd/ccls and ./tidy.sh.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

usage() {
    sed -n '2,23p' "$0" | sed 's/^# \?//'
    exit 1
}

MODE="debug"
COMPILER=""
GENERATOR=""
GENERATOR_EXPLICIT=0
JOBS="${JOBS:-$(nproc)}"
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        release | debug | asan | coverage) MODE="$arg" ;;
        gcc | clang) COMPILER="$arg" ;;
        ninja) GENERATOR="Ninja" GENERATOR_EXPLICIT=1 ;;
        make) GENERATOR="Unix Makefiles" GENERATOR_EXPLICIT=1 ;;
        --clean) CLEAN=1 ;;
        -h | --help) usage ;;
        [[:digit:]] | [[:digit:]][[:digit:]]) JOBS="$arg" ;;
        *)
            echo -e "\n\033[0;31m    Invalid flag <${arg}>\033[0m\n" >&2
            exit 1
            ;;
    esac
done

# Picks the newest gcc/g++ (or clang/clang++) major-version pair that has
# BOTH halves present - not just the unversioned name on PATH, because on
# a machine with several versions installed side by side the unversioned
# name isn't guaranteed to be the newest, and it matters here: confirmed
# on this machine that the unversioned `g++` resolves to g++-13, whose
# own bundled libstdc++.so predates the CXXABI_1.3.15 symbol the system's
# current libstdc++6/ICU expect, breaking the final link with "undefined
# reference to __cxa_call_terminate@CXXABI_1.3.15" - g++-15's bundled
# libstdc++.so does export it (verified via readelf), i.e.
# libstdc++-13-dev and the libstdc++6 runtime have drifted apart on this
# box while libstdc++-15-dev still tracks it. Picking the newest
# installed pair sidesteps that instead of hardcoding a version number
# that would go stale. Falls back to the unversioned pair if no versioned
# one exists at all.
#
# Also used by ./coverage.sh (duplicated there) for picking the matching
# gcov binary at report-generation time.
find_newest_pair() {
    local cc_name="$1" cxx_name="$2"
    local best_ver=-1 best_cxx="" best_cc=""
    local cxx ver cc_candidate
    for cxx in /usr/bin/"$cxx_name"-*; do
        [ -x "$cxx" ] || continue
        ver="${cxx##*"$cxx_name"-}"
        case "$ver" in
            '' | *[!0-9]*) continue ;;
        esac
        cc_candidate="/usr/bin/$cc_name-$ver"
        [ -x "$cc_candidate" ] || continue
        if [ "$ver" -gt "$best_ver" ]; then
            best_ver="$ver"
            best_cxx="$cxx"
            best_cc="$cc_candidate"
        fi
    done
    if [ -z "$best_cxx" ]; then
        command -v "$cxx_name" >/dev/null 2>&1 &&
            command -v "$cc_name" >/dev/null 2>&1 || return 1
        best_cxx="$(command -v "$cxx_name")"
        best_cc="$(command -v "$cc_name")"
    fi
    echo "$best_cc" "$best_cxx"
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
        if [ -n "$COMPILER" ] && [ "$COMPILER" != "gcc" ]; then
            echo "error: coverage requires gcc (gcov), got '$COMPILER'." >&2
            exit 1
        fi
        COMPILER="gcc"
        CMAKE_ARGS=(
            -DCMAKE_BUILD_TYPE=Debug
            "-DCMAKE_CXX_FLAGS=--coverage -O0 -g"
            "-DCMAKE_EXE_LINKER_FLAGS=--coverage"
        )
        ;;
esac

# Only suffix when a compiler was asked for by name - see the header for
# why the unsuffixed paths have to keep working. Coverage is exempt: it
# forces gcc on its own, and coverage.sh expects build_coverage/.
if [ -n "$COMPILER" ] && [ "$MODE" != "coverage" ]; then
    BUILD_DIR="${BUILD_DIR}_${COMPILER}"
fi

if [ -n "$COMPILER" ]; then
    case "$COMPILER" in
        gcc) PAIR="$(find_newest_pair gcc g++)" || PAIR="" ;;
        clang) PAIR="$(find_newest_pair clang clang++)" || PAIR="" ;;
    esac
    if [ -z "$PAIR" ]; then
        echo "error: no usable $COMPILER toolchain found." >&2
        exit 1
    fi
    read -r CC_BIN CXX_BIN <<<"$PAIR"
    echo "==> [$MODE] Compiler: $CXX_BIN / $CC_BIN"
    CMAKE_ARGS+=(-DCMAKE_C_COMPILER="$CC_BIN" -DCMAKE_CXX_COMPILER="$CXX_BIN")
fi

# Ninja by default - meaningfully faster than Make on the incremental
# rebuilds that dominate day-to-day work. Falls back rather than failing
# when it isn't installed: a build helper shouldn't refuse to work over a
# preference the caller never expressed.
if [ "$GENERATOR_EXPLICIT" = "0" ]; then
    if command -v ninja >/dev/null 2>&1; then
        GENERATOR="Ninja"
    else
        GENERATOR="Unix Makefiles"
    fi
fi

if [ "$GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
    echo "error: ninja requested but not installed." >&2
    exit 1
fi

# CMake refuses to switch generator inside an existing build tree, and
# its own error doesn't say what to do about it.
if [ "$CLEAN" = "0" ] && [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHED="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$BUILD_DIR/CMakeCache.txt")"
    if [ -n "$CACHED" ] && [ "$CACHED" != "$GENERATOR" ]; then
        if [ "$GENERATOR_EXPLICIT" = "1" ]; then
            # Asked for something the tree can't provide - say so.
            echo "error: $BUILD_DIR was configured with '$CACHED', not '$GENERATOR'." >&2
            echo "       Re-run with --clean to switch generator." >&2
            exit 1
        fi
        # Only the default disagreed, and the caller never asked for a
        # generator at all. Making Ninja the default must not turn every
        # build directory that predates that change into a hard error -
        # keep using what the tree already has, and say why.
        echo "==> [$MODE] $BUILD_DIR already uses '$CACHED'; keeping it"
        echo "    (./build.sh $MODE ninja --clean to switch)"
        GENERATOR="$CACHED"
    fi
fi

CMAKE_ARGS+=(-G "$GENERATOR")

if [ "$CLEAN" = "1" ]; then
    echo "==> [$MODE] Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo "==> [$MODE] Configuring in $BUILD_DIR"
cmake -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"

ln -sf "$BUILD_DIR/compile_commands.json" compile_commands.json

echo "==> [$MODE] Building with $JOBS jobs"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> [$MODE] Done: $BUILD_DIR/familiar"
