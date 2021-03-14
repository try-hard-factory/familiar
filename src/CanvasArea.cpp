//
// Created by max on 07.03.2021.
//

#include "CanvasArea.h"
#include <gdkmm/general.h> // set_source_pixbuf()
#include <iostream>
#include "Logger.h"

extern Logger logger;

void CanvasArea::on_realize() {
    Gtk::DrawingArea::on_realize();
    Glib::RefPtr<Gdk::Display> display = get_display();
    Glib::RefPtr<Gdk::Window> window = get_window();
    mContentController.init(display, window);
}

bool CanvasArea::on_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
    Gtk::Allocation allocation{ get_allocation() };
    auto const width { (double)allocation.get_width() };
    auto const height { (double)allocation.get_height() };

    // - scale picture to destination size
    Glib::RefPtr<Gdk::Pixbuf>       imageS = image->scale_simple( 180, 180, Gdk::INTERP_BILINEAR);
    // - place scaled pictures to specified position in render context

    Gdk::Cairo::set_source_pixbuf(cr, imageS, 0, 0 );
    // - open a hole for the pixels


    cr->set_line_width(3);
    cr->move_to(100, 0);
    cr->line_to(200, 100);
    cr->line_to(100, 200);
    cr->line_to(0, 100);

    //cr->rectangle( width/2-90, height/2-90, 180, 180 );
    // - show the hole
    cr->fill();

    auto const all        { get_allocation() };
    auto const CtxSize { Point { (double)all.get_width(),
                                     (double)all.get_height() } };


    mContentController.draw(cr, all, CtxSize);

    return true;
}

bool CanvasArea::on_motion_notify_event(GdkEventMotion* event) {
    mContentController.motionNotifyEvent(event);
    queue_draw();
    return true;
}

bool CanvasArea::on_scroll_event(GdkEventScroll* event) {
    mContentController.scrollEvent(event);
    queue_draw();
    return true;
}

bool CanvasArea::on_button_press_event(GdkEventButton* event) {
    mContentController.buttonPressEvent(event);
    queue_draw();
    return true;
}

bool CanvasArea::on_button_release_event(GdkEventButton* event) {
    mContentController.buttonReleaseEvent(event);
    queue_draw();
    return true;
}


void CanvasArea::on_dropped_file(const Glib::RefPtr<Gdk::DragContext> &context, int x, int y,
                                 const Gtk::SelectionData &selection_data, guint info, guint time) {
    if ((selection_data.get_length() >= 0) && (selection_data.get_format() == 8))
    {
        std::vector<Glib::ustring> file_list;

        file_list = selection_data.get_uris();

        if (file_list.size() > 0)
        {
            for (auto& it :file_list) {
                auto path = Glib::filename_from_uri(it);
                //do something here with the 'filename'. eg open the file for reading
                std::cout << "PATH: " << path << '\n';
                std::cout << "x: " << x << '\n';
                std::cout << "y: " << y << '\n';
                image.reset();
                context->drag_finish(true, false, time);
                image  = Gdk::Pixbuf::create_from_file(path);
                queue_draw();
            }

            return;
        }
    }

    context->drag_finish(false, false, time);
}
