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
    RecentFilesView(QWidget* parent, const QStringList& files)
        : QListView(parent)
        , files(files)
    {
        connect(this, &QListView::clicked, this, &RecentFilesView::on_clicked);
        setModel(new RecentFilesModel(nullptr, files)); // TODO: memory leak?
        setMouseTracking(true);
    }

    void update_files(const QStringList& newFiles)
    {
        files = newFiles;
        // QItemSelectionModel* m = selectionModel();
        // setModel(new RecentFilesModel(nullptr, files));
        // delete m;
        auto* model = dynamic_cast<RecentFilesModel*>(this->model());
        model->setFiles(files);
        reset();
    }

    QSize sizeHint() const override
    {
        int height = std::accumulate(
            files.begin(), files.end(), 0, [this](int sum, const QString& file) {
                return sum
                       + sizeHintForRow(this->model()
                                            ->index(this->files.indexOf(file), 0)
                                            .row())
                       + 2;
            });

        std::vector<int> columnWidths(files.size());
        std::transform(files.begin(),
                       files.end(),
                       columnWidths.begin(),
                       [this](const QString& file) {
                           return sizeHintForColumn(
                               model()->index(files.indexOf(file), 0).column());
                       });
        int width = 2
                    + *std::max_element(columnWidths.begin(),
                                        columnWidths.end());
        return QSize(width, height);
    }

protected:
    void mouseMoveEvent(QMouseEvent* event) override
    {
        QModelIndex index = indexAt(event->position().toPoint());
        if (index.isValid())
            setCursor(Qt::PointingHandCursor);
        else
            setCursor(Qt::ArrowCursor);

        QListView::mouseMoveEvent(event);
    }

private slots:
    void on_clicked(const QModelIndex& index)
    {
        if (index.isValid() && index.row() < files.size()) {
            // TODO: open from file
            // parentWidget()->parentWidget()->open_from_file(files[index.row()]);
        }
    }

private:
    QStringList files;
};
