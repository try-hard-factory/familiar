/**
* \file shortcuts_widget.cpp
* \author max
* Created on Sun May  1 14:10:54 2022
*/

#include "shortcuts_widget.h"

#include <QCursor>
#include <QHeaderView>
#include <QRect>
#include <QScreen>
#include <QTableWidget>
#include <QVBoxLayout>

#include <core/qguiappcurrentscreen.h>
#include <ui/setshortcut_widget.h>
#include <core/settingshandler.h>

#include "Logger.h"

extern Logger logger;

ShortcutsWidget::ShortcutsWidget(QWidget* parent)
    : QWidget(parent)
    , table_(new QTableWidget(this))
    , layout_(new QVBoxLayout(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("HotKeys"));

    auto position = frameGeometry();
    auto screen = QGuiAppCurrentScreen().currentScreen();
    position.moveCenter(screen->availableGeometry().center());
    move(position.topLeft());

    layout_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    initInfoTable();
    connect(SettingsHandler::getInstance(),
            &SettingsHandler::fileChanged,
            this,
            &ShortcutsWidget::populateInfoTable);
    show();
}

void ShortcutsWidget::initInfoTable()
{
    table_->setToolTip(tr("Available shortcuts in the screen capture mode."));

    layout_->addWidget(table_);

    table_->setColumnCount(2);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->verticalHeader()->hide();

    // header creation
    QStringList names;
    names << tr("Description") << tr("Key");
    table_->setHorizontalHeaderLabels(names);
    connect(table_,
            &QTableWidget::cellClicked,
            this,
            &ShortcutsWidget::onShortcutCellClicked);

    populateInfoTable();

    table_->horizontalHeader()->setMinimumSectionSize(200);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->horizontalHeader()->setSizePolicy(QSizePolicy::Expanding,
                                              QSizePolicy::Expanding);
    table_->resizeColumnsToContents();
    table_->resizeRowsToContents();
}

void ShortcutsWidget::populateInfoTable()
{
    loadShortCuts();
    table_->setRowCount(shortcuts_.size());

    for (int i = 0; i < shortcuts_.size(); ++i) {
        const auto current_sc = shortcuts_.at(i);   // maybe const auto & ????
        const auto identifier = current_sc.at(0);   // maybe const auto & ????
        const auto description = current_sc.at(1);  // maybe const auto & ????
        const auto key_sequence = current_sc.at(2); // maybe const auto & ????

        table_->setItem(i, 0, new QTableWidgetItem(description));

        QTableWidgetItem* item = new QTableWidgetItem(key_sequence);

        item->setTextAlignment(Qt::AlignCenter);
        table_->setItem(i, 1, item);

        if (identifier.isEmpty()) {
            QFont font;
            font.setBold(true);
            item->setFont(font);
            item->setFlags(item->flags() ^ Qt::ItemIsEnabled);
            table_->item(i, 1)->setFont(font);
        }
    }

    for (int x = 0; x < table_->rowCount(); ++x) {
        for (int y = 0; y < table_->columnCount(); ++y) {
            QTableWidgetItem* item = table_->item(x, y);
            item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        }
    }
}

void ShortcutsWidget::onShortcutCellClicked(int row, int col)
{
    auto settings = SettingsHandler::getInstance();
    if (col != 1) {
        return;
    }

    // Ignore non-changable shortcuts
    const auto& itemFlags = table_->item(row, col)->flags();
    if (!(itemFlags & Qt::ItemIsEnabled)) {
        return;
    }

    const auto& shortcutName = shortcuts_.at(row).at(0);
    auto setShortcutDialog = std::make_unique<SetShortcutDialog>(nullptr,
                                                                 shortcutName);

    if (setShortcutDialog->exec() != 0) {
        auto shortcutValue = setShortcutDialog->shortcut();

        // set no shortcut is Backspace
        if (shortcutValue == QKeySequence(Qt::Key_Backspace)) {
            LOG_WARNING(logger, "BACKSPACE!!!!!!! ", shortcutName.toStdString());
            shortcutValue = QKeySequence("");
        }

        if (settings->setShortcut(shortcutName, shortcutValue.toString())) {
            //                emit SettingsHandler::getInstance()->shortCutChanged(shortcutName);
            populateInfoTable();
        }
    }
}

void ShortcutsWidget::loadShortCuts()
{
    shortcuts_.clear();
    appendShortcut("TYPE_NEW", "New workplace");
    appendShortcut("TYPE_OPEN", "Open project");
    appendShortcut("TYPE_SAVE", "Save current file");
    appendShortcut("TYPE_QUIT", "Quit application");
}

void ShortcutsWidget::appendShortcut(const QString& shortcutName,
                                     const QString& description)
{
    auto settings = SettingsHandler::getInstance();
    QString shortcut = settings->shortcut(shortcutName);
    shortcuts_ << (QStringList()
                   << shortcutName
                   << QObject::tr(description.toStdString().c_str())
                   << shortcut.replace("Return", "Enter"));
}
