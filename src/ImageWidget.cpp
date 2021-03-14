//
// Created by max on 14.03.2021.
//

#include "ImageWidget.h"

void ImageWidget::draw(const Cairo::RefPtr<Cairo::Context> &cr) {
    Gdk::Cairo::set_source_pixbuf(cr, image, x, y );
    Rectangle_t::draw(cr);
}
