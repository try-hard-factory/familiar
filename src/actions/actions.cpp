#include "actions.h"
#include <core/settingshandler.h>
#include <QAction>
#include <QSet>

namespace {

bool sameModifiers(const QStringList& a, const QStringList& b)
{
    return QSet<QString>(a.begin(), a.end())
           == QSet<QString>(b.begin(), b.end());
}

} // namespace

// ─── Action methods ───────────────────────────────────────────────────────────

Action Action::make(const QString& id,
                    const QString& text,
                    const QString& callback,
                    const QStringList& shortcuts,
                    bool checkable,
                    bool checked,
                    const QString& group,
                    const QString& settingsKey,
                    bool enabled,
                    const QString& menuId)
{
    return {id,
            text,
            callback,
            shortcuts,
            checkable,
            checked,
            group,
            settingsKey,
            enabled,
            menuId,
            nullptr};
}

QStringList Action::get_shortcuts() const
{
    return SettingsHandler::getInstance()
        ->getShortcuts(QString::fromLatin1(SETTINGS_GROUP), id, shortcuts);
}

void Action::setShortcuts(const QStringList& values)
{
    SettingsHandler::getInstance()
        ->setShortcuts(QString::fromLatin1(SETTINGS_GROUP), id, values);
    if (qaction) {
        QList<QKeySequence> seqs;
        for (const QString& s : values)
            seqs.append(QKeySequence(s));
        qaction->setShortcuts(seqs);
    }
}

QKeySequence Action::getKeySequence(int index) const
{
    const QStringList sc = get_shortcuts();
    if (index < sc.size())
        return QKeySequence(sc[index]);
    return {};
}

bool Action::shortcutsChanged() const
{
    return get_shortcuts() != shortcuts;
}

QString Action::getDefaultShortcut(int index) const
{
    if (index < shortcuts.size())
        return shortcuts[index];
    return {};
}

QList<Binding> Action::get_mouse_bindings() const
{
    const QStringList serialized
        = KeyboardSettings().getList(QString::fromLatin1(SETTINGS_GROUP),
                                     id + QStringLiteral("_mouse"),
                                     {});
    QList<Binding> out;
    for (const QString& s : serialized)
        out.append(Binding::deserialize(s));
    return out;
}

void Action::setMouseBindings(const QList<Binding>& values)
{
    QStringList serialized;
    for (const Binding& b : values)
        serialized.append(b.serialize());
    KeyboardSettings().setList(QString::fromLatin1(SETTINGS_GROUP),
                               id + QStringLiteral("_mouse"),
                               serialized,
                               {});
}

QString Action::displayText() const
{
    QString t = text;
    t.replace(QLatin1String("&&"), QLatin1String("\x01"));
    t.remove(QLatin1Char('&'));
    t.replace(QLatin1String("\x01"), QLatin1String("&"));
    return t;
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

Action* ActionRegistry::findByShortcut(const QString& excludeId,
                                       const QString& shortcut)
{
    if (shortcut.isEmpty())
        return nullptr;
    for (Action* a : all()) {
        if (a->id == excludeId)
            continue;
        if (a->get_shortcuts().contains(shortcut))
            return a;
    }
    return nullptr;
}

Action* ActionRegistry::findByMouseBinding(const QString& excludeId,
                                           const Binding& candidate)
{
    if (candidate.mouseButton.isEmpty())
        return nullptr;
    for (Action* a : all()) {
        if (a->id == excludeId)
            continue;
        for (const Binding& b : a->get_mouse_bindings()) {
            if (b.mouseButton == candidate.mouseButton
                && sameModifiers(b.mouseModifiers, candidate.mouseModifiers))
                return a;
        }
    }
    return nullptr;
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
    r.add(A::make("new_scene", "New Scene", "on_action_new_scene", {"Ctrl+N"}));
    r.add(A::make("open", "Open", "on_action_open", {"Ctrl+O"}));
    r.add(A::make("save",
                  "Save",
                  "on_action_save",
                  {"Ctrl+S"},
                  false,
                  false,
                  "active_when_items_in_scene"));
    r.add(A::make("save_as",
                  "Save As...",
                  "on_action_save_as",
                  {"Ctrl+Shift+S"},
                  false,
                  false,
                  "active_when_items_in_scene"));
    r.add(A::make("export_scene",
                  "Export Scene...",
                  "on_action_export_scene",
                  {"Ctrl+Shift+E"},
                  false,
                  false,
                  "active_when_items_in_scene"));
    r.add(A::make("export_images",
                  "Export Images...",
                  "on_action_export_images",
                  {},
                  false,
                  false,
                  "active_when_items_in_scene"));
    r.add(A::make("quit", "Quit", "on_action_quit", {"Ctrl+Q"}));

    // ── Edit ──────────────────────────────────────────────────────────────────
    r.add(A::make("undo",
                  "Undo",
                  "on_action_undo",
                  {"Ctrl+Z"},
                  false,
                  false,
                  "active_when_can_undo"));
    r.add(A::make("redo",
                  "Redo",
                  "on_action_redo",
                  {"Ctrl+Shift+Z"},
                  false,
                  false,
                  "active_when_can_redo"));
    r.add(
        A::make("select_all", "Select All", "on_action_select_all", {"Ctrl+A"}));
    r.add(A::make("deselect_all",
                  "Deselect All",
                  "on_action_deselect_all",
                  {"Ctrl+Shift+A"}));
    r.add(A::make("cut",
                  "Cut",
                  "on_action_cut",
                  {"Ctrl+X"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("copy",
                  "Copy",
                  "on_action_copy",
                  {"Ctrl+C"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("paste", "Paste", "on_action_paste", {"Ctrl+V"}));
    r.add(A::make("delete",
                  "Delete",
                  "on_action_delete_items",
                  {"Del"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("raise_to_top",
                  "Raise to Top",
                  "on_action_raise_to_top",
                  {"PgUp"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("lower_to_bottom",
                  "Lower to Bottom",
                  "on_action_lower_to_bottom",
                  {"PgDown"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("group",
                  "Group",
                  "on_action_group",
                  {"Ctrl+G"},
                  false,
                  false,
                  "active_when_multi_selection"));
    r.add(A::make("ungroup",
                  "Ungroup",
                  "on_action_ungroup",
                  {"Ctrl+Shift+G"},
                  false,
                  false,
                  "active_when_group_selected"));

    // ── View ──────────────────────────────────────────────────────────────────
    r.add(A::make("fit_scene", "Fit Scene", "on_action_fit_scene", {"1"}));
    r.add(A::make("fit_selection",
                  "Fit Selection",
                  "on_action_fit_selection",
                  {"2"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("zoom_in", "Zoom In", "on_action_zoom_in", {"Ctrl++"}));
    r.add(A::make("zoom_out", "Zoom Out", "on_action_zoom_out", {"Ctrl+-"}));
    r.add(A::make("fullscreen",
                  "Fullscreen",
                  "on_action_fullscreen",
                  {"F11"},
                  true));
    r.add(A::make("always_on_top",
                  "Always On Top",
                  "on_action_always_on_top",
                  {},
                  true));
    r.add(A::make("show_menubar",
                  "Show Menu Bar",
                  "on_action_show_menubar",
                  {},
                  true,
                  false,
                  {},
                  "View/show_menubar"));
    // Fades the menu bar and the tab bar out while the cursor is away
    // from the top strip. Only meaningful while the menu bar is shown;
    // MainWindow::applyMenubarState_() keeps the QAction disabled
    // otherwise.
    r.add(A::make("auto_hide_ui",
                  "Auto-Hide Interface",
                  "on_action_auto_hide_ui",
                  {},
                  true,
                  false,
                  {},
                  "View/auto_hide_ui"));
    // Scene outliner dock (ui/hierarchy_panel.h) - persisted like
    // show_menubar/auto_hide_ui above, not just a one-shot toggle.
    r.add(A::make("hierarchy",
                  "Hierarchy",
                  "on_action_hierarchy",
                  {"Ctrl+J"},
                  true,
                  false,
                  {},
                  "View/hierarchy_panel"));

    // ── Insert ────────────────────────────────────────────────────────────────
    r.add(A::make("insert_images",
                  "Images...",
                  "on_action_insert_images",
                  {"Ctrl+I"}));
    r.add(A::make("insert_text", "Text", "on_action_insert_text", {"Ctrl+T"}));

    // ── Transform ─────────────────────────────────────────────────────────────
    r.add(A::make("crop",
                  "Crop",
                  "on_action_crop",
                  {"Shift+C"},
                  false,
                  false,
                  "active_when_single_image"));
    r.add(A::make("flip_horizontally",
                  "Flip Horizontally",
                  "on_action_flip_horizontally",
                  {"H"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("flip_vertically",
                  "Flip Vertically",
                  "on_action_flip_vertically",
                  {"V"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("reset_scale",
                  "Reset Scale",
                  "on_action_reset_scale",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("reset_rotation",
                  "Reset Rotation",
                  "on_action_reset_rotation",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("reset_flip",
                  "Reset Flip",
                  "on_action_reset_flip",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("reset_crop",
                  "Reset Crop",
                  "on_action_reset_crop",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("reset_transforms",
                  "Reset All",
                  "on_action_reset_transforms",
                  {"R"},
                  false,
                  false,
                  "active_when_selection"));

    // ── Normalize ─────────────────────────────────────────────────────────────
    r.add(A::make("normalize_height",
                  "Height",
                  "on_action_normalize_height",
                  {"Shift+H"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("normalize_width",
                  "Width",
                  "on_action_normalize_width",
                  {"Shift+W"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("normalize_size",
                  "Size",
                  "on_action_normalize_size",
                  {"Shift+S"},
                  false,
                  false,
                  "active_when_selection"));

    // ── Arrange ───────────────────────────────────────────────────────────────
    r.add(A::make("arrange_optimal",
                  "Optimal",
                  "on_action_arrange_optimal",
                  {"Shift+O"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("arrange_horizontal",
                  "Horizontal (by filename)",
                  "on_action_arrange_horizontal",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("arrange_vertical",
                  "Vertical (by filename)",
                  "on_action_arrange_vertical",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("arrange_square",
                  "Square (by filename)",
                  "on_action_arrange_square",
                  {},
                  false,
                  false,
                  "active_when_selection"));

    // ── Images ────────────────────────────────────────────────────────────────
    r.add(A::make("change_opacity",
                  "Change Opacity...",
                  "on_action_change_opacity",
                  {},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("grayscale",
                  "Toggle Grayscale",
                  "on_action_grayscale",
                  {"G"},
                  false,
                  false,
                  "active_when_selection"));
    r.add(A::make("show_color_gamut",
                  "Show Color Gamut",
                  "on_action_show_color_gamut",
                  {},
                  false,
                  false,
                  "active_when_single_image"));
    r.add(A::make("sample_color",
                  "Sample Color",
                  "on_action_sample_color",
                  {"S"},
                  false,
                  false,
                  "active_when_items_in_scene"));

    // ── Settings ──────────────────────────────────────────────────────────────
    r.add(A::make("settings", "Settings", "on_action_settings"));
    r.add(A::make("keyboard_settings",
                  "Keyboard && Mouse",
                  "on_action_keyboard_settings"));
    r.add(A::make("open_settings_dir",
                  "Open Settings Folder",
                  "on_action_open_settings_dir"));

    // ── Help ──────────────────────────────────────────────────────────────────
    r.add(A::make("help", "Help", "on_action_help", {"F1"}));
    r.add(A::make("about", "About", "on_action_about", {"Shift+F1"}));
    r.add(A::make("debuglog", "Show Debug Log", "on_action_debuglog"));

    return r;
}

ActionRegistry& getActions()
{
    static ActionRegistry inst = buildRegistry();
    return inst;
}
