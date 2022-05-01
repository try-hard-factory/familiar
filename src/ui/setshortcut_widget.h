/**
* \file setshortcut_widget.h
* \author max
* Created on Sun May  1 16:20:23 2022
*/

#ifndef SETSHORTCUT_WIDGET_H
#define SETSHORTCUT_WIDGET_H

#include <QDialog>
#include <QKeySequence>
#include <QObject>

class QVBoxLayout;

class SetShortcutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SetShortcutDialog(QDialog* parent = nullptr,
                               QString shortcutName = "");
    const QKeySequence& shortcut();

public:
    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent* event);

signals:

private:
    QVBoxLayout* layout_;
    QString modifier_;
    QKeySequence ks_;
};

#endif
