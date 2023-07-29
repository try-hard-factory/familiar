#pragma once
#include <QDebug>

template<typename Mixin, typename T>
class MainControlsMixin : public T
{
public:

    explicit MainControlsMixin(T* parent = nullptr)
        : T(parent)
    {}

    void init_main_control()
    {
        this->setContextMenuPolicy(Qt::CustomContextMenu);
        // self.customContextMenuRequested.connect(
        //     self.control_target.on_context_menu)
        this->setAcceptDrops(true);
    }

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        const auto* mimedata = event->mimeData();
        qDebug() << "dragEnterEvent: " << mimedata->;
        if (mimedata->hasUrls()) {
            event->acceptProposedAction();
        } else if (mimedata->hasImage()) {
            event->acceptProposedAction();
        } else {
            qDebug() << "Unknown mime data";
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent *event) override
    {
        
    }
};