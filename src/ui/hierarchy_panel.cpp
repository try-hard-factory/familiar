#include "hierarchy_panel.h"

#include <canvasscene.h>
#include <canvasview.h>
#include <core/settingshandler.h>
#include <moveitem.h>

#include <QFileInfo>
#include <QHeaderView>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

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
    p.drawText(QRectF(0, 0, kIconSize, kIconSize), Qt::AlignCenter, QStringLiteral("T"));
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
        const QPixmap scaled = source.scaled(
            kIconSize, kIconSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        p.drawPixmap((kIconSize - scaled.width()) / 2,
                    (kIconSize - scaled.height()) / 2,
                    scaled);
    } else {
        p.fillRect(pm.rect(), QColor(glyphColor.red(), glyphColor.green(), glyphColor.blue(), 60));
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

} // namespace

HierarchyPanel::HierarchyPanel(QWidget* parent)
    : QDockWidget(QObject::tr("Hierarchy"), parent)
{
    setObjectName(QStringLiteral("hierarchyPanel"));
    // No DockWidgetClosable/Floatable - those draw Qt's native, unstyled
    // float/close glyphs in the title bar (Max: "а что это за
    // кнопочки?" - they stood out against this app's own drawn-icon
    // look). Visibility is already handled by the "hierarchy" action
    // (Ctrl+J/View menu) and floating this into a separate top-level
    // window isn't a use case here - just keep it draggable within the
    // main window's dock areas.
    setFeatures(QDockWidget::DockWidgetMovable);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setIndentation(14);
    tree_->setUniformRowHeights(true);
    connect(tree_, &QTreeWidget::itemClicked, this, &HierarchyPanel::onItemClicked_);
    connect(tree_,
            &QTreeWidget::itemDoubleClicked,
            this,
            &HierarchyPanel::onItemDoubleClicked_);
    setWidget(tree_);

    rebuildTimer_ = new QTimer(this);
    rebuildTimer_->setSingleShot(true);
    rebuildTimer_->setInterval(150);
    connect(rebuildTimer_, &QTimer::timeout, this, &HierarchyPanel::refresh);

    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor& text = colorPreset[EPresetsColorIdx::kTextColor];
    const QColor& background = colorPreset[EPresetsColorIdx::kBackgroundColor];
    const QColor& border = colorPreset[EPresetsColorIdx::kBorderColor];
    const QColor& selection = colorPreset[EPresetsColorIdx::kSelectionColor];
    setStyleSheet(
        QStringLiteral("QTreeWidget {"
                      "  background-color: %1;"
                      "  color: %2;"
                      "  border: none;"
                      "}"
                      "QTreeWidget::item { padding: 3px 2px; }"
                      "QTreeWidget::item:selected { background-color: %3; }"
                      "QDockWidget::title {"
                      "  background-color: %1;"
                      "  color: %2;"
                      "  padding: 5px;"
                      "  border-bottom: 1px solid %4;"
                      "}")
            .arg(background.name(), text.name(), selection.name(), border.name()));
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

    // Every uid that's either a group's child or an attached note's own
    // uid is "consumed" - it gets added as a NESTED node from its
    // owner/anchor's own recursion below, not as a second, redundant
    // top-level root.
    QSet<QUuid> consumed;
    const QList<QGraphicsItem*> allItems = scene_->items();
    for (QGraphicsItem* item : allItems) {
        if (!scene_->itemAddByUser(item))
            continue;
        if (auto* group = dynamic_cast<GroupItem*>(item)) {
            for (const QUuid& childId : group->child_ids())
                consumed.insert(childId);
        }
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            if (!text->attachedToUid().isNull())
                consumed.insert(text->uid());
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
    if (auto* picture = dynamic_cast<PixmapItem*>(item)) { // GifItem IS-A PixmapItem
        for (QGraphicsItem* note : scene_->find_attached_notes(picture->uid()))
            addItemNode_(node, note, added);
    }
    if (auto* gif = dynamic_cast<GifItem*>(item))
        connectGifAnimation_(node, gif);
}

void HierarchyPanel::connectGifAnimation_(QTreeWidgetItem* node, GifItem* gif)
{
    // Max: "получается теперь можно и гифку проигрывать в иерархии?" -
    // yes, now that the panel's rebuild trigger is QUndoStack::
    // indexChanged rather than the animation-driven CanvasScene::
    // changed(), a per-node icon update on frameChanged is a targeted,
    // cheap operation (setIcon() on one row) instead of a full tree
    // rebuild every frame.
    auto colorPreset = SettingsHandler::getInstance()->getCurrentColorPreset();
    const QColor glyphColor = colorPreset[EPresetsColorIdx::kTextColor];
    QMetaObject::Connection conn =
        QObject::connect(gif->movie(), &QMovie::frameChanged, this, [node, gif, glyphColor](int) {
            node->setIcon(0, makePictureIcon(gif->pixmap(), glyphColor));
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
        label = picture->filename_.isEmpty() ? QObject::tr("Untitled")
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
    view_->fitRect(scene_->itemsBoundingRect(false, QList<QGraphicsItem*>{item}), item);
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
