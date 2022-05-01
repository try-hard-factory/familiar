/**
* \file shortcuts_widget.h
* \author max
* Created on Sun May  1 14:10:54 2022
*/

#ifndef SHORTCUTS_WIDGET_H
#define SHORTCUTS_WIDGET_H

#include <QStringList>
#include <QVector>
#include <QWidget>

class QTableWidget;
class QVBoxLayout;

class ShortcutsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShortcutsWidget(QWidget* parent = nullptr);

private:
    void initInfoTable();
    void loadShortCuts();
    void appendShortcut(QString const& shortcutName, QString const& description);

private slots:
    void populateInfoTable();
    void onShortcutCellClicked(int, int);

private:
    QTableWidget* table_ = nullptr;
    QVBoxLayout* layout_ = nullptr;
    QList<QStringList> shortcuts_;
};

#endif
