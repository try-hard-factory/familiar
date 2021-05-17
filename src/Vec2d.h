/** @file Vec2d.h
 *  @brief 2d vector math function
 *
 * 2d vector math function
 *
 *  @author max (angelskie glazki)
 *  @bug No known bugs.
 */

#ifndef Vec2dD_H
#define Vec2dD_H

#include <cmath>

template<typename T>
class Vec2d {
public:
    Vec2d(): x(0), y(0) {}
    Vec2d(T x, T y) : x(x), y(y) {}
    Vec2d(const Vec2d& v) : x(v.x), y(v.y) {}

    T x;
    T y;

    Vec2d& operator=(const Vec2d& v) {
        x = v.x;
        y = v.y;
        return *this;
    }

    Vec2d operator+(Vec2d& v) {
        return Vec2d(x + v.x, y + v.y);
    }
    Vec2d operator-(Vec2d& v) {
        return Vec2d(x - v.x, y - v.y);
    }

    Vec2d& operator+=(Vec2d& v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    Vec2d& operator-=(Vec2d& v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    Vec2d operator+(double s) {
        return Vec2d(x + s, y + s);
    }
    Vec2d operator-(double s) {
        return Vec2d(x - s, y - s);
    }
    Vec2d operator*(double s) {
        return Vec2d(x * s, y * s);
    }
    Vec2d operator/(double s) {
        return Vec2d(x / s, y / s);
    }


    Vec2d& operator+=(double s) {
        x += s;
        y += s;
        return *this;
    }
    Vec2d& operator-=(double s) {
        x -= s;
        y -= s;
        return *this;
    }
    Vec2d& operator*=(double s) {
        x *= s;
        y *= s;
        return *this;
    }
    Vec2d& operator/=(double s) {
        x /= s;
        y /= s;
        return *this;
    }

    void set(T x, T y) {
        this->x = x;
        this->y = y;
    }

    void rotate(double deg) {
        double theta = deg / 180.0 * M_PI;
        double c = cos(theta);
        double s = sin(theta);
        double tx = x * c - y * s;
        double ty = x * s + y * c;
        x = tx;
        y = ty;
    }

    Vec2d& normalize() {
        if (length() == 0) return *this;
        *this *= (1.0 / length());
        return *this;
    }

    float dist(Vec2d v) const {
        Vec2d d(v.x - x, v.y - y);
        return d.length();
    }

    float length() const {
        return std::sqrt(x * x + y * y);
    }

    static float dot(Vec2d v1, Vec2d v2) {
        return v1.x * v2.x + v1.y * v2.y;
    }
    static float cross(Vec2d v1, Vec2d v2) {
        return (v1.x * v2.y) - (v1.y * v2.x);
    }

};


#endif // Vec2dD_H
