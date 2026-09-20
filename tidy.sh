#!/usr/bin/env bash
# clang-tidy helper.
#
# Usage: ./tidy.sh [naming|fast|full] [--fix] [extra run-clang-tidy args...]
#
# Runs clang-tidy over THIS project's own sources (src/*.cpp) using the
# repo's .clang-tidy, writes a timestamped log, and prints a summary
# grouped by check.
#
#   naming  -> only readability-identifier-naming. Minutes. This is the
#              one to use while working through the naming convention.
#   fast    -> everything in .clang-tidy EXCEPT clang-analyzer-*.
#              (default)
#   full    -> everything in .clang-tidy, clang-analyzer-* included.
#              Expect 15-20+ minutes: clang-analyzer does whole-path,
#              cross-procedural analysis and is an order of magnitude
#              slower than every other check combined.
#
# --fix applies clang-tidy's own fixits in place. Commit or stash first;
# renames in particular touch every usage site across the tree.
#
# WARNING about --fix: clang-tidy only rewrites what the compiler sees.
# Method names looked up as STRINGS are not touched, and breakage shows
# up at runtime, not at build time. This project does that in
# actions/action_mixin.h and actions/action_mouse_dispatch.cpp
# (QMetaObject::invokeMethod with a name from Action::callback). Those
# names are all on_action_* today, which the current config leaves
# alone - but check before trusting a bulk rename.

set -u

MODE="${1:-fast}"
case "$MODE" in
    naming | fast | full) shift || true ;;
    -*) MODE="fast" ;; # first arg was a flag, not a mode
    *)
        echo "unknown mode: $MODE (expected naming|fast|full)" >&2
        exit 1
        ;;
esac

cd "$(dirname "$0")" || exit 1
ROOT="$PWD"

for tool in run-clang-tidy clang-tidy; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "$tool not found in PATH" >&2
        exit 1
    }
done

# clang-tidy needs the compilation database to know how each file is
# built (include paths, defines, standard). Without -p pointing at one,
# it falls back to guessing and reports nothing useful.
if [ ! -f "$ROOT/compile_commands.json" ]; then
    echo "compile_commands.json missing - generate it with:" >&2
    echo "  cmake -B build_debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
    exit 1
fi

# Stale database = clang-tidy analysing the project as it was configured,
# not as it is now (wrong standard, missing sources, stale flags). Only a
# warning: a slightly old database still mostly works.
if [ "$ROOT/CMakeLists.txt" -nt "$ROOT/compile_commands.json" ] ||
    [ "$ROOT/src/CMakeLists.txt" -nt "$ROOT/compile_commands.json" ]; then
    echo "WARNING: compile_commands.json is older than the CMakeLists it came from."
    echo "         Re-run cmake if flags or the C++ standard changed."
    echo
fi

CHECKS=()
case "$MODE" in
    naming) CHECKS=(-checks='-*,readability-identifier-naming') ;;
    # Subtractive, NOT a replacement: .clang-tidy is still read and
    # applied, this only removes one family from whatever it enabled.
    fast) CHECKS=(-checks='-clang-analyzer-*') ;;
    full) CHECKS=() ;;
esac

# Anchored at the start on purpose. Without ^, this also matches
# include/libraw/src/**.cpp - vendored code, whose findings are none of
# our business and drown out ours (measured: 1100+ of them).
FILE_RE='^'"$ROOT"'/src/.*\.cpp$'

LOG="$ROOT/tidy-$MODE-$(date +%Y%m%d-%H%M%S).log"

echo "mode:    $MODE"
echo "files:   $(python3 -c "
import json,re,sys
db=json.load(open('$ROOT/compile_commands.json'))
print(sum(1 for e in db if re.search(r'$FILE_RE', e['file'])))
" 2>/dev/null || echo '?')"
echo "log:     $LOG"
echo "jobs:    $(nproc)"
echo

# tee so the run is watchable AND kept - a full run is long enough that
# a terminal with no output reads as a hang (it isn't; the first result
# can take ~30s to appear).
# -Wno-unknown-warning-option: compile_commands.json normally comes
# from a GCC build, and cmake/CompilerWarnings.cmake's GCC_WARNINGS adds
# flags clang's driver doesn't know (-Wduplicated-cond,
# -Wduplicated-branches, -Wlogical-op, -Wuseless-cast). That alone is
# just a warning, but the same command line carries -Werror, so every
# single file died with "Found compiler error(s)" before clang-tidy got
# to run a check at all - a whole 60-file run producing nothing but that.
run-clang-tidy -p "$ROOT" -j"$(nproc)" \
    -extra-arg=-Wno-unknown-warning-option \
    "${CHECKS[@]}" "$@" "$FILE_RE" 2>&1 |
    tee "$LOG"

echo
echo "───────────────────────────────────────────────────────────"

# Deduplicated: the same header is re-analysed for every .cpp that
# includes it, so the raw line count overstates the real work by a lot
# (measured on this project: 13939 hits against 686 distinct places).
UNIQ=$(grep -hoE '^[^ ]+:[0-9]+:[0-9]+: (warning|error): .*' "$LOG" 2>/dev/null |
    sed "s|^$ROOT/||" | grep -vE '^(include|build_|obj-)/' | sort -u)

if [ -z "$UNIQ" ]; then
    echo "no findings"
    exit 0
fi

echo "distinct findings in our own sources: $(echo "$UNIQ" | wc -l)"
echo
echo "by check:"
echo "$UNIQ" | grep -oE '\[[a-z][a-z0-9.,-]+\]$' | sort | uniq -c | sort -rn | head -25
echo
echo "by file (top 15):"
echo "$UNIQ" | cut -d: -f1 | sort | uniq -c | sort -rn | head -15
echo
echo "full list: grep -E 'warning:|error:' $LOG | sort -u | less"
