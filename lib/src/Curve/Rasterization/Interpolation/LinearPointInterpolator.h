#pragma once

#include "../RasterizerConversion.h"
#include "../../Intercept.h"

namespace Rasterization {
    class LinearPointInterpolator {
    public:
        RasterPoint interpolate(const RasterPoint& a, const RasterPoint& b, float proportion) const {
            RasterPoint point;
            point.x = interpolateValue(a.x, b.x, proportion);
            point.y = interpolateValue(a.y, b.y, proportion);
            point.sharpness = interpolateValue(a.sharpness, b.sharpness, proportion);
            point.adjustedX = interpolateValue(a.adjustedX, b.adjustedX, proportion);
            point.padBefore = proportion < 0.5f ? a.padBefore : b.padBefore;
            point.padAfter = proportion < 0.5f ? a.padAfter : b.padAfter;
            point.isWrapped = proportion < 0.5f ? a.isWrapped : b.isWrapped;
            point.source = proportion < 0.5f ? a.source : b.source;

            return point;
        }

        Intercept interpolate(const Intercept& a, const Intercept& b, float proportion) const {
            return toIntercept(interpolate(toRasterPoint(a), toRasterPoint(b), proportion));
        }

    private:
        static float interpolateValue(float a, float b, float proportion) {
            return a + (b - a) * proportion;
        }
    };
}
