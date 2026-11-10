#pragma once

#include <QDialog>

class QCheckBox;
class QMouseEvent;

// Shown once, on the first launch that has no stored answer (see
// telemetry::needs_consent()). Explains what gets sent before anything
// is sent, and carries the checkbox that decides it.
//
// The checkbox starts in the state the build was configured with
// (telemetry::default_enabled()). Nothing is written to disk and no
// install id is generated until the user closes this dialog - declining
// leaves no identifier behind at all.
//
// Modal; stack-allocate it and read accepted_telemetry() after exec(),
// which is the same shape RestoreDefaultsDialog uses and for the same
// reason (WA_DeleteOnClose plus exec() can free the object before exec()
// returns - that dialog hit a real use-after-free that way).
//
// Closing with the × is not a third answer: it means "whatever the
// checkbox currently says", because there is no state in which the
// question can be left unanswered - the alternative is asking again on
// every launch.
class TelemetryConsentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TelemetryConsentDialog(QWidget* parent = nullptr);

    [[nodiscard]] bool accepted_telemetry() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QCheckBox* enabledCheckbox_ = nullptr;
};
