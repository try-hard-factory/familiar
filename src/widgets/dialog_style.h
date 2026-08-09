#pragma once

#include <QColor>
#include <QMessageBox>
#include <QPixmap>
#include <QString>

class QPushButton;
class QWidget;

// Shared building blocks for this app's custom-chrome dialogs
// (CustomMessageBox, SaveAllDialog, and whatever QFileDialog/
// QColorDialog replacements come next - roadmap step 24, Max:
// "кастомные виджеты для всего подряд ... полностью свой дизайн").
// Keeps every custom dialog visually consistent (same rounded panel,
// same pill-button look, same severity icon set) without copy-pasting
// the same QPainter/QSS code into each one.
namespace familiar::dialog_style {

// Rounded, OPAQUE preset-colored panel QSS for a top-level, frameless,
// WA_StyledBackground dialog - `className` must be the dialog's own
// metaObject()->className() (a QSS type selector). Deliberately NOT
// meant for a WA_TranslucentBackground widget: combining
// WA_TranslucentBackground with an auto-painted QSS background on a
// genuinely top-level widget left the whole panel see-through instead
// of just rounding the corners (confirmed via screenshot, see
// CustomMessageBox's own history) - opaque + border-radius alone reads
// as very slightly squared corners on some window managers, but
// actually paints.
QString panelStyleSheet(const char* className,
                        const QColor& background,
                        const QColor& border,
                        const QColor& text);

// Accent-filled pill button - the single "primary" action in a dialog
// (rightmost/most prominent by this app's convention).
void stylePrimaryButton(QPushButton* button, const QColor& accent);

// Outline pill button - every other action.
void styleSecondaryButton(QPushButton* button,
                          const QColor& text,
                          const QColor& border);

// Same drawn-icon approach as every other icon in this app
// (group_toolbar.cpp, gif_playback_toolbar.cpp) - no external asset,
// always matches the current DPI exactly. Warning is a triangle
// (matches common OS iconography - "caution" reads as a triangle),
// everything else a filled circle, all with a bold glyph on top.
// Severity colors (amber/red/blue) are fixed regardless of the active
// preset - they read as universally recognizable regardless of theme.
// `accent` is only used for QMessageBox::Question, which has no real
// "severity" of its own.
QPixmap severityIcon(QMessageBox::Icon icon, const QColor& accent, qreal dpr);

// Clips `widget`'s actual OS-level window shape to a rounded rect
// matching its current size - QSS border-radius alone only PAINTS
// within the rounded outline, it doesn't reshape the window itself, so
// an opaque (non-translucent - see panelStyleSheet()'s comment) window
// still has real, visible square corners of raw window canvas poking
// out past the rounded panel (confirmed by Max via screenshot: solid
// black triangles under the rounded corners). setMask() works without
// any compositor/alpha-channel support, unlike WA_TranslucentBackground
// - call from the dialog's own resizeEvent() override so the mask
// tracks its actual size (word-wrap/content can change it after
// construction).
void applyRoundedMask(QWidget* widget, int radiusPx);

} // namespace familiar::dialog_style
