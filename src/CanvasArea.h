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
    Point(double const & x, double const & y) : x(x), y(y) {}
    template<typename T>
    Point(T const & t) : x(t.x), y(t.y) {}
    double x{0}, y{0};
    Point operator -= (Point const & p)
    {
        x -= p.x;
        y -= p.y;
        return *this;
    }
    Point operator += (Point const & p)
    {
        x += p.x;
        y += p.y;
        return *this;
    }
};

inline bool operator == (const Point& p1, const Point& p2) { return (p1.x == p2.x) && (p1.y == p2.y ); }
inline bool operator != (const Point& p1, const Point& p2) { return !(p1 == p2); }
inline Point operator - (const Point& p1, const Point& p2) { return {p1.x-p2.x, p1.y-p2.y}; }
inline Point operator + (const Point& p1, const Point& p2) { return {p1.x+p2.x, p1.y+p2.y}; }
inline Point operator / (const Point& p, const double& d) { return {p.x/d, p.y/d}; }
inline Point operator * (const Point& p, const double& d) { return {p.x*d, p.y*d}; }

struct Fleck {
    double x{0};
    double y{0};
    double r{0};
};

struct Color {
    double r{0};
    double g{0};
    double b{0};
};

using PointsVector = std::vector<Point>;
using FleckVector = std::vector<Fleck>;

template<typename P, typename P1>
double distance(const P& a, const P1& b) {
    return sqrt( pow((a.x-b.x),2) + pow((a.y-b.y),2) );
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

    bool mShiftInit { true };
    Point mShift {.0,.0};
    Point mEventPress {.0,.0};
    Point mShiftStart {.0,.0};

    Point mMousePos;
    Color mMouseColor{ .5, .5, .5 };
    PointsVector mMouseTrail;
    FleckVector mFleck {
        {30,30,20},
        {380,400,20},
        {500,200,40}
    };

private:
    Glib::RefPtr<Gdk::Pixbuf> image;
};


#endif //FAMILIAR_CANVASAREA_H
