//
// Created by max on 07.03.2021.
//

#include "CanvasArea.h"
#include <gdkmm/general.h> // set_source_pixbuf()
#include <iostream>

bool CanvasArea::on_draw(const Cairo::RefPtr<Cairo::Context> &cr) {
    Gtk::Allocation allocation{ get_allocation() };
    auto const width { (double)allocation.get_width() };
    auto const height { (double)allocation.get_height() };

    // - scale picture to destination size
    Glib::RefPtr<Gdk::Pixbuf>       imageS = image->scale_simple( 180, 180, Gdk::INTERP_BILINEAR);
    // - place scaled pictures to specified position in render context
    Gdk::Cairo::set_source_pixbuf(cr, imageS, width/2-90, height/2-90 );
    // - open a hole for the pixels
    cr->rectangle( width/2-90, height/2-90, 180, 180 );
    // - show the hole
    cr->fill();

    auto const all        { get_allocation() };
    auto const CtxSize { Point { (double)all.get_width(),
                                     (double)all.get_height() } };

    static auto tHome{ Point { CtxSize }/2 };

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
    matrix.translate(mShift.x, mShift.y);

    cr->transform(matrix);



    cr->set_source_rgb( .0,.0,.0 );
    cr->set_line_width(3);
    if ( mMouseTrail.size() )
    {
        cr->move_to(mMouseTrail[0].x,mMouseTrail[0].y);
        for (auto const & a:mMouseTrail)
        {
            cr->line_to( a.x, a.y);
        }
        cr->stroke();
    }

    auto const ic{Collision(mMousePos)};
    int i{0};
    for ( auto const & a:mFleck )
    {
        if ( ic == i++ )
            cr->set_source_rgb( .9, .0, .0 );
        else
            cr->set_source_rgb( .0, .9, .0 );
        cr->arc(a.x, a.y, a.r, 0, 2*M_PI);
        cr->fill();
    }


    cr->set_source_rgb( mMouseColor.r, mMouseColor.b, mMouseColor.b );
    cr->arc(mMousePos.x, mMousePos.y, 11, 0, 2*M_PI);
    cr->fill();

//    // draw a blue circle at last mouse position
//    cr->set_source_rgb(.0,.0,.9);
//    cr->arc(mMousePos.x, mMousePos.y, 5, 0, 2*M_PI);
//    cr->fill();
//
//    cr->set_source_rgb( .0,.0,.0 );
//    cr->set_line_width(3);
//    if ( mMouseTrail.size() )
//    {
//        cr->move_to(mMouseTrail[0].x,mMouseTrail[0].y);
//        for (auto const & a:mMouseTrail)
//        {
//            cr->line_to( a.x, a.y);
//        }
//        cr->stroke();
//    }
//
//    auto const ic{Collision(mMousePos)};
//    int i{0};
//    for ( auto const & a:mFleck )
//    {
//        if ( ic == i++ ) {
//            cr->set_source_rgb(.9, .0, .0);
//        } else {
//            cr->set_source_rgb(.0, .9, .0);
//        }
//        cr->arc(a.x, a.y, a.r, 0, 2*M_PI);
//        cr->fill();
//    }
//
//    cr->set_source_rgb( mMouseColor.r, mMouseColor.b, mMouseColor.b );
//    cr->arc(mMousePos.x, mMousePos.y, 5, 0, 2*M_PI);
//    cr->fill();
    return true;
}

bool CanvasArea::on_motion_notify_event(GdkEventMotion* event) {
    mMousePos = *event - mShift;
    if ((event->type & GDK_MOTION_NOTIFY ) && (event->state & GDK_BUTTON3_MASK)) {
        mShift = mShiftStart - (mEventPress - *event);
    }
    queue_draw();
    return true;
}

bool CanvasArea::on_scroll_event(GdkEventScroll* event) {
    if ( event->delta_y>0 ) {
        mMouseColor = {.9, .0, .0};
    } else {
        mMouseColor = {.0, .9, .0};
    }

    queue_draw();
    return true;
}

bool CanvasArea::on_button_press_event(GdkEventButton* event) {
    mMouseColor = { .0, .0, .9};
    if (event->type == GDK_BUTTON_PRESS) {
        mEventPress = *event;
        mShiftStart = mShift;
    }
    queue_draw();
    return true;
}

bool CanvasArea::on_button_release_event(GdkEventButton* event) {
    if ((event->type & GDK_MOTION_NOTIFY ) && (event->state & GDK_BUTTON1_MASK)) {
        mMouseColor = { .5,.5,.5 };
        mMouseTrail.emplace_back( Point{ *event - mShift } );
    }

    queue_draw();
    return true;
}

int CanvasArea::Collision(const Point& tPoint) {
    int i{0};
    for ( auto& a : mFleck )
    {
        if ( distance(a, tPoint) < a.r )
        {
            return std::move(i);
        }
        ++i;
    }
    return -1;
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
