//
// Created by max on 08.03.2021.
//

#ifndef FAMILIAR_RECTANGLE_H
#define FAMILIAR_RECTANGLE_H

struct Rectangle_t {
    double x{0};
    double y{0};
    double w{0};
    double h{0};
    bool isFocused{false};
    template<typename P>
    Rectangle_t operator -= (const P& p) { x -= p.x; y -= p.y; return *this; }
    template<typename P>
    Rectangle_t operator += (const P& p) { x += p.x; y += p.y; return *this; }
    template<typename P>
    Rectangle_t operator  = (const P& p) { x  = p.x; y  = p.y; return *this; }
    template<typename P>
    bool operator        == (const P& p) { return (x == p.x && y == p.y && w == p.w && h == p.h); }
    bool contain(const Point& p) const { return (p.x > x) && (p.y > y) && (p.x < x+w) && (p.y < y+h); }
};

#endif //FAMILIAR_RECTANGLE_H
