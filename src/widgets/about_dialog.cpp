#include "about_dialog.h"
#include "dialog_style.h"
#include "mainwindow.h"

#include <core/settingshandler.h>

#include <QColor>
#include <QDate>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

#include "utils/utils.h"

namespace {

// Narrow on purpose - the dialog is a short stack of centred lines.
constexpr int kdialogWidth = 320;
// Secondary lines (version, licence) are dimmed rather than given their
// own preset colour.
constexpr int kdimTextAlpha = 180;
// Gaps around the dimmed block.
constexpr int kblockSpacing = 12;
constexpr int kfooterSpacing = 16;
constexpr int kiconSize = 72;
} // namespace

AboutDialog::AboutDialog(MainWindow* wm, QWidget* parent)
    : QDialog(parent ? parent : wm)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("About"));
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
    // Widest of the lot, same reasoning as HelpDialog.
    constexpr QMargins kouterMargins{24, 14, 24, 20};
    outer->setContentsMargins(kouterMargins);
    outer->setSpacing(4);

    auto* closeBtn = familiar::dialog_style::make_close_button(
        this,
        "adCloseBtn");
    connect(closeBtn, &QPushButton::clicked, this, &AboutDialog::close);
    auto* closeRow = new QHBoxLayout();
    closeRow->addStretch();
    closeRow->addWidget(closeBtn);
    outer->addLayout(closeRow);

    // App icon - reuses whatever main.cpp already loaded into
    // QApplication::windowIcon() (data/img/app/familiar.*) via
    // QWidget::windowIcon()'s own app-wide fallback, not a raw file
    // path of its own.
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(wm->windowIcon().pixmap(kiconSize, kiconSize));
    iconLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(iconLabel);
    outer->addSpacing(4);

    auto* nameLabel = new QLabel(QStringLiteral("Familiar"), this);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameFont.setPointSize(nameFont.pointSize() + 4);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(nameLabel);

    // FAMILIAR_VERSION_STRING - same CMakeLists.txt-defined macro
    // fml_archive.cpp already stamps into every saved manifest's
    // appVersion field, single source of truth for the project's own
    // version number.
    auto* versionLabel = new QLabel(
#ifdef FAMILIAR_VERSION_STRING
        tr("Version %1").arg(QStringLiteral(FAMILIAR_VERSION_STRING)),
#else
        tr("Development build"),
#endif
        this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(dimText.name(QColor::HexArgb)));
    outer->addWidget(versionLabel);

    outer->addSpacing(kblockSpacing);

    auto* descLabel = new QLabel(
        tr("A distraction-free reference board for organizing, arranging, "
           "and annotating images."),
        this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(descLabel);

    outer->addSpacing(kfooterSpacing);

    // CPACK_PACKAGE_VENDOR (CMakeLists.txt) - the one place this
    // project's own organization name is actually recorded; GPLv3 per
    // the repo's own top-level LICENSE file.
    auto* copyrightLabel = new QLabel(tr("© %1 try-hard-factory · GPLv3")
                                          .arg(QDate::currentDate().year()),
                                      this);
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(dimText.name(QColor::HexArgb)));
    outer->addWidget(copyrightLabel);

    setStyleSheet(
        familiar::dialog_style::panel_style_sheet("AboutDialog",
                                                  background,
                                                  border,
                                                  textColor,
                                                  /*radiusPx=*/0)
        + familiar::dialog_style::close_button_style_sheet("adCloseBtn",
                                                           textColor,
                                                           accent));

    centered_widget(wm, this);
    show();
}

void AboutDialog::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so this IS the title bar for drag purposes - same
    // idiom SaveAllDialog/CustomMessageBox use for their own frameless
    // windows.
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}
