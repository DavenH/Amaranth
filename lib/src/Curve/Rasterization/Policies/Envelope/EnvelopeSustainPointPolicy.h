#pragma once

#include <vector>

#include "../../../Intercept.h"

namespace Rasterization {
    struct EnvelopeSustainPointContext {
        int sustainIndex { -1 };
        bool addFloorPoint {};
    };

    class EnvelopeSustainPointPolicy {
    public:
        bool apply(std::vector<Intercept>& intercepts, const EnvelopeSustainPointContext& context) const {
            if (!context.addFloorPoint) {
                return false;
            }

            if (context.sustainIndex < 0 || context.sustainIndex >= (int) intercepts.size() - 1) {
                return false;
            }

            Intercept sustainIntercept = intercepts[context.sustainIndex];
            sustainIntercept.cube = nullptr;
            sustainIntercept.x += 0.0001f;

            if (sustainIntercept.y < 0.5f) {
                sustainIntercept.y = 0.5f;
            }

            sustainIntercept.shp = 1.f;
            intercepts.emplace_back(sustainIntercept);

            return true;
        }
    };
}
