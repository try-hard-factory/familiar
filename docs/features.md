# What familiar does

Source material for the README's user-facing half, the website, and the
handbook. Everything below was read out of the code, not remembered: the
per-command wording comes from `src/widgets/setting_descriptions.cpp`,
which is itself written against the handler implementations, and the
command list from `src/actions/actions.cpp` (56 commands) plus
`src/core/controls.cpp` (4 rebindable mouse/wheel controls).

---

## In one paragraph

familiar is a reference board. You throw images onto an endless canvas,
arrange them, annotate them, and keep the whole thing as a single file.
It can sit on top of the application you are drawing in and let your
clicks pass straight through it, so the reference stays visible while
you work. It is free, open source, and runs on Windows, Linux and macOS.

---

## The canvas

**Endless and zoomable.** Drag to pan, wheel to zoom towards the pointer.
Panning, drag-zoom, and horizontal/vertical wheel scrolling are all
rebindable to other mouse buttons and modifiers; wheel-zoom itself is
built in and fixed.

**Fit to what matters.** Fit the whole scene to the window, or fit just
the selection. Zoom in and out in steps around the window centre.

**Tabs.** Several scenes open at once in one window, each with its own
file, its own undo history and its own unsaved state. Opening a file that
is already open switches to its tab rather than opening it twice.

---

## Getting images in

- **Drag and drop** files onto the canvas.
- **Paste** — familiar's own copies, images and files copied in other
  applications, an image copied from a web page, or plain text, which
  arrives as a note.
- **Insert Images** picks files from disk; several at once are laid out
  by the arrange method set in the settings.
- **Camera RAW.** RAW files are recognised and imported through a
  dedicated dialog; the choice is remembered (`Items/raw_import_choice`).
- **Animated GIF** is supported as an animation, with its own playback
  toolbar, not flattened to a still frame.

**Large images** can be downscaled or re-encoded on import, or left
alone, per the **Auto Optimize Imported Images** setting. Whatever it
does, it never modifies the file on disk.

---

## Working with images

| Command | What it does |
|---|---|
| Crop | Drag the edges, then confirm. Nothing is discarded — **Reset Crop** brings the hidden part back in full |
| Flip horizontally / vertically | Mirrors the selection |
| Reset Scale / Rotation / Flip / Crop | Each undone on its own |
| Reset Transforms | Size, angle, mirroring and crop at once, back to as imported |
| Change Opacity | A slider with the canvas updating live — a half-transparent reference over another shows both at once |
| Toggle Grayscale | Display only; the colours are never lost |
| Duplicate | A copy slightly offset from the original, without touching the clipboard |
| Raise to Top / Lower to Bottom | Keeps the order inside the selection |

Items are moved, scaled and rotated directly on the canvas by their
handles.

---

## Notes

A note is a text item on the canvas. Added at the pointer with the cursor
already in it.

The part that is not obvious: **with exactly one picture selected, the
note pins itself to that picture** and follows it from then on — through
moves, scaling and rotation, and into groups. Copying, duplicating or
deleting the picture takes its pinned notes with it, as one undo step.
With a group selected instead, the note lands inside that group.

---

## Groups and hierarchy

**Group** ties two or more items together so they move, scale and rotate
as one. The behaviour is deliberately asymmetric, because both readings
are useful:

- one existing group plus loose items → the items are folded **into** it;
- two or more groups → they are **nested**, which is how a group of
  groups is built.

**Ungroup** dissolves one layer only: a group nested inside stays a
group. Used on a single member of a group, it takes that one item out
instead, leaving both intact.

Nesting is arbitrarily deep.

**The Hierarchy panel** lists everything in the scene as a tree. Items
can be selected and renamed there, and dragged into or out of groups.

---

## Arranging

**Arrange** — Optimal (packs with as little wasted space as possible,
roughly where things already are), Horizontal, Vertical, or Square
(a grid as close to square as the count allows). All but Optimal sort by
file name. Spacing comes from the **Arrange Gap** setting.

Each of them works on the selection; with nothing selected, on the whole
scene; with exactly one group selected, on the contents of that group.

**Normalize** — Height, Width, or Size. Height and Width scale everything
to the average of the selection in that dimension. **Size** equalises
**area**, so a wide and a tall picture end up visually equal in weight
rather than equal in one dimension. Proportions are always kept; needs at
least two items.

---

## Window modes

This is what the application is for, as much as the canvas is.

**Always On Top** keeps the window above other applications' windows even
while they have the focus.

**Transparent To Mouse** makes the window ignore the mouse entirely: it
stays visible, but every click and scroll passes through to whatever is
underneath. Always On Top is switched on with it — otherwise the window
would simply disappear behind the one you are working in. Since clicks no
longer reach the menu either, there are two ways back out: the shortcut,
or giving the window the focus again, which turns the mode off by itself.

**Auto-hide UI** fades the menu bar and tab bar out while the pointer is
away from the top edge. **Hide Menu Bar** removes it entirely; the
shortcuts keep working. **Fullscreen** is there too.

---

## Colour tools

**Show Color Gamut** opens a colour wheel of the colours actually used by
the selected image, with a threshold for how rare a colour still counts.

**Sample Color** turns the pointer into a crosshair showing the colour
under it; a click copies that colour as a hex code to the clipboard.

---

## Files

**`.fml`** is familiar's own scene format — a zip holding a manifest and
the images, documented in `docs/fml_format_design.md`. It carries the
whole scene: images, notes, groups, transforms, crops.

**Export Scene** renders the canvas exactly as it looks — every image,
note and group — into one **PNG**, **JPEG** or **SVG**. That file is a
flat picture and cannot be opened back as a scene.

**Export Images** writes every image in the scene into a folder as a
separate file — the pictures, not the layout — asking what to do about
name collisions.

---

## Not losing work

**Autosave** writes each tab back to its own file on a timer, saving each
tab without flipping the active one. It is **off by default**; the
interval is 5 seconds when switched on.

**Crash recovery** is separate and always on: unsaved scenes are written
to a recovery folder every 30 seconds regardless of the autosave setting. After a crash,
familiar offers the recovered scenes in a checklist — tick what to
restore, leave unticked what to discard. Closing the dialog decides
nothing, so it can be answered later.

**Undo** covers moving, scaling, rotating, cropping, grouping, inserting
and deleting on one history per tab. Depth is set by **Undo History
Size**; 0 means no limit.

---

## Settings

Three pages — **Performance**, **Colors**, **Keyboard Shortcuts** — with
a search box across all of them, a **Restore Defaults** action, and
**Import** / **Export** of the whole settings file.

Every row carries a hover annotation explaining what it actually does,
and `Ctrl` + double-click restores that one setting to its default.

**Keyboard Shortcuts** rebinds all 56 commands and the 4 mouse/wheel
controls, showing each one's default underneath.

**Colors** drives the application's own palette.

Settings live in a single JSON file with a schema version and a
migration path; a file written by a newer version is handled
best-effort rather than refused.

---

## Privacy

Telemetry is **opt-in**, asked once, and refusable; it can be turned off
afterwards in the settings, and compiled out of a build entirely. The
debug log viewer shows this session's log and the previous one — it
records what familiar was doing, with none of the contents of your
scenes.

---

## The short list

For the README and the landing page, in the order that sells it:

1. **Draw with the reference on top.** Always on top plus click-through
   transparency, so the board stays visible over the application you are
   working in.
2. **Notes that stick to pictures.** Annotate a specific reference, and
   the annotation follows it everywhere — including into groups.
3. **Groups that nest.** Arbitrarily deep, with an ungroup that takes one
   item out rather than dissolving everything.
4. **Tabs.** Several boards in one window, each with its own file and its
   own undo history.
5. **Your work survives a crash.** Recovery is always on and independent
   of autosave, and it asks before restoring anything.
6. **Non-destructive.** Crops and transforms are reversible at any time,
   and the files on disk are never modified.
7. **An open file format.** `.fml` is a documented zip, not a black box.
8. **Free, open source, no account.** Windows, Linux and macOS.

---

## How this compares to PureRef 2.1

Based on the published PureRef 2.1 handbook (`pureref.com/handbook`,
every section read), not on testing the application. "Not documented"
means exactly that — absent from their handbook, which is not the same as
absent from the product.

### Where familiar does something PureRef's handbook does not describe

| familiar | Note |
|---|---|
| Tabs | PureRef opens scenes in separate windows instead; its "Open files in…" setting chooses between new window, current window, and the window with that scene |
| SVG scene export | Their documented export formats are jpg, png and bmp |
| Colour gamut wheel | They document "Show color code", which reports one colour, not the distribution across an image |
| Crash-recovery checklist | They keep unsaved changes in a temp file, but the handbook describes no recovery dialog |
| Camera RAW import dialog | Not documented |
| Per-setting hover annotations, `Ctrl`+double-click to reset one setting | Not documented |
| Built-in log viewer | Not documented |
| Opt-in telemetry asked once | Theirs is described as analytics with an on/off setting |

### Where PureRef documents something familiar does not have

Listed honestly, because this is the roadmap:

- **Drawing tools** — pen, line, rectangle, ellipse, eraser, with colour,
  width and style.
- **Grid**, with snapping, in lines or dots.
- **Slideshow** over the selection, with interval and order.
- **Align** left/right/top/bottom, **Distribute** horizontal/vertical,
  and **Stack**.
- **Arrange by order / path / randomly**, and **Normalize Area** and
  **Normalize Scale**.
- **Command palette.**
- **Command-line automation** — load, save, export, clear, exit.
- **Lock** — canvas, window, group, individual item.
- **Always on bottom** — pinning the window below everything else.
- **Manage images** — bulk resolution changes, re-optimisation, location
  changes, preview.
- **Replace image**, **Relink missing images**, **Open source** (the URL
  an image came from).
- **Focus next / previous image.**
- **Theme presets** (Dark, Light, Glass) with user presets bound to
  `Alt`+number, and **application-wide opacity**.
- **Localisation** — six languages plus user-supplied language files.
- **Rich text in notes** — lists, checklists, links, font control,
  highlight colours.
- **Trackpad gestures** and pen/touch/3D-input support.
- **Update checking** in-app, and Windows `.pur` file association with
  Explorer thumbnails.
- **Embed-or-link** choice for local images in the save file.
- **Performance modes** — disk cache, memory, low-zoom-quality — and a
  CPU allocation cap.
- **Export naming templates** and cropped export.

### Where both do the same thing

Always on top, transparent to mouse, notes attached to images, groups with a hierarchy panel, grayscale, crop with reset,
flip, opacity, duplicate, undo/redo with a configurable history size,
arrange optimal, normalize height/width/size, periodic saving, settings
import/export, full keyboard rebinding, an own scene format, export of
the scene and of individual images, large-image optimisation on import.
