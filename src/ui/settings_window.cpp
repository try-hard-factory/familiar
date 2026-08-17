#include "settings_window.h"
#include "mainwindow.h"
#include <core/controls.h>
#include <core/settings.h>
#include <core/settingshandler.h>
#include <ui/colors_widget.h>
#include <widgets/controls/keyboard_shortcuts_page.h>
#include <widgets/controls/search_highlight.h>
#include <widgets/dialog_style.h>
#include <widgets/dialogs.h>
#include <widgets/file_browser_dialog.h>
#include <widgets/setting_row.h>
#include <widgets/settings_style.h>
#include <QAbstractTextDocumentLayout>
#include <QButtonGroup>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWindow>

namespace {

// Both Performance and Images & Items are a QVBoxLayout of
// SettingRowBase-derived rows (widgets/setting_row.h), each carrying its
// display name as objectName() (SettingRowBase's own constructor) - that's
// all this needs to check. Colors has no such decomposition (ColorsWidget
// is one monolithic custom widget), so this simply finds nothing and
// reports no content match there - it can still be reached by matching
// the category name itself.
bool applyGroupFilter(QWidget* page, const QString& text)
{
    bool anyVisible = false;
    for (SettingRowBase* row : page->findChildren<SettingRowBase*>()) {
        const bool matches = text.isEmpty()
                             || row->objectName().contains(text,
                                                           Qt::CaseInsensitive);
        row->setVisible(matches);
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

        // Read from the fixed settings_style palette rather than
        // QPalette::HighlightedText/ButtonText - the checked/hover
        // background is QSS-only (sidebarButtonStyleSheet()), which
        // never updates the real QPalette roles those would resolve to.
        // Every state is a shade of gray (navIdleBg/navHoverBg/
        // navSelectedBg), not a saturated accent, so text stays the
        // same dark color throughout - no need for a light/dark swap.
        const QColor color = familiar::settings_style::palette().text;
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

// The window is frameless now (no native title bar to grab), so this
// plain strip stands in for one: a left-click anywhere on it (not
// consumed by a child, e.g. the centered title QLabel - unhandled mouse
// events on a plain QWidget/QLabel propagate up to their parent) starts
// an OS-level move, same technique as ExportImagesFileExistsDialog/
// ColorPickerDialog (widgets/dialogs.h, widgets/color_picker_dialog.cpp).
class DragTitleBar : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && window()->windowHandle()) {
            window()->windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
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
    , undoHistorySize_(new UndoHistorySizeRow)
    , autoOptimizeImportedImages_(new AutoOptimizeImportedImagesRow)
    , autosaveEnabled_(new AutosaveEnabledRow)
    , autosaveInterval_(new AutosaveIntervalRow)
    , imagesPage_(new QWidget)
    , arrangeGap_(new ArrangeGapRow)
    , allocationLimit_(new MaximumImageSizeRow)
    , arrangeDefault_(new ArrangeDefaultRow)
    , colorsPage_(new QWidget)
    , colors_(new ColorsWidget(this))
    , keyboardShortcutsPage_(new KeyboardShortcutsPage)
{
    setAttribute(Qt::WA_DeleteOnClose);
    // Frameless, custom-chrome window - same convention as every other
    // custom dialog in this app (ExportImagesFileExistsDialog,
    // ColorPickerDialog, ...): no native title bar/min/max, just our own
    // "x" close button (see closeBtn below). Square corners, not a
    // rounded setMask() - see settings_style.cpp's SettingsWindow QSS
    // rule for why that was dropped. This also drops
    // MSWindowsFixedSizeDialogHint's old minimize/maximize buttons that
    // hint never actually suppressed on this WM - there's simply no
    // native chrome left to show them on.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    // This window is deliberately NOT widget-parented to MainWindow (that
    // would cascade its "* { background: transparent }" stylesheet in),
    // so the WM doesn't know it belongs above it - with Always On Top
    // enabled the main window would cover this modal window, leaving the
    // whole app looking frozen. Inherit the hint explicitly instead.
    if (wm && wm->windowFlags().testFlag(Qt::WindowStaysOnTopHint))
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Settings"));
    // Not just resize() - there's no native resize border any more
    // either, so the window needs to actually commit to a fixed size
    // rather than merely start at one (matches the old
    // MSWindowsFixedSizeDialogHint's intent).
    setFixedSize(760, 480);

    // Fixed white palette - always this scheme, unlike
    // the rest of the app which follows the user's chosen accent-color
    // preset (see settings_style.h's own comment).
    setStyleSheet(familiar::settings_style::rootStyleSheet());

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ─── Title bar: centered title + the only window control (close) ──────────
    auto* titleBar = new DragTitleBar(this);
    // Transparent, not the inherited "* { background-color: ... }" white
    // fill - titleBar spans the full width flush to the window's top
    // corners, so an opaque square background here would paint right
    // over SettingsWindow's own rounded corners/border underneath it
    // (children paint on top of their parent). Letting the window's own
    // background/border show through instead is what actually rounds
    // the top corners correctly.
    titleBar->setAttribute(Qt::WA_StyledBackground, true);
    titleBar->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(16, 10, 10, 6);
    titleBarLayout->addStretch(1);
    auto* titleLabel = new QLabel(tr("Settings"), titleBar);
    titleLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    titleBarLayout->addWidget(titleLabel);
    titleBarLayout->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("×"), titleBar);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("settingsCloseBtn"));
    const familiar::settings_style::Palette& sp
        = familiar::settings_style::palette();
    closeBtn->setStyleSheet(
        familiar::dialog_style::closeButtonStyleSheet("settingsCloseBtn",
                                                      sp.text,
                                                      sp.accent));
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);
    titleBarLayout->addWidget(closeBtn);
    outer->addWidget(titleBar);

    auto* root = new QHBoxLayout();
    root->setContentsMargins(16, 0, 16, 16);
    outer->addLayout(root, /*stretch=*/1);

    // ─── Left column: search + category buttons + bottom button row ───────────
    // A real container widget, not just a bare layout added to `root` -
    // narrower than before, and a plain QLayout has no width of its own
    // to pin down: every child (searchBox_, categoryPanel_, the bottom
    // buttons) would otherwise each demand its own natural width and the
    // column would end up as wide as the widest one anyway.
    auto* leftContainer = new QWidget(this);
    leftContainer->setFixedWidth(200);
    auto* leftColumn = new QVBoxLayout(leftContainer);
    leftColumn->setContentsMargins(0, 0, 0, 0);

    searchBox_->setPlaceholderText(tr("Search"));
    searchBox_->setClearButtonEnabled(true);
    // Search: a category's nav button stays visible if its
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
    categoryPanel_->setStyleSheet(
        familiar::settings_style::sidebarButtonStyleSheet());
    auto* categoryLayout = new QVBoxLayout(categoryPanel_);
    // Visible gaps between the filled boxes + a little breathing room
    // around the whole stack - not a flush edge-to-edge list, each
    // category reads as its own block.
    categoryLayout->setContentsMargins(4, 4, 4, 4);
    categoryLayout->setSpacing(6);
    categoryButtons_->setExclusive(true);
    leftColumn->addWidget(categoryPanel_, /*stretch=*/1);

    auto* bottomColumn = new QVBoxLayout();
    bottomColumn->setSpacing(6);
    auto* resetBtn = new QPushButton(tr("Restore Defaults"), this);
    resetBtn->setAutoDefault(false);
    resetBtn->setCursor(Qt::PointingHandCursor);
    resetBtn->setStyleSheet(familiar::settings_style::filledButtonStyleSheet());
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
    bottomColumn->addWidget(resetBtn);

    // Import/Export share a row below Restore Defaults instead of all
    // three sitting side by side - three buttons abreast no longer fit
    // the narrower sidebar (leftContainer above).
    auto* importExportRow = new QHBoxLayout();
    importExportRow->setSpacing(6);

    auto* importBtn = new QPushButton(tr("Import"), this);
    importBtn->setCursor(Qt::PointingHandCursor);
    importBtn->setStyleSheet(familiar::settings_style::filledButtonStyleSheet());
    connect(importBtn, &QPushButton::clicked, this, [this]() {
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

    auto* exportBtn = new QPushButton(tr("Export"), this);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(familiar::settings_style::filledButtonStyleSheet());
    connect(exportBtn, &QPushButton::clicked, this, [this]() {
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
    importExportRow->addWidget(importBtn);
    importExportRow->addWidget(exportBtn);
    bottomColumn->addLayout(importExportRow);
    leftColumn->addLayout(bottomColumn);

    root->addWidget(leftContainer);
    root->addWidget(stack_, /*stretch=*/1);

    // ─── Pages ──────────────────────────────────────────────────────────────

    int categoryIndex = 0;
    auto addCategory = [this,
                        categoryLayout,
                        &categoryIndex](const QString& label, QWidget* page) {
        auto* btn = new CategoryNavButton(categoryPanel_);
        btn->setObjectName(QStringLiteral("categoryButton"));
        btn->setCheckable(true);
        btn->setMinimumHeight(38);
        btn->setLabelText(highlightSearchMatch(label, QString()));
        categoryLayout->addWidget(btn);
        categoryButtons_->addButton(btn, categoryIndex);
        stack_->addWidget(page);
        categories_.append({btn, page, label});
        ++categoryIndex;
    };

    // Performance - a flat column of rows (widgets/setting_row.h) - see
    // setting_row.h's own comment for why: description
    // on hover, not an always-visible paragraph.
    auto* miscLayout = new QVBoxLayout(miscPage_);
    miscLayout->addWidget(undoHistorySize_);
    miscLayout->addWidget(autoOptimizeImportedImages_);
    miscLayout->addWidget(autosaveEnabled_);
    // Not nested/indented as a child widget under autosaveEnabled_ - just
    // the next row down, disabled unless autosave is actually enabled
    // (it's meaningless on its own). A left margin on its own layout is
    // enough to read as "belongs to Autosave" without actual nesting.
    if (auto* intervalLayout = autosaveInterval_->layout())
        intervalLayout->setContentsMargins(24, 4, 0, 4);
    miscLayout->addWidget(autosaveInterval_);
    miscLayout->addStretch(1);
    autosaveInterval_->setControlEnabled(
        FamSettings()
            .valueOrDefault(QStringLiteral("Save/autosave_enabled"))
            .toBool());
    connect(autosaveEnabled_,
            &AutosaveEnabledRow::toggled,
            autosaveInterval_,
            &AutosaveIntervalRow::setControlEnabled);
    addCategory(tr("Performance"), miscPage_);

    // Images & Items - flat column of rows, same shape as Performance
    // above (Items/image_storage_format's own UI dropped here;
    // the setting/facade/get_imgformat() usage elsewhere is untouched).
    auto* imagesLayout = new QVBoxLayout(imagesPage_);
    imagesLayout->addWidget(allocationLimit_);
    imagesLayout->addWidget(arrangeGap_);
    imagesLayout->addWidget(arrangeDefault_);
    imagesLayout->addStretch(1);
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
