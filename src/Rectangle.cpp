//
// Created by max on 14.03.2021.
//

#include "Rectangle.h"

void Rectangle_t::draw(const Cairo::RefPtr<Cairo::Context> &cr) {
    cr->rectangle(x, y, w, h);
    cr->fill();
}

