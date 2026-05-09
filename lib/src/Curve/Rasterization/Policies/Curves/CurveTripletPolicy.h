#pragma once

#include <vector>

#include "../../../Curve.h"
#include "../../../Intercept.h"

namespace Rasterization {
    class CurveTripletPolicy {
    public:
        static void appendAdjacent(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves) {
            for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
                curves.emplace_back(intercepts[i], intercepts[i + 1], intercepts[i + 2]);
            }
        }

        static void appendAdjacentReversed(
                const std::vector<Intercept>& intercepts,
                std::vector<Curve>& curves) {
            for (int i = 0; i < (int) intercepts.size() - 2; ++i) {
                int idx = (int) intercepts.size() - 1 - i;
                curves.emplace_back(intercepts[idx], intercepts[idx - 1], intercepts[idx - 2]);
            }
        }
    };
}
