#include "actions.h"
#include <core/settings.h>
#include <QAction>

// ─── Action methods ───────────────────────────────────────────────────────────

Action Action::make(
    const QString& id, const QString& text,
    const QString& callback,
    const QStringList& shortcuts,
    bool checkable, bool checked,
    const QString& group, const QString& settingsKey,
    bool enabled, const QString& menuId)
{
    return {id, text, callback, shortcuts, checkable, checked,
            group, settingsKey, enabled, menuId, nullptr};
}

QStringList Action::getShortcuts() const
{
    return KeyboardSettings().getShortcuts(
        QString::fromLatin1(SETTINGS_GROUP), id, shortcuts);
}

void Action::setShortcuts(const QStringList& values)
{
    KeyboardSettings().setShortcuts(
        QString::fromLatin1(SETTINGS_GROUP), id, values);
    if (qaction) {
        QList<QKeySequence> seqs;
        for (const QString& s : values)
            seqs.append(QKeySequence(s));
        qaction->setShortcuts(seqs);
    }
}

QKeySequence Action::getKeySequence(int index) const
{
    const QStringList sc = getShortcuts();
    if (index < sc.size())
        return QKeySequence(sc[index]);
    return {};
}

bool Action::shortcutsChanged() const
{
    return getShortcuts() != shortcuts;
}

QString Action::getDefaultShortcut(int index) const
{
    if (index < shortcuts.size())
        return shortcuts[index];
    return {};
}

// ─── ActionRegistry ───────────────────────────────────────────────────────────

void ActionRegistry::add(Action action)
{
    const QString id = action.id;
    if (!map_.contains(id))
        order_.append(id);
    map_[id] = std::move(action);
}

Action& ActionRegistry::operator[](const QString& id)
{
    return map_[id];
}

Action* ActionRegistry::find(const QString& id)
{
    auto it = map_.find(id);
    return (it != map_.end()) ? &it.value() : nullptr;
}

void ActionRegistry::remove(const QString& id)
{
    map_.remove(id);
    order_.removeAll(id);
}

bool ActionRegistry::contains(const QString& id) const
{
    return map_.contains(id);
}

QList<Action*> ActionRegistry::all()
{
    QList<Action*> result;
    result.reserve(order_.size());
    for (const QString& id : order_)
        result.append(&map_[id]);
    return result;
}

QStringList ActionRegistry::keys() const
{
    return order_;
}

// ─── Global actions registry ──────────────────────────────────────────────────

static ActionRegistry buildRegistry()
{
    using A = Action;
    ActionRegistry r;

    // ── File ──────────────────────────────────────────────────────────────────
    r.add(A::make("new_scene",      "&New Scene",           "on_action_new_scene",
                  {"Ctrl+N"}));
    r.add(A::make("open",           "&Open",                "on_action_open",
                  {"Ctrl+O"}));
    r.add(A::make("save",           "&Save",                "on_action_save",
                  {"Ctrl+S"},       false, false, "active_when_items_in_scene"));
    r.add(A::make("save_as",        "Save &As...",          "on_action_save_as",
                  {"Ctrl+Shift+S"}, false, false, "active_when_items_in_scene"));
    r.add(A::make("export_scene",   "E&xport Scene...",     "on_action_export_scene",
                  {"Ctrl+Shift+E"}, false, false, "active_when_items_in_scene"));
    r.add(A::make("export_images",  "Export &Images...",    "on_action_export_images",
                  {},               false, false, "active_when_items_in_scene"));
    r.add(A::make("quit",           "&Quit",                "on_action_quit",
                  {"Ctrl+Q"}));

    // ── Edit ──────────────────────────────────────────────────────────────────
    r.add(A::make("undo",           "&Undo",                "on_action_undo",
                  {"Ctrl+Z"},       false, false, "active_when_can_undo"));
    r.add(A::make("redo",           "&Redo",                "on_action_redo",
                  {"Ctrl+Shift+Z"}, false, false, "active_when_can_redo"));
    r.add(A::make("select_all",     "&Select All",          "on_action_select_all",
                  {"Ctrl+A"}));
    r.add(A::make("deselect_all",   "Deselect &All",        "on_action_deselect_all",
                  {"Ctrl+Shift+A"}));
    r.add(A::make("cut",            "Cu&t",                 "on_action_cut",
                  {"Ctrl+X"},       false, false, "active_when_selection"));
    r.add(A::make("copy",           "&Copy",                "on_action_copy",
                  {"Ctrl+C"},       false, false, "active_when_selection"));
    r.add(A::make("paste",          "&Paste",               "on_action_paste",
                  {"Ctrl+V"}));
    r.add(A::make("delete",         "&Delete",              "on_action_delete_items",
                  {"Del"},          false, false, "active_when_selection"));
    r.add(A::make("raise_to_top",   "&Raise to Top",        "on_action_raise_to_top",
                  {"PgUp"},         false, false, "active_when_selection"));
    r.add(A::make("lower_to_bottom","Lower to Bottom",      "on_action_lower_to_bottom",
                  {"PgDown"},       false, false, "active_when_selection"));

    // ── View ──────────────────────────────────────────────────────────────────
    r.add(A::make("fit_scene",      "&Fit Scene",           "on_action_fit_scene",
                  {"1"}));
    r.add(A::make("fit_selection",  "Fit &Selection",       "on_action_fit_selection",
                  {"2"},            false, false, "active_when_selection"));
    r.add(A::make("fullscreen",     "&Fullscreen",          "on_action_fullscreen",
                  {"F11"},          true));
    r.add(A::make("always_on_top",  "&Always On Top",       "on_action_always_on_top",
                  {},               true));
    r.add(A::make("show_scrollbars","Show &Scrollbars",     "on_action_show_scrollbars",
                  {},               true,  false, {}, "View/show_scrollbars"));
    r.add(A::make("show_menubar",   "Show &Menu Bar",       "on_action_show_menubar",
                  {},               true,  false, {}, "View/show_menubar"));
    r.add(A::make("show_titlebar",  "Show &Title Bar",      "on_action_show_titlebar",
                  {},               true,  true));
    r.add(A::make("move_window",    "Move &Window",         "on_action_move_window",
                  {"Ctrl+M"}));

    // ── Insert ────────────────────────────────────────────────────────────────
    r.add(A::make("insert_images",  "&Images...",           "on_action_insert_images",
                  {"Ctrl+I"}));
    r.add(A::make("insert_text",    "&Text",                "on_action_insert_text",
                  {"Ctrl+T"}));

    // ── Transform ─────────────────────────────────────────────────────────────
    r.add(A::make("crop",              "&Crop",             "on_action_crop",
                  {"Shift+C"},   false, false, "active_when_single_image"));
    r.add(A::make("flip_horizontally", "Flip &Horizontally","on_action_flip_horizontally",
                  {"H"},         false, false, "active_when_selection"));
    r.add(A::make("flip_vertically",   "Flip &Vertically",  "on_action_flip_vertically",
                  {"V"},         false, false, "active_when_selection"));
    r.add(A::make("reset_scale",       "Reset &Scale",      "on_action_reset_scale",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("reset_rotation",    "Reset &Rotation",   "on_action_reset_rotation",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("reset_flip",        "Reset &Flip",       "on_action_reset_flip",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("reset_crop",        "Reset Cro&p",       "on_action_reset_crop",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("reset_transforms",  "Reset &All",        "on_action_reset_transforms",
                  {"R"},         false, false, "active_when_selection"));

    // ── Normalize ─────────────────────────────────────────────────────────────
    r.add(A::make("normalize_height",  "&Height",           "on_action_normalize_height",
                  {"Shift+H"},   false, false, "active_when_selection"));
    r.add(A::make("normalize_width",   "&Width",            "on_action_normalize_width",
                  {"Shift+W"},   false, false, "active_when_selection"));
    r.add(A::make("normalize_size",    "&Size",             "on_action_normalize_size",
                  {"Shift+S"},   false, false, "active_when_selection"));

    // ── Arrange ───────────────────────────────────────────────────────────────
    r.add(A::make("arrange_optimal",    "&Optimal",
                  "on_action_arrange_optimal",
                  {"Shift+O"},   false, false, "active_when_selection"));
    r.add(A::make("arrange_horizontal", "&Horizontal (by filename)",
                  "on_action_arrange_horizontal",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("arrange_vertical",   "&Vertical (by filename)",
                  "on_action_arrange_vertical",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("arrange_square",     "&Square (by filename)",
                  "on_action_arrange_square",
                  {},            false, false, "active_when_selection"));

    // ── Images ────────────────────────────────────────────────────────────────
    r.add(A::make("change_opacity",    "Change &Opacity...", "on_action_change_opacity",
                  {},            false, false, "active_when_selection"));
    r.add(A::make("grayscale",         "&Grayscale",         "on_action_grayscale",
                  {"G"},         true,  false, "active_when_selection"));
    r.add(A::make("show_color_gamut",  "Show &Color Gamut",  "on_action_show_color_gamut",
                  {},            false, false, "active_when_single_image"));
    r.add(A::make("sample_color",      "Sample Color",       "on_action_sample_color",
                  {"S"},         false, false, "active_when_items_in_scene"));

    // ── Settings ──────────────────────────────────────────────────────────────
    r.add(A::make("settings",          "&Settings",          "on_action_settings"));
    r.add(A::make("keyboard_settings", "&Keyboard && Mouse", "on_action_keyboard_settings"));
    r.add(A::make("open_settings_dir", "&Open Settings Folder",
                  "on_action_open_settings_dir"));

    // ── Help ──────────────────────────────────────────────────────────────────
    r.add(A::make("help",     "&Help",            "on_action_help",     {"F1", "Ctrl+H"}));
    r.add(A::make("about",    "&About",           "on_action_about"));
    r.add(A::make("debuglog", "Show &Debug Log",  "on_action_debuglog"));

    return r;
}

ActionRegistry& getActions()
{
    static ActionRegistry inst = buildRegistry();
    return inst;
}
