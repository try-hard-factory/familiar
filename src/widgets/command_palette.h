#pragma once

#include <QColor>
#include <QDialog>
#include <QElapsedTimer>
#include <QHash>
#include <QKeySequence>
#include <QPointer>
#include <QString>
#include <QStringList>

class QAction;
class ActionRegistry;
class QEvent;
class QKeyEvent;
class QLineEdit;
class QShowEvent;
class QTreeWidget;
class QTreeWidgetItem;

// A quick keyboard path to the same QActions used by the menus. The dialog
// returns the chosen action; its caller triggers it after exec() has ended so
// commands that open another dialog never run inside this one's modal loop.
class CommandPalette final : public QDialog
{
public:
    explicit CommandPalette(ActionRegistry& registry, QWidget* parent = nullptr);

    QAction* selected_action() const { return selectedAction_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void refresh_results();
    void move_selection(int direction);
    void activate_item(QTreeWidgetItem* item);
    void activate_action(const QString& id);
    bool handle_shortcut(QKeyEvent* event, bool inSearch, bool commit);
    void remember_action(const QString& id);
    QString unavailable_reason(const QString& id, const QString& group) const;

    ActionRegistry& registry_;
    QLineEdit* search_ = nullptr;
    QTreeWidget* results_ = nullptr;
    QColor detailText_;
    QHash<QString, QString> categories_;
    QStringList recentIds_;
    QKeySequence pendingShortcut_;
    QElapsedTimer shortcutClock_;
    QPointer<QAction> selectedAction_;
};
