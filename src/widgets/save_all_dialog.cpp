#include "save_all_dialog.h"
#include "dialog_style.h"
#include "flat_checkbox.h"
#include "mainwindow.h"

#include <core/settingshandler.h>

#include <QCheckBox>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWindow>

#include "log/log.h"
#include "utils/utils.h"
using namespace familiar::log;

namespace {
constexpr int kiconSize = 40;
}

SaveAllDialog::SaveAllDialog(MainWindow* wm,
                             std::map<int, QString> items,
                             QWidget* parent)
    : QDialog(parent)
    , window_(wm)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("Unsaved changes"));
    setMinimumWidth(380);
    setMaximumWidth(480);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(shadow);

    auto colorPreset = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(20, 16, 16, 16);
    outer->setSpacing(14);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    auto* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(kiconSize, kiconSize);
    iconLabel->setPixmap(
        familiar::dialog_style::severity_icon(QMessageBox::Warning,
                                             accent,
                                             devicePixelRatioF()));
    topRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(4);
    auto* titleLabel = new QLabel(tr("Unsaved changes"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    textCol->addWidget(titleLabel);

    auto* textLabel = new QLabel(
        tr("The following documents have unsaved changes. Choose which "
           "ones to save before closing:"),
        this);
    textLabel->setWordWrap(true);
    textCol->addWidget(textLabel);
    topRow->addLayout(textCol, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setFixedSize(22, 22);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("sadCloseBtn"));
    connect(closeBtn, &QPushButton::clicked, this, &SaveAllDialog::close);
    topRow->addWidget(closeBtn, 0, Qt::AlignTop);

    outer->addLayout(topRow);

    // Scrolls instead of just growing unboundedly - a session with a
    // lot of open modified tabs shouldn't produce a dialog taller than
    // the screen.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMaximumHeight(220);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"));

    auto* listWidget = new QWidget(scroll);
    auto* listLayout = new QVBoxLayout(listWidget);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(6);

    for (auto& [id, path] : items) {
        auto* checkbox = new FlatCheckBox(path.isEmpty() ? tr("Untitled") : path,
                                          textColor,
                                          border,
                                          accent,
                                          listWidget);
        checkbox->setChecked(true);
        checkboxes_.insert(id, checkbox);
        listLayout->addWidget(checkbox);
    }
    listLayout->addStretch();
    scroll->setWidget(listWidget);
    outer->addWidget(scroll);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(8);

    auto* closeWithoutSaveBtn = new QPushButton(tr("Close without saving"),
                                                this);
    familiar::dialog_style::style_secondary_button(closeWithoutSaveBtn,
                                                 textColor,
                                                 border);
    connect(closeWithoutSaveBtn,
            &QPushButton::clicked,
            this,
            &SaveAllDialog::on_close_without_save);
    buttonRow->addWidget(closeWithoutSaveBtn);

    buttonRow->addStretch();

    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    familiar::dialog_style::style_secondary_button(cancelBtn, textColor, border);
    connect(cancelBtn, &QPushButton::clicked, this, &SaveAllDialog::close);
    buttonRow->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton(tr("Save"), this);
    familiar::dialog_style::style_primary_button(saveBtn, accent);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &SaveAllDialog::on_save);
    buttonRow->addWidget(saveBtn);

    outer->addLayout(buttonRow);
    saveBtn->setFocus();

    setStyleSheet(familiar::dialog_style::panel_style_sheet("SaveAllDialog",
                                                          background,
                                                          border,
                                                          textColor,
                                                          /*radiusPx=*/0)
                  + familiar::dialog_style::close_button_style_sheet("sadCloseBtn",
                                                                  textColor,
                                                                  accent));

    centered_widget(window_, this);
    show();
}

void SaveAllDialog::mousePressEvent(QMouseEvent* event)
{
    // Frameless, so this IS the title bar for drag purposes - same
    // idiom MainWindow::tryStartWindowDrag_()/CustomMessageBox use for
    // their own frameless windows.
    if (event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void SaveAllDialog::on_close_without_save()
{
    FLOG_DEBUG(Ch::UI, "SaveAllDialog: close without save");
    MainWindow::exit_project();
}

void SaveAllDialog::on_save()
{
    std::map<int, bool> m;
    for (auto it = checkboxes_.constBegin(); it != checkboxes_.constEnd();
         ++it) {
        FLOG_DEBUG(Ch::UI,
                   "SaveAllDialog: {} {} {}",
                   it.key(),
                   it.value()->text().toStdString(),
                   it.value()->isChecked());
        m.emplace(it.key(), it.value()->isChecked());
    }
    window_->save_all_window_save_cb(this, std::move(m));
}
