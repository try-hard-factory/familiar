//
// Created by max on 08.03.2021.
//

#ifndef FAMILIAR_CONTENTCONTROLLER_H
#define FAMILIAR_CONTENTCONTROLLER_H
#include <gtkmm/drawingarea.h>
#include <glibmm.h>
#include "Point.h"
#include "Logger.h"

extern Logger logger;

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
        renderArray.emplace_back(obj);
    }

    void draw(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle& all, const Point& CtxSize);

    void motionNotifyEvent(GdkEventMotion* event);
    void scrollEvent(GdkEventScroll* event);
    void buttonPressEvent(GdkEventButton* event);
    void buttonReleaseEvent(GdkEventButton* event);

    bool detectCollision(const Point& tPoint);
    void checkFocus(const Point& tPoint);
    void checkMultiFocus(const Point &tPoint);
private:
    void drawFocus(const Cairo::RefPtr<Cairo::Context> &cr);
    uint16_t countOfFocusedObj() const;
    void highlightFocus(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h);
    void calcDirectionOfFocusedObjects();
    TVector renderArray;
    Point mMousePos;
    Point mShift {.0,.0};
    double mScale { 1.0 };
    Point mPressPoint {.0, .0};
    Point mReleasePoint {.0,.0};
    Point mShiftStart {.0,.0};
    Color mMouseColor{ .5, .5, .5 };
    bool mShiftInit { true };
    Collision_t mCollision;

    Point mCtxSize {.0, .0};
};

template<typename T>
void ContentController<T>::draw(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle& all, const Point& CtxSize) {
    mCtxSize = CtxSize;
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
    for (const auto& a : renderArray) {
        if ( ( mCollision.nIndex == i++ ) &&
             ( mCollision.eWhat == Collision_t::EWhat::Rect ) )
            cr->set_source_rgb( .9, .0, .0 );
        else
            cr->set_source_rgb( .0, .9, .0 );
        cr->rectangle(a.x, a.y, a.w, a.h);
        cr->fill();
    }
    drawFocus(cr);

    cr->set_source_rgb( mMouseColor.r, mMouseColor.b, mMouseColor.b );
    cr->arc(mMousePos.x, mMousePos.y, 11, 0, 2*M_PI);
    cr->fill();
}

template<typename T>
void ContentController<T>::motionNotifyEvent(GdkEventMotion *event) {
    mMousePos = (*event - mShift) / mScale;

    if (event->type & GDK_MOTION_NOTIFY ) {
        if (event->state & GDK_BUTTON1_MASK) {
            switch ( mCollision.eWhat  )
            {
                case Collision_t::EWhat::Rect:
                    renderArray[mCollision.nIndex] = mMousePos - mCollision.tOffset;
//                    //LOG_DEBUG(logger, mMousePos.x, " ", mMousePos.y);
//                    LOG_DEBUG(logger, mPressPoint.x, " ", mPressPoint.y);
                    if (countOfFocusedObj() > 1) {
                        for (auto& obj : renderArray) {
                            if (!obj.isFocused || obj == renderArray[mCollision.nIndex]) continue;

                            obj.x = mMousePos.x + obj.x_move_dir;
                            obj.y = mMousePos.y + obj.y_move_dir;
                            //LOG_DEBUG(logger, offset.x, " ", offset.y);
                        }
                    }
                    break;
                case Collision_t::EWhat::none:
                    //mShift = mShiftStart - (mPressPoint - *event);
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
    for (auto& a : renderArray) {
        if (isContainPoint(a, tPoint)) {
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
        mPressPoint = *event;
        calcDirectionOfFocusedObjects();
        mShiftStart = mShift;
        switch (event->button) {
            case 1:
//                LOG_DEBUG(logger, "BTN1 PRESS EVENT. X: ", event->x, ", Y: ", event->y);
                if (event->state & GDK_SHIFT_MASK) {
//                    LOG_DEBUG(logger, "SHIFT PRESS EVENT");
                } else {
                    if (countOfFocusedObj() <= 1) {
                        checkFocus(mMousePos);
                    }
                }
                break;
//            case 2:
//                LOG_DEBUG(logger, "BTN2 PRESS EVENT. X: ", event->x, ", Y: ", event->y);
//                if (event->state & GDK_SHIFT_MASK) {
//                    LOG_DEBUG(logger, "SHIFT PRESS EVENT");
//                }
//                break;
//            case 3:
//                LOG_DEBUG(logger, "BTN3 PRESS EVENT. X: ", event->x, ", Y: ", event->y);
//                if (event->state & GDK_SHIFT_MASK) {
//                    LOG_DEBUG(logger, "SHIFT PRESS EVENT");
//                }
//                //checkMultiFocus(mMousePos);
//                break;
        }
    } else {
        detectCollision(mMousePos);
    }
//    if (event->button = 3) {
//        switch (mCollision.eWhat) {
//            case Collision_t::EWhat::Rect : break;
//            case Collision_t::EWhat::none : break;
//        }
//    }
}

template<typename T>
void ContentController<T>::buttonReleaseEvent(GdkEventButton *event) {
    if (event->type & GDK_MOTION_NOTIFY ) {
        if (event->state & GDK_BUTTON1_MASK) {
            mMouseColor = { .5,.5,.5 };
            //mMouseTrail.emplace_back( Point{ *event - mShift }/mScale );
        }
    }

    if (event->type & GDK_BUTTON_PRESS) {
        switch (event->button) {
            case 1:
//                LOG_DEBUG(logger, "BTN1 RELEASE EVENT. X: ", event->x, ", Y: ", event->y);
                if (event->state & GDK_SHIFT_MASK) { //need check it first and call checkMultiple focus function
//                    LOG_DEBUG(logger, "SHIFT PRESS EVENT");
                    checkMultiFocus(mMousePos);
                    break;
                } else {
                    if (countOfFocusedObj() > 1) {
                        checkFocus(mMousePos);
                    }
                }
                break;
//            case 2:
//                LOG_DEBUG(logger, "BTN2 RELEASE EVENT. X: ", event->x, ", Y: ", event->y);
//                if (event->state & GDK_SHIFT_MASK) {
//                    LOG_DEBUG(logger, "SHIFT PRESS EVENT");
//                }
//                break;
//            case 3:
//                LOG_DEBUG(logger, "BTN3 RELEASE EVENT. X: ", event->x, ", Y: ", event->y);
//                if (event->state & GDK_SHIFT_MASK) {
//                    LOG_DEBUG(logger, "SHIFT PRESS EVENT");
//                }
//                break;
        }

    }
}

template<typename T>
void ContentController<T>::checkFocus(const Point &tPoint) {
    for (auto& a : renderArray) { a.isFocused = false; }

    for (auto& a : renderArray) {
        if (isContainPoint(a, tPoint)) {
            a.isFocused = true;
            return;
        }
    }
}

template<typename T>
void ContentController<T>::checkMultiFocus(const Point &tPoint) {
    for (auto& a : renderArray) {
        if (isContainPoint(a, tPoint)) {
            if (a.isFocused == true) {
                a.isFocused = false;
                std::cout<<a.isFocused<<'\n';
            } else {
                a.isFocused = true;
                std::cout<<a.isFocused<<'\n';
            }
        }
    }
}



template<typename T>
void ContentController<T>::drawFocus(const Cairo::RefPtr<Cairo::Context> &cr) {
    cr->set_source_rgb( .0, .0, .0 );

    double minx = 9999999999;// bad solution, need using screen coords
    double miny = 9999999999;// bad solution, need using screen coords
    double maxx = -9999999999;// bad solution, need using screen coords
    double maxy = -9999999999;// bad solution, need using screen coords

    for (const auto& obj : renderArray) {
        if (!obj.isFocused) continue;
        // line crossing the whole window
        highlightFocus(cr, obj.x, obj.y, obj.w, obj.h);

        minx = std::min(minx, obj.x);
        miny = std::min(miny, obj.y);
        maxx = std::max(maxx, obj.x + obj.w);
        maxy = std::max(maxy, obj.y + obj.h);
    }
    //LOG_DEBUG(logger, minx, " ", miny, " ", maxx, " ", maxy);
    //std::cout<<"COUNT OF FOCUSED: "<<countOfFocusedObj()<<'\n';
    if (countOfFocusedObj() > 1) {
        highlightFocus(cr, minx, miny, maxx - minx, maxy - miny);
    }
}

template<typename T>
uint16_t ContentController<T>::countOfFocusedObj() const {
    return std::count_if(std::begin(renderArray), std::end(renderArray), [](const auto& obj){return obj.isFocused;});
}

template<typename T>
void ContentController<T>::highlightFocus(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h) {
    auto line_w = 2/mScale;
    cr->set_line_width(line_w);
    cr->move_to(x, y);
    cr->line_to(x + w, y);
    cr->line_to(x + w, y + h);
    cr->line_to(x, y + h);
    cr->line_to(x, y);
    cr->stroke();
}

template<typename T>
void ContentController<T>::calcDirectionOfFocusedObjects() {
    auto transformedPress = (mPressPoint - mShift)/mScale;

    for (auto& obj : renderArray) {
        if (!obj.isFocused) continue;
        obj.x_move_dir = obj.x - transformedPress.x;
        obj.y_move_dir = obj.y - transformedPress.y;
    }
}

#endif //FAMILIAR_CONTENTCONTROLLER_H
