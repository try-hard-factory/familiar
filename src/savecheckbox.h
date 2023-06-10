#ifndef SAVECHECKBOX_H
#define SAVECHECKBOX_H

#include <QCheckBox>

class SaveCheckBox : public QCheckBox
{
    Q_OBJECT
public:
    SaveCheckBox(int id, QWidget* parent = 0);
    ~SaveCheckBox() = default;
    int id() const noexcept { return id_; };

private:
    int id_ = 0;
};

#endif // SAVECHECKBOX_H
