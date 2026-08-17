#include "hierarchy_panel.h"

#include <canvasscene.h>
#include <canvasview.h>
#include <commands.h>
#include <core/settingshandler.h>
#include <moveitem.h>
#include <widgets/dialog_style.h>

#include <QAction>
#include <QDialog>
#include <QDropEvent>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWindow>

#include <functional>

#include "log/log.h"
using namespace familiar::log;

namespace {

constexpr int kIconSize = 18;
constexpr int kUidRole = Qt::UserRole;

// Same drawn-icon approach as every other icon in this app
// (group_toolbar.cpp, gif_playback_toolbar.cpp) - no external asset.
QIcon makeGroupIcon(const QColor& glyphColor)
{
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(glyphColor);
    const qreal cell = kIconSize * 0.38;
    const qreal gap = kIconSize * 0.14;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 2; ++col) {
            const qreal x = kIconSize * 0.08 + col * (cell + gap);
            const qreal y = kIconSize * 0.08 + row * (cell + gap);
            p.drawRoundedRect(QRectF(x, y, cell, cell), 2, 2);
        }
    }
    p.end();
    return QIcon(pm);
}

QIcon makeTextIcon(const QColor& glyphColor)
{
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(int(kIconSize * 0.7));
    p.setFont(f);
    p.setPen(glyphColor);
    p.drawText(QRectF(0, 0, kIconSize, kIconSize),
               Qt::AlignCenter,
               QStringLiteral("T"));
    p.end();
    return QIcon(pm);
}

QIcon makePictureIcon(const QPixmap& source, const QColor& glyphColor)
{
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(0.5, 0.5, kIconSize - 1, kIconSize - 1), 3, 3);
    p.setClipPath(clip);
    if (!source.isNull()) {
        // Scaled+cropped to fill the square (KeepAspectRatioByExpanding),
        // not letterboxed - a thumbnail is more recognizable filling the
        // whole icon than shrunk down with empty borders.
        const QPixmap scaled = source.scaled(kIconSize,
                                             kIconSize,
                                             Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation);
        p.drawPixmap((kIconSize - scaled.width()) / 2,
                     (kIconSize - scaled.height()) / 2,
                     scaled);
    } else {
        p.fillRect(pm.rect(),
                   QColor(glyphColor.red(),
                          glyphColor.green(),
                          glyphColor.blue(),
                          60));
    }
    p.setClipping(false);
    QPen border(QColor(0, 0, 0, 60));
    border.setWidthF(1.0);
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawPath(clip);
    p.end();
    return QIcon(pm);
}

// "Export" on a group (context menu, showContextMenu_()) - every picture
// among the group's own members, recursively through nested subgroups.
// GroupItem::resolve_children() already includes members that got there
// via an attachment (a note or picture attached to something already in
// the group becomes a real member too - see CanvasScene::
// attach_item_to()), so this needs no separate attach-chain walk of its
// own, just group membership.
void collectGroupPictures(GroupItem* group, QList<PixmapItem*>& out)
{
    for (QGraphicsItem* child : group->resolve_children()) {
        if (auto* picture = dynamic_cast<PixmapItem*>(child))
            out.append(picture);
        else if (auto* nested = dynamic_cast<GroupItem*>(child))
            collectGroupPictures(nested, out);
    }
}

// Interactive drag-and-drop (current, PureRef-style). Qt's own
// InternalMove drag-drop mode is used only to get the press/drag/drop
// GESTURE for free - the QTreeWidgetItem reparenting it would normally do on drop
// is discarded entirely (dropEvent() below never calls the base class
// implementation): this tree is always a read-only PROJECTION of the
// scene graph, rebuilt wholesale by HierarchyPanel::rebuild_() whenever
// the scene actually changes, so a drop here just translates into a
// CanvasScene call (via onDrop) and lets that call's own undo command
// trigger the next rebuild. No Q_OBJECT/moc needed - onDrop is a plain
// std::function callback, not a signal, and dropEvent() is an ordinary
// virtual override.
class HierarchyTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

    // dragged is always non-null (Qt doesn't start a drag without a
    // current item); target is null when dropped on empty space below
    // the last root row.
    std::function<void(QTreeWidgetItem* dragged, QTreeWidgetItem* target)> onDrop;

protected:
    void dropEvent(QDropEvent* event) override
    {
        QTreeWidgetItem* dragged = currentItem();
        QTreeWidgetItem* target = itemAt(event->position().toPoint());
        // NOT acceptProposedAction() - for InternalMove, the proposed
        // action IS Qt::MoveAction, and accepting AS a move tells
        // QAbstractItemView::startDrag() (back up the call stack, once
        // drag->exec() returns) that a row move actually happened at
        // the MODEL level - it then deletes the dragged QTreeWidgetItem
        // (and its children) itself, entirely bypassing rebuild_()'s
        // own gifIconConnections_ disconnect-before-clear() ordering.
        // Crashed exactly that way (dragging a picture with 2 attached
        // notes into a group - some UNRELATED gif's frameChanged fired
        // into a node Qt had already deleted out from under it). This
        // view's "move" is entirely on the SCENE side (via onDrop's own
        // CanvasScene calls) - explicitly IgnoreAction here tells Qt
        // none of ITS row-ownership bookkeeping applies, so it leaves
        // every QTreeWidgetItem alone and rebuild_() stays the only
        // thing that ever deletes tree nodes.
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        if (dragged && onDrop)
            onDrop(dragged, target);
    }
};

// Rename prompt (HierarchyPanel::startRename_()) - a real, small, modal
// top-level dialog, same custom-chrome convention as every other dialog
// in this app (dialog_style.h: frameless, opaque + setMask() rounding,
// NOT WA_TranslucentBackground - see panelStyleSheet()'s own comment for
// why). Deliberately NOT an inline editor layered over the tree row -
// two different inline approaches were tried and abandoned first:
// QTreeWidget's own item-editing (QAbstractItemView::edit()/
// QStyledItemDelegate's QExpandingLineEdit - Qt-level success confirmed
// (edit() returned true), yet typed characters were read as this app's
// own single-letter tool shortcuts, H/V/R/G/S/1/2 (actions.cpp), instead
// of reaching the editor), then a plain hand-positioned QLineEdit child
// widget (isVisible()/hasFocus() both true, even with a loud debug
// stylesheet applied - never actually appeared on screen at all,
// confirmed with Max). Neither was ever root-caused; a genuine top-level
// window sidesteps both, since it has its own independent backing
// store/compositing, unlike a child widget layered inside this app's
// translucent frameless MainWindow.
class RenameDialog : public QDialog
{
public:
    RenameDialog(const QString& currentName, QWidget* parent)
        : QDialog(parent)
    {
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_StyledBackground);
        setWindowModality(Qt::ApplicationModal);
        setFixedWidth(320);

        auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
        const QColor& textColor = colorPreset[EPresetsColorIdx::kTextColor];
        const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
        const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
        const QColor& accent = colorPreset[EPresetsColorIdx::kSelectionColor];

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(24);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 150));
        setGraphicsEffect(shadow);

        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(18, 14, 18, 16);
        outer->setSpacing(10);

        auto* titleLabel = new QLabel(tr("Rename"), this);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        outer->addWidget(titleLabel);

        edit_ = new QLineEdit(currentName, this);
        edit_->selectAll();
        connect(edit_, &QLineEdit::returnPressed, this, &QDialog::accept);
        outer->addWidget(edit_);

        auto* buttonRow = new QHBoxLayout();
        buttonRow->addStretch();
        auto* cancelBtn = new QPushButton(tr("Cancel"), this);
        familiar::dialog_style::styleSecondaryButton(cancelBtn, textColor, border);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        buttonRow->addWidget(cancelBtn);
        auto* okBtn = new QPushButton(tr("Rename"), this);
        familiar::dialog_style::stylePrimaryButton(okBtn, accent);
        okBtn->setDefault(true);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        buttonRow->addWidget(okBtn);
        outer->addLayout(buttonRow);

        // "QDialog", not "RenameDialog" - this class has no Q_OBJECT (a
        // plain local class, no moc), so metaObject()->className() (what
        // this type-selector actually matches against) reports the base
        // Qt class, "QDialog", not our own subclass name. Passing our
        // own name here silently never matched anything - the panel's
        // own opaque background rule never applied, leaving the window
        // to fall back to MainWindow's app-wide "background: transparent"
        // with nothing underneath to actually paint (rendered solid
        // black, stale content visibly not clearing while typing).
        setStyleSheet(
            familiar::dialog_style::panelStyleSheet("QDialog",
                                                    background,
                                                    border,
                                                    textColor)
            + QStringLiteral("QLineEdit {"
                             "  background-color: rgba(0, 0, 0, 20);"
                             "  color: %1;"
                             "  border: 1px solid %2;"
                             "  border-radius: 4px;"
                             "  padding: 6px 8px;"
                             "}")
                  .arg(textColor.name(), border.name()));
    }

    QString text() const { return edit_->text(); }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemMove();
            event->accept();
            return;
        }
        QDialog::mousePressEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QDialog::resizeEvent(event);
        familiar::dialog_style::applyRoundedMask(this, 10);
    }

private:
    QLineEdit* edit_ = nullptr;
};

} // namespace

HierarchyPanel::HierarchyPanel(QWidget* parent)
    : QDockWidget(QObject::tr("Hierarchy"), parent)
{
    setObjectName(QStringLiteral("hierarchyPanel"));
    // No DockWidgetClosable/Floatable - those draw Qt's native, unstyled
    // float/close glyphs in the title bar, which stood out against this
    // app's own drawn-icon look. Visibility is already handled by the
    // "hierarchy" action (Ctrl+J/View menu) and floating this into a separate top-level
    // window isn't a use case here - just keep it draggable within the
    // main window's dock areas.
    setFeatures(QDockWidget::DockWidgetMovable);

    // Custom title bar - see titleLabel_'s own comment (hierarchy_panel.h)
    // for why: QDockWidget's native title text didn't reliably follow
    // "QDockWidget::title { color: ... }" QSS on this style. A plain
    // QWidget/QLabel does, and still gets QDockWidget's own drag-to-move
    // handling for free (that's generic to whatever titleBarWidget() is,
    // native or not) - DockWidgetMovable above still works.
    titleBar_ = new QWidget(this);
    titleBar_->setAttribute(Qt::WA_StyledBackground);
    auto* titleLayout = new QHBoxLayout(titleBar_);
    titleLayout->setContentsMargins(8, 5, 8, 5);
    titleLabel_ = new QLabel(tr("Hierarchy"), titleBar_);
    titleLayout->addWidget(titleLabel_);
    titleLayout->addStretch(1);
    setTitleBarWidget(titleBar_);

    auto* tree = new HierarchyTreeWidget(this);
    tree_ = tree;
    tree_->setHeaderHidden(true);
    tree_->setIndentation(14);
    tree_->setUniformRowHeights(true);
    // Interactive drag-and-drop (current, PureRef-style) - InternalMove
    // for the press/drag/drop gesture only, see HierarchyTreeWidget's
    // own comment for why the actual reparenting it would do is discarded in favor of a CanvasScene
    // call. Single-item drag only (v1) - SingleSelection keeps
    // currentItem() unambiguous.
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setDragEnabled(true);
    tree_->setAcceptDrops(true);
    tree_->setDropIndicatorShown(true);
    tree_->setDragDropMode(QAbstractItemView::InternalMove);
    tree->onDrop = [this](QTreeWidgetItem* dragged, QTreeWidgetItem* target) {
        handleTreeDrop_(dragged, target);
    };
    connect(tree_,
            &QTreeWidget::itemClicked,
            this,
            &HierarchyPanel::onItemClicked_);
    connect(tree_,
            &QTreeWidget::itemDoubleClicked,
            this,
            &HierarchyPanel::onItemDoubleClicked_);
    auto* renameShortcut = new QShortcut(QKeySequence(Qt::Key_F2), tree_);
    // WindowShortcut, not Widget/WidgetWithChildrenShortcut - neither of
    // those ever fired at all (confirmed via debug log during
    // development: tree_ genuinely never has actual Qt widget focus
    // after a click in this app - frameless window + this app's own
    // custom focus/event handling, see MainWindow's eventFilter).
    // WindowShortcut fires anywhere within the top-level window
    // regardless of which exact child has focus - no other action in
    // this app is bound to F2 (grepped), so this is safe app-wide.
    renameShortcut->setContext(Qt::WindowShortcut);
    connect(renameShortcut, &QShortcut::activated, this, [this] {
        startRename_(tree_->currentItem());
    });
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_,
            &QTreeWidget::customContextMenuRequested,
            this,
            &HierarchyPanel::showContextMenu_);
    setWidget(tree_);

    rebuildTimer_ = new QTimer(this);
    rebuildTimer_->setSingleShot(true);
    rebuildTimer_->setInterval(150);
    connect(rebuildTimer_, &QTimer::timeout, this, &HierarchyPanel::refresh);

    applyColorStyle_();
    // Live-updates on preset/color change - applyColorStyle_() alone only
    // covers the panel's own QSS (background/text/border/selection);
    // refresh() (rebuild_()) is also needed because node icons
    // (makeNode_()/addItemNode_()) bake the text color into a QPixmap at
    // build time rather than reading it via QSS, so a plain
    // setStyleSheet() elsewhere never touches them (Max: it "only
    // applies after a restart").
    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            [this] {
                applyColorStyle_();
                refresh();
            });
}

void HierarchyPanel::applyColorStyle_()
{
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
    setStyleSheet(
        QStringLiteral("QTreeWidget {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: none;"
                       "}"
                       "QTreeWidget::item { padding: 3px 2px; }"
                       "QTreeWidget::item:selected { background-color: %3; }")
            .arg(background.name(), text.name(), selection.name()));

    // Custom title bar (see the constructor's own comment for why this
    // isn't "QDockWidget::title { ... }" QSS any more) - same colors,
    // applied directly to the real widgets instead.
    if (titleBar_) {
        titleBar_->setStyleSheet(
            QStringLiteral("background-color: %1;").arg(background.name()));
    }
    if (titleLabel_) {
        titleLabel_->setStyleSheet(
            QStringLiteral("color: %1;").arg(text.name()));
        // Belt-and-suspenders alongside the stylesheet line above, not a
        // replacement for it - this app has already hit more than one
        // case this session where a QSS "color:" rule quietly didn't
        // take (QDockWidget::title itself, the reason this custom title
        // bar exists at all). QPalette::WindowText/Text is what QLabel
        // actually paints from when nothing overrides it - setting both
        // roles directly removes any doubt.
        QPalette pal = titleLabel_->palette();
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Text, text);
        titleLabel_->setPalette(pal);
    }
}

void HierarchyPanel::setScene(CanvasScene* scene, CanvasView* view)
{
    scene_ = scene;
    view_ = view;
    rebuild_();
}

void HierarchyPanel::refresh()
{
    if (!isVisible()) {
        dirty_ = true;
        return;
    }
    rebuild_();
}

void HierarchyPanel::scheduleRefresh()
{
    // Throttle, not debounce - see this class's header comment for why
    // its trigger changed from CanvasScene::changed() (fired
    // continuously while any GifItem animates, which either starved a
    // plain debounce forever or, once throttled, still forced a
    // pointless rebuild every ~150ms during animation) to
    // QUndoStack::indexChanged. Kept as a throttle regardless, in case
    // indexChanged itself ever fires several times back to back. Only
    // arm the timer if it isn't already pending; further calls while
    // it's running are coalesced into the rebuild it's already going to
    // do.
    if (!rebuildTimer_->isActive())
        rebuildTimer_->start();
}

void HierarchyPanel::showEvent(QShowEvent* event)
{
    QDockWidget::showEvent(event);
    if (dirty_)
        rebuild_();
}

void HierarchyPanel::rebuild_()
{
    dirty_ = false;
    // Tear down every gif-frame connection from the PREVIOUS tree before
    // clear() deletes the QTreeWidgetItems they close over - a
    // still-live QMovie::frameChanged firing into a dangling node
    // pointer would be a use-after-free.
    for (const QMetaObject::Connection& c : gifIconConnections_)
        QObject::disconnect(c);
    gifIconConnections_.clear();
    tree_->clear();
    if (!scene_)
        return;

    // Every uid that's either a group's child or an attached item's own
    // uid is "consumed" - it gets added as a NESTED node from its
    // owner/anchor's own recursion below, not as a second, redundant
    // top-level root. attachedToUid() is generalized to any IBaseItem
    // now, not just TextItem, so this checks the interface rather than
    // dynamic_cast<TextItem*>.
    QSet<QUuid> consumed;
    const QList<QGraphicsItem*> allItems = scene_->items();
    for (QGraphicsItem* item : allItems) {
        if (!scene_->itemAddByUser(item))
            continue;
        if (auto* group = dynamic_cast<GroupItem*>(item)) {
            for (const QUuid& childId : group->child_ids())
                consumed.insert(childId);
        }
        if (auto* base = dynamic_cast<IBaseItem*>(item)) {
            if (!base->attachedToUid().isNull())
                consumed.insert(base->uid());
        }
    }

    QSet<QUuid> added;
    for (QGraphicsItem* item : allItems) {
        if (!scene_->itemAddByUser(item))
            continue;
        auto* base = dynamic_cast<IBaseItem*>(item);
        if (!base || consumed.contains(base->uid()))
            continue;
        addItemNode_(nullptr, item, added);
    }
    tree_->expandAll();
}

void HierarchyPanel::addItemNode_(QTreeWidgetItem* parent,
                                  QGraphicsItem* item,
                                  QSet<QUuid>& added)
{
    auto* base = dynamic_cast<IBaseItem*>(item);
    if (!base || added.contains(base->uid()))
        return; // already placed via some other relationship - see rebuild_()'s comment
    added.insert(base->uid());

    QTreeWidgetItem* node = makeNode_(item);
    if (parent)
        parent->addChild(node);
    else
        tree_->addTopLevelItem(node);

    if (auto* group = dynamic_cast<GroupItem*>(item)) {
        for (QGraphicsItem* child : group->resolve_children())
            addItemNode_(node, child, added);
    }
    if (auto* picture = dynamic_cast<PixmapItem*>(
            item)) { // GifItem IS-A PixmapItem
        for (QGraphicsItem* note : scene_->find_attached_items(picture->uid()))
            addItemNode_(node, note, added);
    }
    if (auto* gif = dynamic_cast<GifItem*>(item))
        connectGifAnimation_(node, gif);
}

void HierarchyPanel::connectGifAnimation_(QTreeWidgetItem* node, GifItem* gif)
{
    // Now that the panel's rebuild trigger is QUndoStack::
    // indexChanged rather than the animation-driven CanvasScene::
    // changed(), a per-node icon update on frameChanged is a targeted,
    // cheap operation (setIcon() on one row) instead of a full tree
    // rebuild every frame.
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor glyphColor = colorPreset[EPresetsColorIdx::kTextColor];
    QMetaObject::Connection conn
        = QObject::connect(gif->movie(),
                           &QMovie::frameChanged,
                           this,
                           [node, gif, glyphColor](int) {
                               node->setIcon(0,
                                             makePictureIcon(gif->pixmap(),
                                                             glyphColor));
                           });
    gifIconConnections_.append(conn);
}

QTreeWidgetItem* HierarchyPanel::makeNode_(QGraphicsItem* item)
{
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];

    QString label;
    QIcon icon;
    if (dynamic_cast<GroupItem*>(item)) {
        label = QObject::tr("Group");
        icon = makeGroupIcon(text);
    } else if (auto* picture = dynamic_cast<PixmapItem*>(item)) {
        label = picture->filename_.isEmpty()
                    ? QObject::tr("Untitled")
                    : QFileInfo(picture->filename_).fileName();
        icon = makePictureIcon(picture->pixmap(), text);
    } else if (auto* txt = dynamic_cast<TextItem*>(item)) {
        const QString plain = txt->toPlainText().trimmed();
        label = plain.isEmpty() ? QObject::tr("Text") : plain.left(40);
        icon = makeTextIcon(text);
    } else {
        label = QObject::tr("Item");
    }

    auto* node = new QTreeWidgetItem();
    node->setText(0, label);
    node->setIcon(0, icon);
    if (auto* base = dynamic_cast<IBaseItem*>(item))
        node->setData(0, kUidRole, base->uid());
    return node;
}

void HierarchyPanel::onItemClicked_(QTreeWidgetItem* node)
{
    if (!scene_ || syncingSelection_)
        return;
    const QUuid uid = node->data(0, kUidRole).toUuid();
    QGraphicsItem* item = scene_->find_by_uid(uid);
    if (!item)
        return;
    scene_->deselect_all_items();
    item->setSelected(true);
}

void HierarchyPanel::onItemDoubleClicked_(QTreeWidgetItem* node)
{
    if (!scene_ || !view_)
        return;
    const QUuid uid = node->data(0, kUidRole).toUuid();
    QGraphicsItem* item = scene_->find_by_uid(uid);
    if (!item)
        return;

    scene_->deselect_all_items();
    item->setSelected(true);

    if (auto* text = dynamic_cast<TextItem*>(item)) {
        text->enter_edit_mode();
        text->setFocus();
        return;
    }
    view_->fitRect(scene_->itemsBoundingRect(false, QList<QGraphicsItem*>{item}),
                   item);
}

void HierarchyPanel::handleTreeDrop_(QTreeWidgetItem* dragged,
                                     QTreeWidgetItem* target)
{
    if (!scene_ || dragged == target)
        return;
    QGraphicsItem* draggedItem = scene_->find_by_uid(
        dragged->data(0, kUidRole).toUuid());
    if (!draggedItem)
        return;

    if (!target) {
        // Dropped on empty space below the last root row - makes it
        // top-level. Covers both an attached item
        // (clears the attachment) and a plain grouped item/nested
        // subgroup (leaves its group) - detach_item() picks whichever
        // applies, no-op if already fully top-level.
        scene_->detach_item(draggedItem);
        return;
    }

    QGraphicsItem* targetItem = scene_->find_by_uid(
        target->data(0, kUidRole).toUuid());
    if (!targetItem)
        return;

    if (auto* targetGroup = dynamic_cast<GroupItem*>(targetItem)) {
        // An attached item's group membership is derived from its
        // anchor, not independently settable (add_to_group() itself
        // also refuses this - see its own comment) - re-attach to a
        // different picture instead if that's the intent.
        scene_->add_to_group(draggedItem, targetGroup);
    } else if (auto* targetPicture = dynamic_cast<PixmapItem*>(targetItem)) {
        // A whole GroupItem can't be "attached" (attach means one
        // riding-along item, not a cluster) - only offer this for a
        // non-group dragged item.
        if (!dynamic_cast<GroupItem*>(draggedItem))
            scene_->attach_item_to(draggedItem, targetPicture->uid());
    }
    // Dropped on a TextItem node: nothing attaches to a note - no-op.
}

void HierarchyPanel::showContextMenu_(const QPoint& pos)
{
    QTreeWidgetItem* node = tree_->itemAt(pos);
    if (!node || !scene_)
        return;
    QGraphicsItem* item = scene_->find_by_uid(node->data(0, kUidRole).toUuid());
    if (!item)
        return;

    auto* picture = dynamic_cast<PixmapItem*>(item);
    auto* group = dynamic_cast<GroupItem*>(item);
    auto* text = dynamic_cast<TextItem*>(item);
    if (!picture && !group && !text)
        return; // nothing actionable for this node type

    QMenu menu(this);
    QAction* renameAction = nullptr;
    if (picture) {
        renameAction = menu.addAction(tr("Rename"));
        renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    }
    QAction* editAction = nullptr;
    if (text)
        editAction = menu.addAction(tr("Edit"));
    QAction* exportAction = nullptr;
    if (picture || group)
        exportAction = menu.addAction(tr("Export..."));

    // Same reasoning as FileBrowserDialog::showContextMenu_() - QMenu is
    // a separate top-level popup, not a plain child widget, so this
    // dock's own setStyleSheet() cascade doesn't reach it; left unstyled
    // it falls back to the app-wide "background: transparent" (MainWindow's
    // own setStyleSheet()) with nothing underneath to actually paint,
    // rendering solid black.
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& menuBg = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& menuBorder = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& menuText = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& menuAccent = colorPreset[EPresetsColorIdx::kSelectionColor];
    menu.setStyleSheet(
        QStringLiteral("QMenu {"
                       "  background-color: %1;"
                       "  color: %2;"
                       "  border: 1px solid %3;"
                       "  border-radius: 6px;"
                       "  padding: 4px;"
                       "}"
                       "QMenu::item {"
                       "  padding: 4px 20px;"
                       "  border-radius: 4px;"
                       "}"
                       "QMenu::item:selected {"
                       "  background-color: %4;"
                       "  color: white;"
                       "}")
            .arg(menuBg.name(), menuText.name(), menuBorder.name(),
                menuAccent.name()));

    // Acted on AFTER exec() returns, not from the actions' own
    // triggered() handlers - QMenu is still mid-close/ungrab at the
    // moment a handler fires from inside its own nested event loop.
    QAction* chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
    // Dismissing the menu without picking anything (Escape, click
    // elsewhere) returns nullptr from exec() - without this guard that
    // matched whichever of renameAction/editAction/exportAction wasn't
    // actually added for this node's type (still nullptr, never
    // reassigned above), so e.g. right-clicking a picture (no Edit
    // action - that's text-only) and dismissing the menu made
    // `chosen == editAction` true via nullptr == nullptr, running the
    // Edit branch's text->enter_edit_mode() on a null `text` and
    // crashing.
    if (!chosen)
        return;
    if (chosen == renameAction) {
        startRename_(node);
    } else if (chosen == editAction) {
        // Same as double-clicking the row (onItemDoubleClicked_) - drops
        // straight into the note's own in-canvas text editing, not a
        // separate dialog/overlay of ours (TextItem::enter_edit_mode()
        // already works fine as-is, nothing like the picture-rename
        // saga's rendering/focus issues to work around here).
        scene_->deselect_all_items();
        item->setSelected(true);
        text->enter_edit_mode();
        text->setFocus();
    } else if (chosen == exportAction) {
        if (!view_)
            return;
        // Single picture -> just itself; a group -> every picture among
        // its own members, recursively through nested subgroups
        // (collectGroupPictures()).
        QList<PixmapItem*> pictures;
        if (picture)
            pictures.append(picture);
        else if (group)
            collectGroupPictures(group, pictures);
        view_->exportPictures(pictures);
    }
}

void HierarchyPanel::startRename_(QTreeWidgetItem* node)
{
    if (!node || !scene_)
        return;
    auto* picture = dynamic_cast<PixmapItem*>(
        scene_->find_by_uid(node->data(0, kUidRole).toUuid()));
    if (!picture)
        return; // F2/Rename only meaningful for a picture row

    RenameDialog dlg(node->text(0), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString currentLabel = picture->filename_.isEmpty()
                                     ? tr("Untitled")
                                     : QFileInfo(picture->filename_).fileName();
    const QString newName = dlg.text().trimmed();
    if (newName.isEmpty() || newName == currentLabel)
        return;

    FLOG_DEBUG(Ch::UI,
              "startRename_: '{}' -> '{}'",
              currentLabel.toStdString(),
              newName.toStdString());
    scene_->undo_stack_->push(
        new RenamePictureCommand(picture, picture->filename_, newName));
}

void HierarchyPanel::syncSelectionFromScene()
{
    if (!scene_)
        return;
    syncingSelection_ = true;
    QSet<QUuid> selectedUids;
    for (QGraphicsItem* item : scene_->selectedItems(true)) {
        if (auto* base = dynamic_cast<IBaseItem*>(item))
            selectedUids.insert(base->uid());
    }

    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* node) {
        const QUuid uid = node->data(0, kUidRole).toUuid();
        node->setSelected(selectedUids.contains(uid));
        for (int i = 0; i < node->childCount(); ++i)
            walk(node->child(i));
    };
    for (int i = 0; i < tree_->topLevelItemCount(); ++i)
        walk(tree_->topLevelItem(i));
    syncingSelection_ = false;
}
