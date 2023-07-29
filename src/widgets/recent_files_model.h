#pragma once

#include <QAbstractListModel>
#include <QFont>
#include <QFileInfo>

class RecentFilesModel : public QAbstractListModel
{
public:
    RecentFilesModel(QObject *parent = nullptr, const QStringList& files)
        : QAbstractListModel(parent), files(files) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);
        return files.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid())
            return QVariant();

        if (index.row() < 0 || index.row() >= files.size())
            return QVariant();

        if (role == Qt::DisplayRole)
            return QFileInfo(files[index.row()]).fileName();

        if (role == Qt::FontRole)
        {
            QFont font;
            font.setUnderline(true);
            return font;
        }

        return QVariant();
    }

private:
    QStringList files;
};