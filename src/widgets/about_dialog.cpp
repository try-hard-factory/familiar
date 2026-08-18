#include "about_dialog.h"
#include "dialog_style.h"
#include "mainwindow.h"

#include <core/settingshandler.h>

#include <QDate>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

#include "utils/utils.h"

namespace {
constexpr int kIconSize = 72;
}

AboutDialog::AboutDialog(MainWindow* wm, QWidget* parent)
    : QDialog(parent ? parent : wm)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("About"));
    setFixedWidth(320);

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
    outer->setContentsMargins(24, 14, 24, 20);
    outer->setSpacing(4);

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("adCloseBtn"));
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
    iconLabel->setPixmap(wm->windowIcon().pixmap(kIconSize, kIconSize));
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

    outer->addSpacing(12);

    auto* descLabel = new QLabel(
        tr("A distraction-free reference board for organizing, arranging, "
           "and annotating images."),
        this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(descLabel);

    outer->addSpacing(16);

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

    setStyleSheet(familiar::dialog_style::panelStyleSheet("AboutDialog",
                                                          background,
                                                          border,
                                                          textColor,
                                                          /*radiusPx=*/0)
                  + familiar::dialog_style::closeButtonStyleSheet("adCloseBtn",
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
