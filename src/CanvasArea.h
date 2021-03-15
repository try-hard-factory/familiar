//
// Created by max on 07.03.2021.
//

#ifndef FAMILIAR_CANVASAREA_H
#define FAMILIAR_CANVASAREA_H



#include "Point.h"
#include "Fleck.h"
#include "Rectangle.h"
#include "ContentController.h"

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
        set_can_focus(true);


        add_events(Gdk::ENTER_NOTIFY_MASK | Gdk::LEAVE_NOTIFY_MASK);
        set_has_window(true);
        set_double_buffered(false);

        std::vector<Gtk::TargetEntry> listTargets;
        listTargets.push_back(Gtk::TargetEntry("text/uri-list"));
        listTargets.push_back(Gtk::TargetEntry("image/png"));
        listTargets.push_back(Gtk::TargetEntry("image/jpeg"));
        listTargets.push_back(Gtk::TargetEntry("text/plain"));





        drag_dest_set(listTargets, (Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP), (Gdk::ACTION_COPY | Gdk::ACTION_MOVE));
        signal_drag_data_received().connect(sigc::mem_fun(*this, &CanvasArea::on_dropped_file));


        // picture png quadratic
        // - load picture
        image  = Gdk::Pixbuf::create_from_file("../../src/ui/kot1.png");
    }
    virtual ~CanvasArea() = default;
protected:
    bool on_key_press_event(GdkEventKey* key_event) override;
    bool on_key_release_event(GdkEventKey* key_event) override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_scroll_event(GdkEventScroll* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
    void on_dropped_file(const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, const Gtk::SelectionData& selection_data, guint info, guint time);
    void on_realize() override;

    void on_clipboard_get(Gtk::SelectionData& selection_data, guint);
    void on_clipboard_clear();

    void on_clipboard_text_received(const Glib::ustring& text);

    ContentController<Rectangle_t> mContentController;


private:
    Glib::RefPtr<Gdk::Pixbuf> image;

};


#endif //FAMILIAR_CANVASAREA_H
