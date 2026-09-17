#pragma once

#include "recent_files_model.h"
#include <algorithm>
#include <numeric>
#include <QCursor>
#include <QFileInfo>
#include <QListView>
#include <QMouseEvent>
#include <QSize>

class MainWindow;

class RecentFilesView : public QListView
{
    Q_OBJECT

public:
    RecentFilesView(QWidget* parent,
                    const QStringList& files,
                    MainWindow* mainWindow = nullptr)
        : QListView(parent)
        , files_(files)
        , mainWindow_(mainWindow)
    {
        connect(this, &QListView::clicked, this, &RecentFilesView::on_clicked);
        setModel(new RecentFilesModel(nullptr, files)); // TODO: memory leak?
        setMouseTracking(true);
    }

    void update_files(const QStringList& newFiles)
    {
        files_ = newFiles;
        // QItemSelectionModel* m = selectionModel();
        // setModel(new RecentFilesModel(nullptr, files));
        // delete m;
        auto* model = dynamic_cast<RecentFilesModel*>(this->model());
        model->setFiles(files_);
        reset();
    }

    QSize sizeHint() const override
    {
        // *std::max_element(...) below is UB on an empty range (returns
        // end(), dereferencing it) - WelcomeOverlay constructs this with
        // an empty files list up front (widgets/welcome_overlay.cpp) and
        // a fresh install/cleared recent-files list keeps it empty, so
        // this isn't a hypothetical edge case - confirmed real crash
        // (EXCEPTION_ACCESS_VIOLATION) triggered by Ctrl+N (insert_text)
        // while the welcome overlay with zero recent files was showing.
        if (files_.isEmpty()) {
            return QSize(0, 0);
        }

        int height
            = std::accumulate(files_.begin(),
                              files_.end(),
                              0,
                              [this](int sum, const QString& file) {
                                  return sum
                                         + sizeHintForRow(
                                             this->model()
                                                 ->index(static_cast<int>(
                                                             this->files_
                                                                 .indexOf(file)),
                                                         0)
                                                 .row())
                                         + 2;
                              });

        std::vector<int> columnWidths(static_cast<std::size_t>(files_.size()));
        std::ranges::transform(
            files_, columnWidths.begin(), [this](const QString& file) {
                return sizeHintForColumn(
                    model()
                        ->index(static_cast<int>(files_.indexOf(file)), 0)
                        .column());
            });
        int width = 2 + *std::ranges::max_element(columnWidths);
        return QSize(width, height);
    }

protected:
    void mouseMoveEvent(QMouseEvent* event) override
    {
        QModelIndex index = indexAt(event->position().toPoint());
        if (index.isValid()) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }

        QListView::mouseMoveEvent(event);
    }

private slots:
    // Defined in recent_files_view.cpp (needs mainwindow.h's full
    // definition to reach FileActions - not included here to avoid
    // pulling that into every widget that includes this header).
    void on_clicked(const QModelIndex& index);

private:
    QStringList files_;
    MainWindow* mainWindow_;
};
