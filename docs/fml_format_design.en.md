# `.fml` project file format design (zip container)

Status: approved design, ready for implementation.
Date: 2026-07-26.

This document is a spec for implementation. Every file and symbol
mentioned here existed in the codebase at the time of writing (verified).
The one thing the implementer needs to check independently is the exact
miniz API signatures at vendoring time (API details are deliberately not
pinned down in this document).

---

## 1. Goals

- Replace the temporary binary QDataStream format (`fml_file_buffer.h`)
  and the `load_fml`/`save_fml` stubs (`src/fileio.cpp:80-99`) with a
  permanent format: **a zip container with `manifest.json` and separate
  image files**.
- Preserve the data semantics already adopted in the codebase (ported
  from beeref): items with fields `type, x, y, z, scale, rotation, flip,
  data` — see beeref's schema (`beeref/fileio/schema.py`) and its
  consumer `CanvasScene::add_queued_items()` (`src/canvasscene.cpp:971`).
- Load/save on a background thread via the existing `ThreadedIO`
  (`src/fileio.h:31`) with progress and cancellation.
- Format evolution without manual block versioning: old code silently
  ignores new JSON fields; an unknown `type` → `ErrorItem` (already
  implemented in `add_queued_items`).

## 2. Non-goals (v1)

- Incremental rewriting of changed images only (fully rewriting the
  archive on every save is acceptable).
- Deduplicating identical images by hash.
- An embedded thumbnail for file-manager previews.
- Undo compatibility and grouping (future roadmap phases).
These items are listed in §13 as groundwork the format already allows
for, without a breaking change.

---

## 3. Container and library

**Container:** a plain zip. Rename it to `.zip`, open it, read it with
your own eyes.

**Library: miniz, vendored into `include/miniz/`** — the same way
`include/quill` and `include/kColorPicker` are already vendored
(`CMakeLists.txt:53-57`). Reasons for this choice:

- KArchive (KZip) rejected: pulls in KDE Frameworks — an unacceptable
  dependency for a small project that also builds under MSVC (see the
  recent "MSVC try fix" commits).
- QZipWriter/QZipReader rejected: Qt private headers, break on Qt
  version changes.
- miniz: 1-2 files (`miniz.h`/`miniz.c`), MIT license, compiles as C
  everywhere, including MSVC.

For the implementer: take the latest miniz release from
https://github.com/richgel999/miniz, put it in `include/miniz/`, wire it
into `src/CMakeLists.txt` (compile `miniz.c` as part of the target or as
a separate static library — implementer's choice).

**Write parameters:**
- `manifest.json` — with compression (deflate, default level).
- `images/*` — **uncompressed** (store): PNG/JPG are already compressed;
  deflating them on top wastes CPU and slows down reading.

---

## 4. Archive layout

```
project.fml  (zip)
├── manifest.json          — the entire scene structure
└── images/
    ├── <uid>.png          — name = the item's uid (§5.1), format from get_imgformat()
    ├── <uid>.jpg
    └── ...
```

Rules:
- Paths inside the archive — ASCII only, forward slashes, no leading `/`.
- Image filename: `images/<uid>.<ext>`, where `uid` is the item's
  permanent UUID (§5.1), `<ext>` is the format from `get_imgformat()`.
  Stable name across saves: an unchanged item produces an identical
  entry — groundwork for incremental rewriting (§13).
- The archive must not contain files the manifest doesn't reference
  (a full rewrite achieves this automatically).

## 5. `manifest.json` schema (formatVersion = 1)

```json
{
  "format": "familiar",
  "formatVersion": 1,
  "appVersion": "0.1.0",
  "scene": {},
  "items": [
    {
      "id": "9f1c2d34-5a6b-4c7d-8e9f-0a1b2c3d4e5f",
      "type": "pixmap",
      "x": -120.5,
      "y": 40.0,
      "z": 0.31,
      "scale": 1.0,
      "rotation": 0.0,
      "flip": 1,
      "image": "images/9f1c2d34-5a6b-4c7d-8e9f-0a1b2c3d4e5f.png",
      "data": {
        "filename": "cat.jpg",
        "opacity": 1.0,
        "grayscale": false,
        "crop": [0, 0, 800, 600]
      }
    },
    {
      "id": "2b7e1512-28ae-4d2a-abf7-158809cf4f3c",
      "type": "text",
      "x": 10.0,
      "y": 20.0,
      "z": 0.32,
      "scale": 1.5,
      "rotation": 0.0,
      "flip": 1,
      "data": { "text": "Note" }
    }
  ]
}
```

Field notes:

| Field | Where it comes from | Note |
|---|---|---|
| `format` | constant `"familiar"` | identification marker; checked on load |
| `formatVersion` | constant `1` | integer; policy in §6 |
| `appVersion` | app version from CMake | diagnostics only, no effect on logic |
| `scene` | empty object for now | reserved for scene settings (background color etc.) |
| `items[].id` | the item's permanent UUID (§5.1) | string, lowercase, no braces; also the image filename in the archive |
| `items[].type` | `IBaseItem::get_type()` | `"pixmap"` / `"text"`; new types are just new values |
| `x, y, z, scale, rotation, flip` | common `QGraphicsItem` properties + `flip()` | the exact same six fields as beeref's schema; `flip` ∈ {1, -1} |
| `image` | pixmap items only | path inside the archive |
| `data` | the item's `get_extra_save_data()` | type-specific dict; see keys at `src/moveitem.h:217` (pixmap) and `src/moveitem.h:801` (text) |

Note on `crop`: keep the `[x, y, w, h]` order — exactly how
`PixmapItem::get_extra_save_data()` writes it and `add_queued_items()`
reads it.

### 5.1. Item identity: a permanent UUID instead of save_id

In beeref, `save_id` is the SQLite rowid (`items.id INTEGER PRIMARY
KEY`), needed for incremental UPDATE/DELETE of rows; a new item doesn't
have one until its first INSERT — hence the `std::optional` and all the
`has_save_id()/clear_save_ids()` machinery. In a zip format that fully
rewrites the archive, that model isn't needed. Instead:

- Every item gets a permanent `QUuid uid_`, generated **once, in the
  constructor** (`QUuid::createUuid()`). Never empty, never reset, no
  renumbering on save.
- Serialization: `uid_.toString(QUuid::WithoutBraces)` — lowercase, no
  braces; the same value is used as the image's filename in the archive.
- **Loading**: the uid is restored from the file. Missing or duplicated
  within one manifest (= a corrupt file) — log a warning and generate a
  new one; don't crash.
- **Copying — a critical rule**: `create_copy()` and any future
  paste/duplicate ALWAYS generate a new uid (i.e. the copy's constructor
  does this itself; uid is not among the copied properties). This is the
  one place a uuid could accidentally get duplicated.
- **ErrorItem** keeps the original item's uid (field `original_save_id`
  at `src/moveitem.h:918` is renamed to `original_uid`, type `QUuid`) —
  round-tripping through an older app version doesn't change the item's
  identity.
- `IBaseItem` interface change: the methods
  `has_save_id()/get_save_id()/set_save_id()/clear_save_id()` and
  `CanvasScene::clear_save_ids()` (`src/canvasscene.cpp:861`) are
  removed; replaced with `QUuid uid() const` + `void
  set_uid(const QUuid&)` (set — only used to restore state on load).
- `get_filename_for_export()` (`src/moveitem.h:231`) moves from
  `save_id` to uid, or to a sequence number generated on the spot —
  implementer's choice (the function is already marked TODOLATER).

Motivation for a stable uid, beyond simplification: an unchanged item
produces an identical image entry across saves (groundwork for
incremental rewriting and smaller diffs, §13), and stable references to
items for future grouping (a group refers to its children by uid).

## 6. Versioning and compatibility

- **Unknown JSON field** — silently ignored (both on read, and
  therefore adding fields doesn't require a version bump).
- **Unknown `type`** — create an `ErrorItem` with a message (already
  implemented in `add_queued_items`). TODOLATER (already flagged at
  `src/moveitem.h:966`): store the item's original JSON inside
  `ErrorItem` and write it back on save, so a file from a newer version
  doesn't lose data when edited in an older one. A visible placeholder is
  enough for v1.
- **`formatVersion` newer than supported** — refuse to load with a clear
  error ("this file was created by a newer version of familiar").
- **Bumping `formatVersion`** — only for an incompatible change to the
  semantics of existing fields (not for adding new ones). Migrations —
  following beeref's `MIGRATIONS` pattern: a dict of `{version:
  JSON-transform function}`.
- **File detection**: a zip starts with the bytes `PK\x03\x04`. If a
  `.fml` file doesn't start with them, it's the legacy format (§10) or
  garbage.

---

## 7. Code architecture

New module: `src/fml_archive.h` / `src/fml_archive.cpp`. A synchronous
core with no knowledge of threads or dialogs:

```cpp
// Result of an operation; error.isEmpty() == success.
struct FmlResult {
    QString error;          // fatal error (file unreadable, etc.)
    QStringList itemErrors; // non-fatal (a single image failed to decode)
};

class FmlArchive {
public:
    // Scene -> in-memory zip bytes. progress/canceled are optional.
    static FmlResult save(CanvasScene* scene, const QString& filename,
                          ThreadedIO* worker = nullptr);

    // zip file -> scene->add_item_later(...) queue.
    // Does NOT touch the QGraphicsScene directly (mutex-guarded queue
    // only) - safe to call from a background thread, like load_images
    // (src/fileio.cpp:29).
    static FmlResult load(const QString& filename, CanvasScene* scene,
                          ThreadedIO* worker = nullptr);
};
```

`load_fml`/`save_fml` in `src/fileio.cpp` become thin wrappers around
`FmlArchive` that emit `worker->finished(...)` — their signatures are
already aligned with beeref and don't need to change.

### 7.1. Isolating the JSON library (requirement)

Qt JSON types (`QJsonDocument`, `QJsonObject`, `QJsonArray`,
`QJsonValue`) **never leave `fml_archive.cpp`** — not in headers, not in
signatures, not anywhere else in the code. The manifest is represented
inside the module by plain DTO structs:

```cpp
struct ManifestItem {
    QUuid id;
    QString type;
    qreal x, y, z, scale, rotation;
    int flip;
    QString image;          // empty for non-pixmap items
    QVariantMap data;       // type-specific data (crop, text, ...)
};

struct Manifest {
    int formatVersion;
    QString appVersion;
    QList<ManifestItem> items;
};
```

The only two points of contact with the JSON library are two free
functions in `fml_archive.cpp`:

```cpp
std::optional<Manifest> parse_manifest(const QByteArray& json, QString& error);
QByteArray write_manifest(const Manifest& m);
```

Rationale: this is the "seam" for a possible future swap of QJson for
another parser — the swap comes down to rewriting the bodies of these
two functions in a single translation unit. A template/virtual wrapper
over the JSON API was deliberately rejected: DOM and on-demand parsers
aren't isomorphic in their APIs, a shared abstraction ends up leaky and
eats the fast backends' whole advantage. `data`'s keys remain a
schema-free `QVariantMap` — it goes into `add_queued_items()` in that
same shape.

Access to item data: `IBaseItem` already has `get_type()`. Interface
changes:
- **add a virtual `QVariantMap get_extra_save_data() const`** (currently
  only declared on the concrete classes `PixmapItem`, `TextItem`);
  check whether `flip()` is already on the interface, add it if not;
- **replace the save_id machinery with a permanent uid** — see §5.1
  (`has_save_id()/get_save_id()/set_save_id()/clear_save_id()` →
  `uid()/set_uid()`).

## 8. Save algorithm

1. `items = scene->items_for_save()` (returns items in
   `Qt::AscendingOrder` — a stable order in the manifest).
2. Build the item's JSON object: `id` = `uid()` (§5.1), common fields +
   `data` from `get_extra_save_data()`.
3. For `PixmapItem`, additionally: `pixmap_to_bytes()`
   (`src/moveitem.h:268`, format chosen by `get_imgformat()`), path
   `images/<uid>.<ext>` — write it into the archive (store, no deflate)
   and into the `image` field. IMPORTANT: call `pixmap_to_bytes()` with
   the default flags (`apply_grayscale=false, apply_crop=false`) -
   grayscale and crop are display-only properties, the original image is
   not modified.
4. `manifest.json` — from
   `QJsonDocument::toJson(QJsonDocument::Indented)` (the indentation
   costs pennies in size after deflate, and being eyeball-debuggable was
   one of the format's goals).
5. **Atomicity**: assemble the whole zip in memory (miniz can write to a
   heap buffer), then write it via `QSaveFile` + `commit()`. An
   interrupted save must not destroy the previous file. Memory estimate:
   a reference board with hundreds of images can reach hundreds of MB;
   acceptable for v1, an alternative (a temp file next to it + rename) is
   noted in §12.
6. Progress: `worker->beginProcessing(count)` before the loop,
   `worker->progress(i)` per item, `worker->canceled` — abort and do NOT
   commit the `QSaveFile`.
7. After success the caller (`FileActions`) does `setModified(false)`.

## 9. Load algorithm

1. Open the zip; if it's not a zip (no `PK\x03\x04`) — attempt a legacy
   read (§10), otherwise error.
2. Read and parse `manifest.json` (`QJsonDocument::fromJson` with
   `QJsonParseError`). Check `format == "familiar"` and
   `formatVersion <= 1`.
3. `worker->beginProcessing(items.size())`.
4. For each manifest item, build the exact `QVariantMap` shape expected
   by `add_queued_items()` (`src/canvasscene.cpp:988-1060`):
   - `type`, `x`, `y`, `z`, `scale`, `rotation`, `flip` — top level;
   - `id` (the uid from the file) — top level; extend `add_queued_items()`
     to call `set_uid()` after creating the item (restoration rules and
     handling a corrupt uid are in §5.1);
   - `data` — nested map (`crop`, `text`, …);
   - for `pixmap`: extract the file at the path from `image`, decode
     into a `QImage` (`QImage::fromData`), put it under the `image` key;
     `filename` comes from `data.filename`. If the image fails to
     decode, add it to `itemErrors` and skip the item (or produce an
     ErrorItem - implementer's choice, beeref adds an ErrorItem).
   - `scene->add_item_later(map, /*selected=*/false)`.
5. `worker->progress(i)` per item; honor `worker->canceled`.
6. `worker->finished(error, itemErrors)`; the GUI thread drains the
   queue with `add_queued_items()` on that signal - following the
   pattern of `CanvasView::do_insert_images` (mentioned at
   `src/fileio.h:72`).

Note: unlike beeref (where save_id is an SQLite rowid assigned after
INSERT), uid here is a permanent property of the item: it's restored on
load and doesn't change across saves (§5.1).

## 10. Legacy format and cleanup

Old binary format: `qint16 count`, then per item `QPointF scenePos,
qint32 h, w, QRectF br, quint16 format, quint64 size, qCompress'd data`
(`src/fml_file_buffer.h:48-79`).

Decision: **keep the legacy reader for one release** as a branch inside
`FmlArchive::load` (a file that doesn't start with `PK\x03\x04` → read
using the old scheme, convert items into the same `QVariantMap`s for the
queue; they won't have an `id` key — items get a fresh uid from their
constructor). Saving always goes to the new format only: open a legacy
file → save it → the file is now a zip. Remove the legacy reader in
roadmap phase 9 (the testing pass) or at the 1.0 release.

Cleanup after the migration:
- Delete `fml_file_buffer.h/.cpp`; the calls in `FileActions`
  (`src/file_actions.cpp:83,95-96,138`) move to `save_fml`/`load_fml`.
- Delete `CanvasScene::fml_payload()` (the stub,
  `src/canvasscene.cpp:1115`) and `create_payload` - payload building
  moves into `FmlArchive`.
- The `addImage(...)` path in `CanvasView` (the legacy reader's
  consumer) - leave it until the legacy reader itself is removed, if
  nothing else uses it by then.

## 11. FileActions integration and commit plan

Order for the implementer — 4 commits, each one buildable and working:

**Commit 1 — miniz + scaffolding.** Vendor `include/miniz`, wire it
into CMake, `fml_archive.h/.cpp` with empty `save/load`, a small unit
test around the zip helpers (write/read an archive in a temp
directory). Criterion: Linux build + the existing MSVC pipeline
(appveyor) stay unbroken.

**Commit 2 — saving.** §8 in full; `save_fml` → `FmlArchive::save`;
`FileActions::saveFile` calls `save_fml` (synchronously for now, no
ThreadedIO yet - progress dialogs are wired up in commit 4). Criterion:
the saved file can be renamed to `.zip`, opened in an archive manager,
`manifest.json` reads fine by eye, images sit under `images/`.

**Commit 3 — loading + legacy.** §9 and §10; `load_fml` →
`FmlArchive::load`; `FileActions::processOpenFile` calls `load_fml`.
Criterion: round-trip (save → open → items back in place with
pos/z/scale/rotation/flip/crop/text), an old `.fml` file opens via the
legacy branch.

**Commit 4 — background processing.** Wrap save/load in `ThreadedIO`
following the `load_images` pattern + a progress dialog; cancellation
must not corrupt the file (`QSaveFile` without `commit()`). Delete
`fml_file_buffer`, `fml_payload()`. Criterion: loading a board with
~100 images doesn't freeze the GUI; cancel works.

## 12. Testing

There's no test infrastructure in the project - the minimum for this
phase:

- A manual round-trip checklist (in the PR description): a pixmap with
  crop + grayscale + rotation + flip, text with Cyrillic, an empty
  scene, a file with an unknown `type` (edit the manifest by hand) →
  ErrorItem, a corrupt zip → error without a crash, a legacy file.
- If the implementer sets up a QtTest target, a round-trip test of
  `FmlArchive::save`→`load` on a synthetic scene would be the first
  candidate - but it isn't a blocker for this phase.

## 13. Groundwork for the future (already possible with this format)

- `scene {}` - scene settings (background color, camera position).
- `thumbnail.png` at the archive root - a preview.
- Deduplication: name images by content SHA-256 instead of uid (the
  `image` field is already a path, not a naming convention - the
  manifest won't need to change).
- Incremental rewriting: a temp file + copying unchanged entries from
  the old archive.
- Groups/layers: new item types + a `children` field - older versions
  will show an ErrorItem, no data lost once storing the original JSON in
  ErrorItem is implemented (§6).
