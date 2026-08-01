#include "new_settings_window.h"
#include "mainwindow.h"
#include <ui/colors_widget.h>
#include <widgets/controls/keyboard_controls.h>
#include <widgets/controls/mouse_controls.h>
#include <widgets/controls/mousewheel_controls.h>
#include <widgets/dialogs.h>
#include <widgets/settings_dialog.h>
#include <core/controls.h>
#include <core/settings.h>
#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

NewSettingsWindow::NewSettingsWindow(MainWindow* wm, QWidget* parent)
    : QWidget(parent)
    , window_(wm)
    , searchBox_(new QLineEdit(this))
    , categoryPanel_(new QWidget(this))
    , categoryButtons_(new QButtonGroup(this))
    , stack_(new QStackedWidget(this))
    , miscPage_(new QWidget)
    , confirmCloseUnsaved_(new ConfirmCloseUnsavedWidget)
    , imagesPage_(new QWidget)
    , imageStorageFormat_(new ImageStorageFormatWidget)
    , arrangeGap_(new ArrangeGapWidget)
    , allocationLimit_(new AllocationLimitWidget)
    , arrangeDefault_(new ArrangeDefaultWidget)
    , colorsPage_(new QWidget)
    , colors_(new ColorsWidget(this))
    , shortcutsPage_(new QWidget)
    , shortcuts_(new KeyboardShortcutsView)
    , mousePage_(new QWidget)
    , mouse_(new MouseView)
    , mouseWheelPage_(new QWidget)
    , mouseWheel_(new MouseWheelView)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Configuration"));
    resize(760, 480);

    auto* root = new QHBoxLayout(this);

    // ─── Left column: search + category buttons + bottom button row ───────────
    auto* leftColumn = new QVBoxLayout();

    searchBox_->setPlaceholderText(tr("Search"));
    connect(searchBox_, &QLineEdit::textChanged, this, [this](const QString& text) {
        for (auto* btn : categoryPanel_->findChildren<QPushButton*>()) {
            btn->setVisible(text.isEmpty()
                            || btn->text().contains(text, Qt::CaseInsensitive));
        }
    });
    leftColumn->addWidget(searchBox_);

    // Vertical nav: plain checkable buttons instead of a QListWidget,
    // one exclusive group so exactly one stays highlighted.
    categoryPanel_->setStyleSheet(
        "QPushButton#categoryButton {"
        "  text-align: left;"
        "  padding: 6px 10px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        "QPushButton#categoryButton:checked {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QPushButton#categoryButton:hover:!checked {"
        "  background: palette(alternate-base);"
        "}");
    auto* categoryLayout = new QVBoxLayout(categoryPanel_);
    categoryLayout->setContentsMargins(0, 0, 0, 0);
    categoryLayout->setSpacing(0);
    categoryButtons_->setExclusive(true);
    leftColumn->addWidget(categoryPanel_, /*stretch=*/1);

    auto* bottomRow = new QHBoxLayout();
    auto* resetBtn = new QPushButton(tr("Restore Defaults"), this);
    resetBtn->setAutoDefault(false);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto reply = showMessageBox(
            QMessageBox::Question,
            this,
            tr("Restore defaults?"),
            tr("Do you want to restore all settings to their default values?"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            FamSettings().restoreDefaults();
            KeyboardSettings().restoreDefaults();
        }
    });
    bottomRow->addWidget(resetBtn);
    bottomRow->addStretch();

    // TODOLATER: wire up once a settings import/export file format exists
    // (see memory/familiar_next_steps.md step 5/6 - single JSON settings
    // file). Shown disabled rather than faked, so the layout is in place
    // without pretending this works yet.
    auto* importBtn = new QToolButton(this);
    importBtn->setText(tr("Import"));
    importBtn->setEnabled(false);
    auto* exportBtn = new QToolButton(this);
    exportBtn->setText(tr("Export"));
    exportBtn->setEnabled(false);
    bottomRow->addWidget(importBtn);
    bottomRow->addWidget(exportBtn);
    leftColumn->addLayout(bottomRow);

    root->addLayout(leftColumn);
    root->addWidget(stack_, /*stretch=*/1);

    // ─── Pages - same content as SettingsWindow's tabs (ui/settings_window.cpp) ─

    int categoryIndex = 0;
    auto addCategory = [this, categoryLayout, &categoryIndex](const QString& label,
                                                               QWidget* page) {
        auto* btn = new QPushButton(label, categoryPanel_);
        btn->setObjectName(QStringLiteral("categoryButton"));
        btn->setCheckable(true);
        categoryLayout->addWidget(btn);
        categoryButtons_->addButton(btn, categoryIndex);
        stack_->addWidget(page);
        ++categoryIndex;
    };

    // Miscellaneous
    auto* miscLayout = new QGridLayout(miscPage_);
    miscLayout->addWidget(confirmCloseUnsaved_, 0, 0);
    addCategory(tr("Miscellaneous"), miscPage_);

    // Images & Items
    auto* imagesLayout = new QGridLayout(imagesPage_);
    imagesLayout->addWidget(imageStorageFormat_, 0, 0);
    imagesLayout->addWidget(allocationLimit_, 0, 1);
    imagesLayout->addWidget(arrangeGap_, 1, 0);
    imagesLayout->addWidget(arrangeDefault_, 1, 1);
    addCategory(tr("Images & Items"), imagesPage_);

    // Colors
    auto* colorsLayout = new QVBoxLayout(colorsPage_);
    colorsLayout->addWidget(colors_);
    addCategory(tr("Colors"), colorsPage_);

    // Keyboard Shortcuts
    auto* shortcutsLayout = new QVBoxLayout(shortcutsPage_);
    shortcutsLayout->addWidget(shortcuts_);
    addCategory(tr("Keyboard Shortcuts"), shortcutsPage_);

    // Mouse
    auto* mouseLayout = new QVBoxLayout(mousePage_);
    mouseLayout->addWidget(mouse_);
    addCategory(tr("Mouse"), mousePage_);

    // Mouse Wheel
    auto* mouseWheelLayout = new QVBoxLayout(mouseWheelPage_);
    mouseWheelLayout->addWidget(mouseWheel_);
    addCategory(tr("Mouse Wheel"), mouseWheelPage_);

    categoryLayout->addStretch(1);

    connect(categoryButtons_,
            &QButtonGroup::idClicked,
            stack_,
            &QStackedWidget::setCurrentIndex);
    categoryButtons_->button(0)->setChecked(true);
    stack_->setCurrentIndex(0);
}

void NewSettingsWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}
