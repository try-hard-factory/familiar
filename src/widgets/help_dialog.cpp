#include "help_dialog.h"
#include "dialog_style.h"
#include "mainwindow.h"

#include <core/settingshandler.h>

#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

#include "utils/utils.h"

namespace {

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
const QList<ControlRow>& defaultControlRows()
{
    static const QList<ControlRow> rows = {
        {QObject::tr("Select images"), QObject::tr("Left click / drag")},
        {QObject::tr("Focus image"), QObject::tr("Double left click")},
        {QObject::tr("Zoom to pointer"), QObject::tr("Scroll wheel")},
        {QObject::tr("Pan"), QObject::tr("Alt + Left drag")},
        {QObject::tr("Move window"), QObject::tr("Drag menu/tab bar")},
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
QLabel* makeInlineLinkParagraph(const QString& before,
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
    setFixedWidth(400);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];
    QColor dimText = textColor;
    dimText.setAlpha(180);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(22, 14, 22, 20);
    outer->setSpacing(10);

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("hdCloseBtn"));
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
    grid->setContentsMargins(14, 10, 14, 10);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(6);
    int row = 0;
    for (const ControlRow& item : defaultControlRows()) {
        auto* actionLabel = new QLabel(item.action, box);
        auto* gestureLabel = new QLabel(item.gesture, box);
        gestureLabel->setStyleSheet(
            QStringLiteral("color: %1;").arg(dimText.name(QColor::HexArgb)));
        grid->addWidget(actionLabel, row, 0);
        grid->addWidget(gestureLabel, row, 1, Qt::AlignRight);
        ++row;
    }
    outer->addWidget(box);

    auto* shortcutsLabel
        = makeInlineLinkParagraph(tr("To see or change every shortcut, open"),
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
            &MainWindow::openKeyboardShortcutsSettings);
    outer->addWidget(shortcutsLabel);

    outer->addSpacing(4);

    auto* supportHeading = new QLabel(tr("Support"), this);
    supportHeading->setFont(headingFont);
    outer->addWidget(supportHeading);

    auto* supportLabel = makeInlineLinkParagraph(
        tr("Something not working right? Write me to discord:"),
        tr("?????"),
        QStringLiteral("."),
        this,
        accent);

    outer->addWidget(supportLabel);

    outer->addSpacing(8);

    auto* aboutLabel = makeInlineLinkParagraph(QString(),
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

    setStyleSheet(familiar::dialog_style::panelStyleSheet("HelpDialog",
                                                          background,
                                                          border,
                                                          textColor,
                                                          /*radiusPx=*/0)
                  + familiar::dialog_style::closeButtonStyleSheet("hdCloseBtn",
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
