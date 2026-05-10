#pragma once

#include <algorithm>
#include <vector>

#include "../../Intercept.h"

namespace Rasterization {
    class PointListSource {
    public:
        explicit PointListSource(std::vector<Intercept>& points) :
                points(points) {
        }

        bool empty() const { return points.empty(); }
        int size() const { return (int) points.size(); }

        void sortByX() {
            std::sort(points.begin(), points.end());
        }

        Intercept& interceptAt(int index) { return points[index]; }
        const Intercept& interceptAt(int index) const { return points[index]; }

        std::vector<Intercept>& intercepts() { return points; }
        const std::vector<Intercept>& intercepts() const { return points; }

    private:
        std::vector<Intercept>& points;
    };
}
