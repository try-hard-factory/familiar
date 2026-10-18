#include "help_dialog.h"
#include "dialog_style.h"
#include "mainwindow.h"

#include <core/settingshandler.h>

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMargins>
#include <QMouseEvent>
#include <QObject>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>
#include <Qt>

#include "utils/utils.h"

namespace {

// Wide enough for the longest gesture description without wrapping.
constexpr int kdialogWidth = 400;
// Secondary text ("Esc to close" and friends) is dimmed rather than given
// its own preset colour.
constexpr int kdimTextAlpha = 180;
constexpr int kouterSpacing = 10;
// The shortcut grid: generous horizontal gap between action and gesture,
// tight vertical one so rows read as a list.
constexpr int kgridHorizontalSpacing = 18;
constexpr int kgridVerticalSpacing = 6;
// Gap before the closing hint at the bottom.
constexpr int kfooterSpacing = 8;

struct ControlRow
{
    QString action;
    QString gesture;
};

// Familiar's OWN actual default gestures - core/controls.cpp's
// mouseActions() (pan default), CanvasScene::mousePressEvent()/
// mouseDoubleClickEvent() (select/focus), CanvasView::wheelEvent()
// (plain-scroll zoom, hardcoded, not user-configurable), MainWindow::
// tryStartWindowDrag_() (empty menu/tab bar drag). Deliberately NOT a
// copy of the reference app's own table, which
// differs in real ways: that app pans via a scroll-click drag and moves
// its window via a right-click drag; familiar's are Alt+Left drag and
// an empty-chrome left-drag respectively, and right-click is currently
// unused on this app's canvas (no context menu here at all).
const QList<ControlRow>& default_control_rows()
{
    static const QList<ControlRow> rows = {
        {.action = QObject::tr("Select images"),
         .gesture = QObject::tr("Left click / drag")},
        {.action = QObject::tr("Focus image"),
         .gesture = QObject::tr("Double left click")},
        {.action = QObject::tr("Zoom to pointer"),
         .gesture = QObject::tr("Scroll wheel")},
        {.action = QObject::tr("Pan"),
         .gesture = QObject::tr("Alt + Left drag, or Middle drag")},
        {.action = QObject::tr("Move window"),
         .gesture = QObject::tr("Drag menu/tab bar")},
    };
    return rows;
}

// A wrapping rich-text QLabel with ONE inline link (`linkText`, plain
// href="#" - never actually navigates, just a hook for linkActivated)
// embedded in a sentence, e.g. "To see or change every shortcut, open
// Keyboard Shortcuts." Deliberately ONE label rather than a separate
// intro QLabel + link QLabel side by side in an HBoxLayout - neither
// widget word-wraps on its own in that layout, so at this dialog's
// width the text just ran past the edge and got clipped instead of
// flowing to a second line. A single wrapping label reflows the WHOLE
// sentence together, link included, exactly like a real paragraph.
QLabel* make_inline_link_paragraph(const QString& before,
                                   const QString& linkText,
                                   const QString& after,
                                   QWidget* parent,
                                   const QColor& accent)
{
    const QString prefix = before.isEmpty() ? QString() : before + QChar(' ');
    auto* label = new QLabel(QStringLiteral("%1<a href=\"#\" style=\"color:%2; "
                                            "text-decoration:none;\">%3</a>%4")
                                 .arg(prefix, accent.name(), linkText, after),
                             parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setCursor(Qt::PointingHandCursor);
    label->setOpenExternalLinks(false);
    return label;
}

} // namespace

HelpDialog::HelpDialog(MainWindow* wm, QWidget* parent)
    : QDialog(parent ? parent : wm)
    , window_(wm)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Help"));
    setFixedWidth(kdialogWidth);

    familiar::dialog_style::apply_dialog_shadow(this);

    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::TextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::BackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::BorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::SelectionColor];
    QColor dimText = textColor;
    dimText.setAlpha(kdimTextAlpha);

    auto* outer = new QVBoxLayout(this);
    // Wider than apply_dialog_margins() on purpose - this dialog is wall-to-
    // wall text and needs the breathing room.
    constexpr QMargins kouterMargins{22, 14, 22, 20};
    outer->setContentsMargins(kouterMargins);
    outer->setSpacing(kouterSpacing);

    auto* closeBtn = familiar::dialog_style::make_close_button(this,
                                                               "hdCloseBtn");
    connect(closeBtn, &QPushButton::clicked, this, &HelpDialog::close);

    QFont headingFont = font();
    headingFont.setBold(true);

    auto* titleLabel = new QLabel(tr("Help"), this);
    QFont titleFont = headingFont;
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    auto* topRow = new QHBoxLayout();
    topRow->addWidget(titleLabel);
    topRow->addStretch();
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);
    outer->addLayout(topRow);

    auto* controlsHeading = new QLabel(tr("Controls"), this);
    controlsHeading->setFont(headingFont);
    outer->addWidget(controlsHeading);

    // Bordered box, matching the reference app's own layout
    // - a simple 2-column grid (action label / gesture), right column
    // right-aligned so it reads as a clean table even without real grid
    // lines.
    auto* box = new QFrame(this);
    box->setObjectName(QStringLiteral("hdControlsBox"));
    box->setStyleSheet(QStringLiteral("#hdControlsBox { border: 1px solid %1; "
                                      "border-radius: 6px; }")
                           .arg(border.name()));
    auto* grid = new QGridLayout(box);
    constexpr QMargins kgridMargins{14, 10, 14, 10};
    grid->setContentsMargins(kgridMargins);
    grid->setHorizontalSpacing(kgridHorizontalSpacing);
    grid->setVerticalSpacing(kgridVerticalSpacing);
    int row = 0;
    for (const ControlRow& item : default_control_rows()) {
        auto* actionLabel = new QLabel(item.action, box);
        auto* gestureLabel = new QLabel(item.gesture, box);
        gestureLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(dimText.name(QColor::HexArgb)));
        grid->addWidget(actionLabel, row, 0);
        grid->addWidget(gestureLabel, row, 1, Qt::AlignRight);
        ++row;
    }
    outer->addWidget(box);

    auto* shortcutsLabel = make_inline_link_paragraph(
        tr("To see or change every shortcut, open"),
        tr("Keyboard Shortcuts"),
        QString(),
        this,
        accent);
    // Opens the settings window already on that specific category
    // (SettingsWindow::selectCategory()), not just wherever
    // settingsWindow() would otherwise land by default.
    connect(shortcutsLabel,
            &QLabel::linkActivated,
            window_,
            &MainWindow::open_keyboard_shortcuts_settings);
    outer->addWidget(shortcutsLabel);

    outer->addSpacing(4);

    auto* supportHeading = new QLabel(tr("Support"), this);
    supportHeading->setFont(headingFont);
    outer->addWidget(supportHeading);

    auto* supportLabel = make_inline_link_paragraph(
        tr("Something not working right? Write me to discord:"),
        tr("?????"),
        QStringLiteral("."),
        this,
        accent);

    outer->addWidget(supportLabel);

    outer->addSpacing(kfooterSpacing);

    auto* aboutLabel = make_inline_link_paragraph(QString(),
                                                  tr("About Familiar"),
                                                  QString(),
                                                  this,
                                                  accent);
    aboutLabel->setAlignment(Qt::AlignHCenter);
    connect(aboutLabel,
            &QLabel::linkActivated,
            window_,
            &MainWindow::on_action_about);
    outer->addWidget(aboutLabel);

    setStyleSheet(
        familiar::dialog_style::panel_style_sheet("HelpDialog",
                                                  background,
                                                  border,
                                                  textColor,
                                                  /*radiusPx=*/0)
        + familiar::dialog_style::close_button_style_sheet("hdCloseBtn",
                                                           textColor,
                                                           accent));

    centered_widget(window_, this);
    show();
}

void HelpDialog::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so this IS the title bar for drag purposes - same
    // idiom SaveAllDialog/CustomMessageBox use for their own frameless
    // windows. Only fires for a click on the dialog's own background -
    // Qt already routes a click on a child widget (the link labels,
    // close button) to that widget first, never reaching here.
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}
