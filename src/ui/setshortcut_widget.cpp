/**
* \file setshortcut_widget.cpp
* \author max
* Created on Sun May  1 16:20:23 2022
*/

#include "setshortcut_widget.h"
// #include "src/utils/globalvalues.h"
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QPixmap>

SetShortcutDialog::SetShortcutDialog(QDialog* parent, QString shortcutName)
  : QDialog(parent)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    // setWindowIcon(QIcon(GlobalValues::iconPath()));
    setWindowTitle(tr("Set Shortcut"));
    ks_ = QKeySequence();

    layout_ = new QVBoxLayout(this);
    layout_->setAlignment(Qt::AlignHCenter);

    auto* infoTop = new QLabel(tr("Enter new shortcut to change "));
    infoTop->setMargin(10);
    infoTop->setAlignment(Qt::AlignCenter);
    layout_->addWidget(infoTop);

    auto* infoIcon = new QLabel();
    infoIcon->setAlignment(Qt::AlignCenter);
    infoIcon->setPixmap(QPixmap(":/img/app/keyboard.svg"));
    layout_->addWidget(infoIcon);

    layout_->addWidget(infoIcon);

    QString msg = tr("Press Esc to cancel or Backspace to disable the keyboard shortcut.");

    auto* infoBottom = new QLabel(msg);
    infoBottom->setMargin(10);
    infoBottom->setAlignment(Qt::AlignCenter);
    layout_->addWidget(infoBottom);
}

const QKeySequence& SetShortcutDialog::shortcut()
{
    return ks_;
}

void SetShortcutDialog::keyPressEvent(QKeyEvent* ke)
{
    if (ke->modifiers() & Qt::ShiftModifier) {
        modifier_ += "Shift+";
    }
    if (ke->modifiers() & Qt::ControlModifier) {
        modifier_ += "Ctrl+";
    }
    if (ke->modifiers() & Qt::AltModifier) {
        modifier_ += "Alt+";
    }
    if (ke->modifiers() & Qt::MetaModifier) {
        modifier_ += "Meta+";
    }

    QString key = QKeySequence(ke->key()).toString();
    ks_ = QKeySequence(modifier_ + key);
}

void SetShortcutDialog::keyReleaseEvent(QKeyEvent* event)
{
    if (ks_ == QKeySequence(Qt::Key_Escape)) {
        reject();
    }
    accept();
}
