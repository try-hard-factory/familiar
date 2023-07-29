#pragma once

#include <QCursor>
#include <QFileInfo>
#include <QListView>
#include <QMouseEvent>
#include <QSize>

#include "recent_files_model.h"

class RecentFilesView : public QListView
{
    Q_OBJECT

public:
    RecentFilesView(QWidget* parent = nullptr, const QStringList& files = {})
        : QListView(parent)
        , files(files)
    {
        connect(this, &QListView::clicked, this, &RecentFilesView::on_clicked);
        setModel(new RecentFilesModel(files)); // TODO: memory leak?
        setMouseTracking(true);
    }

    void update_files(const QStringList& newFiles)
    {
        files = newFiles;
        model()->setData(files);
        reset();
    }

    QSize sizeHint() const override
    {
        int height
            = std::accumulate(files.begin(), files.end(), 0, [this](int sum, const QString& file) {
                  return sum + sizeHintForRow(model()->index(files.indexOf(file), 0)) + 2;
              });

        int width = std::transform_reduce(
            files.begin(), files.end(), 0, [](int max_width, const QString& file) {
                return std::max(max_width,
                                sizeHintForColumn(model()->index(files.indexOf(file), 0)));
            });

        return QSize(width + 2, height);
    }

protected:
    void mouseMoveEvent(QMouseEvent* event) override
    {
        QModelIndex index = indexAt(event->position());
        if (index.isValid())
            setCursor(Qt::PointingHandCursor);
        else
            setCursor(Qt::ArrowCursor);

        QListView::mouseMoveEvent(event);
    }

private slots:
    void on_clicked(const QModelIndex& index)
    {
        if (index.isValid() && index.row() < files.size())
            parentWidget()->parentWidget()->open_from_file(files[index.row()]);
    }

private:
    QStringList files;
};
