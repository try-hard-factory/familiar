#include "text_edit_toolbar.h"

#include <core/settingshandler.h>
#include <moveitem.h>

#include "log/log.h"
using namespace familiar::log;

#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>

namespace {

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

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(4);

    auto makeButton = [this, lay](const QString& glyph,
                                  const QString& tooltip,
                                  bool checkable) {
        auto* b = new QToolButton(this);
        b->setText(glyph);
        b->setToolTip(tooltip);
        b->setCheckable(checkable);
        b->setAutoRaise(true);
        // Don't steal focus from the text item being edited - its cursor/
        // selection is what the buttons operate on.
        b->setFocusPolicy(Qt::NoFocus);
        lay->addWidget(b);
        return b;
    };

    textColorBtn_ = makeButton(QStringLiteral("A"), tr("Text color"), false);
    highlightColorBtn_
        = makeButton(QStringLiteral("H"), tr("Text highlight color"), false);
    fillColorBtn_
        = makeButton(QStringLiteral("BG"), tr("Note fill color"), false);
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

    sizeBox_ = new QComboBox(this);
    sizeBox_->setEditable(true);
    sizeBox_->setInsertPolicy(QComboBox::NoInsert);
    for (int s : {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 64, 72})
        sizeBox_->addItem(QString::number(s));
    sizeBox_->setToolTip(tr("Font size"));
    sizeBox_->setFixedWidth(sizeBox_->fontMetrics().horizontalAdvance("000")
                            + 30);
    lay->addWidget(sizeBox_);

    fontBox_ = new QFontComboBox(this);
    fontBox_->setToolTip(tr("Font"));
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
        if (color.isValid())
            item_->set_fill_color(color);
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
}

void TextEditToolbar::restyleFromPreset()
{
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
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
                       "  border-radius: 6px;"
                       "}"
                       "QToolButton {"
                       "  background: transparent;"
                       "  color: %3;"
                       "  border: none;"
                       "  border-radius: 4px;"
                       "  padding: 2px 6px;"
                       "}"
                       "QToolButton:hover { background-color: %4; }"
                       "QToolButton:pressed { background-color: %5; }"
                       "QToolButton:checked { background-color: %5; }"
                       "QComboBox, QFontComboBox {"
                       "  background-color: %1;"
                       "  color: %3;"
                       "  border: 1px solid %2;"
                       "  border-radius: 4px;"
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
            .arg(rgba(background, 235),
                 border.name(),
                 text.name(),
                 rgba(selection, 90),
                 rgba(selection, 170),
                 background.name(),
                 selection.name()));
}
