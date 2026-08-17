#!/usr/bin/env bash
# Build helper.
#
# Usage:
#   ./build.sh {release|debug|asan|coverage} [--clean] [extra cmake args...]
#   ./build.sh coverage-report [--clean]
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
#                not something this script tries to support)
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
#
# coverage-report:
#   Builds (or reuses) build_coverage/, runs ctest against it (a no-op
#   right now - no tests exist yet, see the "26" roadmap item this is
#   prep for), captures whatever .gcda that run produced via lcov, and
#   renders an HTML report to build_coverage/coverage_html/index.html.
#   Requires `lcov`/`genhtml` on PATH - this script does not install
#   them; on Debian/Ubuntu: `sudo apt install lcov`.

set -euo pipefail

usage() {
    cat >&2 <<'EOF'
Usage:
  ./build.sh {release|debug|asan|coverage} [--clean] [extra cmake args...]
  ./build.sh coverage-report [--clean]
EOF
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

# Finds the gcov binary matching a given g++ binary's major version
# (e.g. g++-15 -> "15" -> gcov-15) - lcov silently produces garbage/empty
# coverage when fed a gcov from a different GCC major version than the
# one that compiled the .gcno files.
find_matching_gcov() {
    local gxx="$1"
    local ver
    ver="$("$gxx" -dumpversion 2>/dev/null | cut -d. -f1)"
    local candidate
    for candidate in "gcov-$ver" "gcov"; do
        if command -v "$candidate" >/dev/null 2>&1; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

run_coverage_report() {
    if ! command -v lcov >/dev/null 2>&1 || ! command -v genhtml >/dev/null 2>&1; then
        echo "error: lcov/genhtml not found on PATH." >&2
        echo "       install with: sudo apt install lcov" >&2
        exit 1
    fi

    local build_dir="build_coverage"
    local gcc_bin gxx_bin
    if ! read -r gcc_bin gxx_bin < <(find_newest_gcc_pair); then
        echo "error: no gcc/g++ pair found on PATH." >&2
        exit 1
    fi
    local gcov_tool
    if ! gcov_tool="$(find_matching_gcov "$gxx_bin")"; then
        echo "error: no gcov matching $gxx_bin's version found on PATH." >&2
        exit 1
    fi

    if [ "$CLEAN" = "1" ] || [ ! -d "$build_dir" ]; then
        "$SCRIPT_DIR/build.sh" coverage $([ "$CLEAN" = "1" ] && echo --clean)
    else
        echo "==> [coverage] Building with $JOBS jobs"
        cmake --build "$build_dir" -j"$JOBS"
    fi

    # No test target exists yet (this is prep for the "write tests"
    # roadmap step) - ctest with zero registered tests is a fast no-op,
    # so it's simplest to just always try it rather than special-case
    # "there are no tests" detection that will go stale the moment tests
    # do show up.
    echo "==> [coverage] Running ctest (no-op until tests exist)"
    (cd "$build_dir" && ctest --output-on-failure) || true

    local gcda_count
    gcda_count="$(find "$build_dir" -name '*.gcda' | wc -l)"
    if [ "$gcda_count" -eq 0 ]; then
        echo "warning: no .gcda files under $build_dir - nothing has" >&2
        echo "         actually run yet. Run $build_dir/familiar (or a" >&2
        echo "         test binary, once those exist) at least once," >&2
        echo "         then re-run: ./build.sh coverage-report" >&2
        exit 1
    fi

    echo "==> [coverage] Capturing coverage data"
    lcov --capture \
         --directory "$build_dir" \
         --base-directory "$SCRIPT_DIR" \
         --gcov-tool "$gcov_tool" \
         --rc branch_coverage=1 \
         --ignore-errors mismatch,negative,unused,empty,inconsistent \
         --output-file "$build_dir/coverage.info"

    echo "==> [coverage] Filtering out system/vendored/generated code"
    lcov --remove "$build_dir/coverage.info" \
         '/usr/*' \
         "$SCRIPT_DIR/include/quill/*" \
         "$SCRIPT_DIR/include/miniz/*" \
         "$build_dir/*" \
         --rc branch_coverage=1 \
         --ignore-errors unused \
         --output-file "$build_dir/coverage.filtered.info"

    echo "==> [coverage] Rendering HTML report"
    genhtml "$build_dir/coverage.filtered.info" \
            --output-directory "$build_dir/coverage_html" \
            --title "familiar coverage" \
            --legend \
            --rc branch_coverage=1

    echo "==> [coverage] Report: $build_dir/coverage_html/index.html"
}

if [ "$MODE" = "coverage-report" ]; then
    run_coverage_report
    exit 0
fi

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
