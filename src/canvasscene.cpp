#include "canvasscene.h"
#include "debug_macros.h"
#include "mainwindow.h"
#include "moveitem.h"

#include <QApplication>
#include <QClipboard>
#include <QGraphicsItem>
#include <QKeyEvent>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainter>
#include <QPen>

#include "Logger.h"

#include "project_settings.h"
#include "selector.h"
#include <core/settings.h>
#include <regex>

#include "commands.h"
#include <QUndoStack>
#include <qassert.h>

#include "core/settings.h"
#include <algorithm>
#include <cmath>

extern Logger logger;

namespace {

QString itemFilename(QGraphicsItem* item)
{
    if (auto* pixmapItem = dynamic_cast<PixmapItem*>(item)) {
        return pixmapItem->filename_;
    }
    return QString();
}

std::optional<int> itemSaveId(QGraphicsItem* item)
{
    if (auto* pixmapItem = dynamic_cast<PixmapItem*>(item)) {
        return pixmapItem->save_id;
    }
    if (auto* textItem = dynamic_cast<TextItem*>(item)) {
        return textItem->save_id;
    }
    return std::nullopt;
}

QList<QGraphicsItem*> sort_by_filename(const QList<QGraphicsItem*>& items)
{
    QList<QGraphicsItem*> byFilename;
    QList<QGraphicsItem*> bySaveId;
    QList<QGraphicsItem*> remaining;

    for (QGraphicsItem* item : items) {
        if (!itemFilename(item).isEmpty()) {
            byFilename.append(item);
        } else if (itemSaveId(item)) {
            bySaveId.append(item);
        } else {
            remaining.append(item);
        }
    }

    std::sort(byFilename.begin(),
              byFilename.end(),
              [](QGraphicsItem* a, QGraphicsItem* b) {
                  return itemFilename(a) < itemFilename(b);
              });
    std::sort(bySaveId.begin(),
              bySaveId.end(),
              [](QGraphicsItem* a, QGraphicsItem* b) {
                  return *itemSaveId(a) < *itemSaveId(b);
              });

    return byFilename + bySaveId + remaining;
}

} // namespace

CanvasScene::CanvasScene(MainWindow& mw,
                         uint64_t& zc,
                         QUndoStack* undoStack,
                         QGraphicsScene* scene)
    : undo_stack_(undoStack)
    , mainwindow_(mw)
    , zCounter_(zc)
{
    settings = FamSettings::getInstance();
    connect(this,
            &CanvasScene::selectionChanged,
            this,
            &CanvasScene::on_selection_change);
    connect(this, &CanvasScene::changed, this, &CanvasScene::on_change);
    (void) scene;
    clear();
    clear_ongoing = false;

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &CanvasScene::settingsChangedSlot);
    settingsChangedSlot();

    imgdownloader_ = new ImageDownloader(*this);

    connect(QApplication::clipboard(),
            &QClipboard::dataChanged,
            this,
            &CanvasScene::clipboardChanged);
}

CanvasScene::~CanvasScene()
{
    // Disconnect everything involving this before ~QGraphicsScene() runs:
    // it deletes remaining items, which can fire selectionChanged()/
    // changed(). By then this object's own (CanvasScene) destructor has
    // already run, so Qt asserts ("class destructor may have already
    // run") if it tries to invoke a slot declared on this class.
    disconnect(this, nullptr, nullptr, nullptr);
    disconnect(SettingsHandler::getInstance(), nullptr, this, nullptr);
    disconnect(QApplication::clipboard(), nullptr, this, nullptr);
    delete projectSettings_;
}

void CanvasScene::clear()
{
    clear_ongoing = true;

    // rubberband_item_/multiselect_item_ are our own long-lived helper
    // items (not user content); on repeat calls (e.g. "New Scene") the
    // previous instances would otherwise leak when overwritten below.
    // Detach first if still in the scene so QGraphicsScene::clear()
    // below doesn't also try to delete them (double free).
    if (rubberband_item_) {
        if (rubberband_item_->scene()) {
            removeItem(rubberband_item_);
        }
        delete rubberband_item_;
    }
    if (multiselect_item_) {
        if (multiselect_item_->scene()) {
            removeItem(multiselect_item_);
        }
        delete multiselect_item_;
    }

    QGraphicsScene::clear();
    internal_clipboard.clear();
    rubberband_item_ = new RubberbandItem();
    multiselect_item_ = new MultiSelectItem();
    clear_ongoing = false;
}

void CanvasScene::addItem(QGraphicsItem* item)
{
    qDebug() << "Adding item" << item;
    QGraphicsScene::addItem(item);
}

void CanvasScene::removeItem(QGraphicsItem* item)
{
    qDebug() << "Removing item" << item;
    QGraphicsScene::removeItem(item);
}

void CanvasScene::cancel_active_modes()
{
    cancel_crop_mode();
    end_rubberband_mode();
}

void CanvasScene::end_rubberband_mode()
{
    Q_ASSERT_X(rubberband_item_,
               "end_rubberband_mode",
               "rubberband_item_ == null!");
    if (rubberband_item_->scene()) {
        qDebug() << "End rubberband mode";
        removeItem(rubberband_item_);
    }
    active_mode_ = kNone;
}

void CanvasScene::cancel_crop_mode()
{
    if (crop_item) {
        qDebug() << "End crop mode";
        crop_item->exit_crop_mode(false);
    }
}

void CanvasScene::copy_selection_to_internal_clipboard()
{
    internal_clipboard.clear();
    for (QGraphicsItem* item : selectedItems(true)) {
        internal_clipboard.append(dynamic_cast<IBaseItem*>(item));
    }
}

void CanvasScene::paste_from_internal_clipboard(QPointF position)
{
    QList<IBaseItem*> copies;
    for (IBaseItem* item : internal_clipboard) {
        IBaseItem* copy = item->create_copy();
        copies.append(copy);
    }

    undo_stack_->push(new InsertItemsCommand(this, copies, position));
}

void CanvasScene::raise_to_top()
{
    cancel_active_modes();
    QList<QGraphicsItem*> items = selectedItems(true);
    std::vector<double> z_values;
    z_values.reserve(items.size());
    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(z_values),
                   [](const auto& i) { return i->zValue(); });
    double min_z_value = *std::min_element(z_values.begin(), z_values.end());
    double delta = max_z + Z_STEP - min_z_value;
    qDebug() << "Raise to top, delta: " << delta;
    for (auto& item : items) {
        dynamic_cast<IBaseItem*>(item)->set_z_value(item->zValue() + delta);
    }
}

void CanvasScene::lower_to_bottom()
{
    cancel_active_modes();
    QList<QGraphicsItem*> items = selectedItems(true);
    std::vector<double> z_values;
    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(z_values),
                   [](const auto& i) { return i->zValue(); });
    double max_z_value = *std::max_element(z_values.begin(), z_values.end());
    double delta = min_z - Z_STEP - max_z_value;
    qDebug() << "Lower to bottom, delta: " << delta;
    for (auto& item : items) {
        dynamic_cast<IBaseItem*>(item)->set_z_value(item->zValue() + delta);
    }
}

void CanvasScene::normalize_width_or_height(const QString& mode)
{
    cancel_active_modes();
    QList<qreal> values;
    QList<QGraphicsItem*> items = selectedItems(true);
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        values.append(mode == "width" ? rect.width() : rect.height());
    }
    if (values.size() < 2)
        return;
    qreal avg = std::accumulate(values.constBegin(), values.constEnd(), 0.0)
                / values.size();
    qDebug() << "Calculated average" << mode << avg;

    QList<qreal> scaleFactors;
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        scaleFactors.append(avg
                            / (mode == "width" ? rect.width() : rect.height()));
    }

    undo_stack_->push(new NormalizeItemsCommand(items, scaleFactors));
}

void CanvasScene::normalize_height()
{
    normalize_width_or_height("height");
}

void CanvasScene::normalize_width()
{
    normalize_width_or_height("width");
}

void CanvasScene::normalize_size()
{
    cancel_crop_mode();
    QList<qreal> sizes;
    QList<QGraphicsItem*> items = selectedItems(true);
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        sizes.append(rect.width() * rect.height());
    }
    if (sizes.size() < 2)
        return;
    qreal avg = std::accumulate(sizes.constBegin(), sizes.constEnd(), 0.0)
                / sizes.size();
    qDebug() << "Calculated average size" << avg;

    QList<qreal> scaleFactors;
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        scaleFactors.append(std::sqrt(avg / (rect.width() * rect.height())));
    }

    undo_stack_->push(new NormalizeItemsCommand(items, scaleFactors));
}

void CanvasScene::arrange_default()
{
    const QString mode
        = settings->valueOrDefault(QStringLiteral("Items/arrange_default"))
              .toString();
    if (mode == QLatin1String("horizontal"))
        arrange(false);
    else if (mode == QLatin1String("vertical"))
        arrange(true);
    else if (mode == QLatin1String("square"))
        arrange_square();
    else // "optimal"
        arrange_optimal();
}
// ============================================================================
// Rectangle Packer для arrange_optimal
// ============================================================================

class RectPacker
{
public:
    struct Size
    {
        int width;
        int height;
    };

    struct Position
    {
        int x;
        int y;
    };

    struct Shelf
    {
        int x = 0;
        int y = 0;
        int maxHeight = 0;
    };

    struct FreeRect
    {
        int x, y, width, height;
    };

    static QList<Position> pack(const QList<Size>& sizes,
                                int maxWidth,
                                int maxHeight,
                                int& outWidth,
                                int& outHeight)
    {
        QList<Position> positions;
        QList<FreeRect> freeRects;
        freeRects.append({0, 0, maxWidth, maxHeight});

        for (const auto& size : sizes) {
            Position pos;
            if (!packRect(size.width, size.height, freeRects, pos)) {
                return QList<Position>(); // Packing impossible
            }
            positions.append(pos);
        }

        // Calculate bounding box
        outWidth = 0;
        outHeight = 0;
        for (int i = 0; i < positions.size(); ++i) {
            outWidth = std::max(outWidth, positions[i].x + sizes[i].width);
            outHeight = std::max(outHeight, positions[i].y + sizes[i].height);
        }

        return positions;
    }

private:
    static bool packRect(int width,
                         int height,
                         QList<FreeRect>& freeRects,
                         Position& pos)
    {
        // Find best free rectangle
        int bestIndex = -1;
        int bestArea = INT_MAX;

        for (int i = 0; i < freeRects.size(); ++i) {
            const auto& fr = freeRects[i];
            if (fr.width >= width && fr.height >= height) {
                int area = fr.width * fr.height;
                if (area < bestArea) {
                    bestArea = area;
                    bestIndex = i;
                }
            }
        }

        if (bestIndex == -1) {
            return false;
        }

        FreeRect fr = freeRects[bestIndex];
        pos.x = fr.x;
        pos.y = fr.y;

        // Split free rectangle
        if (width < fr.width) {
            freeRects.append({fr.x + width, fr.y, fr.width - width, height});
        }
        if (height < fr.height) {
            freeRects.append(
                {fr.x, fr.y + height, fr.width, fr.height - height});
        }

        // Remove used free rectangle
        freeRects.removeAt(bestIndex);

        return true;
    }
};
void CanvasScene::arrange(bool vertical)
{
    cancel_active_modes();

    QList<QGraphicsItem*> items = selectedItems(true);
    if (items.size() < 2)
        return;

    qreal gap = FamSettings()
                    .valueOrDefault(QStringLiteral("Items/arrange_gap"))
                    .toReal();
    QPointF center = get_selection_center();
    QList<QPointF> positions;

    // Структура для хранения прямоугольника и элемента
    struct ItemRect
    {
        QRectF rect;
        QGraphicsItem* item;
    };

    QList<ItemRect> rects;
    for (QGraphicsItem* item : items) {
        rects.append(
            {itemsBoundingRect(false, QList<QGraphicsItem*>{item}), item});
    }

    if (vertical) {
        // Сортировка по вертикали (y)
        std::sort(rects.begin(),
                  rects.end(),
                  [](const ItemRect& a, const ItemRect& b) {
                      return a.rect.topLeft().y() < b.rect.topLeft().y();
                  });

        qreal sum_height = 0;
        for (const auto& r : rects) {
            sum_height += r.rect.height();
        }

        qreal y = std::round(center.y() - sum_height / 2);
        for (const auto& rect : rects) {
            positions.append(
                QPointF(std::round(center.x() - rect.rect.width() / 2), y));
            y += rect.rect.height() + gap;
        }
    } else {
        // Сортировка по горизонтали (x)
        std::sort(rects.begin(),
                  rects.end(),
                  [](const ItemRect& a, const ItemRect& b) {
                      return a.rect.topLeft().x() < b.rect.topLeft().x();
                  });

        qreal sum_width = 0;
        for (const auto& r : rects) {
            sum_width += r.rect.width();
        }

        qreal x = std::round(center.x() - sum_width / 2);
        for (const auto& rect : rects) {
            positions.append(
                QPointF(x, std::round(center.y() - rect.rect.height() / 2)));
            x += rect.rect.width() + gap;
        }
    }

    // Извлекаем отсортированные элементы
    QList<QGraphicsItem*> sortedItems;
    for (const auto& r : rects) {
        sortedItems.append(r.item);
    }

    undo_stack_->push(new ArrangeItemsCommand(this, sortedItems, positions));
}
void CanvasScene::arrange_optimal()
{
    cancel_active_modes();

    QList<QGraphicsItem*> items = selectedItems(true);
    if (items.size() < 2)
        return;

    qreal gap = FamSettings()
                    .valueOrDefault(QStringLiteral("Items/arrange_gap"))
                    .toReal();

    // Получаем размеры элементов
    QList<RectPacker::Size> sizes;
    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        sizes.append({static_cast<int>(std::round(rect.width() + gap)),
                      static_cast<int>(std::round(rect.height() + gap))});
    }

    QPointF center = get_selection_center();

    // Минимальная площадь элементов
    int minArea = 0;
    for (const auto& size : sizes) {
        minArea += size.width * size.height;
    }

    int width = static_cast<int>(std::ceil(std::sqrt(minArea)));

    QList<RectPacker::Position> positions;
    int boundsWidth = 0, boundsHeight = 0;

    // Пробуем увеличить размер контейнера пока не получится упаковать
    while (positions.isEmpty()) {
        positions
            = RectPacker::pack(sizes, width, width, boundsWidth, boundsHeight);
        if (positions.isEmpty()) {
            width = static_cast<int>(std::ceil(width * 1.2));
        }
    }

    // Центрируем элементы вокруг центра выделения
    QPointF diff(center.x() - boundsWidth / 2.0,
                 center.y() - boundsHeight / 2.0);

    QList<QPointF> scenePositions;
    for (const auto& pos : positions) {
        scenePositions.append(QPointF(pos.x + diff.x(), pos.y + diff.y()));
    }

    undo_stack_->push(new ArrangeItemsCommand(this, items, scenePositions));
}

void CanvasScene::arrange_square()
{
    cancel_active_modes();
    qreal maxWidth = 0;
    qreal maxHeight = 0;
    qreal gap = FamSettings()
                    .valueOrDefault(QStringLiteral("Items/arrange_gap"))
                    .toReal();
    QList<QGraphicsItem*> items = sort_by_filename(selectedItems(true));

    if (items.size() < 2)
        return;

    for (QGraphicsItem* item : items) {
        QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
        maxWidth = std::max(maxWidth, rect.width() + gap);
        maxHeight = std::max(maxHeight, rect.height() + gap);
    }

    // We want the items to center around the selection's center,
    // not (0, 0)
    int numRows = static_cast<int>(std::ceil(std::sqrt(items.size())));
    QPointF center = get_selection_center();
    QPointF diff = center - (numRows / 2.0) * QPointF(maxWidth, maxHeight);

    QList<QPointF> positions;
    auto it = items.constBegin();
    for (int j = 0; j < numRows && it != items.constEnd(); ++j) {
        for (int i = 0; i < numRows && it != items.constEnd(); ++i) {
            QGraphicsItem* item = *it;
            ++it;
            QRectF rect = itemsBoundingRect(false, QList<QGraphicsItem*>{item});
            QPointF point(i * maxWidth + (maxWidth - rect.width()) / 2.0,
                          j * maxHeight + (maxHeight - rect.height()) / 2.0);
            positions.append(point + diff);
        }
    }

    undo_stack_->push(new ArrangeItemsCommand(this, items, positions));
}

void CanvasScene::flip_items(bool vertical)
{
    cancel_active_modes();
    undo_stack_->push(new FlipItemsCommand(selectedItems(true),
                                           get_selection_center(),
                                           vertical));
}

void CanvasScene::crop_items()
{
    if (crop_item)
        return;
    if (has_single_image_selection()) {
        IBaseItem* item = dynamic_cast<IBaseItem*>(selectedItems(true).first());
        Q_ASSERT_X(item, "CanvasScene::crop_items", "item == null");
        if (item->is_image()) {
            item->enter_crop_mode();
        }
    }
}

QColor CanvasScene::sample_color_at(const QPointF& position)
{
    auto* itemAtPos = itemAt(position, views().first()->transform());
    if (itemAtPos) {
        if (auto* pixmapItem = dynamic_cast<PixmapItem*>(itemAtPos)) {
            return pixmapItem->sample_color_at(position);
        }
    }
    return QColor();
}

void CanvasScene::select_all_items()
{
    cancel_active_modes();
    QPainterPath path;
    path.addRect(itemsBoundingRect());
    setSelectionArea(path);
}

void CanvasScene::deselect_all_items()
{
    cancel_active_modes();
    clearSelection();
}

bool CanvasScene::has_selection()
{
    return !(selectedItems(true).isEmpty());
}

bool CanvasScene::has_single_selection()
{
    return selectedItems(true).size() == 1;
}

bool CanvasScene::has_multi_selection()
{
    return selectedItems(true).size() > 1;
}

bool CanvasScene::has_single_image_selection()
{
    if (has_single_selection()) {
        auto* item = dynamic_cast<IBaseItem*>(selectedItems(true).first());
        Q_ASSERT_X(item,
                   "CanvasScene::has_single_image_selection",
                   "item == null");
        return item->is_image();
    }
    return false;
}

void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        event_start = event->scenePos();
        auto* item_at_pos = itemAt(event_start, views().first()->transform());

        if (edit_item) {
            if (item_at_pos != edit_item)
                edit_item->exit_edit_mode();
            else {
                QGraphicsScene::mousePressEvent(event);
                return;
            }
        }

        if (crop_item) {
            if (item_at_pos != crop_item)
                cancel_crop_mode();
            else {
                QGraphicsScene::mousePressEvent(event);
                return;
            }
        }

        if (item_at_pos) {
            active_mode_ = kMoveMode;
        } else if (!items().isEmpty()) {
            active_mode_ = kRubberbandMode;
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void CanvasScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
    cancel_active_modes();
    auto* item = itemAt(event->scenePos(), views().first()->transform());
    if (item) {
        if (!item->isSelected()) {
            item->setSelected(true);
        }
        if (dynamic_cast<IBaseItem*>(item)->is_editable()) {
            ((TextItem*) item)->enter_edit_mode();
            // TODOLATER:
            QGraphicsScene::mousePressEvent(event);
        } else {
            CanvasView* view = static_cast<CanvasView*>(views().first());
            view->fitRect(itemsBoundingRect(false, QList<QGraphicsItem*>{item}),
                          item);
        }
        return;
    }

    QGraphicsScene::mouseDoubleClickEvent(event);
}

void CanvasScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (active_mode_ == kRubberbandMode) {
        if (!rubberband_item_->scene()) {
            qDebug() << "Activating rubberband selection";
            addItem(rubberband_item_);
        }
        rubberband_item_->fit(event_start, event->scenePos());
        setSelectionArea(rubberband_item_->shape());
        // Re-assert on top *after* setSelectionArea: selecting an item
        // here can itself trigger ItemMixin::on_selected_change(), which
        // brings that item to front too - without this, the first image
        // touched by the drag would end up with a higher z-value than
        // the rubberband and visually cover it.
        rubberband_item_->bring_to_front();
        CanvasView* view = static_cast<CanvasView*>(views().first());
        Q_ASSERT_X(view, "CanvasScene::mouseMoveEvent", "view == null");
        view->resetPreviousTransform();
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (active_mode_ == kRubberbandMode) {
        end_rubberband_mode();
    }
    if (active_mode_ == kMoveMode && has_selection()
        && !multiselect_item_->is_action_active()
        && !dynamic_cast<IBaseItem*>(selectedItems().first())
                ->is_action_active()) {
        auto delta = event->scenePos() - event_start;
        if (!delta.isNull()) {
            undo_stack_->push(
                new MoveItemsByCommand(selectedItems(), delta, true));
        }
    }
    // Reset after the base call, not before (unlike Python's exact
    // ordering): Qt defers a ctrl+click deselect-toggle on an
    // already-selected item to this base mouseReleaseEvent call, and
    // ItemMixin::on_selected_change() needs active_mode() to still read
    // kMoveMode when that fires.
    QGraphicsScene::mouseReleaseEvent(event);
    active_mode_ = kNone;
}

QList<QGraphicsItem*> CanvasScene::selectedItems(bool userOnly) const
{
    // If user_only is set to true, only return items added by the user
    // (i.e. no multi select outlines and other UI items). Python checks
    // hasattr(i, 'save_id'); itemAddByUser() is the C++ stand-in for that
    // (only PixmapItem/TextItem carry a save_id member).
    QList<QGraphicsItem*> items = QGraphicsScene::selectedItems();
    if (userOnly) {
        QList<QGraphicsItem*> userItems;
        for (QGraphicsItem* item : items) {
            if (itemAddByUser(item))
                userItems.append(item);
        }
        return userItems;
    }
    return items;
}

QList<QGraphicsItem*> CanvasScene::items_by_type(const std::string& type)
{
    QList<QGraphicsItem*> itemsl;
    for (QGraphicsItem* item : items()) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        if (baseItem && baseItem->get_type() == type)
            itemsl.append(item);
    }
    return itemsl;
}

QList<QGraphicsItem*> CanvasScene::items_for_save()
{
    QList<QGraphicsItem*> userItems;
    for (QGraphicsItem* item : items(Qt::AscendingOrder)) {
        if (itemAddByUser(item))
            userItems.append(item);
    }

    return userItems;
}

void CanvasScene::clear_save_ids()
{
    for (QGraphicsItem* item : items_for_save()) {
        dynamic_cast<IBaseItem*>(item)->clear_save_id();
    }
}

void CanvasScene::on_view_scale_change()
{
    for (QGraphicsItem* item : selectedItems()) {
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item))
            baseItem->on_view_scale_change();
    }
}

QRectF CanvasScene::itemsBoundingRect(bool selectionOnly,
                                      QList<QGraphicsItem*> items_in) const
{
    auto filterUserItems =
        [this](const QList<QGraphicsItem*>& itemList) -> QList<QGraphicsItem*> {
        QList<QGraphicsItem*> userItems;
        for (QGraphicsItem* item : itemList) {
            if (this->itemAddByUser(item))
                userItems.append(item);
        }
        return userItems;
    };

    QList<QGraphicsItem*> base;

    if (selectionOnly) {
        base = filterUserItems(selectedItems());
    } else if (!items_in.isEmpty()) {
        base = items_in;
    } else {
        base = filterUserItems(items());
    }

    if (base.isEmpty()) {
        return QRectF(0, 0, 0, 0);
    }

    QList<qreal> x;
    QList<qreal> y;

    for (QGraphicsItem* item : base) {
        IBaseItem* baseItem = dynamic_cast<IBaseItem*>(item);
        Q_ASSERT_X(baseItem,
                   "CanvasScene::itemsBoundingRect",
                   "item is not an IBaseItem");
        QVector<QPointF> corners = baseItem->corners_scene_coords();
        for (const QPointF& corner : corners) {
            x.append(corner.x());
            y.append(corner.y());
        }
    }

    qreal minX = *std::min_element(x.constBegin(), x.constEnd());
    qreal maxX = *std::max_element(x.constBegin(), x.constEnd());
    qreal minY = *std::min_element(y.constBegin(), y.constEnd());
    qreal maxY = *std::max_element(y.constBegin(), y.constEnd());

    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

QPointF CanvasScene::get_selection_center()
{
    auto rect = itemsBoundingRect(true);
    return (rect.topLeft() + rect.bottomRight()) / 2;
}

void CanvasScene::on_selection_change()
{
    if (clear_ongoing) {
        return;
    }

    if (has_multi_selection()) {
        multiselect_item_->fit_selection_area(itemsBoundingRect(true));
    }

    if (has_multi_selection() && !multiselect_item_->scene()) {
        addItem(multiselect_item_);
        multiselect_item_->bring_to_front();
    }

    if (!has_multi_selection() && multiselect_item_->scene()) {
        removeItem(multiselect_item_);
    }
}

void CanvasScene::on_change()
{
    // Ignore events while clearing the scene since the
    // multiselect item will get cleared, too
    if (clear_ongoing) {
        return;
    }

    if (multiselect_item_->scene() && !multiselect_item_->is_action_active()) {
        multiselect_item_->fit_selection_area(itemsBoundingRect(true));
    }
}

void CanvasScene::add_item_later(const QVariantMap& itemdata, bool selected)
{
    QMutexLocker locker(&itemsToAddMutex_);
    items_to_add.push({itemdata, selected});
}

QList<IBaseItem*> CanvasScene::add_queued_items()
{
    QList<IBaseItem*> addedItems;

    while (true) {
        QueuedItemData queuedData;
        {
            QMutexLocker locker(&itemsToAddMutex_);
            if (items_to_add.empty()) {
                break;
            }
            queuedData = items_to_add.front();
            items_to_add.pop();
        }

        QVariantMap data = queuedData.data;
        bool selected = queuedData.selected;

        // Get item type from data
        QString typ = data.value("type").toString();
        data.remove("type");

        // Create item based on type
        QGraphicsItem* item = nullptr;
        if (typ == "pixmap") {
            // For pixmap items, we need an image - this should be provided in data
            QVariant imageVariant = data.value("image");
            if (imageVariant.isValid()) {
                QImage image = imageVariant.value<QImage>();
                QString filename = data.value("filename").toString();
                PixmapItem* pixmapItem = new PixmapItem(image, filename);

                // Handle extra data (crop, etc.)
                QVariant extraData = data.value("data");
                if (extraData.isValid()) {
                    QVariantMap extraMap = extraData.toMap();

                    // Handle crop data if present
                    QVariant cropVariant = extraMap.value("crop");
                    if (cropVariant.isValid()) {
                        QList<QVariant> cropList = cropVariant.toList();
                        if (cropList.size() == 4) {
                            QRectF crop(cropList[0].toReal(),
                                        cropList[1].toReal(),
                                        cropList[2].toReal(),
                                        cropList[3].toReal());
                            pixmapItem->set_crop(crop);
                        }
                    }
                }
                item = pixmapItem;
            }
        } else if (typ == "text") {
            QString text = data.value("data").toMap().value("text").toString();
            if (text.isEmpty()) {
                text = "Text";
            }
            TextItem* textItem = new TextItem();
            textItem->setPlainText(text);
            item = textItem;
        } else {
            // Unknown type - create text item with warning message
            qWarning() << "Encountered item of unknown type:" << typ;
            TextItem* textItem = new TextItem();
            textItem->setPlainText(QString("Item of unknown type: %1").arg(typ));
            item = textItem;
        }

        if (item) {
            // Apply common item properties from data
            IBaseItem* baseItem = dynamic_cast<IBaseItem*>(item);
            if (baseItem) {
                // Set position
                qreal x = data.value("x", 0.0).toReal();
                qreal y = data.value("y", 0.0).toReal();
                item->setPos(x, y);

                // Set z-value
                qreal z = data.value("z", 0.0).toReal();
                item->setZValue(z);

                // Set scale
                qreal scale = data.value("scale", 1.0).toReal();
                item->setScale(scale);

                // Set rotation
                qreal rotation = data.value("rotation", 0.0).toReal();
                item->setRotation(rotation);

                // Handle flip
                int flip = data.value("flip", 1).toInt();
                if (flip == -1) {
                    baseItem->do_flip();
                }

                addItem(item);

                // Force recalculation of min/max z values
                item->setZValue(item->zValue());

                if (selected) {
                    item->setSelected(true);
                    baseItem->bring_to_front();
                }

                addedItems.append(baseItem);
            }
        }
    }

    return addedItems;
}

CanvasScene::ESceneMode CanvasScene::active_mode() const
{
    return active_mode_;
}

bool CanvasScene::itemAddByUser(QGraphicsItem* item) const
{
    auto* baseItem = dynamic_cast<IBaseItem*>(item);
    if (!baseItem) {
        return false;
    }
    const std::string type = baseItem->get_type();
    return type == "pixmap" || type == "text";
}

// ============================================================================
// Legacy
// ============================================================================

void CanvasScene::keyPressEvent(QKeyEvent* event) {}

void CanvasScene::copyToClipboard()
{
    // TODOLATER: copy selected items to clipboard
}

void CanvasScene::pasteFromClipboard()
{
    // TODOLATER: paste items from clipboard
}

QByteArray CanvasScene::fml_payload()
{
    // TODOLATER: fml atchive
    QByteArray arr;
    return arr;
}

void CanvasScene::setProjectSettings(project_settings* ps)
{
    projectSettings_ = ps;
}

void CanvasScene::cleanupWorkplace()
{

}

QString CanvasScene::path()
{
    return projectSettings_->path();
}

void CanvasScene::setPath(const QString& path)
{
    projectSettings_->path(path);
}

QString CanvasScene::projectName()
{
    return projectSettings_->projectName();
}

void CanvasScene::setProjectName(const QString& pn)
{
    projectSettings_->projectName(pn);
}

bool CanvasScene::isModified()
{
    return projectSettings_->modified();
}

void CanvasScene::setModified(bool mod)
{
    projectSettings_->modified(mod);
}

bool CanvasScene::isUntitled()
{
    return projectSettings_->isDefaultProjectName();
}


void CanvasScene::handleHtmlFromClipboard(const QString& html)
{
    // TODOLATER:
    std::regex r("<img[^>]*src=['|\"](.*?)['|\"].*?>");
    std::smatch results;
    auto str = html.toStdString();
    std::sregex_iterator it(std::begin(str), std::end(str), r);
    std::sregex_iterator end;

    if (it != end) {
        imgdownloader_->download(QString::fromStdString((*it)[1].str()), {0, 0});
    } else {
        LOG_WARNING(logger, "[UI]:::CANNOT DISPLAY DATA.");
    }
}

void CanvasScene::settingsChangedSlot()
{
    auto settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    selectionColor_ = colorPreset[EPresetsColorIdx::kSelectionColor];
}

void CanvasScene::clipboardChanged()
{
    mainwindow_.clearClipboardItems();
}

qint16 CanvasScene::objectsCount() const
{
    // TODO: type.h header with all types (image, textline, multitextline)
    const int targetObjectType = 3;
    return std::count_if(items().begin(),
                         items().end(),
                         [targetObjectType](const QGraphicsItem* item) {
                             return item->type() == targetObjectType;
                         });
}
