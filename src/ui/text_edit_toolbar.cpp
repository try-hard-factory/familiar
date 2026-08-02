#include "text_edit_toolbar.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include "log/log.h"
using namespace familiar::log;

#include <QColorDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QShortcut>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include <algorithm>

namespace {

// Square, uniform for every icon-only button on the bar - the plain-text
// glyphs used to size themselves off the text ("BG" wider than "B"),
// which looked ragged sitting side by side.
constexpr int kButtonSize = 30;
constexpr int kIconSize = 20;

// A/H/BG's icon: the letter plus a small rounded color-swatch bar along
// the bottom, so the button shows what color it currently represents
// instead of being a plain, identical-looking letter every time.
QIcon makeColorGlyphIcon(const QString& glyph,
                         const QColor& swatch,
                         const QColor& glyphColor,
                         qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QFont f = p.font();
    f.setPointSizeF(f.pointSizeF() * 1.05);
    f.setBold(true);
    p.setFont(f);
    p.setPen(glyphColor);
    p.drawText(QRect(0, 0, kIconSize, kIconSize - 5),
              Qt::AlignCenter,
              glyph);

    // An unset highlight (background().color() invalid, alpha 0) still
    // gets a visible neutral bar - an invisible bar would look identical
    // to a plain letter and defeat the point of the swatch.
    QColor bar = swatch.isValid() ? swatch : QColor(128, 128, 128);
    bar.setAlpha(qMax(bar.alpha(), 60));
    p.setPen(Qt::NoPen);
    p.setBrush(bar);
    p.drawRoundedRect(QRectF(3, kIconSize - 4, kIconSize - 6, 3), 1.5, 1.5);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// Bullet/numbered list icon: a few short rows, each a marker (dot or
// digit) followed by a stand-in text bar - reads as "list" at a glance
// without needing an external asset, same drawn-icon approach as
// makeColorGlyphIcon() above.
QIcon makeListIcon(bool numbered, const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    constexpr int rows = 3;
    const qreal rowH = kIconSize / qreal(rows);
    const qreal markerW = numbered ? 8.0 : 4.0;

    QFont f = p.font();
    f.setPointSizeF(rowH * 0.62);
    f.setBold(true);
    p.setFont(f);

    for (int i = 0; i < rows; ++i) {
        const qreal top = rowH * i;
        const qreal cy = top + rowH / 2.0;

        p.setPen(Qt::NoPen);
        p.setBrush(glyphColor);
        if (numbered) {
            p.setPen(glyphColor);
            p.drawText(QRectF(0, top, markerW, rowH),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      QString::number(i + 1));
            p.setPen(Qt::NoPen);
        } else {
            p.drawEllipse(QPointF(markerW / 2.0, cy), 1.6, 1.6);
        }
        p.drawRoundedRect(
            QRectF(markerW + 2, cy - 1, kIconSize - markerW - 4, 2), 1, 1);
    }

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

QFrame* makeSeparator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFixedWidth(1);
    return line;
}

// Two overlapping rounded-rect "links" on the diagonal - drawn outlined
// rather than filled (unlike the other icons here), since a solid chain
// link reads as a blob at this size; the outline is what actually makes
// the shape recognizable.
QIcon makeLinkIcon(const QColor& glyphColor, qreal dpr)
{
    QPixmap pm(QSize(kIconSize, kIconSize) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(glyphColor);
    pen.setWidthF(2.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    p.translate(kIconSize / 2.0, kIconSize / 2.0);
    p.rotate(-40);
    p.drawRoundedRect(QRectF(-7, -3, 7, 6), 3, 3);
    p.drawRoundedRect(QRectF(0, -3, 7, 6), 3, 3);

    p.end();
    QIcon icon;
    icon.addPixmap(pm);
    return icon;
}

// Not QColorDialog::getColor(): that static convenience builds/execs/
// destroys the dialog internally, with no chance to apply the fix below
// before it's shown - same reasoning as showMessageBox() in
// widgets/dialogs.h. DontUseNativeDialog because the native one hangs on
// this Qt build (see widgets/dialogs.h); WA_TranslucentBackground(false)
// + an explicit stylesheet because MainWindow's translucent/frameless
// stylesheet ("background: transparent", no selector) cascades into any
// child top-level widget without its own stylesheet, painting it solid
// black otherwise.
QColor pickColor(QWidget* parent,
                 const QColor& initial,
                 const QString& title,
                 bool withAlpha)
{
    // ShowAlphaChannel must be set BEFORE the color (not passed to the
    // constructor together with `initial`, and not set afterwards
    // either): QColorDialog(initial, parent) applies `initial` during
    // construction, before this option would take effect, and enabling
    // the option afterwards resets the stored alpha to 0 - a known Qt
    // quirk. Constructing without a color, enabling the option, then
    // calling setCurrentColor() is the order that actually keeps alpha.
    QColorDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setOption(QColorDialog::DontUseNativeDialog);
    if (withAlpha)
        dialog.setOption(QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor(initial);
    dialog.setAttribute(Qt::WA_TranslucentBackground, false);
    dialog.setStyleSheet("* { background-color: palette(window); color: "
                         "palette(window-text); }");
    // TEMPORARY debug logging (roadmap step 9 fill-color investigation) -
    // remove once the BG/H no-op bug is confirmed fixed.
    const int result = dialog.exec();
    FLOG_DEBUG(Ch::UI,
              "pickColor(\"{}\"): dialog.exec() = {}",
              title.toStdString(),
              result == QDialog::Accepted ? "Accepted" : "Rejected/other");
    if (result != QDialog::Accepted)
        return QColor();
    const QColor picked = dialog.currentColor();
    FLOG_DEBUG(Ch::UI,
              "pickColor(\"{}\"): currentColor() = {},{},{},{}",
              title.toStdString(),
              picked.red(),
              picked.green(),
              picked.blue(),
              picked.alpha());
    return picked;
}

} // namespace

TextEditToolbar::TextEditToolbar(QWidget* parent)
    : QWidget(parent)
{
    // A plain QWidget with a stylesheet background needs this to
    // actually paint it (otherwise it stays transparent over the canvas).
    setAttribute(Qt::WA_StyledBackground);

    // Floating-card feel, separating the bar from the canvas underneath
    // it more clearly than the border alone.
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(18);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(shadow);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(8, 5, 8, 5);
    lay->setSpacing(2);

    auto makeButton = [this, lay](const QString& glyph,
                                  const QString& tooltip,
                                  bool checkable) {
        auto* b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tooltip);
        b->setCheckable(checkable);
        b->setAutoRaise(true);
        b->setFixedSize(kButtonSize, kButtonSize);
        // Don't steal focus from the text item being edited - its cursor/
        // selection is what the buttons operate on.
        b->setFocusPolicy(Qt::NoFocus);
        lay->addWidget(b);
        return b;
    };

    // Icons (not setText()) are drawn lazily by updateColorButtonIcons()
    // once restyleFromPreset() has a glyph color to paint with - empty
    // for now, just reserving the buttons/tooltips/layout slot.
    textColorBtn_ = makeButton(QString(), tr("Text color"), false);
    highlightColorBtn_
        = makeButton(QString(), tr("Text highlight color"), false);
    fillColorBtn_ = makeButton(QString(), tr("Note fill color"), false);
    for (QToolButton* b : {textColorBtn_, highlightColorBtn_, fillColorBtn_})
        b->setIconSize(QSize(kIconSize, kIconSize));

    lay->addWidget(makeSeparator(this));

    boldBtn_ = makeButton(QStringLiteral("B"), tr("Bold"), true);
    italicBtn_ = makeButton(QStringLiteral("I"), tr("Italic"), true);
    underlineBtn_ = makeButton(QStringLiteral("U"), tr("Underline"), true);
    {
        // Make the glyphs self-describing.
        QFont f = boldBtn_->font();
        f.setBold(true);
        boldBtn_->setFont(f);
        f = italicBtn_->font();
        f.setItalic(true);
        italicBtn_->setFont(f);
        f = underlineBtn_->font();
        f.setUnderline(true);
        underlineBtn_->setFont(f);
    }

    linkBtn_ = makeButton(QString(), tr("Insert link"), false);
    linkBtn_->setIconSize(QSize(kIconSize, kIconSize));

    lay->addWidget(makeSeparator(this));

    bulletListBtn_ = makeButton(QString(), tr("Bulleted list"), true);
    numberedListBtn_ = makeButton(QString(), tr("Numbered list"), true);
    for (QToolButton* b : {bulletListBtn_, numberedListBtn_})
        b->setIconSize(QSize(kIconSize, kIconSize));

    lay->addWidget(makeSeparator(this));

    sizeBox_ = new QComboBox(this);
    sizeBox_->setEditable(true);
    sizeBox_->setInsertPolicy(QComboBox::NoInsert);
    for (int s : {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 64, 72})
        sizeBox_->addItem(QString::number(s));
    sizeBox_->setToolTip(tr("Font size"));
    sizeBox_->setFixedWidth(sizeBox_->fontMetrics().horizontalAdvance("000")
                            + 30);
    sizeBox_->setFixedHeight(kButtonSize);
    lay->addWidget(sizeBox_);

    fontBox_ = new QFontComboBox(this);
    fontBox_->setToolTip(tr("Font"));
    fontBox_->setFixedHeight(kButtonSize);
    lay->addWidget(fontBox_);

    connect(textColorBtn_, &QToolButton::clicked, this, [this] {
        if (!item_)
            return;
        const QColor initial
            = item_->textCursor().charFormat().foreground().color();
        const QColor color = pickColor(this,
                                       initial,
                                       tr("Text color"),
                                       /*withAlpha=*/false);
        if (!color.isValid())
            return;
        QTextCharFormat format;
        format.setForeground(color);
        applyCharFormat(format);
        updateColorButtonIcons();
    });

    connect(highlightColorBtn_, &QToolButton::clicked, this, [this] {
        if (!item_)
            return;
        QColor initial = item_->textCursor().charFormat().background().color();
        if (!initial.isValid())
            initial = Qt::yellow; // typical highlighter default
        // Default the alpha slider to fully opaque - a transparent
        // starting point (e.g. an unset highlight, alpha 0) makes the
        // newly picked hue invisible until the user separately remembers
        // to also raise the alpha slider.
        initial.setAlpha(255);
        const QColor color = pickColor(this,
                                       initial,
                                       tr("Text highlight color"),
                                       /*withAlpha=*/true);
        if (!color.isValid())
            return;
        QTextCharFormat format;
        format.setBackground(color);
        applyCharFormat(format);
        updateColorButtonIcons();
    });

    connect(fillColorBtn_, &QToolButton::clicked, this, [this] {
        if (!item_) {
            FLOG_DEBUG(Ch::UI, "fillColorBtn_ clicked but item_ is null");
            return;
        }
        // Same reasoning as highlightColorBtn_ above: default the alpha
        // slider to fully opaque rather than whatever low/zero alpha the
        // note's current fill happens to have.
        QColor initial = item_->fill_color();
        initial.setAlpha(255);
        const QColor color = pickColor(this,
                                       initial,
                                       tr("Note fill color"),
                                       /*withAlpha=*/true);
        FLOG_DEBUG(Ch::UI,
                  "fillColorBtn_: picked color valid={} -> calling set_fill_color",
                  color.isValid());
        if (color.isValid()) {
            item_->set_fill_color(color);
            updateColorButtonIcons();
        }
    });

    connect(boldBtn_, &QToolButton::toggled, this, [this](bool checked) {
        QTextCharFormat format;
        format.setFontWeight(checked ? QFont::Bold : QFont::Normal);
        applyCharFormat(format);
    });
    connect(italicBtn_, &QToolButton::toggled, this, [this](bool checked) {
        QTextCharFormat format;
        format.setFontItalic(checked);
        applyCharFormat(format);
    });
    connect(underlineBtn_, &QToolButton::toggled, this, [this](bool checked) {
        QTextCharFormat format;
        format.setFontUnderline(checked);
        applyCharFormat(format);
    });

    connect(linkBtn_, &QToolButton::clicked, this, &TextEditToolbar::showLinkPopup);

    // clicked, not toggled: whether the block ends up listed (and which
    // style) depends on the CURRENT document state, not a simple bool
    // flip - toggleListStyle() figures that out itself, then
    // syncFromCursor() corrects whatever checked state Qt's own
    // checkable-button click already applied to reflect the real result.
    connect(bulletListBtn_, &QToolButton::clicked, this, [this] {
        toggleListStyle(QTextListFormat::ListDisc);
        syncFromCursor();
    });
    connect(numberedListBtn_, &QToolButton::clicked, this, [this] {
        toggleListStyle(QTextListFormat::ListDecimal);
        syncFromCursor();
    });

    auto applySize = [this](const QString& text) {
        bool ok = false;
        const qreal size = text.toDouble(&ok);
        if (!ok || size <= 0)
            return;
        QTextCharFormat format;
        format.setFontPointSize(size);
        applyCharFormat(format);
    };
    connect(sizeBox_, &QComboBox::textActivated, this, applySize);
    connect(sizeBox_->lineEdit(), &QLineEdit::returnPressed, this, [this, applySize] {
        applySize(sizeBox_->currentText());
    });

    connect(fontBox_,
            &QFontComboBox::currentFontChanged,
            this,
            [this](const QFont& font) {
                QTextCharFormat format;
                format.setFontFamilies(QStringList{font.family()});
                applyCharFormat(format);
            });

    // QGraphicsTextItem exposes no cursorPositionChanged - poll while
    // visible so B/I/U/size/font track the cursor through mixed
    // formatting. Cheap: a handful of format reads 4x a second.
    syncTimer_ = new QTimer(this);
    syncTimer_->setInterval(250);
    connect(syncTimer_, &QTimer::timeout, this, &TextEditToolbar::syncFromCursor);

    restyleFromPreset();
}

void TextEditToolbar::attach(TextItem* item)
{
    item_ = item;
    if (item_) {
        restyleFromPreset();
        syncFromCursor();
        adjustSize();
        syncTimer_->start();
    } else {
        syncTimer_->stop();
    }
}

void TextEditToolbar::applyCharFormat(const QTextCharFormat& format)
{
    if (!item_)
        return;
    QTextCursor cursor = item_->textCursor();
    // No selection -> the whole note. Predictable, and matches how a
    // one-line note is usually formatted; select a range for less.
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::Document);
    cursor.mergeCharFormat(format);
}

void TextEditToolbar::applyLink(const QString& href)
{
    if (!item_ || href.isEmpty())
        return;
    QTextCharFormat format;
    format.setAnchor(true);
    format.setAnchorHref(href);
    // QTextDocument doesn't style anchors on its own - underline + the
    // theme's accent color is what actually makes it read as a link.
    format.setFontUnderline(true);
    format.setForeground(
        SettingsHandler::getInstance()
            ->getCurrentColorPreset()[EPresetsColorIdx::kSelectionColor]);

    // Inserts href AS the visible link text (PureRef-style) rather than
    // formatting whatever's currently selected - a selection is replaced
    // (like typing normally would), not preserved-and-wrapped.
    QTextCursor cursor = item_->textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection())
        cursor.removeSelectedText();
    cursor.insertText(href, format);
    cursor.endEditBlock();
    // insertText() advanced this local cursor copy, but the item's own
    // "live" cursor (what you'd keep typing at) doesn't follow it
    // automatically - without this, the next keystroke would land back
    // wherever editing started instead of after the link just inserted.
    item_->setTextCursor(cursor);
}

void TextEditToolbar::showLinkPopup()
{
    if (!item_)
        return;

    // Qt::Tool, not Qt::Popup: a Popup auto-closes (and, with
    // WA_DeleteOnClose below, gets destroyed) the moment it loses
    // activation - which is exactly what happens the instant the
    // "browse" button opens its own QFileDialog beneath it. That would
    // free `popup`/`edit` while dialog.exec()'s nested event loop is
    // still pumping (deleteLater() is processed by whatever loop is
    // running), making the edit->setText() after exec() returns a
    // use-after-free. A Tool window has no such auto-close behavior, so
    // it survives a child dialog fine; dismissal is explicit instead
    // (Apply, Enter, or Escape below).
    auto* popup = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    // Same reasoning as every other top-level widget in this app
    // (widgets/dialogs.h, MainWindow::updateWindowControlsStyle_()):
    // without this it inherits MainWindow's translucent stylesheet and
    // paints solid black.
    popup->setAttribute(Qt::WA_TranslucentBackground, false);

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    popup->setStyleSheet(
        QStringLiteral("QWidget {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "}"
                       "QLineEdit {"
                       "  background-color: %1;"
                       "  border: 1px solid %3;"
                       "  border-radius: 4px;"
                       "  padding: 3px 6px;"
                       "}"
                       "QToolButton {"
                       "  border: none;"
                       "  border-radius: 4px;"
                       "  padding: 2px;"
                       "}"
                       "QToolButton:hover { background-color: %3; }")
            .arg(background.name(), text.name(), border.name()));

    auto* lay = new QHBoxLayout(popup);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    auto* browseBtn = new QToolButton(popup);
    browseBtn->setText(QStringLiteral("\U0001F4C1")); // folder
    browseBtn->setToolTip(tr("Browse for local file"));
    browseBtn->setAutoRaise(true);
    lay->addWidget(browseBtn);

    auto* edit = new QLineEdit(popup);
    edit->setPlaceholderText(tr("URL or file path"));
    edit->setMinimumWidth(220);
    // Editing an existing link (cursor already inside one) starts from
    // its current target instead of empty.
    const QString existingHref = item_->textCursor().charFormat().anchorHref();
    if (!existingHref.isEmpty())
        edit->setText(existingHref);
    lay->addWidget(edit);

    auto* applyBtn = new QToolButton(popup);
    applyBtn->setText(QStringLiteral("✓")); // checkmark
    applyBtn->setToolTip(tr("Apply"));
    applyBtn->setAutoRaise(true);
    lay->addWidget(applyBtn);

    connect(browseBtn, &QToolButton::clicked, popup, [popup, edit] {
        // Stack-allocated, exec()'d inline - same DontUseNativeDialog +
        // translucency fix as every other file dialog in this project
        // (see file_actions.cpp/canvasview.cpp), the native one hangs on
        // this Qt build.
        QFileDialog dialog(popup);
        dialog.setWindowTitle(tr("Select a file to link to"));
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setAttribute(Qt::WA_TranslucentBackground, false);
        dialog.setStyleSheet("* { background-color: palette(window); color: "
                             "palette(window-text); }");
        if (dialog.exec() == QDialog::Accepted
            && !dialog.selectedFiles().isEmpty())
            edit->setText(
                QUrl::fromLocalFile(dialog.selectedFiles().first()).toString());
    });

    connect(applyBtn, &QToolButton::clicked, this, [this, edit, popup] {
        applyLink(edit->text().trimmed());
        popup->close();
    });
    connect(edit, &QLineEdit::returnPressed, this, [this, edit, popup] {
        applyLink(edit->text().trimmed());
        popup->close();
    });

    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), popup);
    connect(escShortcut, &QShortcut::activated, popup, &QWidget::close);

    popup->move(linkBtn_->mapToGlobal(linkBtn_->rect().bottomLeft()));
    popup->show();
    edit->setFocus();
}

void TextEditToolbar::toggleListStyle(int style)
{
    if (!item_)
        return;
    const auto wanted = static_cast<QTextListFormat::Style>(style);
    QTextCursor cursor = item_->textCursor();

    // QTextList::remove() (qtextlist.cpp) deliberately folds the list's
    // own indent level into the block's *own* QTextBlockFormat::indent()
    // when detaching - by design, so plain "remove list" in a word
    // processor doesn't also snap the paragraph back to the margin
    // (that's a separate "decrease indent" action there). We have no
    // such action, so from our toolbar "remove list" should mean fully
    // plain again - zero the block's own indent right back out after
    // detaching.
    auto detachFromList = [](QTextList* list, const QTextBlock& block) {
        list->remove(block);
        QTextCursor c(block);
        QTextBlockFormat bf = c.blockFormat();
        if (bf.indent() != 0) {
            bf.setIndent(0);
            c.setBlockFormat(bf);
        }
    };

    if (!cursor.hasSelection()) {
        cursor.beginEditBlock();
        if (QTextList* list = cursor.currentList()) {
            if (list->format().style() == wanted) {
                // Already this style - toggle off by detaching the block.
                detachFromList(list, cursor.block());
            } else {
                // Different style - switch the existing list in place
                // rather than nesting a new one inside it.
                QTextListFormat fmt = list->format();
                fmt.setStyle(wanted);
                list->setFormat(fmt);
            }
        } else {
            QTextListFormat fmt;
            fmt.setStyle(wanted);
            cursor.createList(fmt);
        }
        cursor.endEditBlock();
        return;
    }

    // Multi-block selection: QTextCursor::currentList() only ever looks
    // at the single block the cursor's OWN position sits in, never the
    // whole selection - toggling off used to silently stop after the
    // first line because of that. Collect every block the selection
    // actually spans and act on all of them instead.
    //
    // Walk by block IDENTITY (compare to findBlock(selectionEnd()), stop
    // once reached) rather than by a position+length arithmetic check -
    // the arithmetic version was off by one for the last block in the
    // selection (QTextBlock::length() counts the block's own trailing
    // separator, which the *last* block in the document doesn't
    // necessarily have one of), so a selection running to the end of the
    // text silently dropped its last line.
    QTextDocument* doc = item_->document();
    const QTextBlock endBlock = doc->findBlock(cursor.selectionEnd());
    QList<QTextBlock> blocks;
    for (QTextBlock block = doc->findBlock(cursor.selectionStart());
        block.isValid();
        block = block.next()) {
        blocks.append(block);
        if (block == endBlock)
            break;
    }

    const bool anyListed = std::any_of(blocks.begin(), blocks.end(), [](const QTextBlock& b) {
        return QTextCursor(b).currentList() != nullptr;
    });
    // Only a uniform "every line already has this exact style" selection
    // toggles off; anything else (nothing listed, or a mixed selection)
    // is treated as "make it this style".
    const bool turningOff
        = anyListed
          && std::all_of(blocks.begin(), blocks.end(), [wanted](const QTextBlock& b) {
                 QTextList* l = QTextCursor(b).currentList();
                 return l && l->format().style() == wanted;
             });

    cursor.beginEditBlock();
    if (!anyListed) {
        // Clean slate - one call creates a single shared list spanning
        // every block in the selection, instead of a separate QTextList
        // per line (which would restart numbering at 1 on each line).
        QTextListFormat fmt;
        fmt.setStyle(wanted);
        cursor.createList(fmt);
    } else {
        for (const QTextBlock& block : blocks) {
            QTextCursor blockCursor(block);
            QTextList* list = blockCursor.currentList();
            if (turningOff) {
                if (list && list->format().style() == wanted)
                    detachFromList(list, block);
            } else if (list) {
                QTextListFormat fmt = list->format();
                fmt.setStyle(wanted);
                list->setFormat(fmt);
            } else {
                QTextListFormat fmt;
                fmt.setStyle(wanted);
                blockCursor.createList(fmt);
            }
        }
    }
    cursor.endEditBlock();
}

void TextEditToolbar::syncFromCursor()
{
    if (!item_)
        return;
    const QTextCharFormat format = item_->textCursor().charFormat();

    {
        QSignalBlocker b1(boldBtn_), b2(italicBtn_), b3(underlineBtn_);
        boldBtn_->setChecked(format.fontWeight() >= QFont::Bold);
        italicBtn_->setChecked(format.fontItalic());
        underlineBtn_->setChecked(format.fontUnderline());
    }
    {
        QSignalBlocker b1(bulletListBtn_), b2(numberedListBtn_);
        QTextList* list = item_->textCursor().currentList();
        const QTextListFormat::Style style
            = list ? list->format().style() : QTextListFormat::ListStyleUndefined;
        bulletListBtn_->setChecked(style == QTextListFormat::ListDisc);
        numberedListBtn_->setChecked(style == QTextListFormat::ListDecimal);
    }
    {
        QSignalBlocker b(sizeBox_);
        qreal size = format.fontPointSize();
        if (size <= 0)
            size = item_->font().pointSizeF(); // unset -> item default
        sizeBox_->setCurrentText(QString::number(size));
    }
    {
        QSignalBlocker b(fontBox_);
        fontBox_->setCurrentFont(format.font());
    }

    updateColorButtonIcons();
}

void TextEditToolbar::updateColorButtonIcons()
{
    if (!item_)
        return;
    const qreal dpr = devicePixelRatioF();
    const QTextCharFormat format = item_->textCursor().charFormat();

    QColor textC = format.foreground().color();
    if (!textC.isValid())
        textC = item_->defaultTextColor();
    textColorBtn_->setIcon(
        makeColorGlyphIcon(QStringLiteral("A"), textC, iconGlyphColor_, dpr));

    // format.background() comes back as an invalid QColor when no
    // highlight is set - makeColorGlyphIcon() already renders that as a
    // neutral gray bar, so no substitution needed here (unlike the color
    // picker's "initial" value, which needs a real starting hue/alpha).
    highlightColorBtn_->setIcon(makeColorGlyphIcon(QStringLiteral("H"),
                                                   format.background().color(),
                                                   iconGlyphColor_,
                                                   dpr));

    fillColorBtn_->setIcon(makeColorGlyphIcon(QStringLiteral("BG"),
                                              item_->fill_color(),
                                              iconGlyphColor_,
                                              dpr));
}

void TextEditToolbar::restyleFromPreset()
{
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
    iconGlyphColor_ = text;
    auto rgba = [](const QColor& c, int alpha) {
        return QStringLiteral("rgba(%1, %2, %3, %4)")
            .arg(c.red())
            .arg(c.green())
            .arg(c.blue())
            .arg(alpha);
    };

    // Same reasoning as MainWindow::updateWindowControlsStyle_(): over a
    // translucent window every state must be spelled out, and popups/
    // tooltips need fully opaque colors.
    setStyleSheet(
        QStringLiteral("TextEditToolbar {"
                       "  background-color: %1;"
                       "  border: 1px solid %2;"
                       "  border-radius: 8px;"
                       "}"
                       "QToolButton {"
                       "  background: transparent;"
                       "  color: %3;"
                       "  border: none;"
                       "  border-radius: 5px;"
                       "}"
                       "QToolButton:hover { background-color: %4; }"
                       "QToolButton:pressed { background-color: %5; }"
                       "QToolButton:checked { background-color: %5; }"
                       "QFrame {"
                       "  background-color: %2;"
                       "  border: none;"
                       "  margin: 4px 4px;"
                       "}"
                       "QComboBox, QFontComboBox {"
                       "  background-color: %1;"
                       "  color: %3;"
                       "  border: 1px solid %2;"
                       "  border-radius: 5px;"
                       "  padding: 1px 4px;"
                       "}"
                       "QComboBox QAbstractItemView {"
                       "  background-color: %6;"
                       "  color: %3;"
                       "  selection-background-color: %7;"
                       "}"
                       "QToolTip {"
                       "  background-color: %6;"
                       "  color: %3;"
                       "  border: 1px solid %2;"
                       "}")
            .arg(rgba(background, 245),
                 border.name(),
                 text.name(),
                 rgba(selection, 90),
                 rgba(selection, 170),
                 background.name(),
                 selection.name()));

    // Static glyphs (don't depend on item_/cursor state, just the theme
    // color), unlike the three color-swatch icons below.
    const qreal dpr = devicePixelRatioF();
    bulletListBtn_->setIcon(makeListIcon(false, iconGlyphColor_, dpr));
    numberedListBtn_->setIcon(makeListIcon(true, iconGlyphColor_, dpr));
    linkBtn_->setIcon(makeLinkIcon(iconGlyphColor_, dpr));

    updateColorButtonIcons();
}
