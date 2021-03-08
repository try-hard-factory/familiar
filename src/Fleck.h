//
// Created by max on 08.03.2021.
//

#ifndef FAMILIAR_FLECK_H
#define FAMILIAR_FLECK_H

#include <math.h>

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

#endif //FAMILIAR_FLECK_H
