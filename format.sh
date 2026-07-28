#!/usr/bin/env bash
# Formatting helper.
#
# Usage: ./format.sh [--check]
#
# Recursively formats every .cpp/.h file under src/ in place, using the
# .clang-format config at the repo root (clang-format finds it by walking
# up from each file, but -style=file makes that explicit rather than
# silently falling back to some default if the config ever goes missing).
#
# Pass --check to run in dry-run mode instead: reports which files are
# not already formatted (via clang-format's --dry-run/--Werror) without
# touching them, and exits non-zero if any aren't - for CI/pre-commit use.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CHECK=0
if [ "${1:-}" = "--check" ]; then
    CHECK=1
fi

mapfile -d '' -t FILES < <(find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0)

if [ "${#FILES[@]}" -eq 0 ]; then
    echo "No .cpp/.h files found under src/." >&2
    exit 1
fi

if [ "$CHECK" -eq 1 ]; then
    clang-format -style=file --dry-run --Werror "${FILES[@]}"
    echo "==> All ${#FILES[@]} files already formatted."
else
    clang-format -style=file -i "${FILES[@]}"
    echo "==> Formatted ${#FILES[@]} files."
fi
