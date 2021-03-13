//
// Created by max on 07.03.2021.
//

#ifndef FAMILIAR_CANVASAREA_H
#define FAMILIAR_CANVASAREA_H



#include "Point.h"
#include "Fleck.h"
#include "Rectangle.h"
#include "ContentController.h"

template<typename P, typename P1>
double distance(const P& a, const P1& b) {
    return sqrt( pow((a.x-b.x),2) + pow((a.y-b.y),2) );
}

template<typename T>
bool isContainPoint(const T& primitive, const Point& p) {
    return primitive.contain(p);
}



using PointsVector = std::vector<Point>;
using FleckVector = std::vector<Fleck>;
using RectangleVector = std::vector<Rectangle_t>;




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

        mContentController.addObject({-200,-200,50, 50});
        mContentController.addObject({-100,-100,50, 50});
        mContentController.addObject({00,00,50, 50});
//        mContentController.addObject({200,200,50, 50});
//        mContentController.addObject({100,100,50, 50});
//        mContentController.addObject({40,50,25, 25});
//        mContentController.addObject({240,320,30, 30});
//        mContentController.addObject({580,270,45, 45});
    }
    virtual ~CanvasArea() = default;
protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_scroll_event(GdkEventScroll* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
    void on_dropped_file(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);



    ContentController<Rectangle_t> mContentController;


private:
    Glib::RefPtr<Gdk::Pixbuf> image;
};


#endif //FAMILIAR_CANVASAREA_H
