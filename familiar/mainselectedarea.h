#ifndef MAINSELECTEDAREA_H
#define MAINSELECTEDAREA_H

#include <QRectF>
#include <vector>

class MainSelectedArea
{
public:
    MainSelectedArea();

    inline bool isReady() const { return ready_; }

    inline void setReady(bool f) { ready_ = f; }

    QRectF getRect() { return rect_; }

    void setRect(const QRectF& r) { rect_ = r; }

private:
    bool ready_ = false;
    QRectF rect_;
};

#endif // MAINSELECTEDAREA_H
