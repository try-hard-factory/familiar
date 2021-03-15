//
// Created by max on 08.03.2021.
//

#ifndef FAMILIAR_CONTENTCONTROLLER_H
#define FAMILIAR_CONTENTCONTROLLER_H
#include <gdk/gdkkeysyms.h>
#include <gtkmm/drawingarea.h>
#include <glibmm.h>
#include "Point.h"
#include "Logger.h"
#include "FocusPoints.h"
#include "utils.h"
#include "Rectangle.h"
#include "ImageWidget.h"

extern Logger logger;


template<typename T>
static bool isContainPoint(const T& primitive, const Point& p) {
    return primitive.contain(p);
}

static bool isContainPoint(const std::shared_ptr<Rectangle_t>& primitive, const Point& p) {
    return primitive->contain(p);
}

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

enum class EMovingState {
    eHold,
    eMultipleMove,
    eSingleMove
};


template<typename T>
class ContentController {
public:
    using TVector = std::vector<std::shared_ptr<Rectangle_t>>;

    ContentController() {

    }
    void addObject(const std::shared_ptr<Rectangle_t>& obj) {
        obj->x = (obj->x - mShift.x) / mScale;
        obj->y = (obj->y - mShift.y) / mScale;// \TODO: CanvasArea::on_dropped_file I should move thhis callback to ContentController class
        renderArray.emplace_back(obj);
    }

    void draw(const Cairo::RefPtr<Cairo::Context> &cr, const Gdk::Rectangle& all, const Point& CtxSize);

    void init(const Glib::RefPtr<Gdk::Display>& d, const Glib::RefPtr<Gdk::Window>& w) {
        display = d;
        window = w;
        listTargets.push_back(Gtk::TargetEntry("image/png"));
        listTargets.push_back(Gtk::TargetEntry("text/plain"));
        listTargets.push_back(Gtk::TargetEntry("text/uri-list"));

        Gtk::Clipboard::get()->signal_owner_change().connect(sigc::mem_fun(*this, &ContentController<Rectangle_t>::on_clipboard_owner_change) );
    }

    void motionNotifyEvent(GdkEventMotion* event);
    void scrollEvent(GdkEventScroll* event);
    void buttonPressEvent(GdkEventButton* event);
    void buttonReleaseEvent(GdkEventButton* event);

    void keyPressEvent(GdkEventKey* event);
    void keyReleaseEvent(GdkEventKey* event);

    bool detectCollision(const Point& tPoint);
    void checkFocus(const Point& tPoint);
    void resetAllFocuses();
    bool isObjectFocused(const Point& tPoint);
    void checkMultiFocus(const Point &tPoint);

    void on_clipboard_owner_change(GdkEventOwnerChange* event);
private:
    void onClipboardGet(Gtk::SelectionData& selection_data, guint);
    void onClipboardClear();
    void on_clipboard_received(const Gtk::SelectionData& selection_data);
    void on_clipboard_received_targets(const std::vector<Glib::ustring>& targets);
    void on_clipboard_image_received(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf);

    enum ECursorIdx {
        eDefault = 0,
        eNWSEresize = 1,
        eNESWresize = 2,
    };
    static inline std::array<std::string, 3> cursors = {"default", "nwse-resize","nesw-resize"};
    void drawFocus(const Cairo::RefPtr<Cairo::Context> &cr);
    uint16_t countOfFocusedObj() const;
    void highlightFocus(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h);
    void highlightPointsOfFocus(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h);
    void calcDirectionOfFocusedObjects();
    void renderCursor(const Point& mouse_pos);
    std::vector<std::shared_ptr<Rectangle_t>> renderArray;
    Point mMousePos;
    Point mShift {.0,.0};
    double mScale { 1.0 };
    Point mPressPoint {.0, .0};
    Point mReleasePoint {.0,.0};
    Point mShiftStart {.0,.0};
    Color mMouseColor{ .5, .5, .5 };
    bool mShiftInit { true };
    Collision_t mCollision;
    EMovingState moving_state{EMovingState::eHold};
    Point mCtxSize {.0, .0};

    Glib::RefPtr<Gdk::Display> display;
    Glib::RefPtr<Gdk::Window> window;
    FocusPoints  focus_points;
    std::vector<Gtk::TargetEntry> listTargets;
    bool insertFlag { false };


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
    for (auto& a : renderArray) {
        if ( ( mCollision.nIndex == i++ ) &&
             ( mCollision.eWhat == Collision_t::EWhat::Rect ) )
            cr->set_source_rgb( .9, .0, .0 );
        else
            cr->set_source_rgb( .0, .9, .0 );
        a->draw(cr);
    }
    drawFocus(cr);

//    cr->set_source_rgb( mMouseColor.r, mMouseColor.b, mMouseColor.b );
//    cr->arc(mMousePos.x, mMousePos.y, 11, 0, 2*M_PI);
//    cr->fill();
}

template<typename T>
void ContentController<T>::motionNotifyEvent(GdkEventMotion *event) {
    mMousePos = (*event - mShift) / mScale;
    renderCursor(mMousePos);

    if (event->type & GDK_MOTION_NOTIFY ) {
        if (event->state & GDK_BUTTON1_MASK) {
            switch ( mCollision.eWhat  )
            {
                case Collision_t::EWhat::Rect:
                    moving_state = EMovingState::eSingleMove;
                    renderArray[mCollision.nIndex]->setNewPosition(mMousePos - mCollision.tOffset);

                    if (countOfFocusedObj() > 1) {
                        moving_state = EMovingState::eMultipleMove;
                        for (auto& obj : renderArray) {
                            if (!obj->isFocused || obj == renderArray[mCollision.nIndex]) continue;

                            obj->x = mMousePos.x + obj->x_move_dir;
                            obj->y = mMousePos.y + obj->y_move_dir;
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
            mCollision.tOffset = tPoint - *a.get();
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
                    if (mCollision.eWhat == Collision_t::EWhat::none) resetAllFocuses();
                    if ((countOfFocusedObj() <= 1) ||
                            (countOfFocusedObj() > 1 && mCollision.eWhat == Collision_t::EWhat::Rect && !isObjectFocused(mMousePos))) {
                        resetAllFocuses();
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

                    if (countOfFocusedObj() > 1 && moving_state != EMovingState::eMultipleMove) {
                        resetAllFocuses();
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
    moving_state = EMovingState::eHold;
}

template<typename T>
void ContentController<T>::resetAllFocuses() {
    for (auto& a : renderArray) { a->isFocused = false; }
    focus_points.resetFocusPoints();
}

template<typename T>
void ContentController<T>::checkFocus(const Point &tPoint) {
    for (auto& a : renderArray) {
        if (isContainPoint(a, tPoint)) {
            a->isFocused = true;
            return;
        }
    }
}

template<typename T>
void ContentController<T>::checkMultiFocus(const Point &tPoint) {
    for (auto& a : renderArray) {
        if (isContainPoint(a, tPoint)) {
            if (a->isFocused == true) {
                a->isFocused = false;
                std::cout<<a->isFocused<<'\n';
            } else {
                a->isFocused = true;
                std::cout<<a->isFocused<<'\n';
            }
        }
    }
}


template<typename T>
void ContentController<T>::drawFocus(const Cairo::RefPtr<Cairo::Context> &cr) {
    cr->set_source_rgb( .0, .0, .0 );

    double minx = INT_MAX;
    double miny = INT_MAX;
    double maxx = INT_MIN;
    double maxy = INT_MIN;

    for (const auto& obj : renderArray) {
        if (!obj->isFocused) continue;
        // line crossing the whole window
        highlightFocus(cr, obj->x, obj->y, obj->w, obj->h);

        minx = std::min(minx, obj->x);
        miny = std::min(miny, obj->y);
        maxx = std::max(maxx, obj->x + obj->w);
        maxy = std::max(maxy, obj->y + obj->h);
    }
    //LOG_DEBUG(logger, minx, " ", miny, " ", maxx, " ", maxy);
    //std::cout<<"COUNT OF FOCUSED: "<<countOfFocusedObj()<<'\n';
    if (countOfFocusedObj() > 1) {
        highlightFocus(cr, minx, miny, maxx - minx, maxy - miny);
    }
    if (countOfFocusedObj()) {
        highlightPointsOfFocus(cr, minx, miny, maxx - minx, maxy - miny);
    }
}

template<typename T>
uint16_t ContentController<T>::countOfFocusedObj() const {
    return std::count_if(std::begin(renderArray), std::end(renderArray), [](const auto& obj){return obj->isFocused;});
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
void ContentController<T>::highlightPointsOfFocus(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h) {
    cr->arc(x + w, y, 3/mScale, 0, 2*M_PI);
    cr->fill();
    cr->arc(x + w, y + h, 3/mScale, 0, 2*M_PI);
    cr->fill();
    cr->arc(x, y + h, 3/mScale, 0, 2*M_PI);
    cr->fill();
    cr->arc(x, y, 3/mScale, 0, 2*M_PI);
    cr->fill();

    focus_points.setFocusePoints({x, y + h}, {x, y}, {x + w, y}, {x + w, y + h});
}

template<typename T>
void ContentController<T>::calcDirectionOfFocusedObjects() {
    auto transformedPress = (mPressPoint - mShift)/mScale;

    for (auto& obj : renderArray) {
        if (!obj->isFocused) continue;
        obj->x_move_dir = obj->x - transformedPress.x;
        obj->y_move_dir = obj->y - transformedPress.y;
    }
}

template<typename T>
bool ContentController<T>::isObjectFocused(const Point &tPoint) {
    for (auto& obj : renderArray) {
        if (isContainPoint(obj, tPoint)) {
           return obj->isFocused;
        }
    }
    return false;
}

template<typename T>
void ContentController<T>::renderCursor(const Point &mouse_pos) {
    auto focus_point_idx = focus_points.isMouseNear(mouse_pos);
    ECursorIdx cursor_idx = eDefault;
    if (focus_point_idx.has_value()) {
        if (focus_point_idx.value() % 2 == 0) {
            cursor_idx = eNESWresize;
        } else {
            cursor_idx = eNWSEresize;
        }
    }

    auto new_cursor = Gdk::Cursor::create(display, cursors[cursor_idx]);
    window->set_cursor(new_cursor);
}

template<typename T>
void ContentController<T>::keyPressEvent(GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Shift_L) std::cout<<"L SHIFT press"<<'\n';
}

template<typename T>
void ContentController<T>::keyReleaseEvent(GdkEventKey *event) {
    if (event->keyval == GDK_KEY_Shift_L) {
        std::cout<<"L SHIFT release"<<'\n';
        if (insertFlag) {
            Gtk::Clipboard::get()->request_contents("ContentController_INSERT",
                                                    sigc::mem_fun(*this, &ContentController::on_clipboard_received));
        }
        //Gtk::Clipboard::get()->set(listTargets, sigc::mem_fun(*this, &ContentController::onClipboardGet), sigc::mem_fun(*this, &ContentController::onClipboardClear) );
    }
}


template<typename T>
void ContentController<T>::on_clipboard_received(const Gtk::SelectionData& selection_data)
{
    //Glib::ustring clipboard_data = selection_data.get_data_as_string();
    //Do something with the pasted data.
    const std::string target = selection_data.get_target();
    if ("ContentController_INSERT" == target) {
        Gtk::Clipboard::get()->request_image(sigc::mem_fun(*this, &ContentController::on_clipboard_image_received));
    }

    const std::string data_type = selection_data.get_data_type();
    std::cout<<target<<" "<<selection_data.get_data_type()<<'\n';
}

template<typename T>
void ContentController<T>::on_clipboard_image_received(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
{
    std::cout<<"image, " <<pixbuf->get_width() << " " <<pixbuf->get_height() <<'\n';
    auto image_widget = std::make_shared<ImageWidget>( 0,0,pixbuf->get_width(),  pixbuf->get_height(), pixbuf);
    addObject(image_widget);
}

template<typename T>
void ContentController<T>::onClipboardGet(Gtk::SelectionData& selection_data, guint)
{
    //info is meant to indicate the target, but it seems to be always 0,
    //so we use the selection_data's target instead.

    const std::string target = selection_data.get_target();
    const std::string data_type = selection_data.get_data_type();
    std::cout<<target<<" "<<selection_data.get_data_type()<<'\n';
//    if(target == example_target_custom)
//    {
//        // This set() override uses an 8-bit text format for the data.
//        selection_data.set(example_target_custom, m_ClipboardStore);
//    }
//    else if(target == example_target_text)
//    {
//        //Build some arbitrary text representation of the data,
//        //so that people see something when they paste into a text editor:
//        Glib::ustring text_representation;
//
//        text_representation += m_ButtonA1.get_active() ? "A1, " : "";
//        text_representation += m_ButtonA2.get_active() ? "A2, " : "";
//        text_representation += m_ButtonB1.get_active() ? "B1, " : "";
//        text_representation += m_ButtonB2.get_active() ? "B2, " : "";
//
//        selection_data.set_text(text_representation);
//    }
//    else
//    {
//        g_warning("ExampleWindow::on_clipboard_get(): "
//                  "Unexpected clipboard target format.");
//    }
}

template<typename T>
void ContentController<T>::onClipboardClear()
{
    //This isn't really necessary. I guess it might save memory.
//    m_ClipboardStore.clear();
}
template<typename T>
void ContentController<T>::on_clipboard_owner_change(GdkEventOwnerChange* event) {
    std::cout << "Owner: " << event->owner
              << ", window: " << event->window
              << ", type: " << event->type
              << std::endl;
    auto refClipboard = Gtk::Clipboard::get();

    //Discover what targets are available:
    refClipboard->request_targets(sigc::mem_fun(*this, &ContentController::on_clipboard_received_targets) );
//    std::vector<Gtk::TargetEntry> listTargets;
//    listTargets.push_back(Gtk::TargetEntry("text/uri-list"));
//    listTargets.push_back(Gtk::TargetEntry("image/png"));
//    listTargets.push_back(Gtk::TargetEntry("image/jpeg"));
//    listTargets.push_back(Gtk::TargetEntry("text/plain"));
//    Gtk::Clipboard::get()->set(listTargets, sigc::mem_fun(*this, &CanvasArea::on_clipboard_get), sigc::mem_fun(*this, &CanvasArea::on_clipboard_clear) );
//    Gtk::Clipboard::get()->request_text(sigc::mem_fun(*this, &CanvasArea::on_clipboard_text_received));
//    Gtk::Clipboard::get()->request_image(sigc::mem_fun(*this, &CanvasArea::on_clipboard_image_received));
    //gtk_selection_data_get_data_type()

}

template<typename T>
void ContentController<T>::on_clipboard_received_targets(const std::vector<Glib::ustring> &targets) {
    for (auto& it : targets) std::cout<<it<<'\n';
    insertFlag = std::find(targets.begin(), targets.end(),"image/png") != targets.end();
}


#endif //FAMILIAR_CONTENTCONTROLLER_H
