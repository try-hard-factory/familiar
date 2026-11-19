#ifndef MODEL_TOOLBAR_H
#define MODEL_TOOLBAR_H

#include <QWidget>

class ModelItem;
class QToolButton;

// The button belongs to the selected canvas item, while the live 3D widget
// appears only after the user unlocks its rotation.
class ModelToolbar : public QWidget
{
    Q_OBJECT
public:
    explicit ModelToolbar(QWidget* parent = nullptr);

    void attach(ModelItem* item);
    ModelItem* item() const { return item_; }
    void set_editing(bool editing);
    bool is_editing() const { return editing_; }
    void restyle_from_preset();

signals:
    void edit_toggled(ModelItem* item, bool editing);

private:
    void update_button();

    ModelItem* item_ = nullptr;
    QToolButton* button_ = nullptr;
    bool editing_ = false;
};

#endif
