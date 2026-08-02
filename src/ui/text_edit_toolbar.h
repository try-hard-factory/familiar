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
    // Reflect the char format under the item's cursor in the controls.
    // QGraphicsTextItem has no cursorPositionChanged signal, so this is
    // polled by syncTimer_ while the toolbar is visible.
    void syncFromCursor();

    TextItem* item_ = nullptr;
    QToolButton* textColorBtn_ = nullptr;
    // Text highlight - QTextCharFormat::setBackground() on the selected
    // run, distinct from fillColorBtn_ below (the whole note's backdrop).
    QToolButton* highlightColorBtn_ = nullptr;
    QToolButton* fillColorBtn_ = nullptr;
    QToolButton* boldBtn_ = nullptr;
    QToolButton* italicBtn_ = nullptr;
    QToolButton* underlineBtn_ = nullptr;
    QComboBox* sizeBox_ = nullptr;
    QFontComboBox* fontBox_ = nullptr;
    QTimer* syncTimer_ = nullptr;
};

#endif // TEXT_EDIT_TOOLBAR_H
