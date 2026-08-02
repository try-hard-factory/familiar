#include "text_edit_toolbar.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include "log/log.h"
using namespace familiar::log;

#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>

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

QFrame* makeSeparator(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::VLine);
    line->setFixedWidth(1);
    return line;
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

    updateColorButtonIcons();
}
