#include "canvasview.h"
#include "canvasscene.h"
#include "commands.h"
#include "export.h"
#include "fileio.h"
#include "mainwindow.h"
#include "moveitem.h"
#include "project_settings.h"
#include "widgets/color_gamut.h"
#include "widgets/dialogs.h"
#include <cmath>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QRegularExpression>
#include <QUndoStack>
#include <QUrl>
#include <QVariantAnimation>

#include <core/settingshandler.h>

#include "log/log.h"
using namespace familiar::log;


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
    connect(undoStack_.get(),
            &QUndoStack::cleanChanged,
            this,
            &CanvasView::on_undo_clean_changed);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setMouseTracking(true);

    scene_ = new CanvasScene(mw, zCounter_, undoStack_.get());
    connect(scene_, &CanvasScene::changed, this, &CanvasView::on_scene_changed);
    connect(scene_,
            &CanvasScene::selectionChanged,
            this,
            &CanvasView::on_selection_changed);
    connect(scene_,
            &CanvasScene::cursor_changed,
            this,
            &CanvasView::on_cursor_changed);
    connect(scene_,
            &CanvasScene::cursor_cleared,
            this,
            &CanvasView::on_cursor_cleared);
    setScene(scene_);

    connect(SettingsHandler::getInstance(),
            &SettingsHandler::settingsChanged,
            this,
            &CanvasView::settingsChangedSlot);
    settingsChangedSlot();

    init_main_controls(&mw);
    setContextMenuPolicy(Qt::DefaultContextMenu);
    viewport()->setMouseTracking(true);

    // See selectionOutlineOpacity(): fades selection chrome out after
    // this window loses activation and the cursor isn't hovering it
    // either. Values chosen to feel like a quick "peek" fade, not a
    // proper design pass - revisit if it feels off.
    selectionFadeAnim_ = new QVariantAnimation(this);
    selectionFadeAnim_->setStartValue(1.0);
    selectionFadeAnim_->setEndValue(0.0);
    selectionFadeAnim_->setDuration(600);
    connect(selectionFadeAnim_,
            &QVariantAnimation::valueChanged,
            this,
            [this](const QVariant& value) {
                selectionOutlineOpacity_ = value.toReal();
                viewport()->update();
            });
}

CanvasView::~CanvasView()
{
    // undoStack_.reset() below (via QUndoStack::clear() inside
    // ~QUndoStack()) fires cleanChanged(), and scene_'s own destruction
    // can fire changed()/selectionChanged() - both are still connected to
    // slots on this half-destroyed CanvasView at this point (Qt only
    // drops a connection once the receiving QObject's own destructor has
    // fully run, not partway through it). cleanChanged() in particular
    // reaches on_undo_clean_changed() -> setModified() ->
    // project_settings::modified() -> TabPane::setCurrentTabTitle(),
    // which touches the QTabWidget that's *also* mid-destruction right
    // now: this CanvasView is being destroyed as part of that same
    // QTabWidget's own teardown cascade (e.g. "close without saving"
    // closing every tab). Same class of bug as CanvasScene::
    // ~CanvasScene()'s disconnect() - fixed the same way.
    undoStack_->disconnect(this);
    scene_->disconnect(this);

    // undoStack_ must be destroyed before scene_: commands like
    // InsertItemsCommand hold item pointers and, in their own
    // destructor, check item->scene() to decide whether they still own
    // the item and must free it. If scene_ (and the items still attached
    // to it) were destroyed first, those checks - and the dynamic_cast
    // before them - would run on already-freed items.
    undoStack_.reset();
    delete scene_;
    delete welcomeOverlay_;
}

// ─── Scene slots ──────────────────────────────────────────────────────────────

void CanvasView::on_scene_changed()
{
    const QRectF currentItemsRect = scene_->itemsBoundingRect();
    if (!currentItemsRect.isEmpty()) {
        canvasRect_ = canvasRect_.united(currentItemsRect);
    } else if (!sceneEverHadItems_) {
        // Only wipe the drawn canvas frame for a tab that never had any
        // content. Once it has, going back to zero items - deleting the
        // last image, a Cut, whatever - keeps showing the frame: it's
        // still the same (modified) project, just emptied out, not a
        // fresh/untitled scene, so there's no reason to reset the view
        // or bring back the welcome/recent-files overlay for it either.
        canvasRect_ = QRectF();
    }

    if (scene_->items().isEmpty()) {
        if (!sceneEverHadItems_) {
            // Stays set across every redundant on_scene_changed() firing
            // while the scene remains empty (setTransform() below itself
            // re-triggers scene_->changed(), so this can fire several
            // times in a row for one Cut) - only cleared once real
            // content is back (the else branch), not "used up" on the
            // first call.
            if (!suppressNextEmptySceneReset_) {
                setTransform(QTransform());
            }
            welcomeOverlay_->setFocus();
            clearFocus();
            welcomeOverlay_->show();
        } else {
            // An existing (sceneEverHadItems_) project that's currently
            // empty - e.g. just loaded from a file saved with zero items
            // (see CanvasView::restoreCanvasRect()), which never took the
            // else branch below to hide this in the first place, since
            // add_queued_items() added nothing. Keep it hidden either way.
            setFocus();
            welcomeOverlay_->clearFocus();
            welcomeOverlay_->hide();
        }
    } else {
        sceneEverHadItems_ = true;
        suppressNextEmptySceneReset_ = false;
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

void CanvasView::on_undo_clean_changed(bool clean)
{
    setModified(!clean);
}

void CanvasView::settingsChangedSlot()
{
    auto* settings = SettingsHandler::getInstance();
    auto colorPreset = settings->getCurrentColorPreset();
    canvasColor_ = colorPreset[EPresetsColorIdx::kCanvasColor];
    borderColor_ = colorPreset[EPresetsColorIdx::kBorderColor];
    currentOpacity_ = settings->getCurrentOpacity();
}

// ─── Active modes ─────────────────────────────────────────────────────────────

void CanvasView::cancelSampleColorMode()
{
    activeMode_ = ModeNone;
    viewport()->unsetCursor();
    if (sampleColorWidget_) {
        delete sampleColorWidget_;
        sampleColorWidget_ = nullptr;
    }
    if (scene_->has_multi_selection()) {
        scene_->multiselect_item_->bring_to_front();
    }
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

    // Falls back to the remembered extent so the view's actual
    // navigable/scrollable sceneRect() still covers it when there are no
    // items - without this, fitInView(canvasRect_) in on_action_fit_scene()
    // computes a transform aiming at that area, but QGraphicsView clamps
    // scrolling back to whatever (tiny/default) sceneRect() Qt derives on
    // its own from zero items, so nothing ever actually scrolls there and
    // drawBackground() never gets asked to paint it.
    QRectF itemsRect = scene_->itemsBoundingRect();
    QRectF rect = itemsRect.isEmpty() ? canvasRect_ : itemsRect;
    if (rect.isEmpty())
        return;

    QPoint topleft = mapFromScene(rect.topLeft());
    topleft = mapToScene(QPoint(topleft.x() - size().width(),
                                topleft.y() - size().height()))
                  .toPoint();
    QPoint bottomright = mapFromScene(rect.bottomRight());
    bottomright = mapToScene(QPoint(bottomright.x() + size().width(),
                                    bottomright.y() + size().height()))
                      .toPoint();
    setSceneRect(QRectF(topleft, bottomright));
}

void CanvasView::resetPreviousTransform(QGraphicsItem* toggleItem)
{
    if (previousTransform_ && previousTransform_->toggleItem != toggleItem) {
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
    // Blocked only for a scene that's never had content (where the
    // welcome overlay is showing and eating wheel events anyway) - once
    // it has (sceneEverHadItems_), the user can still be looking at a
    // real, empty-for-now canvas and should be able to zoom/pan it, e.g.
    // to reposition before pasting something back in.
    if (scene_->items().isEmpty() && !sceneEverHadItems_)
        return;

    QPoint anchorPt(qRound(anchor.x()), qRound(anchor.y()));
    QPointF refPoint = mapToScene(anchorPt);

    if (delta == 0.0)
        return;

    double factor = 1.0 + std::abs(delta / 1000.0);
    // The min/max-size clamp below is measured against the items
    // themselves (getZoomSize() -> scene_->itemsBoundingRect()), so it's
    // meaningless with no items - an empty (but sceneEverHadItems_)
    // canvas has nothing to clamp against and should just zoom freely.
    bool hasItems = !scene_->items().isEmpty();
    if (delta > 0) {
        if (!hasItems || getZoomSize([](double w, double h) {
                             return std::max(w, h);
                         }) < 10000000.0)
            doScale(factor, factor);
        else
            return;
    } else {
        if (!hasItems || getZoomSize([](double w, double h) {
                             return std::min(w, h);
                         }) > 10.0)
            doScale(1.0 / factor, 1.0 / factor);
        else
            return;
    }

    pan(QPointF(mapFromScene(refPoint)) - QPointF(anchorPt));
    resetPreviousTransform();
}

void CanvasView::pan(QPointF delta)
{
    // See zoom() for why this only blocks a never-had-content scene.
    if (scene_->items().isEmpty() && !sceneEverHadItems_)
        return;
    horizontalScrollBar()->setValue(
        qRound(horizontalScrollBar()->value() + delta.x()));
    verticalScrollBar()->setValue(
        qRound(verticalScrollBar()->value() + delta.y()));
}

// ─── Event handlers ───────────────────────────────────────────────────────────

void CanvasView::wheelEvent(QWheelEvent* event)
{
    auto match = SettingsHandler::getInstance()->mousewheelActionForEvent(event);
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
            QColor color = scene_->sample_color_at(mapToScene(event->pos()));
            if (color.isValid()) {
                QString name = color.name();
                QApplication::clipboard()->setText(name);
                scene_->internal_clipboard.clear();
                FLOG_DEBUG(Ch::View, "Copied color to clipboard: {}", name);
                new FamNotification(this,
                                    QString("Copied color to clipboard: %1")
                                        .arg(name));
            } else {
                FLOG_DEBUG(Ch::View, "No color found");
            }
        }
        cancelSampleColorMode();
        event->accept();
        return;
    }

    auto match = SettingsHandler::getInstance()->mouseActionForEvent(event);
    if (match) {
        if (match->group == QLatin1String("zoom")) {
            activeMode_ = ModeZoom;
            eventStart_ = event->position();
            eventAnchor_ = event->position();
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
        sampleColorWidget_->update(event->position(),
                                   scene_->sample_color_at(
                                       mapToScene(event->pos())));
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
    // setTransformationAnchor(NoAnchor) (see the constructor) means Qt
    // keeps the viewport's top-left corner fixed in scene coordinates
    // across a resize, not the center - deliberately, so it doesn't
    // fight our own zoom-under-cursor math in zoom(). But that also
    // means a drastic resize (entering/exiting fullscreen) visibly
    // shifts previously-centered content toward the bottom-right edge.
    // Recompute where the OLD center was (scrollbar position/transform
    // haven't changed yet at this point, only the geometry) and re-center
    // on it once the resize is done, so content stays visually in place.
    QPointF oldCenter;
    if (!previousTransform_) {
        oldCenter = mapToScene(QPoint(event->oldSize().width() / 2,
                                      event->oldSize().height() / 2));
    }

    QGraphicsView::resizeEvent(event);
    recalcSceneRect();
    if (!previousTransform_) {
        centerOn(oldCenter);
    }
    welcomeOverlay_->resize(size());
}


void CanvasView::drawBackground(QPainter* painter, const QRectF& rect)
{
    qreal opacity = qreal(currentOpacity_) / 255.0;
    painter->setOpacity(opacity);
    setCacheMode(CacheNone);
    painter->save();
    scene_->setBackgroundBrush(QBrush(canvasColor_));
    // Not scene_->sceneRect(): QGraphicsScene's own sceneRect grows with
    // items but never shrinks back (documented Qt behavior), so after Cut
    // removed the only item it stayed stuck at the old bounds forever.
    // canvasRect_ (see on_scene_changed()) reimplements the same
    // grow-never-shrink look ourselves, with an explicit reset to empty
    // when the scene genuinely has zero items.
    static constexpr qreal kCanvasMargin = 10;
    const QRectF paddedCanvasRect = canvasRect_.isEmpty()
                                        ? canvasRect_
                                        : canvasRect_.marginsAdded(
                                              QMarginsF(kCanvasMargin,
                                                        kCanvasMargin,
                                                        kCanvasMargin,
                                                        kCanvasMargin));
    painter->fillRect(paddedCanvasRect, scene_->backgroundBrush());
    // Cosmetic: keeps the border a constant width in screen pixels
    // regardless of zoom, matching the selection outline's pen (see
    // SelectableMixin::paint_selectable() in selector.h) instead of
    // scaling up with the view like a plain scene-space pen would.
    QPen borderPen(borderColor_, 2);
    borderPen.setCosmetic(true);
    painter->setPen(borderPen);
    painter->drawRect(paddedCanvasRect);
    painter->restore();
}

namespace {

// Some browsers only put rendered HTML on the clipboard/drag data for a
// copied image (no text/uri-list, no raw image/* data) - pull the first
// <img> src out of it as a last resort before falling back to pasting
// the literal HTML source as plain text.
QString extract_first_img_src(const QString& html)
{
    static const QRegularExpression
        re(QStringLiteral("<img[^>]*\\ssrc=[\"']([^\"']*)[\"']"),
           QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(html);
    return match.hasMatch() ? match.captured(1) : QString();
}

} // namespace

void CanvasView::dropEvent(QDropEvent* event)
{
    QPoint pos(qRound(event->position().x()), qRound(event->position().y()));
    handleDrop(event->mimeData(), pos);
    event->acceptProposedAction();
}

void CanvasView::enterEvent(QEnterEvent* event)
{
    QGraphicsView::enterEvent(event);
    selectionOutlineHover_ = true;
    updateSelectionVisibility();
}

void CanvasView::leaveEvent(QEvent* event)
{
    QGraphicsView::leaveEvent(event);
    selectionOutlineHover_ = false;
    updateSelectionVisibility();
}

void CanvasView::updateSelectionVisibility()
{
    const bool visible = isActiveWindow() || selectionOutlineHover_;

    if (visible) {
        selectionFadeAnim_->stop();
        selectionOutlineOpacity_ = 1.0;
        viewport()->update();
    } else if (selectionFadeAnim_->state() != QAbstractAnimation::Running) {
        selectionFadeAnim_->setStartValue(selectionOutlineOpacity_);
        selectionFadeAnim_->start();
    }
}

void CanvasView::handleDrop(const QMimeData* mimedata, const QPoint& pos)
{
    FLOG_DEBUG(Ch::View,
               "CanvasView::Handling file drop: {}",
               debugString(mimedata->formats()));

    if (mimedata->hasUrls()) {
        FLOG_DEBUG(Ch::View,
                   "Found dropped urls: {}",
                   debugString(mimedata->urls()));

        QList<QUrl> urls = mimedata->urls();
        // Some sites' drag data doesn't give a direct image link at all -
        // Google Images gives its own search-result page ("imgres?...");
        // kp.ru (and presumably others) gives the containing article
        // page. Either way the actual target can be slow/wrong to fetch
        // (observed: a real site taking 15s+ to serve one image, or an
        // HTML page instead of image bytes), while the browser's own
        // rendered thumbnail is right there in text/html's <img src> -
        // prefer that, the same fast preview PureRef ends up using.
        // Only for a single-image drag: with several URLs dropped at
        // once there's no way to tell which one (if any) this one <img>
        // match corresponds to.
        QList<int> nonLocalIdx;
        for (int i = 0; i < urls.size(); ++i) {
            // Matches do_insert_images()'s own filter below: a second
            // uri-list entry with no scheme at all (e.g. Google's/kp.ru's
            // alt-text-as-a-"URL" quirk) isn't a real candidate either.
            if (!urls[i].isEmpty() && urls[i].isValid()
                && !urls[i].isLocalFile() && !urls[i].scheme().isEmpty()) {
                nonLocalIdx.append(i);
            }
        }
        if (nonLocalIdx.size() == 1 && mimedata->hasHtml()) {
            QString htmlSrc = extract_first_img_src(mimedata->html());
            if (!htmlSrc.isEmpty()) {
                QUrl htmlUrl(htmlSrc);
                FLOG_DEBUG(Ch::View,
                           "Preferring rendered thumbnail over dropped page "
                           "URL: {}",
                           htmlUrl);
                urls[nonLocalIdx.first()] = htmlUrl;
            }
        }

        if (scene_->items().isEmpty()) {
            // TODOLATER:
        }
        this->do_insert_images(urls, pos);
    } else if (mimedata->hasImage()) {
        QImage img = qvariant_cast<QImage>(mimedata->imageData());
        if (!img.isNull()) {
            // TODOLATER: create PixmapItem and insert via InsertItems command
            FLOG_DEBUG(Ch::View, "Image drop not yet implemented");
        }
    } else {
        FLOG_DEBUG(Ch::View, "Drop not an image");
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
    cancelActiveModes();

    if (scene_->itemsBoundingRect().isEmpty()) {
        showMessageBox(QMessageBox::Information,
                       &mainwindow_,
                       tr("Export Scene"),
                       tr("The scene is empty - nothing to export."));
        return;
    }

    QFileDialog* fileDialog = new QFileDialog(this);
    // See FileActions::openFile(): MainWindow's translucent/frameless
    // stylesheet cascades into this otherwise-unstyled top-level dialog,
    // painting it solid black instead.
    fileDialog->setAttribute(Qt::WA_TranslucentBackground, false);
    fileDialog->setStyleSheet("* { background-color: palette(window); color: "
                              "palette(window-text); }");
    fileDialog->setWindowTitle(tr("Export Scene to Image"));
    fileDialog->setNameFilter(tr("Image Files (*.png *.jpg *.jpeg *.svg)"
                                 ";;PNG (*.png)"
                                 ";;JPEG (*.jpg *.jpeg)"
                                 ";;SVG (*.svg)"));
    fileDialog->setDirectory(
        path().isEmpty() ? QDir::homePath() : QFileInfo(path()).absolutePath());
    // Native file dialogs have hung on this Qt build in the past - see
    // FileActions' dialogs for the same workaround.
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptSave);
    fileDialog->setFileMode(QFileDialog::AnyFile);

    if (fileDialog->exec() != QDialog::Accepted
        || fileDialog->selectedFiles().isEmpty()) {
        delete fileDialog;
        return;
    }
    QString filename = fileDialog->selectedFiles().first();
    delete fileDialog;

    if (QFileInfo(filename).suffix().isEmpty()) {
        filename += QStringLiteral(".png");
    }

    sceneExporter_ = createSceneExporter(QFileInfo(filename).suffix(), scene_);
    if (!sceneExporter_->getUserInput(this)) {
        sceneExporter_.reset();
        return;
    }

    auto* worker = new ThreadedIO([this, filename](ThreadedIO* w) {
        sceneExporter_->exportTo(filename, w);
    });
    connect(worker,
            &ThreadedIO::finished,
            this,
            &CanvasView::on_export_scene_finished);
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    new ProgressDialog(tr("Exporting %1").arg(filename), worker, 0, this);
    worker->start();
}

void CanvasView::on_export_scene_finished(const QString& filename,
                                          const QStringList& errors)
{
    sceneExporter_.reset();
    if (!errors.isEmpty()) {
        showMessageBox(QMessageBox::Warning,
                       &mainwindow_,
                       tr("Problem writing file"),
                       tr("<p>Problem writing file %1</p><p>%2</p>")
                           .arg(filename, errors.join(QStringLiteral("<br/>"))));
    }
}

void CanvasView::on_action_export_images()
{
    cancelActiveModes();

    if (scene_->items_by_type("pixmap").isEmpty()) {
        showMessageBox(QMessageBox::Information,
                       &mainwindow_,
                       tr("Export Images"),
                       tr("There are no images to export."));
        return;
    }

    QFileDialog* fileDialog = new QFileDialog(this);
    // See FileActions::openFile(): MainWindow's translucent/frameless
    // stylesheet cascades into this otherwise-unstyled top-level dialog,
    // painting it solid black instead.
    fileDialog->setAttribute(Qt::WA_TranslucentBackground, false);
    fileDialog->setStyleSheet("* { background-color: palette(window); color: "
                              "palette(window-text); }");
    fileDialog->setWindowTitle(tr("Export Images"));
    fileDialog->setDirectory(
        path().isEmpty() ? QDir::homePath() : QFileInfo(path()).absolutePath());
    // Native file dialogs have hung on this Qt build in the past - see
    // FileActions' dialogs for the same workaround.
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setFileMode(QFileDialog::Directory);
    fileDialog->setOption(QFileDialog::ShowDirsOnly, true);

    if (fileDialog->exec() != QDialog::Accepted
        || fileDialog->selectedFiles().isEmpty()) {
        delete fileDialog;
        return;
    }
    QString directory = fileDialog->selectedFiles().first();
    delete fileDialog;

    imagesExporter_ = std::make_unique<ImagesToDirectoryExporter>(scene_,
                                                                  directory);

    imageExportWorker_ = new ThreadedIO(
        [this](ThreadedIO* w) { imagesExporter_->exportTo(w); });
    connect(imageExportWorker_,
            &ThreadedIO::userInputRequired,
            this,
            &CanvasView::on_export_images_file_exists);
    connect(imageExportWorker_,
            &ThreadedIO::finished,
            this,
            &CanvasView::on_export_images_finished);
    // Deliberately not deleteLater-on-QThread::finished here (unlike
    // loadFmlIntoCurrentTab's worker): QThread::finished() also fires on
    // every pause-for-conflict-resolution, and the same worker gets
    // start()ed again on resume (see on_export_images_file_exists()).
    // Cleanup happens once, in on_export_images_finished().

    new ProgressDialog(tr("Exporting to %1").arg(directory),
                       imageExportWorker_,
                       0,
                       this);
    imageExportWorker_->start();
}

void CanvasView::on_export_images_file_exists(const QString& filename)
{
    ExportImagesFileExistsDialog dlg(this, filename);
    if (dlg.exec() == QDialog::Accepted) {
        imagesExporter_->setHandleExisting(dlg.getAnswer());
        new ProgressDialog(tr("Exporting to %1").arg(imagesExporter_->dirname()),
                           imageExportWorker_,
                           0,
                           this);
        imageExportWorker_->start();
    } else {
        imageExportWorker_->deleteLater();
        imageExportWorker_ = nullptr;
        imagesExporter_.reset();
    }
}

void CanvasView::on_export_images_finished(const QString& dirname,
                                           const QStringList& errors)
{
    Q_UNUSED(dirname)
    if (!errors.isEmpty()) {
        showMessageBox(QMessageBox::Warning,
                       &mainwindow_,
                       tr("Problem writing file"),
                       tr("<p>Problem writing files</p><p>%1</p>")
                           .arg(errors.join(QStringLiteral("<br/>"))));
    }
    if (imageExportWorker_) {
        imageExportWorker_->deleteLater();
        imageExportWorker_ = nullptr;
    }
    imagesExporter_.reset();
}

// ─── Edit actions ─────────────────────────────────────────────────────────────

void CanvasView::on_action_undo()
{
    cancelActiveModes();
    // Mirrors the suppression on_action_delete_items()/on_action_cut()
    // set for the initial action: undoing an InsertItemsCommand can
    // empty the scene the same way a delete does, and on_scene_changed()
    // would otherwise jarringly reset the zoom/pan for what's often just
    // one step in a longer undo/redo sequence.
    if (dynamic_cast<const InsertItemsCommand*>(
            undoStack_->command(undoStack_->index() - 1))) {
        suppressNextEmptySceneReset_ = true;
    }
    undoStack_->undo();
}

void CanvasView::on_action_redo()
{
    cancelActiveModes();
    // See on_action_undo(): redoing a DeleteItemsCommand re-empties the
    // scene the same way the original delete/cut action did, but that
    // action's own suppressNextEmptySceneReset_ was already consumed by
    // the first empty-scene transition - without this, a second delete
    // via redo resets the transform, leaving it unfitted if the scene
    // then gets un-emptied again by a later undo.
    if (dynamic_cast<const DeleteItemsCommand*>(
            undoStack_->command(undoStack_->index()))) {
        suppressNextEmptySceneReset_ = true;
    }
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
    // Dropping items can include whatever previousTransform_->toggleItem
    // points to (set by double-click zoom-to-fit); left dangling, it
    // would permanently block recalcSceneRect()'s early-return guard.
    resetPreviousTransform();
    // Same as on_action_cut(): don't snap the zoom back to identity for
    // what might just be a transient empty scene - e.g. Undo bringing
    // the deleted item(s) straight back. suppressNextEmptySceneReset_
    // stays armed until the scene is non-empty again (on_scene_changed()),
    // however that happens, so this covers undo just as well as paste.
    suppressNextEmptySceneReset_ = true;
    undoStack_->push(
        new DeleteItemsCommand(scene_, scene_->selectedItems(true)));
}

void CanvasView::on_action_cut()
{
    on_action_copy();
    suppressNextEmptySceneReset_ = true;
    resetPreviousTransform();
    undoStack_->push(
        new DeleteItemsCommand(scene_, scene_->selectedItems(true)));
}

void CanvasView::on_action_copy()
{
    FLOG_DEBUG(Ch::View, "Copying to clipboard...");
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
    FLOG_DEBUG(Ch::View, "Pasting from clipboard...");
    QClipboard* clipboard = QApplication::clipboard();
    QPoint pos = mapFromGlobal(cursor().pos());

    // See if we need to look up the internal clipboard:
    QByteArray marker = clipboard->mimeData()->data(
        QStringLiteral("familiar/items"));
    FLOG_DEBUG(Ch::View, "Custom data in clipboard: {}", debugString(marker));
    if (!marker.isEmpty() && !scene_->internal_clipboard.isEmpty()) {
        // Checking that the internal clipboard exists since the user
        // may have opened a new scene since copying.
        bool wasEmpty = scene_->items().isEmpty();
        scene_->paste_from_internal_clipboard(mapToScene(pos));
        if (wasEmpty) {
            // First items in this scene
            // paste_from_internal_clipboard, since it's single-scene and
            // can never hit this: there's nowhere to have copied from
            // otherwise. Our internal clipboard is shared across tabs, so
            // copy-on-tab-A/paste-into-fresh-tab-B is a real path here.
            on_action_fit_scene();
        }
        return;
    }

    // Raw pixel data, when present, is unambiguous ground truth - prefer
    // it over any URL representation, which can point to something we
    // can't actually fetch (e.g. web apps like Telegram Web/Discord Web
    // copy images as a browser-internal "blob:" URL, resolvable only
    // inside that page's own JS context, alongside the real image bytes
    // under image/* - grabbing that directly sidesteps the unusable URL
    // entirely rather than trying and failing to "download" it).
    QImage img = clipboard->image();
    if (!img.isNull()) {
        bool wasEmpty = scene_->items().isEmpty();
        auto* item = new PixmapItem(img);
        undoStack_->push(new InsertItemsCommand(scene_,
                                                QList<IBaseItem*>{item},
                                                mapToScene(pos)));
        if (wasEmpty) {
            // This is the first image in the scene
            on_action_fit_scene();
        }
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

    // Last resort before falling through to plain text: some browsers
    // (e.g. copying an image from certain pages) only put rendered HTML
    // on the clipboard, with neither a text/uri-list nor raw image/*
    // data - pull the <img> src out and load it the same way as a
    // dropped URL (handles a plain http(s) link, an embedded data: URI,
    // or a Google Images redirect - see fileio.cpp's load_images()).
    if (clipboard->mimeData()->hasHtml()) {
        QString src = extract_first_img_src(clipboard->mimeData()->html());
        if (!src.isEmpty()) {
            do_insert_images(QList<QUrl>{QUrl(src)}, pos);
            return;
        }
    }

    QString text = clipboard->text();
    if (!text.isEmpty()) {
        auto* item = new TextItem(text);
        item->setScale(1.0 / get_scale());
        undoStack_->push(new InsertItemsCommand(scene_,
                                                QList<IBaseItem*>{item},
                                                mapToScene(pos)));
        return;
    }

    // TODOLATER: user-facing notification (Python shows a BeeNotification)
    FLOG_DEBUG(Ch::View, "No image data or text in clipboard or image too big");
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
    // Falls back to the remembered (possibly item-less) extent so
    // fitting still does something sensible right after loading a
    // project that was saved with zero items - QGraphicsView::
    // fitInView() silently no-ops on a null rect, so this is a no-op
    // itself for a scene that's genuinely never had any content either.
    QRectF rect = scene_->itemsBoundingRect();
    fitRect(rect.isEmpty() ? canvasRect_ : rect);
}

void CanvasView::on_action_fit_selection()
{
    fitRect(scene_->itemsBoundingRect(true));
}

// ─── Insert actions ───────────────────────────────────────────────────────────

QString CanvasView::getSupportedImageFormats() const
{
    QStringList formats;
    for (const QByteArray& f : QImageReader::supportedImageFormats()) {
        formats << QStringLiteral("*.%1").arg(QString::fromLatin1(f));
    }
    return formats.join(QStringLiteral(" "));
}

void CanvasView::on_action_insert_images()
{
    cancelActiveModes();
    const QString formats = getSupportedImageFormats();
    FLOG_DEBUG(Ch::View, "Supported image types for reading: {}", formats);

    QFileDialog* fileDialog = new QFileDialog(&mainwindow_);
    // See FileActions::openFile()/saveFileAs() for why this is needed:
    // MainWindow is a translucent/frameless overlay (transparent
    // stylesheet + WA_TranslucentBackground); as a separate top-level
    // window without its own alpha channel, this dialog would otherwise
    // inherit "background: transparent" and paint solid black, with an
    // empty competing stylesheet not being enough to cancel it out.
    fileDialog->setAttribute(Qt::WA_TranslucentBackground, false);
    fileDialog->setStyleSheet("* { background-color: palette(window); color: "
                              "palette(window-text); }");
    fileDialog->setWindowTitle(tr("Select one or more images to open"));
    fileDialog->setNameFilter(tr("Images (%1)").arg(formats));
    fileDialog->setOption(QFileDialog::DontUseNativeDialog, true);
    fileDialog->setAcceptMode(QFileDialog::AcceptMode::AcceptOpen);
    fileDialog->setFileMode(QFileDialog::ExistingFiles);
    fileDialog->resize(800, 500);

    QStringList filenames;
    if (fileDialog->exec()) {
        filenames = fileDialog->selectedFiles();
    }
    delete fileDialog;
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
    auto* item = new TextItem();
    QPointF pos = mapToScene(mapFromGlobal(cursor().pos()));
    item->setScale(1.0 / get_scale());
    undoStack_->push(
        new InsertItemsCommand(scene_, QList<IBaseItem*>{item}, pos));
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
    undoStack_->push(new ResetScaleCommand(scene_->selectedItems(true),
                                           scene_->get_selection_center()));
}

void CanvasView::on_action_reset_rotation()
{
    cancelActiveModes();
    undoStack_->push(new ResetRotationCommand(scene_->selectedItems(true),
                                              scene_->get_selection_center()));
}

void CanvasView::on_action_reset_flip()
{
    cancelActiveModes();
    undoStack_->push(new ResetFlipCommand(scene_->selectedItems(true),
                                          scene_->get_selection_center()));
}

void CanvasView::on_action_reset_crop()
{
    cancelActiveModes();
    QList<IBaseItem*> items;
    for (QGraphicsItem* item : scene_->selectedItems(true)) {
        items.append(dynamic_cast<IBaseItem*>(item));
    }
    undoStack_->push(new ResetCropCommand(items));
}

void CanvasView::on_action_reset_transforms()
{
    cancelActiveModes();
    QList<IBaseItem*> items;
    for (QGraphicsItem* item : scene_->selectedItems(true)) {
        items.append(dynamic_cast<IBaseItem*>(item));
    }
    undoStack_->push(
        new ResetTransformsCommand(items, scene_->get_selection_center()));
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
    scene_->arrange_square();
    // TODOLATER: implement arrange_square in CanvasScene
}

// ─── Image actions ────────────────────────────────────────────────────────────

void CanvasView::on_action_change_opacity()
{
    QList<QGraphicsItem*> images;
    for (QGraphicsItem* item : scene_->selectedItems(true)) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        if (baseItem && baseItem->is_image()) {
            images.append(item);
        }
    }
    new ChangeOpacityDialog(this, images, undoStack_.get());
}

void CanvasView::on_action_grayscale()
{
    QList<PixmapItem*> images;
    for (QGraphicsItem* item : scene_->selectedItems(true)) {
        auto* baseItem = dynamic_cast<IBaseItem*>(item);
        if (baseItem && baseItem->is_image()) {
            images.append(dynamic_cast<PixmapItem*>(item));
        }
    }
    if (!images.isEmpty()) {
        undoStack_->push(new ToggleGrayscaleCommand(images));
    }
}

void CanvasView::on_action_show_color_gamut()
{
    auto* item = dynamic_cast<PixmapItem*>(scene_->selectedItems().first());
    new GamutDialog(this, item);
}

void CanvasView::on_action_sample_color()
{
    cancelActiveModes();
    FLOG_DEBUG(Ch::View, "Entering sample color mode");
    viewport()->setCursor(Qt::CrossCursor);
    activeMode_ = ModeSampleColor;

    if (scene_->has_multi_selection()) {
        // We don't want to sample the multi select item, so temporarily
        // send it to the back:
        scene_->multiselect_item_->lower_behind_selection();
    }

    QPoint pos = mapFromGlobal(cursor().pos());
    sampleColorWidget_ = new SampleColorWidget(this,
                                               pos,
                                               scene_->sample_color_at(
                                                   mapToScene(pos)));
}

// ─── Project helpers (existing interface) ────────────────────────────────────

void CanvasView::setProjectSettings(project_settings* ps)
{
    scene_->setProjectSettings(ps);
}

void CanvasView::on_insert_images_finished(const QString& /*filename*/,
                                           const QStringList& errors)
{
    FLOG_DEBUG(Ch::View, "Insert images finished");
    insertImagesInsertedItems_ += scene_->add_queued_items();

    if (!errors.isEmpty()) {
        QStringList names;
        for (const QString& fn : errors)
            names.append(QStringLiteral("<li>%1</li>").arg(fn));
        showMessageBox(QMessageBox::Warning,
                       this,
                       tr("Problem loading images"),
                       tr("%1 image(s) could not be opened.<br/>"
                          "Unknown format or too big?<ul>%2</ul>")
                           .arg(errors.size())
                           .arg(names.join(QStringLiteral("\n"))));
    }

    if (!insertImagesInsertedItems_.isEmpty()) {
        undoStack_->push(new InsertItemsCommand(scene_,
                                                insertImagesInsertedItems_,
                                                std::nullopt,
                                                true));
        scene_->arrange_default();
    }

    // Items were added one at a time as they loaded (see on_items_loaded()),
    // each triggering its own repaint at whatever raw/overlapping position
    // add_queued_items() gave it - visible as a flicker/scatter before
    // arrange_default() above snaps them into their final layout.
    // Suppressed since do_insert_images() below; one repaint now shows
    // only the already-arranged result.
    setUpdatesEnabled(true);

    if (insertImagesNewScene_) {
        on_action_fit_scene();
    }
}

void CanvasView::do_insert_images(const QList<QUrl>& urls,
                                  std::optional<QPoint> pos)
{
    QPoint insertPos = pos.value_or(getViewCenter().toPoint());
    QPointF scenePos = mapToScene(insertPos);

    insertImagesNewScene_ = scene_->items().isEmpty();
    insertImagesInsertedItems_.clear();

    scene_->deselect_all_items();

    // Drop mime data's text/uri-list sometimes has extra entries that
    // aren't real image sources at all: a trailing blank line becomes a
    // spurious empty QUrl, and some browsers (seen from Google Images)
    // add a second, schemeless entry that's really just descriptive
    // alt-text (e.g. "Image of ..."), not a link. Neither is something
    // to try loading, and definitely not worth reporting as a failed
    // image - skip both silently.
    QList<QUrl> validUrls;
    for (const QUrl& url : urls) {
        if (url.isEmpty() || !url.isValid()) {
            FLOG_DEBUG(Ch::View, "Skipping invalid/empty dropped URL");
            continue;
        }
        if (!url.isLocalFile() && url.scheme().isEmpty()) {
            FLOG_DEBUG(Ch::View, "Skipping non-URI dropped entry: {}", url);
            continue;
        }
        validUrls.append(url);
    }

    CanvasScene* scene = scene_;
    auto* worker = new ThreadedIO([validUrls, scenePos, scene](ThreadedIO* w) {
        load_images(validUrls, scenePos, scene, w);
    });

    connect(worker, &ThreadedIO::progress, this, &CanvasView::on_items_loaded);
    connect(worker,
            &ThreadedIO::finished,
            this,
            &CanvasView::on_insert_images_finished);

    // QThread's own finished() (not ThreadedIO's same-named result signal)
    // only fires once the thread has actually stopped running, which is
    // the documented-safe point to delete a QThread object.
    // connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QThread::finished, this, [worker]() {
        const QString threadId = worker->objectName().isEmpty()
                                     ? QStringLiteral("unnamed")
                                     : worker->objectName();
        FLOG_DEBUG(Ch::View,
                   "Thread finished. Scheduling deleteLater for Thread ID: {}",
                   threadId);

        worker->deleteLater();

        FLOG_DEBUG(Ch::View,
                   "deleteLater() called for worker {}",
                   debugString(worker));
    });

    new ProgressDialog(tr("Loading images"), worker, 0, this);

    // Re-enabled in on_insert_images_finished() once arrange_default()
    // has given every item its final position - see the comment there.
    setUpdatesEnabled(false);
    worker->start();
}

void CanvasView::on_items_loaded(int /*value*/)
{
    FLOG_DEBUG(Ch::View, "On items loaded: add queued items");
    insertImagesInsertedItems_ += scene_->add_queued_items();
}

void CanvasView::cleanupWorkplace()
{
    scene_->cleanupWorkplace();
}

QString CanvasView::path()
{
    return scene_->path();
}
void CanvasView::setPath(const QString& path)
{
    scene_->setPath(path);
}
QString CanvasView::projectName()
{
    return scene_->projectName();
}
void CanvasView::setProjectName(const QString& pn)
{
    scene_->setProjectName(pn);
}
bool CanvasView::isModified()
{
    return scene_->isModified();
}
void CanvasView::setModified(bool mod)
{
    scene_->setModified(mod);
}
bool CanvasView::isUntitled()
{
    return scene_->isUntitled();
}
