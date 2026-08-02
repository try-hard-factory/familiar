#ifndef TEXT_EDIT_TOOLBAR_H
#define TEXT_EDIT_TOOLBAR_H

/**
 *  @file   text_edit_toolbar.h
 *  \~russian @brief  Плавающая панель форматирования текстового айтема
 *                    (PureRef-стиль): цвет текста/заливки, B/I/U, размер,
 *                    шрифт. Показывается CanvasView'ом над айтемом, пока
 *                    тот в режиме редактирования.
 */

#include <QTextCharFormat>
#include <QWidget>

class TextItem;
class QToolButton;
class QComboBox;
class QFontComboBox;
class QTimer;

class TextEditToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit TextEditToolbar(QWidget* parent = nullptr);

    // nullptr detaches (the caller hides the widget). While attached the
    // toolbar applies formats to the item's text cursor: to the current
    // selection if there is one, to the whole note otherwise.
    void attach(TextItem* item);
    TextItem* item() const { return item_; }

    // Re-derive the QSS from the current color preset (called on attach
    // and whenever settings change).
    void restyleFromPreset();

private:
    void applyCharFormat(const QTextCharFormat& format);
    // Toggles the current block(s) in/out of a list of the given style:
    // off if already that style, switched in place if a different style,
    // otherwise a new list is created from the selection. See
    // moveitem.h/QTextListFormat - a block-level format, not a
    // QTextCharFormat, so it doesn't go through applyCharFormat().
    void toggleListStyle(int style);
    // Small popup (URL field + browse-for-local-file + apply) anchored
    // under linkBtn_, PureRef-style - see the .cpp for the widget itself.
    void showLinkPopup();
    // Applies (or, if href is empty, does nothing - the popup's Apply
    // button already guards against that) an anchor format to the
    // current selection.
    void applyLink(const QString& href);
    // Reflect the char format under the item's cursor in the controls.
    // QGraphicsTextItem has no cursorPositionChanged signal, so this is
    // polled by syncTimer_ while the toolbar is visible.
    void syncFromCursor();
    // Redraws textColorBtn_/highlightColorBtn_/fillColorBtn_'s icons from
    // whatever color each currently represents - called from
    // syncFromCursor() (so the swatch tracks the cursor like B/I/U do)
    // and right after a color picker closes (so a pick is reflected
    // immediately, not only at the next poll).
    void updateColorButtonIcons();

    TextItem* item_ = nullptr;
    QToolButton* textColorBtn_ = nullptr;
    // Text highlight - QTextCharFormat::setBackground() on the selected
    // run, distinct from fillColorBtn_ below (the whole note's backdrop).
    QToolButton* highlightColorBtn_ = nullptr;
    QToolButton* fillColorBtn_ = nullptr;
    QToolButton* boldBtn_ = nullptr;
    QToolButton* italicBtn_ = nullptr;
    QToolButton* underlineBtn_ = nullptr;
    QToolButton* linkBtn_ = nullptr;
    QToolButton* bulletListBtn_ = nullptr;
    QToolButton* numberedListBtn_ = nullptr;
    // Resets a manually-resized field (edit-mode square handles,
    // moveitem.h TextItem::resize_field()) back to its natural
    // content-driven size.
    QToolButton* autosizeBtn_ = nullptr;
    QComboBox* sizeBox_ = nullptr;
    QFontComboBox* fontBox_ = nullptr;
    QTimer* syncTimer_ = nullptr;
    // Glyph color used when (re)drawing the three color-button icons;
    // refreshed by restyleFromPreset() alongside the rest of the QSS.
    QColor iconGlyphColor_;
};

#endif // TEXT_EDIT_TOOLBAR_H
