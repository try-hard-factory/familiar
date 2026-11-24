#include "command_palette.h"

#include "actions/actions.h"
#include "actions/menu_structure.h"
#include "core/settingshandler.h"
#include "widgets/dialog_style.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QChar>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QKeyCombination>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QShowEvent>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kdialogWidth = 760;
constexpr int kdialogHeight = 390;
constexpr int ksecondaryColumnWidth = 195;
constexpr int kshortcutColumnWidth = 155;
constexpr int kresultHeight = 28;
constexpr int krowSpacing = 5;
constexpr int kdimTextAlpha = 155;
constexpr int kselectionAccentPercent = 24;
constexpr int kRecentCommandLimit = 5;
constexpr int kShortcutTimeoutMs = 1500;

class ShortcutDelegate final : public QStyledItemDelegate
{
public:
    ShortcutDelegate(const QColor& text, QWidget* parent)
        : QStyledItemDelegate(parent)
        , text_(text)
    {}

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem background = option;
        initStyleOption(&background, index);
        background.text.clear();
        const QStyle* style = option.widget ? option.widget->style()
                                            : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,
                           &background,
                           painter,
                           option.widget);

        const QString shortcut = index.data(Qt::DisplayRole).toString();
        if (shortcut.isEmpty()) {
            return;
        }

        if (shortcut.contains(QLatin1String(", "))) {
            painter->save();
            painter->setPen(text_);
            painter->drawText(option.rect.adjusted(0, 0, -7, 0),
                              Qt::AlignRight | Qt::AlignVCenter,
                              shortcut);
            painter->restore();
            return;
        }

        QStringList keys = shortcut.split(QLatin1Char('+'), Qt::SkipEmptyParts);
        if (shortcut.endsWith(QLatin1Char('+'))) {
            keys.append(QStringLiteral("+"));
        }
        const QFontMetrics metrics(option.font);
        constexpr int horizontalPadding = 5;
        constexpr int gap = 3;
        constexpr int badgeHeight = 20;
        int right = option.rect.right() - 7;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        for (auto it = keys.crbegin(); it != keys.crend(); ++it) {
            const QString key = it->trimmed();
            const int width = metrics.horizontalAdvance(key)
                              + 2 * horizontalPadding;
            const QRect badge(right - width + 1,
                              option.rect.center().y() - badgeHeight / 2,
                              width,
                              badgeHeight);
            if (badge.left() < option.rect.left()) {
                break;
            }
            QColor ink = text_;
            ink.setAlpha(option.state & QStyle::State_Enabled ? 190 : 95);
            QColor outline = text_;
            outline.setAlpha(75);
            QColor fill = text_;
            fill.setAlpha(18);
            painter->setPen(outline);
            painter->setBrush(fill);
            painter->drawRoundedRect(badge, 3, 3);
            painter->setPen(ink);
            painter->drawText(badge, Qt::AlignCenter, key);
            right = badge.left() - gap - 1;
        }
        painter->restore();
    }

private:
    QColor text_;
};

void collect_categories(const QList<MenuNode>& nodes,
                        const QString& category,
                        QHash<QString, QString>& categories)
{
    for (const MenuNode& node : nodes) {
        if (node.type == MenuNode::Type::Action) {
            categories.insert(node.id, category);
        } else if (node.type == MenuNode::Type::Submenu) {
            collect_categories(node.children,
                               category.isEmpty() ? node.label : category,
                               categories);
        }
    }
}

QString shortcut_text(const QAction& action)
{
    QStringList labels;
    for (const QKeySequence& shortcut : action.shortcuts()) {
        if (!shortcut.isEmpty()) {
            labels.append(shortcut.toString(QKeySequence::NativeText));
        }
    }
    return labels.join(QStringLiteral(", "));
}

} // namespace

CommandPalette::CommandPalette(ActionRegistry& registry, QWidget* parent)
    : QDialog(parent)
    , registry_(registry)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground);
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Command Palette"));
    setFixedSize(kdialogWidth, kdialogHeight);

    familiar::dialog_style::apply_dialog_shadow(this);

    const auto colors
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor& background = colors[EPresetsColorIdx::BackgroundColor];
    const QColor& textColor = colors[EPresetsColorIdx::TextColor];
    const QColor& border = colors[EPresetsColorIdx::BorderColor];
    const QColor& accent = colors[EPresetsColorIdx::SelectionColor];
    QColor dimText = textColor;
    dimText.setAlpha(kdimTextAlpha);
    detailText_ = dimText;
    const auto blend = [](int base, int highlight) {
        return (base * (100 - kselectionAccentPercent)
                + highlight * kselectionAccentPercent)
               / 100;
    };
    const QColor selection(blend(background.red(), accent.red()),
                           blend(background.green(), accent.green()),
                           blend(background.blue(), accent.blue()));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(7, 7, 7, 7);
    outer->setSpacing(krowSpacing);

    auto* searchBox = new QWidget(this);
    searchBox->setObjectName(QStringLiteral("commandPaletteSearchBox"));
    auto* searchLayout = new QHBoxLayout(searchBox);
    searchLayout->setContentsMargins(9, 0, 5, 0);
    searchLayout->setSpacing(6);
    auto* prompt = new QLabel(QStringLiteral(">"), searchBox);
    prompt->setObjectName(QStringLiteral("commandPalettePrompt"));
    searchLayout->addWidget(prompt);

    search_ = new QLineEdit(searchBox);
    search_->setObjectName(QStringLiteral("commandPaletteSearch"));
    search_->setPlaceholderText(tr("Type a command..."));
    search_->setClearButtonEnabled(true);
    search_->setAccessibleName(tr("Search commands"));
    search_->installEventFilter(this);
    searchLayout->addWidget(search_);
    searchBox->setFixedHeight(38);
    outer->addWidget(searchBox);

    results_ = new QTreeWidget(this);
    results_->setObjectName(QStringLiteral("commandPaletteResults"));
    results_->setColumnCount(3);
    results_->setHeaderHidden(true);
    results_->setRootIsDecorated(false);
    results_->setIndentation(0);
    results_->setUniformRowHeights(true);
    results_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    results_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    results_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    results_->header()->resizeSection(1, ksecondaryColumnWidth);
    results_->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    results_->header()->resizeSection(2, kshortcutColumnWidth);
    results_->setItemDelegateForColumn(2,
                                       new ShortcutDelegate(textColor,
                                                            results_));
    results_->setAccessibleName(tr("Matching commands"));
    results_->installEventFilter(this);
    outer->addWidget(results_, 1);

    const QString sheet
        = familiar::dialog_style::panel_style_sheet("QDialog",
                                                    background,
                                                    border,
                                                    textColor,
                                                    0)
          + QStringLiteral(
                "QWidget#commandPaletteSearchBox {"
                "  background-color: %1; border: 1px solid %3;"
                "  border-radius: 5px;"
                "}"
                "QLabel#commandPalettePrompt { color: %3; font-size: 15px; }"
                "QLineEdit#commandPaletteSearch {"
                "  background-color: %1; color: %2;"
                "  border: none; padding: 0; font-size: 14px;"
                "  selection-background-color: %3;"
                "}"
                "QTreeWidget#commandPaletteResults {"
                "  background-color: %1; color: %2; border: none;"
                "  outline: none;"
                "}"
                "QTreeWidget#commandPaletteResults::item {"
                "  padding: 2px 6px;"
                "}"
                "QTreeWidget#commandPaletteResults::item:selected {"
                "  background-color: %4; color: %2;"
                "}"
                "QTreeWidget#commandPaletteResults::item:disabled {"
                "  color: %5;"
                "}")
                .arg(background.name(),
                     textColor.name(),
                     accent.name(),
                     selection.name(),
                     dimText.name(QColor::HexArgb));
    setStyleSheet(sheet);

    collect_categories(menu_structure(), {}, categories_);
    const QJsonArray stored = SettingsHandler::get_instance()
                                  ->json_value(QStringLiteral("CommandPalette"),
                                               QStringLiteral("recent_actions"))
                                  .toArray();
    for (const QJsonValue& value : stored) {
        const QString id = value.toString();
        if (!id.isEmpty() && !recentIds_.contains(id)
            && recentIds_.size() < kRecentCommandLimit) {
            recentIds_.append(id);
        }
    }
    connect(search_, &QLineEdit::textChanged, this, [this] {
        refresh_results();
    });
    connect(search_, &QLineEdit::returnPressed, this, [this] {
        activate_item(results_->currentItem());
    });
    connect(results_,
            &QTreeWidget::itemActivated,
            this,
            [this](QTreeWidgetItem* item) { activate_item(item); });
    refresh_results();
}

void CommandPalette::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    search_->setFocus();
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == search_ || watched == results_) {
        const bool commit = event->type() == QEvent::KeyPress;
        if (commit || event->type() == QEvent::ShortcutOverride) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (commit) {
                if (watched == search_ && key->key() == Qt::Key_Down) {
                    move_selection(1);
                    return true;
                }
                if (watched == search_ && key->key() == Qt::Key_Up) {
                    move_selection(-1);
                    return true;
                }
                if (key->key() == Qt::Key_Escape) {
                    reject();
                    return true;
                }
            }
            if (handle_shortcut(key, watched == search_, commit)) {
                key->accept();
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}

bool CommandPalette::handle_shortcut(QKeyEvent* event,
                                     bool inSearch,
                                     bool commit)
{
    if (event->isAutoRepeat() || event->key() == Qt::Key_unknown
        || event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift
        || event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        return false;
    }
    if (!pendingShortcut_.isEmpty()
        && shortcutClock_.hasExpired(kShortcutTimeoutMs)) {
        pendingShortcut_ = {};
    }

    const Qt::KeyboardModifiers modifiers = event->modifiers();
    if (pendingShortcut_.isEmpty() && inSearch) {
        if (!search_->text().isEmpty()
            && (event->key() == Qt::Key_Delete
                || event->key() == Qt::Key_Backspace)) {
            return false;
        }
        const bool plainText = modifiers == Qt::NoModifier
                               || modifiers == Qt::ShiftModifier;
        if (plainText && !event->text().isEmpty()) {
            return false;
        }
        const bool commandModifier = modifiers.testFlag(Qt::ControlModifier)
                                     || modifiers.testFlag(Qt::MetaModifier);
        const bool noAlt = !modifiers.testFlag(Qt::AltModifier);
        if (commandModifier && noAlt) {
            const int key = event->key();
            if (key == Qt::Key_V
                || (!search_->text().isEmpty()
                    && (key == Qt::Key_A || key == Qt::Key_C || key == Qt::Key_X
                        || key == Qt::Key_Z || key == Qt::Key_Y))) {
                return false;
            }
        }
    }

    const auto candidate_for = [this](Qt::KeyboardModifiers keyModifiers,
                                      Qt::Key key) {
        const QKeySequence stroke(QKeyCombination(keyModifiers, key));
        return pendingShortcut_.isEmpty()
                   ? stroke
                   : QKeySequence(pendingShortcut_.toString(
                                      QKeySequence::PortableText)
                                      + QStringLiteral(", ")
                                      + stroke.toString(
                                          QKeySequence::PortableText),
                                  QKeySequence::PortableText);
    };
    QList<QKeySequence> candidates;
    candidates.append(candidate_for(modifiers, Qt::Key(event->key())));
    const bool shiftedSymbol
        = modifiers.testFlag(Qt::ShiftModifier)
          && event->key() >= Qt::Key_Exclam
          && event->key() <= Qt::Key_AsciiTilde
          && !QChar(ushort(event->key())).isLetterOrNumber();
    if (shiftedSymbol) {
        candidates.append(candidate_for(modifiers & ~Qt::ShiftModifier,
                                        Qt::Key(event->key())));
    }
    QString exactId;
    QKeySequence partialCandidate;
    bool hasDisabledMatch = false;
    for (const QKeySequence& candidate : candidates) {
        for (const Action* action : registry_.all()) {
            const QAction* qaction = action->qaction();
            if (!qaction) {
                continue;
            }
            for (const QKeySequence& shortcut : qaction->shortcuts()) {
                const auto match = candidate.matches(shortcut);
                if (match == QKeySequence::ExactMatch) {
                    if (qaction->isEnabled()) {
                        exactId = action->id();
                        break;
                    }
                    hasDisabledMatch = true;
                } else if (match == QKeySequence::PartialMatch
                           && partialCandidate.isEmpty()) {
                    partialCandidate = candidate;
                }
            }
            if (!exactId.isEmpty()) {
                break;
            }
        }
        if (!exactId.isEmpty()) {
            break;
        }
    }

    if (!exactId.isEmpty() || hasDisabledMatch) {
        if (commit) {
            pendingShortcut_ = {};
            if (exactId == QLatin1String("command_palette")) {
                reject();
            } else if (!exactId.isEmpty()) {
                activate_action(exactId);
            }
        }
        return true;
    }
    if (!partialCandidate.isEmpty()) {
        if (commit) {
            pendingShortcut_ = partialCandidate;
            shortcutClock_.start();
        }
        return true;
    }
    if (!pendingShortcut_.isEmpty()) {
        pendingShortcut_ = {};
        return handle_shortcut(event, inSearch, commit);
    }
    return false;
}

void CommandPalette::remember_action(const QString& id)
{
    if (id == QLatin1String("command_palette")
        || id.startsWith(QLatin1String("recent_files_"))) {
        return;
    }
    if (!recentIds_.isEmpty() && recentIds_.constFirst() == id) {
        return;
    }
    recentIds_.removeAll(id);
    recentIds_.prepend(id);
    while (recentIds_.size() > kRecentCommandLimit) {
        recentIds_.removeLast();
    }
    QJsonArray stored;
    for (const QString& recentId : recentIds_) {
        stored.append(recentId);
    }
    SettingsHandler::get_instance()
        ->set_json_value(QStringLiteral("CommandPalette"),
                         QStringLiteral("recent_actions"),
                         stored);
}

QString CommandPalette::unavailable_reason(const QString& id,
                                           const QString& group) const
{
    if (group == QLatin1String("active_when_items_in_scene")) {
        return tr("Add an item first");
    }
    if (group == QLatin1String("active_when_selection")) {
        return tr("Select an item");
    }
    if (group == QLatin1String("active_when_single_image")) {
        return tr("Select one image");
    }
    if (group == QLatin1String("active_when_multi_selection")) {
        return tr("Select 2+ items");
    }
    if (group == QLatin1String("active_when_group_selected")) {
        return tr("Select a group");
    }
    if (group == QLatin1String("active_when_can_undo")) {
        return tr("Nothing to undo");
    }
    if (group == QLatin1String("active_when_can_redo")) {
        return tr("Nothing to redo");
    }
    if (id == QLatin1String("auto_hide_ui")) {
        return tr("Show the menu bar first");
    }
    return tr("Unavailable right now");
}

void CommandPalette::refresh_results()
{
    const QStringList terms
        = search_->text().simplified().split(QLatin1Char(' '),
                                             Qt::SkipEmptyParts);
    const QString previousId
        = results_->currentItem()
              ? results_->currentItem()->data(0, Qt::UserRole).toString()
              : QString();
    results_->clear();
    QTreeWidgetItem* firstEnabled = nullptr;
    QTreeWidgetItem* previous = nullptr;

    const auto matches_search = [&terms](const Action* action) {
        QAction* qaction = action->qaction();
        if (!qaction) {
            return false;
        }
        const bool recent = action->id().startsWith(
            QLatin1String("recent_files_"));
        const QString name = recent ? tr("Open Recent: %1").arg(qaction->text())
                                    : action->display_text();
        return std::all_of(terms.cbegin(),
                           terms.cend(),
                           [&name](const QString& term) {
                               return name.contains(term, Qt::CaseInsensitive);
                           });
    };

    QList<const Action*> recentActions;
    for (const QString& id : recentIds_) {
        if (const Action* action = registry_.find(id);
            action && matches_search(action)) {
            recentActions.append(action);
        }
    }
    QList<const Action*> otherActions;
    for (const Action* action : registry_.all()) {
        if (!recentIds_.contains(action->id()) && matches_search(action)) {
            otherActions.append(action);
        }
    }

    const auto add_heading = [this](const QString& title) {
        auto* item = new QTreeWidgetItem(results_);
        item->setText(0, title.toUpper());
        item->setForeground(0, detailText_);
        QFont font = results_->font();
        font.setBold(true);
        item->setFont(0, font);
        item->setSizeHint(0, QSize(0, kresultHeight));
        item->setDisabled(true);
    };

    const auto add_action = [this, &firstEnabled, &previous, &previousId](
                                const Action* action) {
        QAction* qaction = action->qaction();
        const bool recent = action->id().startsWith(
            QLatin1String("recent_files_"));
        const QString name = recent ? tr("Open Recent: %1").arg(qaction->text())
                                    : action->display_text();

        const bool enabled = qaction->isEnabled();
        const QString detail = enabled ? categories_.value(action->id(),
                                                           recent ? tr("File")
                                                                  : QString())
                                       : unavailable_reason(action->id(),
                                                            action->group());
        QString label = name;
        if (qaction->isCheckable() && qaction->isChecked()) {
            label.prepend(QStringLiteral("✓  "));
        }
        auto* item = new QTreeWidgetItem(results_);
        item->setText(0, label);
        item->setText(1, detail);
        item->setForeground(1, detailText_);
        item->setText(2, shortcut_text(*qaction));
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(0, Qt::UserRole, action->id());
        item->setSizeHint(0, QSize(0, kresultHeight));
        item->setToolTip(0, name);
        if (!enabled) {
            item->setToolTip(1, detail);
            item->setDisabled(true);
        }
        if (enabled && !firstEnabled) {
            firstEnabled = item;
        }
        if (action->id() == previousId) {
            previous = item;
        }
    };

    if (!recentActions.isEmpty() && terms.isEmpty()) {
        add_heading(tr("Recently used"));
    }
    for (const Action* action : recentActions) {
        add_action(action);
    }
    if (!recentActions.isEmpty() && !otherActions.isEmpty() && terms.isEmpty()) {
        add_heading(tr("Other commands"));
    }
    for (const Action* action : otherActions) {
        add_action(action);
    }

    if (recentActions.isEmpty() && otherActions.isEmpty()) {
        auto* empty = new QTreeWidgetItem(results_);
        empty->setText(0, tr("No matching commands"));
        empty->setDisabled(true);
        return;
    }
    if (previous && !previous->isDisabled()) {
        results_->setCurrentItem(previous);
    } else if (firstEnabled) {
        results_->setCurrentItem(firstEnabled);
    }
}

void CommandPalette::move_selection(int direction)
{
    const int count = results_->topLevelItemCount();
    if (count == 0) {
        return;
    }
    const int current = results_->indexOfTopLevelItem(results_->currentItem());
    const int start = current < 0 ? (direction > 0 ? count - 1 : 0) : current;
    for (int step = 1; step <= count; ++step) {
        const int index = (start + direction * step + count * 2) % count;
        QTreeWidgetItem* item = results_->topLevelItem(index);
        if (!item->isDisabled()) {
            results_->setCurrentItem(item);
            results_->scrollToItem(item);
            return;
        }
    }
}

void CommandPalette::activate_item(QTreeWidgetItem* item)
{
    if (!item || item->isDisabled()) {
        return;
    }
    activate_action(item->data(0, Qt::UserRole).toString());
}

void CommandPalette::activate_action(const QString& id)
{
    const Action* action = registry_.find(id);
    QAction* qaction = action ? action->qaction() : nullptr;
    if (!qaction || !qaction->isEnabled()) {
        refresh_results();
        return;
    }
    selectedAction_ = qaction;
    remember_action(id);
    accept();
}
