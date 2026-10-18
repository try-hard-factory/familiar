#pragma once

#include <QColor>
#include <QMessageBox>
#include <QPixmap>
#include <QString>

class QLayout;
class QPushButton;
class QWidget;

namespace familiar::dialog_style {

// The rounded-panel radius every custom dialog uses, both for the QSS
// border-radius (panel_style_sheet()'s default below) and for the real
// setMask() shape (apply_rounded_mask()). One value so the painted
// corner and the clipped corner can't drift apart. Smaller widgets that
// are not dialogs - a combo box popup, say - pass their own.
constexpr int kdialogCornerRadius = 10;

// OPAQUE preset-colored panel QSS for a top-level, frameless,
// WA_StyledBackground dialog - `className` must be the dialog's own
// metaObject()->className() (a QSS type selector). Deliberately NOT
// meant for a WA_TranslucentBackground widget: combining
// WA_TranslucentBackground with an auto-painted QSS background on a
// genuinely top-level widget left the whole panel see-through instead
// of just rounding the corners (confirmed visually, see
// CustomMessageBox's own history) - opaque + border-radius alone reads
// as very slightly squared corners on some window managers, but
// actually paints.
// `radiusPx` defaults to the usual rounded-panel look every other
// custom dialog uses - pass 0 for a dialog that should read as square
// (CustomMessageBox does; still needs applyRoundedMask() skipped too at
// its own call site, or the window's actual clipped shape stays rounded
// regardless of what this QSS paints).
QString panel_style_sheet(const char* className,
                          const QColor& background,
                          const QColor& border,
                          const QColor& text,
                          int radiusPx = kdialogCornerRadius);

// Accent-filled pill button - the single "primary" action in a dialog
// (rightmost/most prominent by this app's convention).
void style_primary_button(QPushButton* button, const QColor& accent);

// Outline pill button - every other action.
void style_secondary_button(QPushButton* button,
                            const QColor& text,
                            const QColor& border);

// QSS for the small round "×" close glyph every custom dialog puts in
// its own top-right corner (there's no native title bar/X to provide
// one) - `objectName` must match setObjectName() on that QPushButton.
// Hover fill is the preset's accent color (same one the rubberband/
// selection outline uses) at the same alpha this app's other hover
// states already use (MainWindow::updateWindowControlsStyle_()) - was
// a flat white tint before, changed to match the accent instead.
QString close_button_style_sheet(const char* objectName,
                                 const QColor& text,
                                 const QColor& accent);

// The fixed amber/red/blue severity colors severityIcon() below paints
// with - exposed on its own (not just baked into severityIcon()) so
// other severity-flavored controls that aren't a QPixmap icon (e.g.
// DebugLogDialog's per-level filter checkboxes, widgets/dialogs.h) can
// reuse the exact same colors instead of re-guessing their own. `accent`
// is only used for QMessageBox::Question (and any other icon value),
// which has no real "severity" of its own.
QColor severity_color(QMessageBox::Icon icon, const QColor& accent);

// Same drawn-icon approach as every other icon in this app
// (group_toolbar.cpp, gif_playback_toolbar.cpp) - no external asset,
// always matches the current DPI exactly. Warning is a triangle
// (matches common OS iconography - "caution" reads as a triangle),
// everything else a filled circle, all with a bold glyph on top.
// Severity colors (amber/red/blue) are fixed regardless of the active
// preset - they read as universally recognizable regardless of theme.
// `accent` is only used for QMessageBox::Question, which has no real
// "severity" of its own.
QPixmap severity_icon(QMessageBox::Icon icon, const QColor& accent, qreal dpr);

// Clips `widget`'s actual OS-level window shape to a rounded rect
// matching its current size - QSS border-radius alone only PAINTS
// within the rounded outline, it doesn't reshape the window itself, so
// an opaque (non-translucent - see panelStyleSheet()'s comment) window
// still has real, visible square corners of raw window canvas poking
// out past the rounded panel (confirmed visually: solid
// black triangles under the rounded corners). setMask() works without
// any compositor/alpha-channel support, unlike WA_TranslucentBackground
// - call from the dialog's own resizeEvent() override so the mask
// tracks its actual size (word-wrap/content can change it after
// construction).
void apply_rounded_mask(QWidget* widget, int radiusPx);

// The drop shadow every frameless custom dialog puts behind itself. Was
// copy-pasted into 14 of them verbatim (and three toolbars with their own
// slightly tighter numbers), which meant changing how dialogs cast a
// shadow touched 17 files and 51 literals. Two variants because that is
// how many the UI actually has, not as a style knob: a dialog sits on top
// of the whole window and casts further, a floating toolbar hugs the
// canvas and casts less.
void apply_dialog_shadow(QWidget* widget);
void apply_toolbar_shadow(QWidget* widget);

// The "x" button in the top-right corner of every frameless custom
// dialog: fixed square, hand cursor, no focus stop. Was five identical
// lines in 13 places. `objectName` stays a parameter because each dialog
// passes its own to close_button_style_sheet() above - the QSS is a type
// selector on that name, so they must not collide. The caller still wires
// up clicked() itself: some close(), some reject().
QPushButton* make_close_button(QWidget* parent, const char* objectName);

// Inner padding for the standard custom dialog, on its top-level layout.
// Four dialogs (message_box, save_all, raw_import, restore_defaults) used
// to pass (20, 16, 16, 16) here - 20 on the left against 16 on the right,
// which pushed their content 4px off centre. Every other dialog in the
// project is left/right symmetric, so that was one typo copy-pasted four
// times rather than a deliberate look; they now share this.
//
// NOT for every dialog: about/help run wider (more text), color_picker
// and file_browser tighter (dense tool dialogs), hierarchy_panel is a
// panel rather than a dialog. Those keep their own values on purpose.
void apply_dialog_margins(QLayout* outer);

} // namespace familiar::dialog_style
