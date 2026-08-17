# familiar

Reference board for 2D/3D artists — a canvas for collecting, arranging,
and annotating reference images. Qt6/C++.

## NEW! TABS!
https://user-images.githubusercontent.com/9413534/132137667-e4bb966c-191d-4e1d-b89d-6fd50d6387b5.mp4

## Example
https://user-images.githubusercontent.com/9413534/129893087-aacb513a-322b-4bdb-b9e8-8482acb09689.mp4

## Building

Requires Qt6 (Widgets + Network) and CMake. Build via `./build.sh`, not
a manual `cmake`/`make` invocation — it also links `./compile_commands.json`
to whichever mode you last built, for clangd/ccls.

```sh
./build.sh {release|debug|asan|coverage} [--clean] [extra cmake args...]
```

| Mode | Build dir | What it is |
|---|---|---|
| `release` | `build/` | `RelWithDebInfo` |
| `debug` | `build_debug/` | `Debug` |
| `asan` | `build_asan/` | `Debug` + AddressSanitizer/UBSan |
| `coverage` | `build_coverage/` | `Debug` + `--coverage` instrumentation, forced onto a matched GCC/gcov pair regardless of the other modes' default compiler (see `build.sh`'s own comments — mixing GCC/Clang gcov output isn't supported) |

`--clean` wipes that mode's build directory first and reconfigures from
scratch (needed when switching compilers/flags, or if a build is stuck).
Extra args are forwarded to the `cmake` configure step, e.g.:

```sh
./build.sh debug --clean
```

The first configure of any mode needs network access once, to fetch
GoogleTest via `FetchContent` (see [Tests](#tests) below) — after that
it's cached and configures offline.

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
