//
// Created by max on 14.03.2021.
//

#include "FocusPoints.h"
#include "utils.h"


std::optional<EPointIdx> FocusPoints::isMouseNear(const Point& mouse_pos) {
    for (size_t i = 0; i < focuse_points.size(); ++i) {
        if (familliar_utils::distance(focuse_points[i], mouse_pos) < 5) return std::make_optional((EPointIdx)i);
    }
    return std::nullopt;
}

void FocusPoints::resetFocusPoints() {
    focuse_points[0] = default_point;
    focuse_points[1] = default_point;
    focuse_points[2] = default_point;
    focuse_points[3] = default_point;
}

void FocusPoints::setFocusePoints(const Point &p0, const Point &p1, const Point &p2, const Point &p3) {
    focuse_points[0] = p0;
    focuse_points[1] = p1;
    focuse_points[2] = p2;
    focuse_points[3] = p3;
}
