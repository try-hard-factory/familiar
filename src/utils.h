//
// Created by max on 14.03.2021.
//

#ifndef FAMILIAR_UTILS_H
#define FAMILIAR_UTILS_H
#include <cmath>

namespace  familliar_utils {


    template<typename P, typename P1>
    double distance(const P &a, const P1 &b) {
        return sqrt(pow((a.x - b.x), 2) + pow((a.y - b.y), 2));
    }

}
#endif //FAMILIAR_UTILS_H
