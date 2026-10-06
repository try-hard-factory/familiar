#include "export.h"

#include "canvasscene.h"
#include "fileio.h"
#include "moveitem.h"
#include "widgets/dialogs.h"

#include <core/settingshandler.h>

#include "log/log.h"
using namespace familiar::log;

#include <QDir>
#include <QFile>
#include <QFont>
#include <QMap>
#include <QMargins>
#include <QPainter>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>

// ============================================================================
// ExporterBase
// ============================================================================

void ExporterBase::emit_begin_processing(ThreadedIO* worker, int total) const
{
    if (worker) {
        emit worker->begin_processing(total);
    }
}

void ExporterBase::emit_progress(ThreadedIO* worker, int value) const
{
    if (worker) {
        emit worker->progress(value);
    }
}

void ExporterBase::emit_finished(ThreadedIO* worker,
                                const QString& target,
                                const QStringList& errors) const
{
    if (worker) {
        emit worker->finished(target, errors);
    }
}

void ExporterBase::emit_user_input_required(ThreadedIO* worker,
                                         const QString& message) const
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
    // into the export.
    scene_->deselect_all_items();

    const QRectF rect = scene_->itemsBoundingRect();
    const QSize contentSize(qRound(rect.width()), qRound(rect.height()));
    margin_ = std::max(contentSize.width(), contentSize.height()) * 0.03;
    const int marginInt = qRound(margin_);
    defaultSize_ = contentSize.grownBy(
        QMargins(marginInt, marginInt, marginInt, marginInt));
}

// ============================================================================
// SceneToPixmapExporter
// ============================================================================

bool SceneToPixmapExporter::get_user_input(QWidget* parent)
{
    SceneToPixmapExporterDialog dialog(parent, defaultSize_);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    size_ = dialog.value();
    return true;
}

QImage SceneToPixmapExporter::render_to_image() const
{
    const qreal finalMargin = margin_ * size_.width() / defaultSize_.width();

    auto colorPreset = SettingsHandler::get_instance()->get_current_color_preset();
    const QColor canvasColor = colorPreset[EPresetsColorIdx::kCanvasColor];

    QImage image(size_, QImage::Format_RGB32);
    image.fill(canvasColor);
    QPainter painter(&image);
    const QRectF targetRect(finalMargin,
                      finalMargin,
                      size_.width() - 2 * finalMargin,
                      size_.height() - 2 * finalMargin);
    scene_->render(&painter, targetRect, scene_->itemsBoundingRect());
    painter.end();
    return image;
}

void SceneToPixmapExporter::export_to(const QString& filename, ThreadedIO* worker)
{
    emit_begin_processing(worker, 1);
    const QImage image = render_to_image();

    if (worker && worker->canceled) {
        emit_finished(worker, filename, {});
        return;
    }

    if (!image.save(filename, nullptr, 90)) {
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
    size_ = defaultSize_;
    return true;
}

QString SceneToSVGExporter::text_styles(TextItem* item) const
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

QString SceneToSVGExporter::render_to_svg(ThreadedIO* worker) const
{
    QString output;
    QXmlStreamWriter xml(&output);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("svg"));
    xml.writeAttribute(QStringLiteral("width"), QString::number(size_.width()));
    xml.writeAttribute(QStringLiteral("height"),
                       QString::number(size_.height()));
    xml.writeAttribute(QStringLiteral("xmlns"),
                       QStringLiteral("http://www.w3.org/2000/svg"));
    xml.writeAttribute(QStringLiteral("xmlns:xlink"),
                       QStringLiteral("http://www.w3.org/1999/xlink"));

    const QRectF rect = scene_->itemsBoundingRect();
    const QPointF offset = rect.topLeft() - QPointF(margin_, margin_);

    QList<QGraphicsItem*> items = scene_->items_for_save();

    for (int i = 0; i < items.size(); ++i) {
        QGraphicsItem* gitem = items[i];
        auto* baseItem = dynamic_cast<IBaseItem*>(gitem);
        const std::string type = baseItem->get_type();

        QPointF pos = gitem->pos() - offset;
        const QPointF anchor = pos;

        if (type == "text") {
            auto* textItem = static_cast<TextItem*>(gitem);
            xml.writeStartElement(QStringLiteral("text"));
            xml.writeAttribute(QStringLiteral("style"), text_styles(textItem));
            xml.writeAttribute(QStringLiteral("dominant-baseline"),
                               QStringLiteral("hanging"));
        } else if (type == "pixmap") {
            auto* pixmapItem = static_cast<PixmapItem*>(gitem);
            const qreal width = pixmapItem->width() * pixmapItem->scale();
            const qreal height = pixmapItem->height() * pixmapItem->scale();
            auto [bytes, imgformat]
                = pixmapItem->pixmap_to_bytes(/*apply_grayscale=*/true,
                                              /*apply_crop=*/true);
            const QString b64 = QString::fromLatin1(bytes.toBase64());

            xml.writeStartElement(QStringLiteral("image"));
            xml.writeAttribute(QStringLiteral("xlink:href"),
                               QStringLiteral("data:image/%1;base64,%2")
                                   .arg(imgformat, b64));
            xml.writeAttribute(QStringLiteral("width"), QString::number(width));
            xml.writeAttribute(QStringLiteral("height"),
                               QString::number(height));
            xml.writeAttribute(QStringLiteral("image-rendering"),
                               pixmapItem->scale() > 2
                                   ? QStringLiteral("crisp-edges")
                                   : QStringLiteral("optimizeQuality"));
            pos = pos + pixmapItem->crop().topLeft();
        } else {
            // items_for_save() only returns pixmap/text.
            continue;
        }

        QStringList transforms;
        if (baseItem->flip() == -1) {
            // SVG has no "transform-origin"; fake one by translating to
            // the anchor, flipping, then translating back.
            transforms << QStringLiteral("translate(%1 %2)")
                              .arg(anchor.x())
                              .arg(anchor.y());
            transforms << QStringLiteral("scale(%1 1)").arg(baseItem->flip());
            transforms << QStringLiteral("translate(%1 %2)")
                              .arg(-anchor.x())
                              .arg(-anchor.y());
        }
        transforms << QStringLiteral("rotate(%1 %2 %3)")
                          .arg(gitem->rotation())
                          .arg(anchor.x())
                          .arg(anchor.y());

        xml.writeAttribute(QStringLiteral("transform"),
                           transforms.join(QStringLiteral(" ")));
        xml.writeAttribute(QStringLiteral("x"), QString::number(pos.x()));
        xml.writeAttribute(QStringLiteral("y"), QString::number(pos.y()));
        xml.writeAttribute(QStringLiteral("opacity"),
                           QString::number(gitem->opacity()));

        if (type == "text") {
            xml.writeCharacters(static_cast<TextItem*>(gitem)->toPlainText());
        }
        xml.writeEndElement(); // text | image

        emit_progress(worker, i);
        if (worker && worker->canceled) {
            return {};
        }
    }

    xml.writeEndElement(); // svg
    xml.writeEndDocument();
    return output;
}

void SceneToSVGExporter::export_to(const QString& filename, ThreadedIO* worker)
{
    emit_begin_processing(worker,
                        static_cast<int>(scene_->items_for_save().size()));
    const QString svg = render_to_svg(worker);

    if (worker && worker->canceled) {
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

std::unique_ptr<SceneExporterBase> create_scene_exporter(const QString& extension,
                                                       CanvasScene* scene)
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

void ImagesToDirectoryExporter::export_to(ThreadedIO* worker)
{
    const int total = static_cast<int>(items_.size());
    emit_begin_processing(worker, total);
    emit_progress(worker, startFrom_);

    for (int i = startFrom_; i < total; ++i) {
        if (worker && worker->canceled) {
            emit_finished(worker, dirname_, {});
            return;
        }

        PixmapItem* item = items_[i];
        // GifItem checked BEFORE the generic PixmapItem case - same
        // reasoning as FmlArchive::save(): pixmap_to_bytes() only ever
        // encodes the CURRENTLY DISPLAYED frame as a static raster,
        // silently discarding the animation. GifItem doesn't override
        // it, so without this check every exported gif quietly became a
        // single still frame.
        QByteArray bytes;
        QString filename;
        if (auto* gifItem = dynamic_cast<GifItem*>(item)) {
            bytes = gifItem->gif_bytes();
            filename = gifItem->get_filename_for_export(QStringLiteral("gif"));
        } else {
            const auto [pixBytes, imgformat] = item->pixmap_to_bytes();
            bytes = pixBytes;
            filename = item->get_filename_for_export(imgformat);
        }
        const QString path = QDir(dirname_).filePath(filename);

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
