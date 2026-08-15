#pragma once

#include <QColor>
#include <QComboBox>
#include <QStyledItemDelegate>

class QPaintEvent;
class QEnterEvent;

// Row painter for FlatComboBox's popup (below) - QSS styling of
// QAbstractItemView/::item inside a QComboBox's native popup proved
// unreliable in this Qt/style combo: background/padding/border-radius
// on ::item took, but item TEXT color kept coming out a stray native
// link-blue no matter which selector it was pinned on (::item, the view
// itself, ...) - Max, by screenshot, more than once. A delegate paints
// every row directly with QPainter, same reasoning as FlatCheckBox/
// FlatSpinBox, just applied to list rows instead of a single control.
class FlatComboItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    FlatComboItemDelegate(const QColor& text,
                          const QColor& highlightBackground,
                          QObject* parent = nullptr);

    void paint(QPainter* painter,
              const QStyleOptionViewItem& option,
              const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                  const QModelIndex& index) const override;

private:
    QColor text_;
    QColor highlightBackground_;
};

// Hand-painted QComboBox: rounded flat-color background, custom-drawn
// dropdown arrow instead of the OS's own - same reasoning/precedent as
// FlatSpinBox (widgets/flat_spinbox.h). Row content is
// FlatComboItemDelegate above. Clicking anywhere on the closed box to
// open it is native QComboBox::mousePressEvent behavior, untouched;
// only the closed box's own chrome needed hand-painting.
//
// The popup's actual top-level window is QComboBoxPrivateContainer, a
// Qt-private QFrame wrapping view() - separate from view() itself, and
// with no public accessor, so styling "QComboBox QAbstractItemView" via
// QSS (settings_style.cpp) only ever reached the view, never this outer
// container. Its own native frame/background was showing through as a
// squared-off rectangle around/behind the rounded view (Max, by
// screenshot, more than once). showPopup() below reaches it anyway via
// view()->window() - it only exists once the popup is actually shown,
// which is exactly when this fires.
class FlatComboBox : public QComboBox
{
    Q_OBJECT
public:
    FlatComboBox(const QColor& background,
                const QColor& text,
                const QColor& hoverBackground,
                const QColor& itemHighlight,
                QWidget* parent = nullptr);

    void showPopup() override;
protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QColor background_;
    QColor text_;
    QColor hoverBackground_;
};
