#include "export.h"

#include "canvasscene.h"
#include "fileio.h"
#include "moveitem.h"
#include "widgets/dialogs.h"

#include <core/settingshandler.h>

#include "log/log.h"
using namespace familiar::log;

namespace {

// Margin around the exported scene, as a fraction of its larger side.
constexpr qreal kmarginRatio = 0.03;

} // namespace

#include <QAbstractGraphicsShapeItem>
#include <QColor>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFont>
#include <QHashFunctions>
#include <QImage>
#include <QList>
#include <QMap>
#include <QMargins>
#include <QPaintDevice>
#include <QPainter>
#include <QPoint>
#include <QSize>
#include <QStringView>
#include <QUuid>
#include <QXmlStreamWriter>
#include <Qt>
#include <QtContainerFwd>
#include <QtNumeric>
#include <QtTypes>

#include <algorithm>
#include <memory>
#include <string>

// ============================================================================
// ExporterBase
// ============================================================================

void ExporterBase::emit_begin_processing(ThreadedIO* worker, int total)
{
    if (worker) {
        emit worker->begin_processing(total);
    }
}

void ExporterBase::emit_progress(ThreadedIO* worker, int value)
{
    if (worker) {
        emit worker->progress(value);
    }
}

void ExporterBase::emit_finished(ThreadedIO* worker,
                                 const QString& target,
                                 const QStringList& errors)
{
    if (worker) {
        emit worker->finished(target, errors);
    }
}

void ExporterBase::emit_user_input_required(ThreadedIO* worker,
                                            const QString& message)
{
    if (worker) {
        emit worker->user_input_required(message);
    }
}

// ============================================================================
// SceneExporterBase
// ============================================================================

SceneExporterBase::SceneExporterBase(CanvasScene* scene)
    : scene_(scene)
{
    scene_->cancel_active_modes();
    // Selection outlines/handles would otherwise get rendered/serialized
    // into the export - but the user's selection is theirs, so it is
    // taken down for the duration and put back by the destructor rather
    // than silently thrown away.
    for (QGraphicsItem* item : scene_->selectedItems(true)) {
        if (auto* baseItem = dynamic_cast<IBaseItem*>(item)) {
            selectedUids_.append(baseItem->uid());
        }
    }
    scene_->deselect_all_items();

    const QRectF rect = scene_->itemsBoundingRect();
    const QSize contentSize(qRound(rect.width()), qRound(rect.height()));
    margin_ = std::max(contentSize.width(), contentSize.height())
              * kmarginRatio;
    const int marginInt = qRound(margin_);
    defaultSize_ = contentSize.grownBy(
        QMargins(marginInt, marginInt, marginInt, marginInt));
}

SceneExporterBase::~SceneExporterBase()
{
    for (const QUuid& uid : selectedUids_) {
        // Resolved now rather than held as a pointer: an item that is
        // gone by the time the export finishes is simply skipped.
        if (QGraphicsItem* item = scene_->find_by_uid(uid)) {
            item->setSelected(true);
        }
    }
}

// ============================================================================
// SceneToPixmapExporter
// ============================================================================

bool SceneToPixmapExporter::get_user_input(QWidget* parent)
{
    SceneToPixmapExporterDialog dialog(parent, default_size());
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    size_ = dialog.value();
    return true;
}

QImage SceneToPixmapExporter::render_to_image() const
{
    const qreal finalMargin = margin() * size_.width() / default_size().width();

    auto colorPreset
        = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor canvasColor = colorPreset[EPresetsColorIdx::CanvasColor];

    QImage image(size_, QImage::Format_RGB32);
    image.fill(canvasColor);
    QPainter painter(&image);
    const QRectF targetRect(finalMargin,
                            finalMargin,
                            size_.width() - (2 * finalMargin),
                            size_.height() - (2 * finalMargin));
    scene()->render(&painter, targetRect, scene()->itemsBoundingRect());
    painter.end();
    return image;
}

void SceneToPixmapExporter::prepare()
{
    rendered_ = render_to_image();
}

void SceneToPixmapExporter::export_to(const QString& filename,
                                      ThreadedIO* worker)
{
    emit_begin_processing(worker, 1);
    // Already rendered by prepare(); QImage is a value type, so this
    // owns its pixels outright and the scene is not read here at all.
    const QImage image = rendered_;

    if (worker && worker->is_canceled()) {
        emit_finished(worker, filename, {});
        return;
    }

    // item_consts::kjpegQuality (moveitem.h) - the same quality a picture is
    // stored at inside a .fml, so an export does not look different
    // from what the project already holds.
    if (!image.save(filename, nullptr, item_consts::kjpegQuality)) {
        emit_finished(worker, filename, {QStringLiteral("Error writing file")});
        return;
    }

    emit_progress(worker, 1);
    emit_finished(worker, filename, {});
}

// ============================================================================
// SceneToSVGExporter
// ============================================================================

bool SceneToSVGExporter::get_user_input(QWidget* /*parent*/)
{
    size_ = default_size();
    return true;
}

QString SceneToSVGExporter::text_styles(TextItem* item)
{
    static const QMap<QFont::Style, QString> styleNames{
        {QFont::StyleNormal, QStringLiteral("normal")},
        {QFont::StyleItalic, QStringLiteral("italic")},
        {QFont::StyleOblique, QStringLiteral("oblique")},
    };

    const QFont font = item->font();
    const qreal fontsize = font.pointSize() * item->scale();
    const QString families = font.families().join(QStringLiteral(", "));

    QStringList styles;
    styles << QStringLiteral("white-space:pre")
           << QStringLiteral("font-size:%1pt").arg(fontsize)
           << QStringLiteral("font-family:%1").arg(families)
           << QStringLiteral("font-weight:%1")
                  .arg(static_cast<int>(font.weight()))
           << QStringLiteral("font-stretch:%1").arg(font.stretch())
           << QStringLiteral("font-style:%1").arg(styleNames.value(font.style()));
    return styles.join(QStringLiteral(";"));
}

void SceneToSVGExporter::prepare()
{
    nodes_.clear();

    const QRectF rect = scene()->itemsBoundingRect();
    const QPointF offset = rect.topLeft() - QPointF(margin(), margin());

    for (QGraphicsItem* gitem : scene()->items_for_save()) {
        auto* baseItem = dynamic_cast<IBaseItem*>(gitem);
        const std::string type = baseItem->get_type();

        Node node;
        node.pos = gitem->pos() - offset;
        node.anchor = node.pos;
        node.rotation = gitem->rotation();
        node.flip = baseItem->flip();
        node.opacity = gitem->opacity();

        if (type == "text") {
            auto* textItem = dynamic_cast<TextItem*>(gitem);
            node.isText = true;
            node.style = text_styles(textItem);
            node.text = textItem->toPlainText();
        } else if (type == "pixmap") {
            auto* pixmapItem = dynamic_cast<PixmapItem*>(gitem);
            node.width = pixmapItem->width() * pixmapItem->scale();
            node.height = pixmapItem->height() * pixmapItem->scale();
            node.crispEdges = pixmapItem->scale() > 2;
            node.source
                = pixmapItem->export_source(/*applyGrayscale=*/true,
                                            /*applyCrop=*/true);
            node.pos = node.pos + pixmapItem->crop().topLeft();
        } else {
            // Everything else items_for_save() hands out is left out of
            // the SVG on purpose, and "everything else" is not nothing:
            // it returns group and gif items too (item_add_by_user(),
            // canvasscene.cpp). A GroupItem is a backdrop rectangle, not
            // user content. A gif would have to be embedded either as
            // its original bytes - which ignore the crop and grayscale
            // this branch applies, so a cropped one would land wrong -
            // or as one flattened frame. Neither is wanted here:
            // Export Images writes gifs out verbatim and is the path
            // for them.
            continue;
        }

        nodes_.append(node);
    }
}

QString SceneToSVGExporter::nodes_to_svg(const QList<Node>& nodes,
                                         QSize size,
                                         ThreadedIO* worker)
{
    QString output;
    QXmlStreamWriter xml(&output);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("svg"));
    xml.writeAttribute(QStringLiteral("width"), QString::number(size.width()));
    xml.writeAttribute(QStringLiteral("height"),
                       QString::number(size.height()));
    xml.writeAttribute(QStringLiteral("xmlns"),
                       QStringLiteral("http://www.w3.org/2000/svg"));
    xml.writeAttribute(QStringLiteral("xmlns:xlink"),
                       QStringLiteral("http://www.w3.org/1999/xlink"));

    for (int i = 0; i < nodes.size(); ++i) {
        const Node& node = nodes[i];

        if (node.isText) {
            xml.writeStartElement(QStringLiteral("text"));
            xml.writeAttribute(QStringLiteral("style"), node.style);
            xml.writeAttribute(QStringLiteral("dominant-baseline"),
                               QStringLiteral("hanging"));
        } else {
            // The one genuinely slow step in here, and the reason this
            // half is worth running off the GUI thread at all.
            auto [bytes, imgformat] = PixmapItem::encode(node.source);
            const QString b64 = QString::fromLatin1(bytes.toBase64());

            xml.writeStartElement(QStringLiteral("image"));
            xml.writeAttribute(QStringLiteral("xlink:href"),
                               QStringLiteral("data:image/%1;base64,%2")
                                   .arg(imgformat, b64));
            xml.writeAttribute(QStringLiteral("width"),
                               QString::number(node.width));
            xml.writeAttribute(QStringLiteral("height"),
                               QString::number(node.height));
            xml.writeAttribute(QStringLiteral("image-rendering"),
                               node.crispEdges
                                   ? QStringLiteral("crisp-edges")
                                   : QStringLiteral("optimizeQuality"));
        }

        QStringList transforms;
        if (node.flip == -1) {
            // SVG has no "transform-origin"; fake one by translating to
            // the anchor, flipping, then translating back.
            transforms << QStringLiteral("translate(%1 %2)")
                              .arg(node.anchor.x())
                              .arg(node.anchor.y());
            transforms << QStringLiteral("scale(%1 1)").arg(node.flip);
            transforms << QStringLiteral("translate(%1 %2)")
                              .arg(-node.anchor.x())
                              .arg(-node.anchor.y());
        }
        transforms << QStringLiteral("rotate(%1 %2 %3)")
                          .arg(node.rotation)
                          .arg(node.anchor.x())
                          .arg(node.anchor.y());

        xml.writeAttribute(QStringLiteral("transform"),
                           transforms.join(QStringLiteral(" ")));
        xml.writeAttribute(QStringLiteral("x"), QString::number(node.pos.x()));
        xml.writeAttribute(QStringLiteral("y"), QString::number(node.pos.y()));
        xml.writeAttribute(QStringLiteral("opacity"),
                           QString::number(node.opacity));

        if (node.isText) {
            xml.writeCharacters(node.text);
        }
        xml.writeEndElement(); // text | image

        emit_progress(worker, i);
        if (worker && worker->is_canceled()) {
            return {};
        }
    }

    xml.writeEndElement(); // svg
    xml.writeEndDocument();
    return output;
}

void SceneToSVGExporter::export_to(const QString& filename, ThreadedIO* worker)
{
    emit_begin_processing(worker, static_cast<int>(nodes_.size()));
    const QString svg = nodes_to_svg(nodes_, size_, worker);

    if (worker && worker->is_canceled()) {
        emit_finished(worker, filename, {});
        return;
    }

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        FLOG_WARN(Ch::IO,
                  "SceneToSVGExporter::export_to: could not open {}: {}",
                  filename,
                  file.errorString());
        emit_finished(worker, filename, {file.errorString()});
        return;
    }
    file.write(svg.toUtf8());
    file.close();

    emit_finished(worker, filename, {});
}

// ============================================================================
// createSceneExporter
// ============================================================================

std::unique_ptr<SceneExporterBase> create_scene_exporter(
    const QString& extension, CanvasScene* scene)
{
    if (extension.compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<SceneToSVGExporter>(scene);
    }
    return std::make_unique<SceneToPixmapExporter>(scene);
}

// ============================================================================
// ImagesToDirectoryExporter
// ============================================================================

ImagesToDirectoryExporter::ImagesToDirectoryExporter(CanvasScene* scene,
                                                     const QString& dirname)
    : dirname_(dirname)
{
    for (QGraphicsItem* item : scene->items_by_type("pixmap")) {
        if (auto* pixmapItem = dynamic_cast<PixmapItem*>(item)) {
            items_.append(pixmapItem);
        }
    }
}

ImagesToDirectoryExporter::ImagesToDirectoryExporter(
    const QList<PixmapItem*>& items, const QString& dirname)
    : items_(items)
    , dirname_(dirname)
{}

void ImagesToDirectoryExporter::prepare()
{
    jobs_.clear();
    jobs_.reserve(items_.size());

    for (PixmapItem* item : items_) {
        Job job;
        // GifItem checked BEFORE the generic PixmapItem case - same
        // reasoning as FmlArchive::save(): encoding only ever produces
        // the CURRENTLY DISPLAYED frame as a static raster, silently
        // discarding the animation. GifItem doesn't override that, so
        // without this check every exported gif quietly became a single
        // still frame.
        if (auto* gifItem = dynamic_cast<GifItem*>(item)) {
            job.gifBytes = gifItem->gif_bytes();
            job.filename
                = gifItem->get_filename_for_export(QStringLiteral("gif"));
        } else {
            // items_ only ever gets non-null pointers (the CanvasScene
            // constructor's dynamic_cast guard); the analyzer reaches
            // here by assuming the dynamic_cast above failed BECAUSE
            // item was null.
            // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
            job.source = item->export_source();
            // Decided here, with the item still in hand, so the target
            // name is settled before the worker runs - export_source()
            // has already chosen the format the bytes will be in.
            job.filename = item->get_filename_for_export(job.source.format);
        }
        jobs_.append(job);
    }
}

void ImagesToDirectoryExporter::export_to(ThreadedIO* worker)
{
    const int total = static_cast<int>(jobs_.size());
    emit_begin_processing(worker, total);
    emit_progress(worker, startFrom_);

    for (int i = startFrom_; i < total; ++i) {
        if (worker && worker->is_canceled()) {
            emit_finished(worker, dirname_, {});
            return;
        }

        const Job& job = jobs_[i];
        const QByteArray bytes = job.gifBytes.isEmpty()
                                     ? PixmapItem::encode(job.source).first
                                     : job.gifBytes;
        const QString path = QDir(dirname_).filePath(job.filename);

        if (QFile::exists(path)) {
            if (handleExisting_.isEmpty()) {
                startFrom_ = i;
                emit_user_input_required(worker, path);
                return;
            }
            if (handleExisting_ == QStringLiteral("skip")) {
                handleExisting_.clear();
                continue;
            }
            if (handleExisting_ == QStringLiteral("skip_all")) {
                continue;
            }
            if (handleExisting_ == QStringLiteral("overwrite")) {
                handleExisting_.clear();
            }
            // "overwrite_all": falls through and keeps writing for the
            // rest of the loop.
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)
            || file.write(bytes) != bytes.size()) {
            emit_finished(worker,
                          dirname_,
                          {QStringLiteral("Could not write %1").arg(path)});
            return;
        }
        file.close();

        emit_progress(worker, i);
    }

    emit_finished(worker, dirname_, {});
}
