#include "canvasview.h"
#include "canvasscene.h"
#include "commands.h"
#include "fileio.h"
#include "mainwindow.h"
#include "moveitem.h"
#include "project_settings.h"
#include "widgets/dialogs.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QUndoStack>
#include <QUrl>
#include <qdebug.h>
#include <cmath>

#include <core/settingshandler.h>


CanvasView::CanvasView(MainWindow& mw, QWidget* parent)
    : MainControlsMixin<CanvasView, QGraphicsView>()
    , mainwindow_(mw)
    , welcomeOverlay_(new WelcomeOverlay(this, &mw))
    , undoStack_(std::make_unique<QUndoStack>(this))
{
    // TODOLATER:
    controlTarget_ = this;
    setFrameShape(QFrame::NoFrame);
    setRenderHint(QPainter::Antialiasing, true);

    undoStack_->setUndoLimit(100);
    connect(undoStack_.get(), &QUndoStack::cleanChanged,   this, &CanvasView::on_undo_clean_changed);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setMouseTracking(true);

    scene_ = new CanvasScene(mw, zCounter_, undoStack_.get());
    connect(scene_, &CanvasScene::changed,          this, &CanvasView::on_scene_changed);
    connect(scene_, &CanvasScene::selectionChanged, this, &CanvasView::on_selection_changed);
    connect(scene_, &CanvasScene::cursor_changed,   this, &CanvasView::on_cursor_changed);
    connect(scene_, &CanvasScene::cursor_cleared,   this, &CanvasView::on_cursor_cleared);
    setScene(scene_);

    connect(SettingsHandler::getInstance(), &SettingsHandler::settingsChanged,
            this, &CanvasView::settingsChangedSlot);
    settingsChangedSlot();

    init_main_controls(&mw);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    viewport()->setMouseTracking(true);
}

CanvasView::~CanvasView()
{
    delete scene_;
    delete welcomeOverlay_;
}

// ─── Scene slots ──────────────────────────────────────────────────────────────

void CanvasView::on_scene_changed()
{
    if (scene_->items().isEmpty()) {
        setTransform(QTransform());
        welcomeOverlay_->setFocus();
        clearFocus();
        welcomeOverlay_->show();
    } else {
        setFocus();
        welcomeOverlay_->clearFocus();
        welcomeOverlay_->hide();
    }
    recalcSceneRect();
}

void CanvasView::on_selection_changed()
{
    // TODOLATER: update grayscale action checked state from selected item
    viewport()->repaint();
}

void CanvasView::on_context_menu(const QPoint& point)
{
    mainwindow_.context_menu()->exec(mapToGlobal(point));
}

void CanvasView::on_cursor_changed(QCursor cursor)
{
    if (activeMode_ == ModeNone)
        viewport()->setCursor(cursor);
}

void CanvasView::on_cursor_cleared()
{
    if (activeMode_ == ModeNone)
        viewport()->unsetCursor();
}

void CanvasView::on_undo_clean_changed(bool /*clean*/)
{
    // TODOLATER: update window title
}

void CanvasView::settingsChangedSlot()
{
    auto* settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    canvasColor_ = colorPreset[EPresetsColorIdx::kCanvasColor];
    borderColor_  = colorPreset[EPresetsColorIdx::kBorderColor];
    currentOpacity_ = settings->getCurrentOpacity();
}

// ─── Active modes ─────────────────────────────────────────────────────────────

void CanvasView::cancelSampleColorMode()
{
    activeMode_ = ModeNone;
    viewport()->unsetCursor();
    // TODOLATER: hide/delete sample_color_widget
}

void CanvasView::cancelActiveModes()
{
    scene_->cancel_active_modes();
    cancelSampleColorMode();
    activeMode_ = ModeNone;
}

// ─── View geometry ────────────────────────────────────────────────────────────

QPointF CanvasView::getViewCenter() const
{
    return QPointF(qRound(size().width() / 2.0), qRound(size().height() / 2.0));
}

void CanvasView::recalcSceneRect()
{
    if (previousTransform_)
        return;

    QRectF itemsRect = scene_->itemsBoundingRect();
    if (itemsRect.isEmpty())
        return;

    QPoint topleft = mapFromScene(itemsRect.topLeft());
    topleft = mapToScene(QPoint(topleft.x() - size().width(),
                                topleft.y() - size().height())).toPoint();
    QPoint bottomright = mapFromScene(itemsRect.bottomRight());
    bottomright = mapToScene(QPoint(bottomright.x() + size().width(),
                                    bottomright.y() + size().height())).toPoint();
    setSceneRect(QRectF(topleft, bottomright));
}

void CanvasView::resetPreviousTransform(QGraphicsItem* toggleItem)
{
    if (previousTransform_ && previousTransform_->toggleItem != toggleItem)
    {
        // TODOLATER: std::optional ???
        previousTransform_.reset();
    }
}

void CanvasView::fitRect(const QRectF& rect, QGraphicsItem* toggleItem)
{
    if (toggleItem && previousTransform_) {
        setTransform(previousTransform_->transform);
        centerOn(previousTransform_->center);
        previousTransform_.reset();
        return;
    }
    if (toggleItem) {
        previousTransform_ = std::make_unique<PreviousTransform>();
        previousTransform_->toggleItem = toggleItem;
        previousTransform_->transform = QTransform(transform());
        previousTransform_->center = mapToScene(getViewCenter().toPoint());
    } else {
        previousTransform_.reset();
    }
    fitInView(rect, Qt::KeepAspectRatio);
    recalcSceneRect();
    fitInView(rect, Qt::KeepAspectRatio);
}

// ─── Zoom / pan ───────────────────────────────────────────────────────────────

void CanvasView::doScale(qreal sx, qreal sy)
{
    QGraphicsView::scale(sx, sy);
    scene_->on_view_scale_change();
    recalcSceneRect();
}

double CanvasView::getZoomSize(std::function<double(double, double)> func) const
{
    QRectF rect = scene_->itemsBoundingRect();
    QPoint tl = mapFromScene(rect.topLeft());
    QPoint br = mapFromScene(rect.bottomRight());
    return func(double(br.x() - tl.x()), double(br.y() - tl.y()));
}

void CanvasView::zoom(double delta, QPointF anchor)
{
    if (scene_->items().isEmpty())
        return;

    QPoint anchorPt(qRound(anchor.x()), qRound(anchor.y()));
    QPointF refPoint = mapToScene(anchorPt);

    if (delta == 0.0)
        return;

    double factor = 1.0 + std::abs(delta / 1000.0);
    if (delta > 0) {
        if (getZoomSize([](double w, double h) { return std::max(w, h); }) < 10000000.0)
            doScale(factor, factor);
        else
            return;
    } else {
        if (getZoomSize([](double w, double h) { return std::min(w, h); }) > 50.0)
            doScale(1.0 / factor, 1.0 / factor);
        else
            return;
    }

    pan(QPointF(mapFromScene(refPoint)) - QPointF(anchorPt));
    resetPreviousTransform();
}

void CanvasView::pan(QPointF delta)
{
    if (scene_->items().isEmpty())
        return;
    horizontalScrollBar()->setValue(qRound(horizontalScrollBar()->value() + delta.x()));
    verticalScrollBar()->setValue(qRound(verticalScrollBar()->value() + delta.y()));
}

// ─── Event handlers ───────────────────────────────────────────────────────────

void CanvasView::wheelEvent(QWheelEvent* event)
{
    auto match = KeyboardSettings().mousewheelActionForEvent(event);
    if (!match)
        return;

    double delta = event->angleDelta().y();
    if (match->inverted)
        delta = -delta;

    if (match->group == QLatin1String("zoom")) {
        zoom(delta, event->position());
        event->accept();
    } else if (match->group == QLatin1String("pan_horizontal")) {
        pan(QPointF(0.0, 0.5 * delta));
        event->accept();
    } else if (match->group == QLatin1String("pan_vertical")) {
        pan(QPointF(0.5 * delta, 0.0));
        event->accept();
    }
}

void CanvasView::mousePressEvent(QMouseEvent* event)
{
    if (mousePressEventMainControls(event))
        return;

    if (activeMode_ == ModeSampleColor) {
        if (event->button() == Qt::LeftButton) {
            // TODOLATER: sample color at click position and copy to clipboard
        }
        cancelSampleColorMode();
        event->accept();
        return;
    }

    auto match = KeyboardSettings().mouseActionForEvent(event);
    if (match) {
        if (match->group == QLatin1String("zoom")) {
            activeMode_    = ModeZoom;
            eventStart_    = event->position();
            eventAnchor_   = event->position();
            eventInverted_ = match->inverted;
            event->accept();
            return;
        }
        if (match->group == QLatin1String("pan")) {
            activeMode_ = ModePan;
            eventStart_ = event->position();
            viewport()->setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }

    QGraphicsView::mousePressEvent(event);
}

void CanvasView::mouseMoveEvent(QMouseEvent* event)
{
    if (activeMode_ == ModePan) {
        resetPreviousTransform();
        QPointF pos = event->position();
        pan(eventStart_ - pos);
        eventStart_ = pos;
        event->accept();
        return;
    }

    if (activeMode_ == ModeZoom) {
        resetPreviousTransform();
        QPointF pos = event->position();
        double delta = (eventStart_ - pos).y();
        if (eventInverted_)
            delta *= -1;
        eventStart_ = pos;
        zoom(delta * 20.0, eventAnchor_);
        event->accept();
        return;
    }

    if (activeMode_ == ModeSampleColor) {
        // TODOLATER: update sample_color_widget position and color
        event->accept();
        return;
    }

    if (mouseMoveEventMainControls(event))
        return;
    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    if (activeMode_ == ModePan) {
        viewport()->unsetCursor();
        activeMode_ = ModeNone;
        event->accept();
        return;
    }
    if (activeMode_ == ModeZoom) {
        activeMode_ = ModeNone;
        event->accept();
        return;
    }
    if (mouseReleaseEventMainControls(event))
        return;
    QGraphicsView::mouseReleaseEvent(event);
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QGraphicsView::mouseDoubleClickEvent(event);
}

void CanvasView::keyPressEvent(QKeyEvent* event)
{
    if (keyPressEventMainControls(event))
        return;
    if (activeMode_ == ModeSampleColor) {
        cancelSampleColorMode();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void CanvasView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    recalcSceneRect();
    welcomeOverlay_->resize(size());
}


void CanvasView::drawBackground(QPainter* painter, const QRectF& rect)
{
    qreal opacity = qreal(currentOpacity_) / 255.0;
    painter->setOpacity(opacity);
    setCacheMode(CacheNone);
    painter->save();
    scene_->setBackgroundBrush(QBrush(canvasColor_));
    painter->fillRect(scene_->sceneRect(), scene_->backgroundBrush());
    painter->setPen(QPen(borderColor_, 2));
    painter->drawRect(scene_->sceneRect());
    painter->restore();
}

void CanvasView::dropEvent(QDropEvent* event)
{
    QPoint pos(qRound(event->position().x()), qRound(event->position().y()));
    handleDrop(event->mimeData(), pos);
    event->acceptProposedAction();
}

void CanvasView::handleDrop(const QMimeData* mimedata, const QPoint& pos)
{
    qDebug() << "CanvasView::Handling file drop:" << mimedata->formats();

    if (mimedata->hasUrls()) {
        qDebug() << "Found dropped urls:" << mimedata->urls();
        if (scene_->items().isEmpty()) {
            // TODOLATER:
        }
        this->do_insert_images(mimedata->urls(), pos);
    } else if (mimedata->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimedata->imageData());
        if (!img.isNull()) {
            // TODOLATER: create PixmapItem and insert via InsertItems command
            qDebug() << "Image drop not yet implemented";
        }
    } else {
        qDebug() << "Drop not an image";
    }
}

// ─── File actions ─────────────────────────────────────────────────────────────

void CanvasView::on_action_save()
{
    cancelActiveModes();
    mainwindow_.fileActions().saveFile();
}

void CanvasView::on_action_save_as()
{
    cancelActiveModes();
    mainwindow_.fileActions().saveFileAs();
}

void CanvasView::on_action_export_scene()
{
    // TODOLATER: export scene to image file
}

void CanvasView::on_action_export_images()
{
    // TODOLATER: export individual images to directory
}

// ─── Edit actions ─────────────────────────────────────────────────────────────

void CanvasView::on_action_undo()
{
    cancelActiveModes();
    undoStack_->undo();
}

void CanvasView::on_action_redo()
{
    cancelActiveModes();
    undoStack_->redo();
}

void CanvasView::on_action_select_all()
{
    scene_->select_all_items();
}

void CanvasView::on_action_deselect_all()
{
    scene_->deselect_all_items();
}

void CanvasView::on_action_delete_items()
{
    cancelActiveModes();
    undoStack_->push(new DeleteItemsCommand(scene_, scene_->selectedItems(true)));
}

void CanvasView::on_action_cut()
{
    on_action_copy();
    undoStack_->push(new DeleteItemsCommand(scene_, scene_->selectedItems(true)));
}

void CanvasView::on_action_copy()
{
    qDebug() << "Copying to clipboard...";
    cancelActiveModes();
    QClipboard* clipboard = QApplication::clipboard();
    QList<QGraphicsItem*> items = scene_->selectedItems(true);
    if (items.isEmpty()) {
        return;
    }

    // At the moment, we can only copy one image to the global
    // clipboard. (Later, we might create an image of the whole
    // selection for external copying.)
    if (auto* pixmapItem = dynamic_cast<PixmapItem*>(items.first())) {
        pixmapItem->copy_to_clipboard(clipboard);
    } else if (auto* textItem = dynamic_cast<TextItem*>(items.first())) {
        textItem->copy_to_clipboard(clipboard);
    }

    // However, we can copy all items to the internal clipboard:
    scene_->copy_selection_to_internal_clipboard();

    // We set a marker for ourselves in the clipboard so that we know to
    // look up the internal clipboard when pasting. copy_to_clipboard()
    // above already replaced the clipboard's QMimeData via
    // setPixmap()/setText(), and QClipboard::mimeData() only returns a
    // const snapshot, so the marker has to go in via a fresh QMimeData
    // that also carries over whatever was just set.
    auto* mimeData = new QMimeData();
    for (const QString& format : clipboard->mimeData()->formats()) {
        mimeData->setData(format, clipboard->mimeData()->data(format));
    }
    mimeData->setData(QStringLiteral("familiar/items"),
                      QByteArray::number(items.size()));
    clipboard->setMimeData(mimeData);
}

void CanvasView::on_action_paste()
{
    cancelActiveModes();
    qDebug() << "Pasting from clipboard...";
    QClipboard* clipboard = QApplication::clipboard();
    QPoint pos = mapFromGlobal(cursor().pos());

    // See if we need to look up the internal clipboard:
    QByteArray marker
        = clipboard->mimeData()->data(QStringLiteral("familiar/items"));
    qDebug() << "Custom data in clipboard:" << marker;
    if (!marker.isEmpty() && !scene_->internal_clipboard.isEmpty()) {
        // Checking that the internal clipboard exists since the user
        // may have opened a new scene since copying.
        scene_->paste_from_internal_clipboard(mapToScene(pos));
        return;
    }

    // A file copied in a file manager (e.g. Nautilus) puts a list of
    // file:// URLs on the clipboard rather than actual image data - load
    // it the same way as drag-and-dropped/inserted images, instead of
    // falling through to pasting the raw path as text below.
    if (clipboard->mimeData()->hasUrls()) {
        do_insert_images(clipboard->mimeData()->urls(), pos);
        return;
    }

    QImage img = clipboard->image();
    if (!img.isNull()) {
        bool wasEmpty = scene_->items().isEmpty();
        auto* item = new PixmapItem(img);
        undoStack_->push(new InsertItemsCommand(
            scene_, QList<IBaseItem*>{item}, mapToScene(pos)));
        if (wasEmpty) {
            // This is the first image in the scene
            on_action_fit_scene();
        }
        return;
    }

    QString text = clipboard->text();
    if (!text.isEmpty()) {
        auto* item = new TextItem(text);
        item->setScale(1.0 / get_scale());
        undoStack_->push(new InsertItemsCommand(
            scene_, QList<IBaseItem*>{item}, mapToScene(pos)));
        return;
    }

    // TODOLATER: user-facing notification (Python shows a BeeNotification)
    qDebug() << "No image data or text in clipboard or image too big";
}

void CanvasView::on_action_raise_to_top()
{
    scene_->raise_to_top();
}

void CanvasView::on_action_lower_to_bottom()
{
    scene_->lower_to_bottom();
}

// ─── View actions ─────────────────────────────────────────────────────────────

void CanvasView::on_action_fit_scene()
{
    fitRect(scene_->itemsBoundingRect());
}

void CanvasView::on_action_fit_selection()
{
    fitRect(scene_->itemsBoundingRect(true));
}

void CanvasView::on_action_show_scrollbars(bool checked)
{
    if (checked) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

// ─── Insert actions ───────────────────────────────────────────────────────────

void CanvasView::on_action_insert_images()
{
    cancelActiveModes();
    QStringList filenames = QFileDialog::getOpenFileNames(
        this,
        "Select one or more images to open",
        QString(),
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff *.tif)");
    if (filenames.isEmpty())
        return;
    QList<QUrl> urls;
    urls.reserve(filenames.size());
    for (const QString& fn : filenames)
        urls.append(QUrl::fromLocalFile(fn));
    do_insert_images(urls);
}

void CanvasView::on_action_insert_text()
{
    cancelActiveModes();
    // TODOLATER: create TextItem and insert via InsertItems command
}

// ─── Transform actions ────────────────────────────────────────────────────────

void CanvasView::on_action_crop()
{
    scene_->crop_items();
}

void CanvasView::on_action_flip_horizontally()
{
    scene_->flip_items(false);
}

void CanvasView::on_action_flip_vertically()
{
    scene_->flip_items(true);
}

void CanvasView::on_action_reset_scale()
{
    cancelActiveModes();
    undoStack_->push(new ResetScaleCommand(scene_->selectedItems(true)));
}

void CanvasView::on_action_reset_rotation()
{
    cancelActiveModes();
    undoStack_->push(new ResetRotationCommand(scene_->selectedItems(true)));
}

void CanvasView::on_action_reset_flip()
{
    cancelActiveModes();
    undoStack_->push(new ResetFlipCommand(scene_->selectedItems(true)));
}

void CanvasView::on_action_reset_crop()
{
    cancelActiveModes();
    // TODOLATER: filter selectedItems to PixmapItem* for ResetCropCommand
}

void CanvasView::on_action_reset_transforms()
{
    cancelActiveModes();
    // TODOLATER: cast selectedItems to IBaseItem* for ResetTransformsCommand
}

// ─── Normalize actions ────────────────────────────────────────────────────────

void CanvasView::on_action_normalize_height()
{
    scene_->normalize_height();
}

void CanvasView::on_action_normalize_width()
{
    scene_->normalize_width();
}

void CanvasView::on_action_normalize_size()
{
    scene_->normalize_size();
}

// ─── Arrange actions ──────────────────────────────────────────────────────────

void CanvasView::on_action_arrange_optimal()
{
    scene_->arrange_optimal();
}

void CanvasView::on_action_arrange_horizontal()
{
    scene_->arrange(false);
}

void CanvasView::on_action_arrange_vertical()
{
    scene_->arrange(true);
}

void CanvasView::on_action_arrange_square()
{
    // TODOLATER: implement arrange_square in CanvasScene
}

// ─── Image actions ────────────────────────────────────────────────────────────

void CanvasView::on_action_change_opacity()
{
    // TODOLATER: open ChangeOpacityDialog
}

void CanvasView::on_action_grayscale(bool /*checked*/)
{
    // TODOLATER: push ToggleGrayscaleCommand for selected PixmapItems
}

void CanvasView::on_action_show_color_gamut()
{
    // TODOLATER: open GamutDialog
}

void CanvasView::on_action_sample_color()
{
    cancelActiveModes();
    viewport()->setCursor(Qt::CrossCursor);
    activeMode_ = ModeSampleColor;
    // TODOLATER: show SampleColorWidget
}

// ─── Project helpers (existing interface) ────────────────────────────────────

void CanvasView::setProjectSettings(project_settings* ps)
{
    scene_->setProjectSettings(ps);
}

QByteArray CanvasView::fml_payload()
{
    return scene_->fml_payload();
}

void CanvasView::on_insert_images_finished(const QString& /*filename*/,
                                           const QStringList& errors)
{
    qDebug() << "Insert images finished";
    insertImagesInsertedItems_ += scene_->add_queued_items();

    QStringList allErrors = insertImagesImmediateErrors_ + errors;
    if (!allErrors.isEmpty()) {
        QStringList names;
        for (const QString& fn : allErrors)
            names.append(QStringLiteral("<li>%1</li>").arg(fn));
        QMessageBox::warning(
            this,
            tr("Problem loading images"),
            tr("%1 image(s) could not be opened.<br/>"
               "Unknown format or too big?<ul>%2</ul>")
                .arg(allErrors.size())
                .arg(names.join(QStringLiteral("\n"))));
    }

    if (!insertImagesInsertedItems_.isEmpty()) {
        undoStack_->push(new InsertItemsCommand(
            scene_, insertImagesInsertedItems_, QPointF(), true));
        scene_->arrange_default();
    }
    undoStack_->endMacro();

    if (insertImagesNewScene_) {
        on_action_fit_scene();
    }
}

void CanvasView::do_insert_images(const QList<QUrl>& urls, std::optional<QPoint> pos)
{
    QPoint insertPos = pos.value_or(getViewCenter().toPoint());
    QPointF scenePos = mapToScene(insertPos);

    insertImagesNewScene_ = scene_->items().isEmpty();
    insertImagesImmediateErrors_.clear();
    insertImagesInsertedItems_.clear();

    scene_->deselect_all_items();
    undoStack_->beginMacro(tr("Insert Images"));

    QStringList filenames;
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) {
            // TODOLATER: remote URLs via ImageDownloader
            qDebug() << "Remote URL not yet supported:" << url;
            insertImagesImmediateErrors_.append(url.toString());
            continue;
        }
        filenames.append(url.toLocalFile());
    }

    CanvasScene* scene = scene_;
    auto* worker = new ThreadedIO([filenames, scenePos, scene](ThreadedIO* w) {
        load_images(filenames, scenePos, scene, w);
    });

    connect(worker, &ThreadedIO::progress, this, &CanvasView::on_items_loaded);
    connect(worker, &ThreadedIO::finished, this,
            &CanvasView::on_insert_images_finished);

    // QThread's own finished() (not ThreadedIO's same-named result signal)
    // only fires once the thread has actually stopped running, which is
    // the documented-safe point to delete a QThread object.
    // connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QThread::finished, this, [worker]() {
        qDebug() << "Thread finished. Scheduling deleteLater for" 
                 << "Thread ID:" << (worker->objectName().isEmpty() 
                    ? "unnamed" : worker->objectName());
        
        worker->deleteLater();
        
        qDebug() << "deleteLater() called for worker" << worker;
    });

    new ProgressDialog(tr("Loading images"), worker, 0, this);

    worker->start();
}

void CanvasView::on_items_loaded(int /*value*/)
{
    qDebug() << "On items loaded: add queued items";
    insertImagesInsertedItems_ += scene_->add_queued_items();
}

void CanvasView::addImage(const QString& /*path*/, QPointF /*point*/) {}
void CanvasView::addImage(QImage* /*img*/, QPointF /*point*/) {}
void CanvasView::addImage(QByteArray /*ba*/, int /*w*/, int /*h*/,
                           QRect /*br*/, qsizetype /*bpl*/, QImage::Format /*f*/) {}

void CanvasView::cleanupWorkplace()
{
    scene_->cleanupWorkplace();
}

QString CanvasView::path()          { return scene_->path(); }
void CanvasView::setPath(const QString& path) { scene_->setPath(path); }
QString CanvasView::projectName()   { return scene_->projectName(); }
void CanvasView::setProjectName(const QString& pn) { scene_->setProjectName(pn); }
bool CanvasView::isModified()       { return scene_->isModified(); }
void CanvasView::setModified(bool mod) { scene_->setModified(mod); }
bool CanvasView::isUntitled()       { return scene_->isUntitled(); }
