//
// Created by max on 14.03.2021.
//

#ifndef FAMILIAR_FOCUSPOINTS_H
#define FAMILIAR_FOCUSPOINTS_H

#include <array>
#include <optional>
#include <climits>
#include "Point.h"
#include "utils.h"

enum EPointIdx {
    eNone = -1,
    ePoint0 = 0,
    ePoint1 = 1,
    ePoint2 = 2,
    ePoint3 = 3,
};

class FocusPoints {
public:
    ~FocusPoints() = default;
    void resetFocusPoints();
    void setFocusePoints(const Point& p0, const Point& p1, const Point& p2, const Point& p3);
    std::optional<EPointIdx> isMouseNear(const Point& mp);
private:
    static inline Point default_point = Point(INT_MAX, INT_MAX);
    std::array<Point, 4> focuse_points = { default_point,
                                           default_point,
                                           default_point,
                                           default_point };
};


#endif //FAMILIAR_FOCUSPOINTS_H
