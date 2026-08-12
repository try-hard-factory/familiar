#include "canvasview.h"
#include "canvasscene.h"
#include "commands.h"
#include "export.h"
#include "fileio.h"
#include "mainwindow.h"
#include "moveitem.h"
#include "project_settings.h"
#include "ui/gif_playback_toolbar.h"
#include "ui/group_toolbar.h"
#include "ui/text_edit_toolbar.h"
#include "widgets/color_gamut.h"
#include "widgets/dialogs.h"
#include "widgets/file_browser_dialog.h"
#include <cmath>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
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
    connect(scene_,
            &CanvasScene::edit_item_changed,
            this,
            &CanvasView::on_edit_item_changed);
    setScene(scene_);

    // Floating text-format toolbar; hidden until a
    // TextItem enters edit mode. Pans move the view via the (hidden)
    // scrollbars, so their valueChanged covers repositioning; zoom is
    // handled in doScale(), resizes in resizeEvent().
    textToolbar_ = new TextEditToolbar(viewport());
    textToolbar_->hide();

    // Floating GIF-playback toolbar; hidden until a
    // GifItem is selected (see on_selection_changed()) - unlike the text
    // toolbar, this doesn't need a dedicated "entered edit mode" signal.
    gifToolbar_ = new GifPlaybackToolbar(viewport());
    gifToolbar_->hide();
    // Toggling the frames filmstrip (or anything else) resizes the
    // toolbar without CanvasView calling move() again - reposition
    // whenever that happens so it doesn't drift off its centered/clamped
    // spot (see GifPlaybackToolbar::geometryChanged()'s doc comment).
    connect(gifToolbar_,
            &GifPlaybackToolbar::geometryChanged,
            this,
            &CanvasView::updateGifToolbarPos_);

    // Floating group toolbar; hidden until a
    // GroupItem is selected - same "applies as soon as simply selected,
    // no dedicated mode-entered signal" reasoning as the GIF toolbar.
    groupToolbar_ = new GroupToolbar(viewport());
    groupToolbar_->hide();
    connect(groupToolbar_,
            &GroupToolbar::geometryChanged,
            this,
            &CanvasView::updateGroupToolbarPos_);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        updateTextToolbarPos_();
        updateGifToolbarPos_();
        updateGroupToolbarPos_();
    });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        updateTextToolbarPos_();
        updateGifToolbarPos_();
        updateGroupToolbarPos_();
    });

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
    // Keeps the GIF/group toolbars glued to their item while it's being
    // dragged (this fires on every scene change, animation ticks
    // included, but repositioning to an unchanged position is cheap -
    // simpler than hooking drag-specific events separately).
    updateGifToolbarPos_();
    updateGroupToolbarPos_();
}

void CanvasView::on_selection_changed()
{
    // TODOLATER: update grayscale action checked state from selected item
    viewport()->repaint();

    // GIF playback toolbar: shown for exactly a single selected GifItem,
    // unlike the text toolbar it doesn't need a dedicated "entered edit
    // mode" signal - selecting the item is enough.
    GifItem* gifItem = nullptr;
    QList<QGraphicsItem*> selected = scene_->selectedItems(true);
    if (selected.size() == 1)
        gifItem = dynamic_cast<GifItem*>(selected.first());

    gifToolbar_->attach(gifItem);
    if (gifItem) {
        gifToolbar_->show();
        gifToolbar_->raise();
        updateGifToolbarPos_();
    } else {
        gifToolbar_->hide();
    }

    // Group toolbar - same "single selected item is enough" shape.
    GroupItem* groupItem = nullptr;
    if (selected.size() == 1)
        groupItem = dynamic_cast<GroupItem*>(selected.first());

    groupToolbar_->attach(groupItem);
    if (groupItem) {
        groupToolbar_->show();
        groupToolbar_->raise();
        updateGroupToolbarPos_();
    } else {
        groupToolbar_->hide();
    }
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
    if (textToolbar_)
        textToolbar_->restyleFromPreset();
    if (gifToolbar_)
        gifToolbar_->restyleFromPreset();
    if (groupToolbar_)
        groupToolbar_->restyleFromPreset();
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
    updateTextToolbarPos_();
    updateGifToolbarPos_();
    updateGroupToolbarPos_();
}

void CanvasView::on_edit_item_changed(TextItem* item)
{
    textToolbar_->attach(item);
    if (item) {
        textToolbar_->show();
        textToolbar_->raise();
        updateTextToolbarPos_();
    } else {
        textToolbar_->hide();
    }
}

void CanvasView::updateTextToolbarPos_()
{
    if (!textToolbar_ || !textToolbar_->isVisible() || !textToolbar_->item())
        return;
    const QRectF itemRect = textToolbar_->item()->sceneBoundingRect();
    const QPoint top = mapFromScene(itemRect.topLeft());
    int x = top.x();
    int y = top.y() - textToolbar_->height() - 8;
    // Keep it inside the viewport; when the item is at the very top the
    // toolbar drops below the top edge (over the note) rather than
    // getting clipped away.
    x = qBound(0, x, qMax(0, viewport()->width() - textToolbar_->width()));
    y = qMax(0, y);
    textToolbar_->move(x, y);
}

void CanvasView::updateGifToolbarPos_()
{
    if (!gifToolbar_ || !gifToolbar_->isVisible() || !gifToolbar_->item())
        return;
    // Below the item and horizontally centered under it (PureRef's own
    // placement). Near the edge of the visible canvas, the qBound() clamp
    // below takes over and slides the toolbar to stay on-screen - that's
    // the only case it should ever look "snapped" rather than centered.
    const QRectF itemRect = gifToolbar_->item()->sceneBoundingRect();
    const int leftView = mapFromScene(itemRect.bottomLeft()).x();
    const int rightView = mapFromScene(itemRect.bottomRight()).x();
    const int itemCenterView = (leftView + rightView) / 2;
    int x = itemCenterView - gifToolbar_->width() / 2;
    int y = mapFromScene(itemRect.bottomLeft()).y() + 8;
    x = qBound(0, x, qMax(0, viewport()->width() - gifToolbar_->width()));
    y = qMin(y, qMax(0, viewport()->height() - gifToolbar_->height()));
    gifToolbar_->move(x, y);
    // The control row is positioned separately from the toolbar's own
    // move() above, using the item's center re-expressed in
    // toolbar-local coordinates - if the toolbar itself just got clamped
    // against the viewport edge (item near the edge of the canvas, wide
    // filmstrip open), the toolbar's own center no longer lines up with
    // the item's, and a row simply centered within the toolbar would
    // drift away from the item towards the middle of the filmstrip.
    gifToolbar_->positionControlsRow(itemCenterView - x);
}

void CanvasView::updateGroupToolbarPos_()
{
    if (!groupToolbar_ || !groupToolbar_->isVisible() || !groupToolbar_->item())
        return;
    // Same bottom-center placement (with edge clamp) as the GIF toolbar
    // above - no left-snap/positionControlsRow complexity needed here,
    // this toolbar has no filmstrip-like element that can outgrow the
    // item it's attached to.
    const QRectF itemRect = groupToolbar_->item()->sceneBoundingRect();
    const int leftView = mapFromScene(itemRect.bottomLeft()).x();
    const int rightView = mapFromScene(itemRect.bottomRight()).x();
    const int itemCenterView = (leftView + rightView) / 2;
    int x = itemCenterView - groupToolbar_->width() / 2;
    int y = mapFromScene(itemRect.bottomLeft()).y() + 8;
    x = qBound(0, x, qMax(0, viewport()->width() - groupToolbar_->width()));
    y = qMin(y, qMax(0, viewport()->height() - groupToolbar_->height()));
    groupToolbar_->move(x, y);
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
    // Plain scroll-to-zoom isn't user-configurable (obvious default, not
    // worth exposing as a Control - see KeyboardSettings::
    // mousewheelActions()) - handled directly, before the configurable
    // pan bindings below.
    if (event->modifiers() == Qt::NoModifier) {
        // angleDelta().y() is positive for a wheel-up/forward notch (the
        // conventional "zoom in" direction) - zoom() itself already
        // treats a positive delta as zoom-in, so pass it straight
        // through instead of negating it.
        zoom(event->angleDelta().y(), event->position());
        event->accept();
        return;
    }

    auto match = SettingsHandler::getInstance()->mousewheelActionForEvent(event);
    if (!match)
        return;

    double delta = event->angleDelta().y();
    if (match->inverted)
        delta = -delta;

    if (match->group == QLatin1String("pan_horizontal")) {
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

    // Hide the GIF/group toolbars for whatever's about to happen below
    // (resize/move/click-elsewhere) - see mouseReleaseEvent(), which
    // re-shows them once the interaction is over. Left visible, they
    // stay anchored to the item's PRE-drag bounding rect and visibly
    // lag/overlap the resize handles while dragging.
    if (gifToolbar_->isVisible())
        gifToolbar_->hide();
    if (groupToolbar_->isVisible())
        groupToolbar_->hide();

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

    // Covers the case mousePressEvent()'s own hide can't: pressing on a
    // not-yet-selected GIF/group selects it (synchronously, inside
    // QGraphicsView::mousePressEvent() below) and that selection change
    // shows the toolbar via on_selection_changed() - AFTER
    // mousePressEvent()'s hide line already ran. Without this, that
    // select-and-drag-in-one-motion case leaves the toolbar visible and
    // lagging behind the item for the whole drag.
    if (event->buttons() & Qt::LeftButton) {
        if (gifToolbar_->isVisible())
            gifToolbar_->hide();
        if (groupToolbar_->isVisible())
            groupToolbar_->hide();
    }

    QGraphicsView::mouseMoveEvent(event);
}

void CanvasView::mouseReleaseEvent(QMouseEvent* event)
{
    FLOG_DEBUG(Ch::View,
               "CanvasView::mouseReleaseEvent activeMode_={} spontaneous={}",
               int(activeMode_),
               event->spontaneous());
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

    // Re-show the GIF/group toolbars hidden in mousePressEvent(), but
    // only if their item is still the sole selection - a click that
    // deselected it (or selected something else) already got it hidden/
    // detached via on_selection_changed(), and re-showing it here too
    // would undo that.
    QList<QGraphicsItem*> selected = scene_->selectedItems(true);
    if (GifItem* item = gifToolbar_->item()) {
        if (!gifToolbar_->isVisible() && selected.size() == 1
            && selected.first() == item) {
            gifToolbar_->show();
            gifToolbar_->raise();
            updateGifToolbarPos_();
        }
    }
    if (GroupItem* item = groupToolbar_->item()) {
        if (!groupToolbar_->isVisible() && selected.size() == 1
            && selected.first() == item) {
            groupToolbar_->show();
            groupToolbar_->raise();
            updateGroupToolbarPos_();
        }
    }
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
    if (tryControlKeyNudge(event)) {
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

bool CanvasView::tryControlKeyNudge(QKeyEvent* event)
{
    const QString pressed = keyEventToSequenceString(event);
    if (pressed.isEmpty())
        return false;

    for (const MouseConfig& cfg : KeyboardSettings::mouseActions()) {
        if (cfg.group() != QLatin1String("zoom"))
            continue;
        for (const Binding& b : cfg.getBindings()) {
            if (b.keySequence == pressed) {
                zoom(120.0, getViewCenter());
                return true;
            }
        }
    }

    for (const MouseWheelConfig& cfg : KeyboardSettings::mousewheelActions()) {
        for (const Binding& b : cfg.getBindings()) {
            if (b.keySequence != pressed)
                continue;
            const double delta = b.inverted ? -120.0 : 120.0;
            if (cfg.group() == QLatin1String("pan_horizontal")) {
                pan(QPointF(0.0, 0.5 * delta));
                return true;
            }
            if (cfg.group() == QLatin1String("pan_vertical")) {
                pan(QPointF(0.5 * delta, 0.0));
                return true;
            }
        }
    }
    return false;
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
    updateTextToolbarPos_();
    updateGifToolbarPos_();
    updateGroupToolbarPos_();
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
    // qApp->activeWindow(), not isActiveWindow(): the latter is specific
    // to THIS top-level window, so it goes false the moment any other
    // window of ours - a modal-ish dialog like ChangeOpacityDialog, a
    // color picker, the settings window - takes OS focus, triggering the
    // same "user switched away" fade this was designed for (see the
    // fades_with_window_focus() comment above) even though the user
    // never left the app. qApp->activeWindow() stays non-null as long as
    // ANY window of this application has focus, and is only null once
    // focus genuinely moves to a different application.
    const bool visible = qApp->activeWindow() != nullptr
                         || selectionOutlineHover_;

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
        // prefer that as a fast preview instead.
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

    // showSaveFileDialog() already appends the right extension for
    // whichever filter is selected (its primaryExt) if the typed name
    // doesn't have one, so the old manual ".png" fallback isn't needed
    // separately.
    const QString filename
        = showSaveFileDialog(this,
                             tr("Export Scene to Image"),
                             path().isEmpty()
                                 ? QDir::homePath()
                                 : QFileInfo(path()).absolutePath(),
                             tr("Image Files (*.png *.jpg *.jpeg *.svg)"
                                ";;PNG (*.png)"
                                ";;JPEG (*.jpg *.jpeg)"
                                ";;SVG (*.svg)"));
    if (filename.isEmpty())
        return;

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

    const QString directory
        = showSelectFolderDialog(this,
                                 tr("Export Images"),
                                 path().isEmpty()
                                     ? QDir::homePath()
                                     : QFileInfo(path()).absolutePath());
    if (directory.isEmpty())
        return;

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
    // ChangeGroupFillColorCommand (and anything else touching the
    // attached group's properties) changes the item directly, not
    // through GroupToolbar - re-attach to re-sync the toolbar's own
    // displayed state (fill swatch, lock checkbox) with whatever undo
    // just reverted it to.
    if (groupToolbar_->item())
        groupToolbar_->attach(groupToolbar_->item());
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
    // See on_action_undo() above - same re-sync, needed after redo too.
    if (groupToolbar_->item())
        groupToolbar_->attach(groupToolbar_->item());
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
    // with_related_items() - deleting a picture also deletes any notes
    // pinned to it, and deleting a group deletes its whole subtree
    // (nested subgroups included), all bundled into this same
    // DeleteItemsCommand so one undo restores everything.
    undoStack_->push(new DeleteItemsCommand(scene_,
                                            scene_->with_related_items(
                                                scene_->selectedItems(true))));
}

void CanvasView::on_action_cut()
{
    // on_action_copy() (via CanvasScene::copy_selection_to_internal_
    // clipboard()) and the delete below both expand the selection
    // through the SAME with_related_items() - what gets deleted here
    // always matches what just got copied, so Cut never leaves orphaned
    // group members or attached items behind on the canvas.
    on_action_copy();
    suppressNextEmptySceneReset_ = true;
    resetPreviousTransform();
    undoStack_->push(new DeleteItemsCommand(scene_,
                                            scene_->with_related_items(
                                                scene_->selectedItems(true))));
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

void CanvasView::on_action_duplicate()
{
    cancelActiveModes();
    scene_->duplicate_selection();
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

void CanvasView::on_action_group()
{
    scene_->group_selection();
}

void CanvasView::on_action_ungroup()
{
    scene_->ungroup_selection();
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

void CanvasView::on_action_zoom_in()
{
    // 120 matches one wheelEvent() notch (angleDelta().y() == ±120) -
    // same visible step size as scrolling to zoom.
    zoom(120.0, getViewCenter());
}

void CanvasView::on_action_zoom_out()
{
    zoom(-120.0, getViewCenter());
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

    const QStringList filenames
        = showOpenFilesDialog(&mainwindow_,
                              tr("Select one or more images to open"),
                              QString(),
                              tr("Images (%1)").arg(formats));
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
    // Auto-attach - if exactly one picture/gif is
    // currently selected, the new note pins to it. GifItem IS-A
    // PixmapItem, covered by the same dynamic_cast.
    QList<QGraphicsItem*> selected = scene_->selectedItems(true);
    GroupItem* targetGroup = nullptr;
    if (selected.size() == 1) {
        if (auto* picture = dynamic_cast<PixmapItem*>(selected.first()))
            item->set_attached_to(picture->uid());
        else if (auto* group = dynamic_cast<GroupItem*>(selected.first()))
            targetGroup = group;
    }
    if (targetGroup) {
        // Same "add text with a group selected -> lands inside it"
        // expectation as the picture-attach case above. Bundled
        // as one undo step, same pattern as the drag-drop-transfer
        // macro in CanvasScene::maybe_add_dropped_items_to_group().
        undoStack_->beginMacro(tr("Insert text"));
        undoStack_->push(
            new InsertItemsCommand(scene_, QList<IBaseItem*>{item}, pos));
        undoStack_->push(new AddToGroupCommand(scene_, targetGroup, {item}));
        undoStack_->endMacro();
    } else {
        undoStack_->push(
            new InsertItemsCommand(scene_, QList<IBaseItem*>{item}, pos));
    }
    // Drop straight into edit mode - same enter_edit_mode() a
    // double-click on an existing note triggers
    // (CanvasScene::mousePressEvent()). item is already on the scene by
    // now (both branches above pushed InsertItemsCommand, whose redo()
    // ran synchronously).
    //
    // Re-select it explicitly first: the targetGroup branch's
    // AddToGroupCommand::redo() ends by deselecting everything and
    // selecting the GROUP instead (matches the "Group" button's own
    // convention) - has_selection_outline()/has_selection_handles()
    // (moveitem.h) both key off isSelected(), so without this the new
    // note would enter edit mode with no selection rectangle or square
    // handles at all. Select-all over the "Text" placeholder so the first
    // keystroke replaces it outright, instead of inserting into the
    // middle of it.
    scene_->deselect_all_items();
    item->setSelected(true);
    item->enter_edit_mode();
    item->setFocus();
    QTextCursor cursor = item->textCursor();
    cursor.select(QTextCursor::Document);
    item->setTextCursor(cursor);
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
QUuid CanvasView::recoveryId()
{
    return scene_->recoveryId();
}
