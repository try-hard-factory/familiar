#pragma once

#include <QAction>
#include <QFileInfo>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMenuBar>

#include "actions.h"
#include "menu_structure.h"
#include <core/settings.h>

// TODO:

// CRTP mixin — inherit as: class MyWindow : public ActionsMixin<QMainWindow>
//
// Requirements on T:
//   • inherits QWidget (for addAction / removeAction)
//   • declares slots whose names match the Action::callback strings
//     (invoked via QMetaObject::invokeMethod)
//   • declares slot: void on_action_open_recent_file(const QString& path)
template<typename T>
class ActionsMixin : public T
{
public:
    explicit ActionsMixin(QWidget* parent = nullptr) : T(parent) {}

    // Enable/disable every QAction belonging to the named group.
    void actiongroupSetEnabled(const QString& group, bool enabled)
    {
        for (QAction* a : actionGroups_.value(group))
            a->setEnabled(enabled);
    }

    // Build all QActions and populate contextMenu_ / toplevelMenus_.
    // Call once after the widget is fully constructed.
    void buildMenuAndActions()
    {
        contextMenu_ = new QMenu(static_cast<T*>(this));
        toplevelMenus_.clear();
        actionGroups_.clear();
        createActions_();
        createMenu_(contextMenu_, menuStructure());
    }

    // Rebuild the "Open Recent" submenu (call after recent-files list changes).
    void updateMenuAndActions()
    {
        buildRecentFiles_();
    }

    QMenuBar* createMenubar()
    {
        QMenuBar* bar = new QMenuBar();
        for (QMenu* m : toplevelMenus_)
            bar->addMenu(m);
        return bar;
    }

    QMenu* contextMenu() const { return contextMenu_; }

private:
    void createActions_()
    {
        for (Action* action : getActions().all()) {
            QAction* qa = new QAction(action->text, static_cast<T*>(this));
            qa->setAutoRepeat(false);

            const QStringList sc = action->getShortcuts();
            if (!sc.isEmpty()) {
                QList<QKeySequence> seqs;
                for (const QString& s : sc)
                    seqs.append(QKeySequence(s));
                qa->setShortcuts(seqs);
            }

            if (action->checkable) {
                qa->setCheckable(true);
                const bool init = action->settingsKey.isEmpty()
                    ? action->checked
                    : FamSettings().value(action->settingsKey, action->checked).toBool();
                qa->setChecked(init);

                if (!action->settingsKey.isEmpty()) {
                    const QString key = action->settingsKey;
                    QObject::connect(qa, &QAction::toggled, [key](bool v) {
                        FamSettings().setValue(key, v);
                    });
                    // fire callback immediately with initial value
                    if (!action->callback.isEmpty())
                        QMetaObject::invokeMethod(
                            static_cast<T*>(this),
                            action->callback.toUtf8().constData(),
                            Qt::DirectConnection,
                            Q_ARG(bool, init));
                }
                if (!action->callback.isEmpty()) {
                    const QByteArray cb = action->callback.toUtf8();
                    QObject::connect(qa, &QAction::toggled,
                                     static_cast<T*>(this), [this, cb](bool v) {
                        QMetaObject::invokeMethod(
                            static_cast<T*>(this), cb.constData(),
                            Qt::DirectConnection, Q_ARG(bool, v));
                    });
                }
            } else if (!action->callback.isEmpty()) {
                const QByteArray cb = action->callback.toUtf8();
                QObject::connect(qa, &QAction::triggered,
                                 static_cast<T*>(this), [this, cb]() {
                    QMetaObject::invokeMethod(
                        static_cast<T*>(this), cb.constData(),
                        Qt::DirectConnection);
                });
            }

            static_cast<T*>(this)->addAction(qa);

            if (!action->group.isEmpty()) {
                actionGroups_[action->group].append(qa);
                qa->setEnabled(false);
            } else {
                qa->setEnabled(action->enabled);
            }

            action->qaction = qa;
        }
    }

    void createMenu_(QMenu* menu, const QList<MenuNode>& nodes)
    {
        for (const MenuNode& node : nodes) {
            switch (node.type) {
            case MenuNode::Type::Action:
                if (Action* a = getActions().find(node.id))
                    menu->addAction(a->qaction);
                break;
            case MenuNode::Type::Separator:
                menu->addSeparator();
                break;
            case MenuNode::Type::Submenu: {
                QMenu* sub = menu->addMenu(node.label);
                if (menu == contextMenu_)
                    toplevelMenus_.append(sub);
                createMenu_(sub, node.children);
                break;
            }
            case MenuNode::Type::Dynamic:
                buildRecentFiles_(menu);
                break;
            }
        }
    }

    void buildRecentFiles_(QMenu* menu = nullptr)
    {
        if (menu)
            recentFilesSubmenu_ = menu;
        clearRecentFiles_();

        if (!recentFilesSubmenu_)
            return;

        const QStringList files = FamSettings().getRecentFiles(/*existingOnly=*/true);

        for (int i = 0; i < 10; ++i) {
            const QString aid = QStringLiteral("recent_files_%1").arg(i);
            const int key = (i == 9) ? 0 : i + 1;

            Action a = Action::make(
                aid,
                QStringLiteral("File %1").arg(i + 1),
                {},
                {QStringLiteral("Ctrl+%1").arg(key)},
                false, false, {},
                {},
                true,
                QStringLiteral("_build_recent_files"));
            getActions().add(a);

            if (i < files.size()) {
                const QString filename = files[i];
                QAction* qa = new QAction(
                    QFileInfo(filename).fileName(),
                    static_cast<T*>(this));

                const QStringList sc = getActions()[aid].getShortcuts();
                QList<QKeySequence> seqs;
                for (const QString& s : sc)
                    seqs.append(QKeySequence(s));
                qa->setShortcuts(seqs);

                QObject::connect(qa, &QAction::triggered,
                                 static_cast<T*>(this), [this, filename]() {
                    QMetaObject::invokeMethod(
                        static_cast<T*>(this),
                        "on_action_open_recent_file",
                        Qt::DirectConnection,
                        Q_ARG(QString, filename));
                });
                static_cast<T*>(this)->addAction(qa);
                getActions()[aid].qaction = qa;
                recentFilesSubmenu_->addAction(qa);
            }
        }
    }

    void clearRecentFiles_()
    {
        if (!recentFilesSubmenu_)
            return;
        for (QAction* a : recentFilesSubmenu_->actions())
            static_cast<T*>(this)->removeAction(a);
        recentFilesSubmenu_->clear();

        for (const QString& k : getActions().keys()) {
            if (k.startsWith(QLatin1String("recent_files_")))
                getActions()[k].qaction = nullptr;
        }
    }

    QMenu* contextMenu_ = nullptr;
    QList<QMenu*> toplevelMenus_;
    QMap<QString, QList<QAction*>> actionGroups_;
    QMenu* recentFilesSubmenu_ = nullptr;
};
