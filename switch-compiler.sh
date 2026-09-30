#!/usr/bin/env bash
# Switches the SYSTEM-WIDE default compiler (via update-alternatives)
# between GCC and Clang: /usr/bin/cc and /usr/bin/c++ always, plus
# /usr/bin/gcc and /usr/bin/g++ when switching to GCC. Affects every
# project on this machine, not just familiar - that's the whole point:
# build.sh's debug/release/asan modes never pin a compiler themselves,
# they just take whatever this points to.
#
# For a single build instead of the whole machine (no sudo, nothing
# system-wide), name the compiler to build.sh directly:
#   ./build.sh debug clang
#
# Usage: sudo ./switch-compiler.sh {gcc|clang}
#
# Needs sudo - update-alternatives writes to /etc/alternatives.
#
# Picks the newest installed -N pair for the chosen toolchain (mirrors
# build.sh's own find_newest_gcc_pair()), not just whichever one
# update-alternatives currently has registered with the highest
# priority - so `switch-compiler.sh gcc` reliably means "the newest gcc
# on this box", not whatever priority numbers happen to say today.
#
# Remember: any build dir configured before the switch keeps using
# whatever compiler CMake resolved on ITS first configure - CMake caches
# that path and never rechecks it. `./build.sh <mode> --clean` after
# switching to actually pick up the new default.

set -euo pipefail

usage() {
    echo "Usage: sudo $0 {gcc|clang} [version]" >&2
    echo >&2
    echo "  version  major version number, e.g. 18. Omit for the newest installed." >&2
    echo >&2
    echo "Installed:" >&2
    for n in gcc clang; do
        printf '  %-6s' "$n"
        # shellcheck disable=SC2012
        ls /usr/bin/"$n"-* 2>/dev/null |
            sed "s|.*/$n-||" | grep -xE '[0-9]+' | sort -n | tr '\n' ' '
        echo
    done >&2
    exit 1
}

[ $# -ge 1 ] && [ $# -le 2 ] || usage
TOOLCHAIN="$1"
VERSION="${2:-}"

case "$VERSION" in
    '' | *[0-9]) ;;
    *)
        echo "error: version must be a number, got '$VERSION'." >&2
        exit 1
        ;;
esac

if [ "$(id -u)" -ne 0 ]; then
    echo "error: needs sudo (update-alternatives writes to /etc/alternatives)." >&2
    exit 1
fi

# Newest installed "-N" pair for the given cc/cxx binary names - not just
# the unversioned name, which isn't guaranteed to be the newest one on a
# machine with several versions installed side by side.
find_newest_pair() {
    local cc_prefix="$1" cxx_prefix="$2"
    local best_ver=-1 best_cxx="" best_cc=""
    local cxx ver cc_candidate
    for cxx in /usr/bin/"$cxx_prefix"-*; do
        [ -x "$cxx" ] || continue
        ver="${cxx##*-}"
        case "$ver" in
            ''|*[!0-9]*) continue ;;
        esac
        cc_candidate="/usr/bin/$cc_prefix-$ver"
        [ -x "$cc_candidate" ] || continue
        if [ "$ver" -gt "$best_ver" ]; then
            best_ver="$ver"
            best_cxx="$cxx"
            best_cc="$cc_candidate"
        fi
    done
    if [ -z "$best_cxx" ]; then
        command -v "$cxx_prefix" >/dev/null 2>&1 && command -v "$cc_prefix" >/dev/null 2>&1 || return 1
        best_cxx="$(command -v "$cxx_prefix")"
        best_cc="$(command -v "$cc_prefix")"
    fi
    echo "$best_cc" "$best_cxx"
}

case "$TOOLCHAIN" in
    gcc)   CC_NAME=gcc   CXX_NAME=g++ ;;
    clang) CC_NAME=clang CXX_NAME=clang++ ;;
    *)     usage ;;
esac

CC_BIN="" CXX_BIN=""
if [ -n "$VERSION" ]; then
    # Exact version asked for - both halves must exist, and the error has
    # to name which one is missing: `clang-18` without `clang++-18` is a
    # real state (the clang package splits them), and "not found" alone
    # sends you looking in the wrong place.
    CC_BIN="/usr/bin/$CC_NAME-$VERSION"
    CXX_BIN="/usr/bin/$CXX_NAME-$VERSION"
    for bin in "$CC_BIN" "$CXX_BIN"; do
        if [ ! -x "$bin" ]; then
            echo "error: $bin not found." >&2
            echo "       Installed $TOOLCHAIN versions:" >&2
            # shellcheck disable=SC2012
            ls /usr/bin/"$CC_NAME"-* 2>/dev/null |
                sed "s|.*/$CC_NAME-||" | grep -xE '[0-9]+' | sort -n |
                sed 's/^/         /' >&2
            exit 1
        fi
    done
else
    read -r CC_BIN CXX_BIN < <(find_newest_pair "$CC_NAME" "$CXX_NAME")
fi

if [ -z "$CC_BIN" ]; then
    echo "error: no $TOOLCHAIN pair found on PATH." >&2
    exit 1
fi

echo "==> Switching system cc/c++ to $TOOLCHAIN ($CC_BIN / $CXX_BIN)"
update-alternatives --set cc "$CC_BIN"
update-alternatives --set c++ "$CXX_BIN"

# cc/c++ are not the only groups involved: each toolchain ALSO has its
# own pair (gcc/g++, clang/clang++), and they're independent. Setting
# just cc/c++ - all this script used to do - leaves `gcc -v` / `clang -v`
# reporting whatever version those groups were left on, and anything
# invoking the compiler by name (a Makefile, CC=gcc, a stray script)
# silently gets it. Both halves of that were observed for real here:
# after "switched to gcc-15", gcc -v said 13.4.0; after "switched to
# clang-21", clang -v said 18.1.8.
#
# Not cosmetic: g++-13's bundled libstdc++.so on this machine predates
# the CXXABI_1.3.15 symbol the system libstdc++6 expects, which breaks
# linking outright - see build.sh's find_newest_pair() comment.
#
# Driven off the toolchain's own binary names, so gcc updates gcc/g++
# and clang updates clang/clang++. Cross-setting is neither possible nor
# wanted - those groups only carry their own compiler as candidates,
# and `gcc` should never run clang.
if update-alternatives --list "$CC_NAME" >/dev/null 2>&1; then
    echo "==> Switching system $CC_NAME/$CXX_NAME to the same pair"
    update-alternatives --set "$CC_NAME" "$CC_BIN"
    update-alternatives --set "$CXX_NAME" "$CXX_BIN"
fi

# All six, not just the pair that changed - an inconsistency between
# groups is exactly the failure this section exists to prevent, and it's
# only visible when they're listed side by side.
echo "==> Now resolving to:"
for link in cc c++ gcc g++ clang clang++; do
    [ -e "/usr/bin/$link" ] || continue
    # One hop through /etc/alternatives, NOT readlink -f. Resolving all
    # the way gives a misleading answer for clang: clang++ is a symlink
    # to the clang binary itself (it picks C vs C++ mode from the name
    # it was invoked as), so a fully-resolved c++ reads ".../bin/clang"
    # and looks like the C compiler got installed in the wrong slot.
    # The alternative's own target - clang++-21 - is the useful answer.
    target="$(readlink "/etc/alternatives/$link" 2>/dev/null ||
        readlink -f "/usr/bin/$link")"
    printf '      %-7s -> %s\n' "$link" "$target"
done

echo "==> Done. Existing build dirs won't notice by themselves - reconfigure with:"
echo "      ./build.sh <mode> --clean"
