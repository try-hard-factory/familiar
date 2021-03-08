//
// Created by max on 07.03.2021.
//

#ifndef FAMILIAR_CANVASAREA_H
#define FAMILIAR_CANVASAREA_H

#include <gtkmm/drawingarea.h>
#include <glibmm.h>

struct Point
{
    Point() = default;
    Point(const Point& src) = default;
    template<typename T>
    Point(const T& x, const T& y) : x(x), y(y) {}
    template<typename T>
    Point(const T& t) : x(t.x), y(t.y) {}

    double x{0};
    double y{0};

    template<typename P>
    Point operator  = (const P& p) { x  = p.x; y  = p.y; return *this; }
    template<typename P>
    Point operator -= (const P& p) { x -= p.x; y -= p.y; return *this; }
    template<typename P>
    Point operator += (const P& p) { x += p.x; y += p.y; return *this; }
};

inline bool operator == (const Point& p1, const Point& p2) { return (p1.x == p2.x) && (p1.y == p2.y ); }
inline bool operator != (const Point& p1, const Point& p2) { return !(p1 == p2); }
inline Point operator - (const Point& p1, const Point& p2) { return {p1.x-p2.x, p1.y-p2.y}; }
inline Point operator + (const Point& p1, const Point& p2) { return {p1.x+p2.x, p1.y+p2.y}; }
inline Point operator / (const Point& p, const double& d) { return {p.x/d, p.y/d}; }
inline Point operator * (const Point& p, const double& d) { return {p.x*d, p.y*d}; }

template<typename P, typename P1>
double distance(const P& a, const P1& b) {
    return sqrt( pow((a.x-b.x),2) + pow((a.y-b.y),2) );
}

struct Fleck {
    double x{0};
    double y{0};
    double r{0};
    template<typename P>
    Fleck operator -= (const P& p) { x -= p.x; y -= p.y; return *this; }
    template<typename P>
    Fleck operator += (const P& p) { x += p.x; y += p.y; return *this; }
    template<typename P>
    Fleck operator  = (const P& p) { x  = p.x; y  = p.y; return *this; }

    bool contain(const Point& p) { return sqrt( pow((x-p.x),2) + pow((y-p.y),2)) < r; }
};

struct Rectangle_t {
    double x{0};
    double y{0};
    double w{0};
    double h{0};
    template<typename P>
    Rectangle_t operator -= (const P& p) { x -= p.x; y -= p.y; return *this; }
    template<typename P>
    Rectangle_t operator += (const P& p) { x += p.x; y += p.y; return *this; }
    template<typename P>
    Rectangle_t operator  = (const P& p) { x  = p.x; y  = p.y; return *this; }

    bool contain(const Point& p) const { return (p.x > x) && (p.y > y) && (p.x < x+w) && (p.y < y+h); }
};

struct Color {
    double r{0};
    double g{0};
    double b{0};
};

using PointsVector = std::vector<Point>;
using FleckVector = std::vector<Fleck>;
using RectangleVector = std::vector<Rectangle_t>;


template<typename T>
bool isContainPoint(const T& primitive, const Point& p) {
    return primitive.contain(p);
}

class CanvasArea : public Gtk::DrawingArea {
public:
    CanvasArea() {
        add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
        add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
        add_events(Gdk::BUTTON1_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::POINTER_MOTION_MASK);
        add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);

        std::vector<Gtk::TargetEntry> listTargets;
        listTargets.push_back(Gtk::TargetEntry("text/uri-list"));
        drag_dest_set(listTargets, (Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP), (Gdk::ACTION_COPY | Gdk::ACTION_MOVE));

        signal_drag_data_received().connect(sigc::mem_fun(*this, &CanvasArea::on_dropped_file));
        // picture png quadratic
        // - load picture
        image  = Gdk::Pixbuf::create_from_file("../../src/ui/kot1.png");
    }
    virtual ~CanvasArea() = default;
protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_scroll_event(GdkEventScroll* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_release_event(GdkEventButton* event) override;

    void on_dropped_file(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);


    int Collision(const Point& tPoint);

    Point mCtxSize {.0, .0};
    double mScale { 1.0 };
    bool mShiftInit { true };
    Point mShift {.0,.0};
    Point mEventPress {.0,.0};
    Point mEventRelease {.0,.0};
    Point mShiftStart {.0,.0};

    Point mMousePos;
    Color mMouseColor{ .5, .5, .5 };
    PointsVector mMouseTrail;
    FleckVector mFleck {
            {30,30,20}, {300,300,50}, {500,200,40},
            {40,50,25}, {240,320,30}, {580,270,45}
    };
    RectangleVector mRectangles {
            {30,30,20, 20}, {300,300,50, 50}, {500,200,40, 40},
            {40,50,25, 25}, {240,320,30, 30}, {580,270,45, 45}
    };
    struct Collision_t {
        Point tWhere { .0, .0 };
        Point tOffset { .0, .0 };
        enum class EWhat
        {
            none,       // there was no collision
            Fleck,      // move a Fleck
            Line,       // move a Line
        }eWhat {EWhat::none};
        int nIndex {0};	// O: L1, L2, L3
        int nSubIx {0};	// L: P1, PM, P2
    } mCollision;
private:
    Glib::RefPtr<Gdk::Pixbuf> image;
};


#endif //FAMILIAR_CANVASAREA_H
