//
// Created by max on 08.03.2021.
//

#ifndef FAMILIAR_CONTENTCONTROLLER_H
#define FAMILIAR_CONTENTCONTROLLER_H
#include <gtkmm/drawingarea.h>
#include <glibmm.h>
#include "Point.h"

struct Color {
    double r{0};
    double g{0};
    double b{0};
};

struct Collision_t {
    Point tWhere { .0, .0 };
    Point tOffset { .0, .0 };
    enum class EWhat
    {
        none,       // there was no collision
        Rect,      // move a Fleck
    }eWhat {EWhat::none};
    int nIndex {0};	// O: L1, L2, L3
    int nSubIx {0};	// L: P1, PM, P2
};

template<typename T>
class ContentController {
public:
    using TVector = std::vector<T>;


    void addObject(const T& obj) {
        array.emplace_back(obj);
    }

    void draw(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle& all, const Point& CtxSize);

    void motionNotifyEvent(GdkEventMotion* event);
    void scrollEvent(GdkEventScroll* event);
    void buttonPressEvent(GdkEventButton* event);
    void buttonReleaseEvent(GdkEventButton* event);

    bool detectCollision(const Point& tPoint);

private:
    TVector array;
    Point mMousePos;
    Point mShift {.0,.0};
    double mScale { 1.0 };
    Point mEventPress {.0,.0};
    Point mShiftStart {.0,.0};
    Color mMouseColor{ .5, .5, .5 };
    bool mShiftInit { true };
    Collision_t mCollision;
};

template<typename T>
void ContentController<T>::draw(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle& all, const Point& CtxSize) {

    auto tHome{ Point { CtxSize }/2 };

    if ( mShiftInit )
    {
        tHome = mShift = CtxSize/2;
        mShiftInit = false;
    }
    auto const tSizeHalf{CtxSize/2};
    if ( tHome != tSizeHalf )
    {
        mShift -= tHome - tSizeHalf; tHome = tSizeHalf;
    }

    Cairo::Matrix matrix(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    matrix.scale(mScale, mScale);
    matrix.translate(mShift.x/mScale, mShift.y/mScale);
    cr->transform(matrix);

    int i{0};
    for (const auto& a : array) {
        if ( ( mCollision.nIndex == i++ ) &&
             ( mCollision.eWhat == Collision_t::EWhat::Rect ) )
            cr->set_source_rgb( .9, .0, .0 );
        else
            cr->set_source_rgb( .0, .9, .0 );
        cr->rectangle(a.x, a.y, a.w, a.h);
        cr->fill();
    }

    cr->set_source_rgb( mMouseColor.r, mMouseColor.b, mMouseColor.b );
    cr->arc(mMousePos.x, mMousePos.y, 11, 0, 2*M_PI);
    cr->fill();
}

template<typename T>
void ContentController<T>::motionNotifyEvent(GdkEventMotion *event) {
    mMousePos = (*event - mShift) / mScale;

    if (event->type & GDK_MOTION_NOTIFY ) {
        if (event->state & GDK_BUTTON3_MASK) {
            switch ( mCollision.eWhat  )
            {
                case Collision_t::EWhat::Rect:
                    array[mCollision.nIndex] = mMousePos - mCollision.tOffset;
                    break;
                case Collision_t::EWhat::none:
                    mShift = mShiftStart - (mEventPress - *event);
                    break;
            }
        } else {
            detectCollision(mMousePos);
        }
    }
}

template<typename T>
bool ContentController<T>::detectCollision(const Point &tPoint) {
    mCollision.eWhat = Collision_t::EWhat::none;
    int i{0};
    for ( auto& a : array )
    {
        if ( isContainPoint(a, tPoint) )
        {
            mCollision.tWhere = tPoint;
            mCollision.tOffset = tPoint - a;
            mCollision.eWhat = Collision_t::EWhat::Rect;
            mCollision.nIndex = i;
            return true;
        }
        ++i;
    }
    return false;
}

template<typename T>
void ContentController<T>::scrollEvent(GdkEventScroll* event) {
    if ( event->delta_y>0 ) {
        mMouseColor = {.9, .0, .0};
    } else {
        mMouseColor = {.0, .9, .0};
    }

    Point const p0{ (*event - mShift) / mScale };
    mScale *= (event->delta_y>0) ? 0.9 : 1.1;
    if (mScale < 0.01) mScale = 0.01;
    Point const p1{ (*event - mShift)/mScale };
    mShift -= (p0-p1)*mScale;
}

template<typename T>
void ContentController<T>::buttonPressEvent(GdkEventButton *event) {
    mMouseColor = {.0,.0,.9};
    if (event->type == GDK_BUTTON_PRESS) {
        mEventPress = *event;
        mShiftStart = mShift;
    } else {
        detectCollision(mMousePos);
    }

    if (event->button = 3) {
        switch (mCollision.eWhat) {
            case Collision_t::EWhat::Rect : break;
            case Collision_t::EWhat::none : break;
        }
    }
}

template<typename T>
void ContentController<T>::buttonReleaseEvent(GdkEventButton *event) {
    if (event->type & GDK_MOTION_NOTIFY ) {
        if (event->state & GDK_BUTTON1_MASK) {
            mMouseColor = { .5,.5,.5 };
            //mMouseTrail.emplace_back( Point{ *event - mShift }/mScale );
        }
    }
}

#endif //FAMILIAR_CONTENTCONTROLLER_H
