#pragma once

#include <QDialog>
#include <QString>

#include "fileio.h" // RawImportChoice

class QCheckBox;
class QMouseEvent;

// PureRef-style "this RAW file needs converting" prompt - shown by
// CanvasView::on_raw_import_choice_required() (canvasview.cpp) when
// ImageImportSession (fileio.h) pauses on a RAW file with no decided
// handling yet (Items/raw_import_choice == "ask", the default, and no
// in-queue decision made so far either - see ImageImportSession::
// setQueueChoice()/setOneShotChoice()). Modal; caller reads choice()/
// applyToQueue()/rememberChoice() only after exec() returns Accepted.
//
// Unlike PureRef's own reference dialog (its wording implies the
// original file bytes get literally embedded when you don't optimize),
// familiar's "Keep original" doesn't preserve the source .NEF/.CR3 file
// at all - both choices decode to a QImage that gets re-encoded for
// storage like any other picture (Items/image_storage_format), same as
// every other imported format. "Keep original" here means a full RAW
// demosaic (best quality, slow) instead of the fast embedded-preview
// path "Optimize image" uses - see fileio.h's decode_raw_preview()/
// decode_raw_full() for exactly what each does.
class RawImportDialog : public QDialog
{
    Q_OBJECT

public:
    RawImportDialog(QWidget* parent, const QString& filename);

    RawImportChoice choice() const { return choice_; }
    bool applyToQueue() const;
    bool rememberChoice() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    RawImportChoice choice_ = RawImportChoice::Optimize;
    QCheckBox* applyToQueueCheckbox_ = nullptr;
    QCheckBox* rememberCheckbox_ = nullptr;
};
