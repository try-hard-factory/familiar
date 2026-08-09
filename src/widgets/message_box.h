#pragma once

#include <QDialog>
#include <QMessageBox>
#include <QPixmap>

class QLabel;
class QMouseEvent;
class QResizeEvent;

// Fully custom, frameless replacement for the native QMessageBox
// (roadmap step 24 - Max: "кастомные виджеты для всего подряд ...
// полностью свой дизайн/лейаут", explicitly starting with confirmations/
// errors). Reuses QMessageBox's own Icon/StandardButton(s) enums as the
// public interface purely because they're a good, familiar fit and
// every existing call site already speaks them via showMessageBox()
// below - nothing here actually constructs or shows a real QMessageBox.
//
// Frameless, opaque (NOT translucent - see the .cpp constructor's own
// comment for why), rounded panel drawn via QSS (same border-radius/
// preset-color convention as GroupToolbar's own settings popup,
// ui/group_toolbar.cpp) - the first genuinely custom-chrome QDialog in
// this codebase; every other one so far just falls back to the native
// OS window frame (see showMessageBox()'s own old comment, before this
// file existed). Draggable via the same
// windowHandle()->startSystemMove() MainWindow itself uses for its own
// frameless window.
//
// Each button's click calls done(int(thatButton)) directly (not
// accept()/reject()) - QDialog::exec()'s return value IS then already
// the QMessageBox::StandardButton the user picked, no separate
// "clicked()" accessor needed, and callers can keep treating exec()'s
// result exactly like the native QMessageBox always did.
class CustomMessageBox : public QDialog
{
    Q_OBJECT

public:
    CustomMessageBox(QMessageBox::Icon icon,
                     QWidget* parent,
                     const QString& title,
                     const QString& text,
                     QMessageBox::StandardButtons buttons,
                     QMessageBox::StandardButton defaultButton);

    // Overrides the severity glyph `icon` would otherwise draw - for
    // MainWindow::on_action_about()'s app-logo icon, the one case that
    // isn't really a severity at all (icon == QMessageBox::NoIcon).
    void setIconPixmap(const QPixmap& pixmap);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void reject() override;

private:
    QLabel* iconLabel_ = nullptr;
    QMessageBox::Icon icon_;
    // The button Escape/the corner close glyph resolves to - Cancel if
    // present, else No, else whatever the caller marked default, so
    // dismissing without picking anything still maps to a sensible
    // "didn't confirm" answer instead of always plain 0/Rejected
    // regardless of which buttons are actually on screen.
    QMessageBox::StandardButton escapeButton_ = QMessageBox::NoButton;
};

// Builds, execs and destroys a CustomMessageBox - drop-in for the
// static QMessageBox::warning/critical/information/question
// convenience overloads: those build/exec/destroy internally, with no
// chance to apply per-instance fixes first. Modal; returns the button
// clicked, or its escape button (see CustomMessageBox::reject()) if
// dismissed via Escape/the corner close glyph instead.
QMessageBox::StandardButton showMessageBox(
    QMessageBox::Icon icon,
    QWidget* parent,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButtons buttons = QMessageBox::Ok,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
