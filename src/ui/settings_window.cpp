#include "settings_window.h"
#include "mainwindow.h"
#include <core/controls.h>
#include <core/settings.h>
#include <core/settingshandler.h>
#include <ui/colors_widget.h>
#include <widgets/controls/keyboard_shortcuts_page.h>
#include <widgets/controls/search_highlight.h>
#include <widgets/dialogs.h>
#include <widgets/file_browser_dialog.h>
#include <widgets/settings_dialog.h>
#include <QAbstractTextDocumentLayout>
#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Misc/Images & Items are a QGridLayout of SettingsGroupBase-derived
// QGroupBoxes (settings_dialog.h), each already carrying its display
// title as objectName() (see SettingsGroupBase::updateTitle()) - reused
// here as-is rather than adding a parallel "searchable label" API.
// Colors has no such children (ColorsWidget is one monolithic custom
// widget, not decomposed into named sub-items), so this simply finds
// nothing and reports no content match there - it can still be reached
// by matching the category name itself.
bool applyGroupFilter(QWidget* page, const QString& text)
{
    bool anyVisible = false;
    for (SettingsGroupBase* group : page->findChildren<SettingsGroupBase*>()) {
        const bool matches
            = text.isEmpty()
              || group->objectName().contains(text, Qt::CaseInsensitive);
        group->setVisible(matches);
        anyVisible = anyVisible || matches;
    }
    return anyVisible;
}

// QPushButton::setText() can't render the <b> a search match needs to be
// bolded - nesting a QLayout+QLabel inside the button to work around that
// visually broke the checked/hover background (it ended up painted at a
// different, stale geometry than the label, a QSS/QAbstractButton
// layout-caching mismatch). Painting the rich text directly instead:
// draw the normal button chrome via the style with its text left empty,
// then lay `richText_` out with QTextDocument on top, in the same paint
// pass - so background and text can never drift apart again.
class CategoryNavButton : public QPushButton
{
public:
    explicit CategoryNavButton(QWidget* parent)
        : QPushButton(parent)
    {}

    void setLabelText(const QString& richText)
    {
        richText_ = richText;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QStylePainter painter(this);
        QStyleOptionButton opt;
        initStyleOption(&opt);
        opt.text.clear();
        painter.drawControl(QStyle::CE_PushButton, opt);

        const QColor color = palette().color(
            isChecked() ? QPalette::HighlightedText : QPalette::ButtonText);
        QTextDocument doc;
        doc.setDefaultFont(font());
        doc.setDefaultStyleSheet(
            QStringLiteral("body { color: %1; }").arg(color.name()));
        doc.setHtml(QStringLiteral("<body>%1</body>").arg(richText_));
        doc.setTextWidth(width() - 20);

        painter.save();
        painter.translate(10, (height() - doc.size().height()) / 2);
        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.palette.setColor(QPalette::Text, color);
        doc.documentLayout()->draw(&painter, ctx);
        painter.restore();
    }

private:
    QString richText_;
};

} // namespace

SettingsWindow::SettingsWindow(MainWindow* wm, QWidget* parent)
    : QWidget(parent)
    , window_(wm)
    , searchBox_(new QLineEdit(this))
    , categoryPanel_(new QWidget(this))
    , categoryButtons_(new QButtonGroup(this))
    , stack_(new QStackedWidget(this))
    , miscPage_(new QWidget)
    , confirmCloseUnsaved_(new ConfirmCloseUnsavedWidget)
    , autosaveEnabled_(new AutosaveEnabledWidget)
    , autosaveInterval_(new AutosaveIntervalWidget)
    , imagesPage_(new QWidget)
    , imageStorageFormat_(new ImageStorageFormatWidget)
    , arrangeGap_(new ArrangeGapWidget)
    , allocationLimit_(new AllocationLimitWidget)
    , arrangeDefault_(new ArrangeDefaultWidget)
    , colorsPage_(new QWidget)
    , colors_(new ColorsWidget(this))
    , keyboardShortcutsPage_(new KeyboardShortcutsPage)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint
                   | Qt::MSWindowsFixedSizeDialogHint);
    // This window is deliberately NOT widget-parented to MainWindow (that
    // would cascade its "* { background: transparent }" stylesheet in),
    // so the WM doesn't know it belongs above it - with Always On Top
    // enabled the main window would cover this modal window, leaving the
    // whole app looking frozen. Inherit the hint explicitly instead.
    if (wm && wm->windowFlags().testFlag(Qt::WindowStaysOnTopHint))
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Configuration"));
    resize(760, 480);

    auto* root = new QHBoxLayout(this);

    // ─── Left column: search + category buttons + bottom button row ───────────
    auto* leftColumn = new QVBoxLayout();

    searchBox_->setPlaceholderText(tr("Search"));
    searchBox_->setClearButtonEnabled(true);
    // PureRef-style search: a category's nav button stays visible if its
    // own name matches OR any item inside it does; the page itself is
    // filtered down to just the matching items (whole page shown
    // unfiltered when the category name itself is what matched). If the
    // currently open category drops out, jump to the first one that's
    // still visible so the user isn't left looking at a hidden page.
    connect(searchBox_, &QLineEdit::textChanged, this, [this](const QString& text) {
        QPushButton* firstVisible = nullptr;
        bool currentStillVisible = false;
        for (const SettingsCategory& cat : categories_) {
            const bool nameMatches = text.isEmpty()
                                     || cat.name.contains(text,
                                                          Qt::CaseInsensitive);
            const QString contentFilter = nameMatches ? QString() : text;
            bool contentMatches;
            if (auto* kb = qobject_cast<KeyboardShortcutsPage*>(cat.page))
                contentMatches = kb->applySearchFilter(contentFilter);
            else
                contentMatches = applyGroupFilter(cat.page, contentFilter);

            // Only the category's own name is a candidate for bolding here
            // - if nothing but its content matched, the name itself has
            // no matched substring to highlight.
            static_cast<CategoryNavButton*>(cat.button)
                ->setLabelText(
                    highlightSearchMatch(cat.name,
                                         nameMatches ? text : QString()));

            const bool visible = nameMatches || contentMatches;
            cat.button->setVisible(visible);
            if (visible) {
                if (!firstVisible)
                    firstVisible = cat.button;
                if (cat.button->isChecked())
                    currentStillVisible = true;
            }
        }
        if (!currentStillVisible && firstVisible)
            firstVisible->click();
    });
    leftColumn->addWidget(searchBox_);

    // Vertical nav: plain checkable buttons instead of a QListWidget,
    // one exclusive group so exactly one stays highlighted. Text color
    // is picked in CategoryNavButton::paintEvent() directly rather than
    // through this stylesheet's "color" property, since that's only
    // read by the style's own (now-unused) text drawing.
    categoryPanel_->setStyleSheet("QPushButton#categoryButton {"
                                  "  border: none;"
                                  "  background: transparent;"
                                  "}"
                                  "QPushButton#categoryButton:checked {"
                                  "  background: palette(highlight);"
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

    auto* importBtn = new QToolButton(this);
    importBtn->setText(tr("Import"));
    connect(importBtn, &QToolButton::clicked, this, [this]() {
        const QString path = showOpenFileDialog(this,
                                                tr("Import Settings"),
                                                QString(),
                                                tr("JSON files (*.json)"));
        if (path.isEmpty())
            return;
        if (!SettingsHandler::getInstance()->importSettingsFrom(path)) {
            showMessageBox(QMessageBox::Warning,
                           this,
                           tr("Import failed"),
                           tr("Could not read settings from %1.").arg(path));
            return;
        }
        // Not restoreDefaults() - that would wipe out what was just
        // imported instead of showing it. These are the same three
        // signals that already keep every live settings widget in sync
        // (Misc/Images group boxes, Keyboard Shortcuts tree, Colors) -
        // see bindingsChanged()/refreshAll() wiring above and
        // ColorsWidget::updateComponents()'s presetsChanged connection.
        emit SettingsEvents::instance().restoreDefaults();
        emit SettingsEvents::instance().restoreKeyboardDefaults();
        emit SettingsHandler::getInstance() -> presetsChanged();
    });

    auto* exportBtn = new QToolButton(this);
    exportBtn->setText(tr("Export"));
    connect(exportBtn, &QToolButton::clicked, this, [this]() {
        const QString path = showSaveFileDialog(this,
                                                tr("Export Settings"),
                                                QString(),
                                                tr("JSON files (*.json)"),
                                                QStringLiteral(
                                                    "familiar-settings.json"));
        if (path.isEmpty())
            return;
        if (!SettingsHandler::getInstance()->exportSettingsTo(path)) {
            showMessageBox(QMessageBox::Warning,
                           this,
                           tr("Export failed"),
                           tr("Could not write settings to %1.").arg(path));
        }
    });
    bottomRow->addWidget(importBtn);
    bottomRow->addWidget(exportBtn);
    leftColumn->addLayout(bottomRow);

    root->addLayout(leftColumn);
    root->addWidget(stack_, /*stretch=*/1);

    // ─── Pages ──────────────────────────────────────────────────────────────

    int categoryIndex = 0;
    auto addCategory = [this,
                        categoryLayout,
                        &categoryIndex](const QString& label, QWidget* page) {
        auto* btn = new CategoryNavButton(categoryPanel_);
        btn->setObjectName(QStringLiteral("categoryButton"));
        btn->setCheckable(true);
        btn->setMinimumHeight(28);
        btn->setLabelText(highlightSearchMatch(label, QString()));
        categoryLayout->addWidget(btn);
        categoryButtons_->addButton(btn, categoryIndex);
        stack_->addWidget(page);
        categories_.append({btn, page, label});
        ++categoryIndex;
    };

    // Miscellaneous
    auto* miscLayout = new QGridLayout(miscPage_);
    miscLayout->addWidget(confirmCloseUnsaved_, 0, 0);
    miscLayout->addWidget(autosaveEnabled_, 1, 0);
    // Nested inside the checkbox's own group box (not a separate row of
    // its own) and disabled unless autosave is actually enabled - the
    // interval is meaningless on its own.
    autosaveEnabled_->addNestedWidget(autosaveInterval_);
    autosaveInterval_->setEnabled(
        FamSettings()
            .valueOrDefault(QStringLiteral("Save/autosave_enabled"))
            .toBool());
    connect(autosaveEnabled_,
            &AutosaveEnabledWidget::toggled,
            autosaveInterval_,
            &AutosaveIntervalWidget::setEnabled);
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

    // Keyboard Shortcuts (Actions + Controls sections, replaces the old
    // separate Keyboard Shortcuts/Mouse/Mouse Wheel categories)
    addCategory(tr("Keyboard Shortcuts"), keyboardShortcutsPage_);

    categoryLayout->addStretch(1);

    connect(categoryButtons_,
            &QButtonGroup::idClicked,
            stack_,
            &QStackedWidget::setCurrentIndex);
    categoryButtons_->button(0)->setChecked(true);
    stack_->setCurrentIndex(0);
}

void SettingsWindow::selectCategory(const QString& name)
{
    for (int i = 0; i < categories_.size(); ++i) {
        if (categories_[i].name != name)
            continue;
        // setChecked() alone doesn't fire idClicked (that's only
        // emitted on an actual user click) - the stack switch above is
        // wired to that signal, so it needs its own explicit call here,
        // same as the constructor's own last two lines for index 0.
        categoryButtons_->button(i)->setChecked(true);
        stack_->setCurrentIndex(i);
        return;
    }
}

void SettingsWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        close();
    }
}
