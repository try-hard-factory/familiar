#pragma once

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

namespace familiar::pureref {

struct ImportResult {
    QList<QVariantMap> items;
    QRectF bounds;
    QString error;
    QStringList warnings;
};

// Reads PureRef 2.x projects without changing the source file. The returned
// item data is ready for CanvasScene::add_item_later() on the GUI thread.
ImportResult read_project(const QString& path,
                          const std::function<bool()>& isCanceled = {});

} // namespace familiar::pureref
