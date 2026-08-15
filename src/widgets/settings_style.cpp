#include "settings_style.h"

namespace familiar::settings_style {

const Palette& palette()
{
    static const Palette p{
        QColor(0xFF, 0xFF, 0xFF), // background
        QColor(0x22, 0x22, 0x22), // text
        QColor(0x8A, 0x8A, 0x8A), // mutedText
        QColor(0xD8, 0xD8, 0xD8), // border
        QColor(0xF1, 0xF1, 0xF1), // hoverBg
        QColor(0xD2, 0x60, 0x1C), // accent
        QColor(0xE3, 0xE3, 0xE3), // chipBackground
        QColor(0xEC, 0xEC, 0xEC), // navIdleBg
        QColor(0xDD, 0xDD, 0xDD), // navHoverBg
        QColor(0xBD, 0xBD, 0xBD), // navSelectedBg
    };
    return p;
}

QString rootStyleSheet()
{
    const Palette& p = palette();
    return QStringLiteral("* {"
                          "  background-color: %1;"
                          "  color: %2;"
                          "}"
                          // 1px hairline around the whole frameless
                          // window - square, not rounded: a setMask()
                          // rounded corner used to sit here, but its
                          // polygon approximation of the curve never
                          // quite lined up with this QSS border's own
                          // (analytic, smoother) rounding, leaving
                          // visible jagged pixels at the corners. Plain
                          // rectangle avoids that mismatch entirely.
                          "SettingsWindow {"
                          "  border: 1px solid %3;"
                          "}"
                          "QLineEdit {"
                          "  border: 1px solid %3;"
                          "  border-radius: 5px;"
                          "  padding: 4px 8px;"
                          "}"
                          "QLineEdit:focus { border-color: %4; }"
                          // Filled gray, not a white box with a border
                          // line - same chipBackground language as the
                          // shortcut chips/sidebar/buttons, not a
                          // separate "form field" look of its own.
                          // Deliberately NOT touching ::up-button/
                          // ::down-button/::drop-down/::*-arrow here -
                          // two rounds of trying (width/background,
                          // then explicit ::up-arrow/::down-arrow too)
                          // both left the buttons and their arrows
                          // invisible outside hover (Max, by
                          // screenshot). This QSS engine only seems to
                          // draw those sub-controls from an explicit
                          // "image:" (an external asset, against this
                          // app's own convention of drawing icons in
                          // code) - so the buttons are left fully
                          // native/unstyled instead. Trade-off: visible
                          // and functional beats pixel-matched but
                          // invisible; a real custom-painted spinbox
                          // (same idea as FlatCheckBox) is the option if
                          // the native chrome still looks wrong here.
                          // selection-background-color deliberately NOT
                          // set here - QAbstractSpinBox applies it to the
                          // internal QLineEdit's own palette directly
                          // regardless of whether paintEvent() is
                          // overridden (unlike background/border, which
                          // only paint through the default paintEvent
                          // FlatSpinBox skips calling) - it was
                          // overriding FlatSpinBox's own per-instance
                          // "selection looks like normal text" fix
                          // (widgets/flat_spinbox.cpp) with this orange
                          // accent instead (Max, by screenshot).
                          "QSpinBox {"
                          "  border: none;"
                          "  border-radius: 6px;"
                          "  padding: 4px 8px;"
                          "  background: %6;"
                          "}"
                          "QSpinBox:focus { background: %7; }"
                          "QComboBox {"
                          "  border: none;"
                          "  border-radius: 6px;"
                          "  padding: 4px 10px;"
                          "  background: %6;"
                          "}"
                          "QComboBox:hover { background: %7; }"
                          // Just padding + no border/background of its
                          // own - FlatComboBox::showPopup() styles the
                          // actual popup frame (QComboBoxPrivateContainer,
                          // a Qt-private class with no public accessor
                          // from here) directly in code instead, so this
                          // rule used to draw a second, redundant rounded
                          // border nested a few px inside that one.
                          // Row content (text color, hover/selected pill)
                          // isn't styled here at all any more -
                          // FlatComboItemDelegate (widgets/flat_combobox.h)
                          // paints every row directly instead, after
                          // ::item/the view's own "color:" both proved
                          // unreliable for text specifically (items kept
                          // coming out in a stray native link-blue no
                          // matter which selector it was pinned on -
                          // Max, by screenshot, more than once).
                          "QComboBox QAbstractItemView {"
                          "  border: none;"
                          "  background: transparent;"
                          "  padding: 4px;"
                          "  outline: none;"
                          "}"
                          "QGroupBox {"
                          "  border: 1px solid %3;"
                          "  border-radius: 6px;"
                          "  margin-top: 10px;"
                          "  font-weight: 600;"
                          "}"
                          "QGroupBox::title {"
                          "  subcontrol-origin: margin;"
                          "  left: 8px;"
                          "  padding: 0 4px;"
                          "}"
                          "QScrollArea, QScrollArea > QWidget > QWidget {"
                          "  border: none;"
                          "}"
                          "QScrollBar:vertical {"
                          "  background: transparent;"
                          "  width: 10px;"
                          "  margin: 2px;"
                          "}"
                          "QScrollBar::handle:vertical {"
                          "  background: %3;"
                          "  border-radius: 4px;"
                          "  min-height: 24px;"
                          "}"
                          "QScrollBar::handle:vertical:hover {"
                          "  background: %5;"
                          "}"
                          "QScrollBar::add-line:vertical,"
                          "QScrollBar::sub-line:vertical {"
                          "  height: 0;"
                          "  border: none;"
                          "  background: none;"
                          "}"
                          "QScrollBar::add-page:vertical,"
                          "QScrollBar::sub-page:vertical {"
                          "  background: none;"
                          "}")
        .arg(p.background.name(),
             p.text.name(),
             p.border.name(),
             p.accent.name(),
             p.navSelectedBg.name(),
             p.chipBackground.name(),
             p.chipBackground.darker(112).name());
}

QString sidebarButtonStyleSheet()
{
    const Palette& p = palette();
    return QStringLiteral("QPushButton#categoryButton {"
                          "  border: none;"
                          "  border-radius: 8px;"
                          "  background: %1;"
                          "  text-align: left;"
                          "}"
                          "QPushButton#categoryButton:checked {"
                          "  background: %2;"
                          "}"
                          "QPushButton#categoryButton:hover:!checked {"
                          "  background: %3;"
                          "}")
        .arg(p.navIdleBg.name(), p.navSelectedBg.name(), p.navHoverBg.name());
}

QString shortcutChipStyleSheet()
{
    const Palette& p = palette();
    return QStringLiteral("QPushButton {"
                          "  background-color: %1;"
                          "  border: 1px solid %2;"
                          "  border-radius: 4px;"
                          "  color: %3;"
                          "  padding: 2px 8px;"
                          "}"
                          "QPushButton:hover {"
                          "  background-color: %4;"
                          "}")
        .arg(p.chipBackground.name(),
             p.border.name(),
             p.text.name(),
             p.chipBackground.darker(112).name());
}

QString miniButtonStyleSheet()
{
    const Palette& p = palette();
    // Same box as shortcutChipStyleSheet() (chipBackground fill + a real
    // border, darkens on hover) rather than the sidebar's borderless
    // filled-gray look - it sits directly next to a chip, so it should
    // read as part of the same control, not a mismatched neighbor.
    return QStringLiteral("QToolButton {"
                          "  background-color: %1;"
                          "  border: 1px solid %2;"
                          "  border-radius: 4px;"
                          "  color: %3;"
                          "  min-width: 18px;"
                          "  max-width: 18px;"
                          "  min-height: 18px;"
                          "  max-height: 18px;"
                          "}"
                          "QToolButton:hover {"
                          "  background-color: %4;"
                          "}")
        .arg(p.chipBackground.name(),
             p.border.name(),
             p.text.name(),
             p.chipBackground.darker(112).name());
}

QString outlineButtonStyleSheet()
{
    const Palette& p = palette();
    // Explicit :focus/:default color rules, not just the base
    // QPushButton one - some styles (Fusion included) paint a focused
    // or "default" push button's text in the native accent/link color
    // regardless of an unqualified "color:" rule, which is how these
    // ended up blue instead of the fixed dark text color. "outline:
    // none" additionally drops the dotted native focus rectangle, which
    // has no equivalent in the PureRef reference either.
    return QStringLiteral("QPushButton {"
                          "  background-color: transparent;"
                          "  color: %1;"
                          "  border: 1px solid %2;"
                          "  border-radius: 6px;"
                          "  padding: 4px 14px;"
                          "  min-height: 22px;"
                          "  outline: none;"
                          "}"
                          "QPushButton:focus, QPushButton:default {"
                          "  color: %1;"
                          "}"
                          "QPushButton:hover {"
                          "  background-color: %3;"
                          "  border-color: %4;"
                          "}"
                          "QPushButton:pressed {"
                          "  background-color: %2;"
                          "}")
        .arg(p.text.name(), p.border.name(), p.hoverBg.name(), p.accent.name());
}

QString filledButtonStyleSheet()
{
    const Palette& p = palette();
    // Same :focus/:default/outline reasoning as outlineButtonStyleSheet()
    // above - without pinning these, some styles paint a focused/default
    // button's text in the native accent color instead of the fixed one.
    return QStringLiteral("QPushButton {"
                          "  background-color: %1;"
                          "  border: none;"
                          "  border-radius: 6px;"
                          "  color: %2;"
                          "  padding: 4px 14px;"
                          "  min-height: 22px;"
                          "  outline: none;"
                          "}"
                          "QPushButton:focus, QPushButton:default {"
                          "  color: %2;"
                          "}"
                          "QPushButton:hover {"
                          "  background-color: %3;"
                          "}"
                          "QPushButton:pressed {"
                          "  background-color: %4;"
                          "}")
        .arg(p.navIdleBg.name(),
             p.text.name(),
             p.navHoverBg.name(),
             p.navSelectedBg.name());
}

} // namespace familiar::settings_style
