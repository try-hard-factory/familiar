//
// Created by max on 08.03.2021.
//

#ifndef FAMILIAR_POINT_H
#define FAMILIAR_POINT_H

struct Point
{
    Point() = default;
    Point(const Point& src) = default;

    template<typename T>
    Point(const T& x, const T& y) : x(x), y(y) {}

    template<typename T>
    Point(const T& t) : x(t.x), y(t.y) {}

    double x{0};
    double y{0};

    template<typename P>
    Point operator  = (const P& p) { x  = p.x; y  = p.y; return *this; }
    template<typename P>
    Point operator -= (const P& p) { x -= p.x; y -= p.y; return *this; }
    template<typename P>
    Point operator += (const P& p) { x += p.x; y += p.y; return *this; }
};

inline bool operator == (const Point& p1, const Point& p2) { return (p1.x == p2.x) && (p1.y == p2.y ); }
inline bool operator != (const Point& p1, const Point& p2) { return !(p1 == p2); }
inline Point operator - (const Point& p1, const Point& p2) { return {p1.x-p2.x, p1.y-p2.y}; }
inline Point operator + (const Point& p1, const Point& p2) { return {p1.x+p2.x, p1.y+p2.y}; }
inline Point operator / (const Point& p, const double& d) { return {p.x/d, p.y/d}; }
inline Point operator * (const Point& p, const double& d) { return {p.x*d, p.y*d}; }

#endif //FAMILIAR_POINT_H
