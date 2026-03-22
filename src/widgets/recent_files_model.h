#pragma once

#include <QAbstractListModel>
#include <QFileInfo>
#include <QFont>

class RecentFilesModel : public QAbstractListModel
{
public:
    RecentFilesModel(QObject* parent, const QStringList& files)
        : QAbstractListModel(parent)
        , files_(files)
    {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent);
        return files_.size();
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid())
            return QVariant();

        if (index.row() < 0 || index.row() >= files_.size())
            return QVariant();

        if (role == Qt::DisplayRole)
            return QFileInfo(files_[index.row()]).fileName();

        if (role == Qt::FontRole) {
            QFont font;
            font.setUnderline(true);
            return font;
        }

        return QVariant();
    }

    void setFiles(const QStringList& files) { files_ = files; }

private:
    QStringList files_;
};