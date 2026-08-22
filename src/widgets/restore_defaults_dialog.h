#pragma once

#include <QDialog>
#include <QMap>
#include <QStringList>

class QCheckBox;
class QMouseEvent;

namespace familiar {

// One flag per settings-window category (ui/settings_window.cpp's own
// addCategory() calls) - kept in sync with that file's page contents by
// hand, since FamSettings::fields() keys aren't self-describing by
// which page shows them (see restore_defaults_dialog.cpp's own comment
// on the exact key lists this maps to).
enum class SettingsCategory {
    Performance,
    ImagesAndItems,
    Colors,
    KeyboardShortcuts,
};

} // namespace familiar

// PureRef-style "Restore Defaults" confirmation: instead of one blanket
// yes/no, a checkbox per category (+ an "All" parent checkbox) so the
// user can restore just Colors, just Keyboard Shortcuts, etc. Modal;
// caller reads checkedCategories() only after exec() returns Accepted.
class RestoreDefaultsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RestoreDefaultsDialog(QWidget* parent = nullptr);

    QList<familiar::SettingsCategory> checkedCategories() const;

    // FamSettings "Group/key" keys shown on that category's settings
    // page - empty for Colors/KeyboardShortcuts, which reset through a
    // different mechanism entirely (SettingsHandler::
    // setDefaultCurrentPreset() / KeyboardSettings::restoreDefaults()),
    // not a per-key FamSettings reset. The single source of truth for
    // this mapping - both this dialog's own "pre-check what's actually
    // changed" logic and settings_window.cpp's real reset call this,
    // instead of keeping two copies of the same list in sync by hand.
    static QStringList famSettingsKeysFor(familiar::SettingsCategory category);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void onAllToggled_(int state);
    void onCategoryToggled_();

    QCheckBox* allCheckbox_ = nullptr;
    QMap<familiar::SettingsCategory, QCheckBox*> categoryCheckboxes_;
    // Guards against the All<->category sync handlers re-entering each
    // other while one of them is already busy applying its own update.
    bool syncing_ = false;
};
