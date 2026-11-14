#include "setting_descriptions.h"

#include <QMap>

namespace familiar::setting_descriptions {

namespace {

// Fallback for a row whose key is not in either table below. Nothing
// shipped uses it today - every settings row and every Keyboard
// Shortcuts / Controls row has a real entry - but a row added later
// shows this rather than an empty popup until its copy is written.
const QString& placeholder()
{
    static const QString text = QStringLiteral("Description coming soon.");
    return text;
}

// Rich HTML is fine here - SettingInfoPopup's body QLabel
// (widgets/setting_row.cpp) is explicitly Qt::RichText, so <b>, <br/>,
// <ul>/<li>, etc. all render (same subset QLabel/QTextDocument always
// support elsewhere in this app, e.g. HelpDialog). Keep it to inline
// tags, not full <html>/<body> wrapping - QLabel doesn't need it.
const QMap<QString, QString>& settings_table()
{
    static const QMap<QString, QString> table = {
        {QStringLiteral("Telemetry/enabled"),
         QStringLiteral(
             "Sends two things to familiar's own server, at most once a "
             "day: a random number generated on this computer, and the "
             "version you are running."
             "<br/><br/>Nothing else - no file names, no images, no canvas "
             "contents, no name or email. The server stores no IP "
             "addresses and no time of day, only the date."
             "<br/><br/>It exists to count installations and launches, and "
             "to see which versions are still in use."
             "<br/><br/>Setting <b>DO_NOT_TRACK=1</b> in the environment, "
             "or passing <b>--no-telemetry</b>, overrides this for that "
             "run.")},
        {QStringLiteral("Items/undo_history_size"),
         QStringLiteral(
             "How many steps of canvas history a tab keeps. Older steps "
             "fall off the end and can no longer be undone."
             "<br/><br/><b>Set to 0</b> for no limit - the history then "
             "grows for as long as the tab stays open."
             "<br/><br/>A tab reads this once, when it opens. Tabs that "
             "are already open keep the number they started with.")},
        {QStringLiteral("Items/auto_optimize_imported_images"),
         QStringLiteral(
             "What to do with an imported image larger than 4096px on its "
             "long side."
             "<br/><br/><b>Large image warning</b> imports it untouched "
             "and lists it afterwards; <b>Optimize large images</b> "
             "scales it down to that limit as it is imported; <b>Off</b> "
             "does neither."
             "<br/><br/>The file on disk is never modified either way. "
             "Animated GIFs are left alone, and camera RAW files follow "
             "<b>RAW Import</b> below instead of this.")},
        {QStringLiteral("Save/autosave_enabled"),
         QStringLiteral(
             "Writes open tabs back to their own <b>.fml</b> files on a "
             "timer, without switching away from the tab you are working "
             "in."
             "<br/><br/>Only tabs that have both a file and unsaved "
             "changes are written. A tab that has never been saved has "
             "nowhere to write to and is skipped - that one is covered by "
             "crash recovery, which keeps its own copy every 30 seconds "
             "whatever this is set to.")},
        {QStringLiteral("Save/autosave_interval_seconds"),
         QStringLiteral("How long to wait between autosaves."
                        "<br/><br/>Has no effect while <b>Enable "
                        "Autosave</b> is off.")},
        {QStringLiteral("Items/arrange_gap"),
         QStringLiteral("The gap between images when using arrange actions.")},
        {QStringLiteral("Items/image_allocation_limit"),
         QStringLiteral(
             "The maximum image size that can be loaded (in megabytes)."
             "<br/><br/><b>Set to 0</b> for no limitation.")},
        {QStringLiteral("Items/raw_import_choice"),
         QStringLiteral(
             "How camera RAW files (.NEF, .CR3, ...) are converted when "
             "imported."
             "<br/><br/><b>Optimize</b> converts faster at slightly lower "
             "quality; <b>Keep original</b> uses the best conversion your "
             "camera's profile allows, which can take several seconds per "
             "file."
             "<br/><br/>Set this back to <b>Ask every time</b> to bring the "
             "import prompt back after ticking \"Remember choice for future "
             "files\".")},
        {QStringLiteral("Items/arrange_default"),
         QStringLiteral(
             "How images are arranged when inserted in batch."
             "<br/><br/><b>Optimal</b> packs images to minimize wasted "
             "space; the others sort <b>by filename</b> along one axis.")},
    };
    return table;
}

// Keyed by BindingTarget::id() - for an Action this is Action::id
// (actions/actions.cpp's A::make() calls), not its display text, e.g.
// "open" for the Ctrl+O row, not "Open". The four Controls ids come from
// KeyboardSettings::mouse_actions()/mousewheel_actions() (core/
// controls.cpp).
//
// Deliberately no key names in the copy: BindingsTreeWidget::buildRow()
// already puts the live "Default: X" line under every primary row, and a
// chord spelled out in prose here would go stale the moment the row is
// rebound. Same reason the text describes what the action DOES rather
// than what it is called - the row's own label is right next to it.
const QMap<QString, QString>& binding_targets_table()
{
    static const QMap<QString, QString> table = {
        // ── File ──────────────────────────────────────────────────────
        {QStringLiteral("new_scene"),
         QStringLiteral("Adds a new, empty tab and switches to it. Tabs "
                        "that are already open are left alone.")},
        {QStringLiteral("open"),
         QStringLiteral(
             "Opens a <b>.fml</b> scene in a new tab - or switches to that "
             "tab, if the file is already open in one. Picking several "
             "files at once opens each of them."
             "<br/><br/>The current tab is reused only while it is still "
             "an empty, never-saved one.")},
        {QStringLiteral("save"),
         QStringLiteral(
             "Writes the current tab back to its <b>.fml</b> file."
             "<br/><br/>A tab that has never been saved asks for a name "
             "and a folder first.")},
        {QStringLiteral("save_as"),
         QStringLiteral("Writes the current tab to a different <b>.fml</b> "
                        "file and keeps working in that one.")},
        {QStringLiteral("export_scene"),
         QStringLiteral(
             "Renders the whole canvas - every image, note and group, laid "
             "out exactly as it is on screen - into one <b>PNG</b>, "
             "<b>JPEG</b> or <b>SVG</b> file."
             "<br/><br/>That file is a flat picture: it cannot be opened "
             "back as a scene. Use <b>Save</b> for that.")},
        {QStringLiteral("export_images"),
         QStringLiteral(
             "Writes every image in the scene into a folder of your "
             "choice as a separate file - the pictures themselves, not "
             "the layout."
             "<br/><br/>If a name is already taken in that folder, you "
             "are asked what to do with it.")},
        {QStringLiteral("quit"),
         QStringLiteral("Closes familiar. Tabs with unsaved changes ask "
                        "what to do with them first.")},

        // ── Edit ──────────────────────────────────────────────────────
        {QStringLiteral("undo"),
         QStringLiteral(
             "Takes back the last change to the canvas. Moving, scaling, "
             "rotating, cropping, grouping, inserting and deleting all go "
             "on the same history."
             "<br/><br/>How many steps are kept is set on the "
             "<b>Performance</b> page.")},
        {QStringLiteral("redo"),
         QStringLiteral("Re-applies the step that Undo just took back.")},
        {QStringLiteral("select_all"),
         QStringLiteral("Selects everything in the scene.")},
        {QStringLiteral("deselect_all"),
         QStringLiteral("Clears the selection, and leaves crop or "
                        "colour-sampling mode if one is active.")},
        {QStringLiteral("cut"),
         QStringLiteral(
             "Copies the selection and removes it from the canvas in one "
             "step."
             "<br/><br/>Exactly as much leaves as was copied: a group "
             "takes its whole contents with it, and a picture takes the "
             "notes pinned to it.")},
        {QStringLiteral("copy"),
         QStringLiteral(
             "Copies the selection, a group's whole contents and the "
             "notes pinned to a copied picture included."
             "<br/><br/>If the first item copied is an image or a note, it "
             "also goes to the system clipboard, so it can be pasted into "
             "another application.")},
        {QStringLiteral("paste"),
         QStringLiteral(
             "Puts the clipboard on the canvas at the pointer."
             "<br/><br/>Besides familiar's own copies, this takes images "
             "and files copied in other applications, an image copied "
             "from a web page, and plain text - which arrives as a note."
             "<br/><br/>Pasting into an empty scene also fits the view to "
             "what arrived.")},
        {QStringLiteral("duplicate"),
         QStringLiteral(
             "Makes a copy of the selection and drops it slightly offset "
             "from the original, without touching the clipboard."
             "<br/><br/>A group is duplicated whole, notes and nested "
             "groups included.")},
        {QStringLiteral("delete"),
         QStringLiteral(
             "Removes the selection from the canvas."
             "<br/><br/>A group takes its whole contents with it and a "
             "picture takes its pinned notes, all as one step - a single "
             "Undo brings everything back.")},
        {QStringLiteral("raise_to_top"),
         QStringLiteral("Moves the selection in front of everything else, "
                        "keeping the order inside the selection.")},
        {QStringLiteral("lower_to_bottom"),
         QStringLiteral("Moves the selection behind everything else, "
                        "keeping the order inside the selection.")},
        {QStringLiteral("group"),
         QStringLiteral(
             "Ties two or more selected items together, so they move, "
             "scale and rotate as one."
             "<br/><br/>Selecting one existing group plus loose items "
             "folds those items into it instead of wrapping everything in "
             "another layer. Selecting two or more groups does nest them, "
             "which is how a group of groups is built.")},
        {QStringLiteral("ungroup"),
         QStringLiteral(
             "Dissolves the selected group and releases its members. Only "
             "one layer comes apart - a group nested inside it stays a "
             "group."
             "<br/><br/>Used on an item that is itself a member of a "
             "group, this takes that one item out of the group instead, "
             "leaving both intact. A nested subgroup treated this way "
             "leaves its parent and stays whole.")},

        // ── View ──────────────────────────────────────────────────────
        {QStringLiteral("fit_scene"),
         QStringLiteral("Zooms and pans so that the whole scene is "
                        "visible.")},
        {QStringLiteral("fit_selection"),
         QStringLiteral("Zooms and pans so that the selection fills the "
                        "window.")},
        {QStringLiteral("zoom_in"),
         QStringLiteral(
             "Zooms in one step around the centre of the window."
             "<br/><br/>The wheel zooms too, but towards the pointer "
             "rather than the centre.")},
        {QStringLiteral("zoom_out"),
         QStringLiteral("Zooms out one step around the centre of the "
                        "window.")},
        {QStringLiteral("fullscreen"),
         QStringLiteral("Lets the window take the whole screen.")},
        {QStringLiteral("always_on_top"),
         QStringLiteral("Keeps the window above the windows of other "
                        "applications, even while they have the focus.")},
        {QStringLiteral("transparent_to_mouse"),
         QStringLiteral(
             "Makes the window ignore the mouse entirely: it stays "
             "visible, but every click and scroll passes through to "
             "whatever is underneath. For drawing in another application "
             "with a reference on top of it."
             "<br/><br/><b>Always On Top</b> is switched on with it, "
             "otherwise the window would simply disappear behind the one "
             "you are working in."
             "<br/><br/>Since clicks no longer reach the menu either, "
             "there are two ways back out: this shortcut, or giving the "
             "window the focus again (Alt+Tab, the taskbar), which turns "
             "the mode off by itself.")},
        {QStringLiteral("show_menubar"),
         QStringLiteral("Shows or hides the menu bar at the top of the "
                        "window. The shortcuts keep working while it is "
                        "hidden.")},
        {QStringLiteral("auto_hide_ui"),
         QStringLiteral(
             "Fades the menu bar and the tab bar out while the pointer is "
             "away from the top edge, and back in when it returns."
             "<br/><br/>Only has an effect while the menu bar is shown.")},
        {QStringLiteral("hierarchy"),
         QStringLiteral("Shows or hides the Hierarchy panel - the list of "
                        "everything in the scene, where items can be "
                        "selected and renamed.")},

        // ── Insert ────────────────────────────────────────────────────
        {QStringLiteral("insert_images"),
         QStringLiteral("Picks image files and places them on the canvas."
                        "<br/><br/>Several at once are laid out the way "
                        "<b>Default Arrange Method</b> in the settings says.")},
        {QStringLiteral("insert_text"),
         QStringLiteral(
             "Adds a note at the pointer and puts the cursor in it."
             "<br/><br/>With exactly one picture selected, the note pins "
             "itself above that picture and follows it from then on; with "
             "a group selected, it lands inside that group.")},

        // ── Transform ─────────────────────────────────────────────────
        {QStringLiteral("crop"),
         QStringLiteral(
             "Starts cropping the selected image: drag the edges, then "
             "confirm."
             "<br/><br/>Nothing is thrown away - the hidden part can be "
             "brought back at any time with <b>Reset Crop</b>.")},
        {QStringLiteral("flip_horizontally"),
         QStringLiteral("Mirrors the selection left to right.")},
        {QStringLiteral("flip_vertically"),
         QStringLiteral("Mirrors the selection top to bottom.")},
        {QStringLiteral("reset_scale"),
         QStringLiteral("Returns the selection to its original size, "
                        "around the centre of the selection.")},
        {QStringLiteral("reset_rotation"),
         QStringLiteral("Returns the selection to its original angle.")},
        {QStringLiteral("reset_flip"),
         QStringLiteral("Undoes mirroring of the selection, leaving size "
                        "and angle as they are.")},
        {QStringLiteral("reset_crop"),
         QStringLiteral("Brings back the cropped-away part of the "
                        "selected images in full.")},
        {QStringLiteral("reset_transforms"),
         QStringLiteral("Returns size, angle, mirroring and crop of the "
                        "selection all at once - back to the images as "
                        "they were imported.")},

        // ── Normalize ─────────────────────────────────────────────────
        {QStringLiteral("normalize_height"),
         QStringLiteral(
             "Scales every selected item so that they all end up the same "
             "height - the average height of the selection. Proportions "
             "are kept."
             "<br/><br/>Needs at least two items selected.")},
        {QStringLiteral("normalize_width"),
         QStringLiteral(
             "Scales every selected item to the same width - the average "
             "width of the selection. Proportions are kept."
             "<br/><br/>Needs at least two items selected.")},
        {QStringLiteral("normalize_size"),
         QStringLiteral(
             "Scales every selected item to the same <b>area</b> - the "
             "average of the selection - so a wide and a tall picture end "
             "up visually equal in weight rather than equal in one "
             "dimension."
             "<br/><br/>Needs at least two items selected.")},

        // ── Arrange ───────────────────────────────────────────────────
        {QStringLiteral("arrange_optimal"),
         QStringLiteral(
             "Packs the items together with as little wasted space as "
             "possible, roughly around where they already were."
             "<br/><br/>With nothing selected this arranges the whole "
             "scene; with exactly one group selected, the contents of "
             "that group. The spacing comes from <b>Arrange Gap</b> in "
             "the settings.")},
        {QStringLiteral("arrange_horizontal"),
         QStringLiteral(
             "Lays the items out in one row, sorted by file name."
             "<br/><br/>Same targets as <b>Arrange Optimal</b>: the "
             "selection, the whole scene when nothing is selected, or the "
             "contents of a single selected group.")},
        {QStringLiteral("arrange_vertical"),
         QStringLiteral("Lays the items out in one column, sorted by file "
                        "name.")},
        {QStringLiteral("arrange_square"),
         QStringLiteral(
             "Lays the items out in a grid as close to square as their "
             "number allows, sorted by file name."
             "<br/><br/>Every cell is as large as the biggest item, so "
             "rows and columns line up.")},

        // ── Images ────────────────────────────────────────────────────
        {QStringLiteral("change_opacity"),
         QStringLiteral(
             "Opens a slider for the opacity of the selected images, with "
             "the canvas updating as it moves."
             "<br/><br/>Useful for tracing: a half-transparent reference "
             "over another one shows both at once.")},
        {QStringLiteral("grayscale"),
         QStringLiteral(
             "Shows the selected images without colour, and shows them in "
             "colour again when pressed a second time."
             "<br/><br/>Only the display changes; the colours are never "
             "lost.")},
        {QStringLiteral("show_color_gamut"),
         QStringLiteral("Opens a colour wheel of the colours actually "
                        "used by the selected image, with a threshold for "
                        "how rare a colour still counts.")},
        {QStringLiteral("sample_color"),
         QStringLiteral(
             "Turns the pointer into a crosshair and shows the colour "
             "under it. A click copies that colour as a hex code to the "
             "clipboard."
             "<br/><br/>Any key leaves the mode without picking "
             "anything.")},

        // ── Settings and Help ─────────────────────────────────────────
        {QStringLiteral("settings"), QStringLiteral("Opens this window.")},
        {QStringLiteral("open_settings_dir"),
         QStringLiteral("Opens the folder holding familiar's own files - "
                        "settings, log and the crash-recovery copies of "
                        "unsaved scenes - in the file manager.")},
        {QStringLiteral("help"),
         QStringLiteral("Opens the short list of mouse controls, with a "
                        "way back into these shortcuts.")},
        {QStringLiteral("about"),
         QStringLiteral("Shows the version, the licence and the people "
                        "behind familiar.")},
        {QStringLiteral("debuglog"),
         QStringLiteral(
             "Shows this session's log, and the one before it."
             "<br/><br/>Worth attaching when reporting a problem: it "
             "records what familiar was doing, without any of the "
             "contents of your scenes.")},

        // ── Controls (mouse and wheel) ────────────────────────────────
        {QStringLiteral("zoom"),
         QStringLiteral(
             "Drag to zoom, anchored where the drag started."
             "<br/><br/>The wheel zooms as well, towards the pointer; "
             "that one is built in and cannot be rebound.")},
        {QStringLiteral("pan"),
         QStringLiteral("Drag to move the canvas under the pointer, "
                        "without touching what is on it.")},
        {QStringLiteral("pan_horizontal"),
         QStringLiteral("Scrolls the canvas left and right instead of "
                        "zooming.")},
        {QStringLiteral("pan_vertical"),
         QStringLiteral("Scrolls the canvas up and down instead of "
                        "zooming.")},
    };
    return table;
}

} // namespace

QString for_settings_key(const QString& key)
{
    return settings_table().value(key, placeholder());
}

QString for_binding_target_id(const QString& id)
{
    return binding_targets_table().value(id, placeholder());
}

} // namespace familiar::setting_descriptions
