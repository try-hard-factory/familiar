#pragma once

#include <QColor>
#include <QString>

namespace familiar::settings_style {

// Fixed, PureRef-style white palette for the Settings window
// (ui/settings_window.cpp and everything it hosts) - deliberately NOT
// sourced from SettingsHandler::getCurrentColorPreset() like
// dialog_style.h's helpers. Max wants this one window to always render
// in the same scheme regardless of the user's chosen app-wide accent.
struct Palette
{
    QColor background;
    QColor text;
    QColor mutedText;
    QColor border;
    QColor hoverBg;
    QColor accent;
    QColor chipBackground; // shortcut chip box (see shortcutChipStyleSheet())
    QColor navIdleBg;      // sidebar button, unselected
    QColor navHoverBg;     // sidebar button, unselected + hovered
    QColor navSelectedBg;  // sidebar button, selected
    QColor popupBackground; // dropdown popup panel (FlatComboBox)
    QColor popupItemHover;  // dropdown row under the cursor (FlatComboItemDelegate)
};

const Palette& palette();

// Global "* {}" rule for the window root - forces background/text on
// every descendant regardless of OS theme, same technique as
// ChangeOpacityDialog's "* { background-color: ...; color: ...; }"
// (widgets/dialogs.h) but with this window's fixed colors instead of
// palette(window)/palette(window-text). Also carries the QScrollBar
// rules (thin rounded handle, no arrow buttons) since that's global to
// every scroll area in the window, not any one widget's own chrome.
QString rootStyleSheet();

// CategoryNavButton (ui/settings_window.cpp) chrome: every row is a
// filled gray box (not just the selected one) - navIdleBg normally,
// navHoverBg on hover, navSelectedBg when checked/selected. Matches
// PureRef's own Configuration sidebar (every category is its own
// button-shaped block, distinguished by shade). Text color itself is
// painted directly in CategoryNavButton::paintEvent() (not through this
// QSS - see that class for why).
QString sidebarButtonStyleSheet();

// Shortcut "chip" (widgets/controls/bindings_tree_widget.cpp): a gray
// rounded box around the shortcut text, matching PureRef's own
// Configuration window - hover darkens the box slightly.
QString shortcutChipStyleSheet();

// Small square +/- alias buttons next to a chip: white bg, thin border,
// rounded corners, centered glyph.
QString miniButtonStyleSheet();

// Outline pill button - unused by the window's own chrome any more (see
// filledButtonStyleSheet() below) but kept for other callers.
QString outlineButtonStyleSheet();

// Restore Defaults / Import / Export: same filled-gray-box treatment as
// the sidebar (sidebarButtonStyleSheet()) - navIdleBg normally,
// navHoverBg on hover, navSelectedBg while pressed. No border, unlike
// outlineButtonStyleSheet() - matches the sidebar buttons exactly rather
// than the earlier white-outline design.
QString filledButtonStyleSheet();

// QSlider chrome (ui/colors_widget.cpp's Master opacity slider): thin
// rounded groove/fill + a round navSelectedBg-gray handle (not accent -
// see this function's own .cpp comment) - same explicit
// ::groove/::sub-page/::handle sub-control rules ChangeOpacityDialog's
// own slider QSS uses (widgets/dialogs.h), not a bare "QSlider { color:
// ... }" rule (kills native rendering with no sub-control targeted, per
// this app's established QSS-subcontrol lesson).
QString sliderStyleSheet();

} // namespace familiar::settings_style
