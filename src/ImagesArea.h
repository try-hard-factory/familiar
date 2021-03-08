//
// Created by max on 07.03.2021.
//

#ifndef FAMILIAR_IMAGESAREA_H
#define FAMILIAR_IMAGESAREA_H

#include <gtkmm/drawingarea.h>
#include <gdkmm/pixbuf.h>


class ImagesArea : public Gtk::DrawingArea
{
public:
    ImagesArea();
    virtual ~ImagesArea();

protected:
    //Override default signal handler:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    Glib::RefPtr<Gdk::Pixbuf> m_image;
};

#endif //FAMILIAR_IMAGESAREA_H
