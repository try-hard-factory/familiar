//
// Created by max on 14.03.2021.
//

#ifndef FAMILIAR_IMAGEWIDGET_H
#define FAMILIAR_IMAGEWIDGET_H

#include <glibmm.h>
#include <gdkmm/general.h>
#include "Rectangle.h"

class ImageWidget : public Rectangle_t {
public:
    ImageWidget(double x, double y, double w, double h, const Glib::RefPtr<Gdk::Pixbuf>& im) :
        Rectangle_t(x, y, w, h),
        image(im) {}

    void draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
private:
    Glib::RefPtr<Gdk::Pixbuf> image;
};


#endif //FAMILIAR_IMAGEWIDGET_H
