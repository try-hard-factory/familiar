# familiar

![Build](https://img.shields.io/github/actions/workflow/status/try-hard-factory/familiar/build_cmake.yml?label=build)
![Windows Pack](https://img.shields.io/github/actions/workflow/status/try-hard-factory/familiar/Windows-pack.yml?label=windows%20package)
![Release](https://img.shields.io/github/v/release/try-hard-factory/familiar)
![Downloads](https://img.shields.io/github/downloads/try-hard-factory/familiar/total)
![License](https://img.shields.io/github/license/try-hard-factory/familiar)
![Issues](https://img.shields.io/github/issues/try-hard-factory/familiar)
![Stars](https://img.shields.io/github/stars/try-hard-factory/familiar)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)

Reference board for 2D/3D artists — a canvas for collecting, arranging,
and annotating reference images. Qt6/C++.


## Building

Requires Qt6 (Widgets + Network) and CMake. Build via `./build.sh`, not
a manual `cmake`/`make` invocation — it also links `./compile_commands.json`
to whichever mode you last built, for clangd/ccls.

```sh
./build.sh [mode] [compiler] [generator] [jobs] [--clean]
```

Every argument is optional and **order doesn't matter** — each is
recognised by what it is, not by where it sits:

| Argument | Values | Default |
|---|---|---|
| mode | `release` `debug` `asan` `coverage` | `debug` |
| compiler | `gcc` `clang` | whatever `/usr/bin/c++` points at |
| generator | `ninja` `make` | `ninja` if installed, else `make` |
| jobs | number `1`–`99`, e.g. `8` | `nproc` |
| `--clean` | — | off |

```sh
./build.sh                       # debug, default compiler
./build.sh release ninja
./build.sh debug clang ninja 8
./build.sh 4 asan --clean
```

Any other argument is rejected with `Invalid flag <...>` and exit code 1.

| Mode | Build dir | What it is |
|---|---|---|
| `release` | `build/` | `RelWithDebInfo` |
| `debug` | `build_debug/` | `Debug` |
| `asan` | `build_asan/` | `Debug` + AddressSanitizer/UBSan |
| `coverage` | `build_coverage/` | `Debug` + `--coverage` instrumentation, forced onto a matched GCC/gcov pair regardless of the other modes' default compiler (see `build.sh`'s own comments — mixing GCC/Clang gcov output isn't supported) |

Naming a compiler explicitly appends a suffix — `./build.sh debug clang`
builds in `build_debug_clang/` and leaves `build_debug/` alone. Both
trees then coexist, so switching back and forth costs nothing: CMake
caches the compiler in its own config and never re-checks it, so sharing
one directory would force a full rebuild on every switch.

Picking a compiler here affects only that one build — it doesn't touch
`update-alternatives` and needs no `sudo`. For the machine-wide default
instead, see [Switching the system compiler](#switching-the-system-compiler).

`--clean` wipes that build directory first and reconfigures from scratch
(needed when switching generator, or if a build is stuck).

CMake can't change generator inside an existing build tree. A directory
configured before Ninja became the default keeps using Make, with a note
saying so — only an *explicit* `ninja`/`make` that the tree can't satisfy
is an error, and `--clean` is then the fix.

The first configure of any mode needs network access once, to fetch
GoogleTest via `FetchContent` (see [Tests](#tests) below) — after that
it's cached and configures offline.

### Switching the system compiler

Changes the machine-wide default rather than one build, so most of the
time `./build.sh debug clang` is what you actually want.

```sh
sudo ./switch-compiler.sh clang        # newest installed clang
sudo ./switch-compiler.sh gcc 13       # a specific major version
./switch-compiler.sh                   # lists what's installed
```

Repoints `cc`/`c++` via `update-alternatives`, plus the toolchain's own
pair — `gcc`/`g++` or `clang`/`clang++`. Those are six independent
alternatives groups: setting only `cc`/`c++` leaves `gcc -v` / `clang -v`
reporting the old version, and anything invoking the compiler by name
silently gets it. The script prints all six afterwards, so a mismatch is
visible rather than lurking.

Build directories configured before the switch keep using whatever
compiler CMake resolved on their first configure — it caches the path and
never re-checks. Reconfigure with `./build.sh <mode> --clean` afterwards.

Note that `c++` resolving to a path ending in `clang` is correct, not a
mistake: `clang++` is a symlink to the same binary, which picks C or C++
mode from the name it was invoked as.

## Tests

The GoogleTest suite is linked directly into the `familiar` binary
itself (every build mode, not just `coverage`) rather than a separate
test executable. Run it with a `-t` flag in place of the normal GUI
startup:

```sh
./build.sh debug
build_debug/familiar -t
```

Standard GoogleTest flags work too (`--gtest_filter=...`,
`--gtest_list_tests`, ...). Test sources live under `tests/`, listed in
`tests/FamiliarTestsEmbed.cmake` — add a new `list(APPEND FamiliarTestsSrc ...)`
entry there when adding a test file, no need to touch `src/CMakeLists.txt`.

## Coverage

```sh
./coverage.sh
```

Builds the `coverage` mode, runs `familiar -t`, and renders an HTML lcov
report to `coverage_reports/html/index.html`. Requires `lcov`/`genhtml`
on `PATH` (`sudo apt install lcov` on Debian/Ubuntu).

## Formatting

```sh
./format.sh           # rewrite in place
./format.sh --check   # report only, non-zero exit if anything differs
```

Uses [`.clang-format`](.clang-format) at the repo root over `src/` and
`tests/`.

## Static analysis

```sh
./tidy.sh naming   # only readability-identifier-naming — minutes
./tidy.sh          # everything except clang-analyzer-* (default)
./tidy.sh full     # everything, clang-analyzer-* included — 15-20+ min
./tidy.sh naming --fix   # apply clang-tidy's own fixits
```

Runs clang-tidy over this project's own sources using
[`.clang-tidy`](.clang-tidy), writes a timestamped log, and prints a
summary grouped by check and by file. Needs `compile_commands.json`,
which `./build.sh` links automatically.

`clang-analyzer-*` is what makes a run long — it does whole-path,
cross-procedural analysis and dominates the runtime, hence the default
excluding it.

Counts are deduplicated in the summary: a header is re-analysed for every
`.cpp` that includes it, so the raw hit count overstates the real work
several times over.

### Naming conventions

Encoded in `.clang-tidy`, so they're checkable rather than folklore:

| Kind | Style | Example |
|---|---|---|
| Functions, methods | `snake_case` | `add_queued_items()` |
| Overrides of Qt virtuals | `camelCase` (forced by Qt) | `paintEvent()` |
| Private/protected members | `camelCase_` | `imageImportWorker_` |
| Parameters, locals | `camelCase` | `knownMaximum` |
| Types, classes, `enum class` values | `PascalCase` | `ProgressDialog`, `TooLarge` |
| Constants | `kPascalCase` | `kLargeImageMaxDimension` |

The rule is *functions are actions, data is data* — not "ours vs Qt":
members are ours too and are `camelCase_`.

Methods overriding a Qt virtual are exempt automatically — clang-tidy
skips any method that overrides a base declaration, since renaming one
would break the override rather than fix a name.

Public members are deliberately unchecked. The trailing underscore marks
"state owned by an object", which is wrong for a plain aggregate
(`LoadedImage{image, bytes}`), and clang-tidy can't tell an aggregate
from a class with behaviour.

### Compiler warnings

The flag set lives in
[`cmake/CompilerWarnings.cmake`](cmake/CompilerWarnings.cmake) and
applies to the `familiar` target only — vendored submodules under
`include/` are compiled as `SYSTEM` headers so their findings stay out of
the way.

`-Werror` (`/WX` on MSVC) is **on by default**, for every compiler — a
warning nobody is forced to look at is a warning that accumulates.
