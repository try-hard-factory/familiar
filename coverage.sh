#!/usr/bin/env bash
# Coverage check - modeled after ms-realm's check_tests_coverage.sh:
# build with instrumentation, run the test suite via the app's own -t
# flag (no separate test binary/ctest - see main.cpp's FAMILIAR_BUILD_TESTS
# block and src/CMakeLists.txt), capture with lcov, render with genhtml.
#
# Usage: ./coverage.sh [--clean]
#   --clean forwards to build.sh coverage (wipe+reconfigure build_coverage/
#   from scratch - use when switching compilers/flags).

DoOrDie() {
    ec=$?
    [ $ec -ne 0 ] && {
        echo -e "\n\033[0;31m    Failed (error code = $ec): \033[0m $*\n"
        exit $ec
    }
    echo -e "\n\033[1;32m    Successful: \033[0m $*\n"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_DIR="${SCRIPT_DIR}/build_coverage"
REPORTS_DIR="${SCRIPT_DIR}/coverage_reports"

if ! command -v lcov >/dev/null 2>&1 || ! command -v genhtml >/dev/null 2>&1; then
    echo "error: lcov/genhtml not found on PATH." >&2
    echo "       install with: sudo apt install lcov" >&2
    exit 1
fi

# Same pairing logic as build.sh's own `coverage` mode (duplicated, not
# sourced - keeps this script runnable standalone) - lcov needs a gcov
# from the SAME GCC major version that compiled the .gcno files, and the
# unversioned `gcov` on PATH isn't guaranteed to match the unversioned
# `g++` build.sh actually built with. See build.sh's own comment on
# find_newest_gcc_pair for the concrete failure this avoids
# (CXXABI_1.3.15 link error from a version-mismatched g++/libstdc++ pair).
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

find_matching_gcov() {
    local gxx="$1" ver candidate
    ver="$("$gxx" -dumpversion 2>/dev/null | cut -d. -f1)"
    for candidate in "gcov-$ver" "gcov"; do
        if command -v "$candidate" >/dev/null 2>&1; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

GCC_BIN="" GXX_BIN=""
read -r GCC_BIN GXX_BIN < <(find_newest_gcc_pair)
DoOrDie "Locate matching gcc/g++ pair"

GCOV_TOOL="$(find_matching_gcov "$GXX_BIN")"
DoOrDie "Locate gcov matching $GXX_BIN"

mkdir -p "$REPORTS_DIR"

./build.sh coverage "$@"
DoOrDie "Build familiar (coverage instrumentation)"

# Wipes previous .gcda so this run's numbers aren't mixed with a stale
# prior run's (a rebuild alone doesn't remove them - gcov data
# accumulates across runs of the same binary otherwise).
find "$BUILD_DIR" -name '*.gcda' -delete

"${BUILD_DIR}/familiar" -t
DoOrDie "Run familiar's GoogleTest suite (familiar -t)"

lcov --capture \
     --directory "$BUILD_DIR" \
     --base-directory "$SCRIPT_DIR" \
     --gcov-tool "$GCOV_TOOL" \
     --rc branch_coverage=1 \
     --ignore-errors mismatch,negative,unused,empty,inconsistent \
     --output-file "${REPORTS_DIR}/familiar_tests.info"
DoOrDie "Generate coverage report"

lcov --remove "${REPORTS_DIR}/familiar_tests.info" \
     '/usr/*' \
     '/opt/*' \
     "${SCRIPT_DIR}/include/quill/*" \
     "${SCRIPT_DIR}/include/miniz/*" \
     "${BUILD_DIR}/*" \
     --rc branch_coverage=1 \
     --ignore-errors mismatch,negative,unused,empty,inconsistent \
     --output-file "${REPORTS_DIR}/familiar_tests.filtered.info"
DoOrDie "Filter out system/vendored/generated code"

genhtml "${REPORTS_DIR}/familiar_tests.filtered.info" \
        --output-directory "${REPORTS_DIR}/html" \
        --title "familiar coverage" \
        --legend \
        --ignore-errors mismatch,negative,unused,empty,inconsistent \
        --rc branch_coverage=1
DoOrDie "Render HTML report"

echo "Open ${REPORTS_DIR}/html/index.html for details"
