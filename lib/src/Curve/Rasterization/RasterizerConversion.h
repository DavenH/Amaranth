#pragma once

#include "RasterizerTypes.h"
#include "../Intercept.h"

namespace Rasterization {
    inline RasterPointSource sourceForIntercept(const Intercept& intercept) {
        return intercept.cube != nullptr
             ? RasterPointSource::meshCube()
             : RasterPointSource::none();
    }

    inline RasterPoint toRasterPoint(
            const Intercept& intercept,
            RasterPointSource source = RasterPointSource::none()) {
        if (source.type == RasterPointSource::Type::None) {
            source = sourceForIntercept(intercept);
        }

        RasterPoint point;
        point.x          = intercept.x;
        point.y          = intercept.y;
        point.sharpness  = intercept.shp;
        point.adjustedX  = intercept.adjustedX;
        point.padBefore  = intercept.padBefore;
        point.padAfter   = intercept.padAfter;
        point.isWrapped  = intercept.isWrapped;
        point.source     = source;

        return point;
    }

    inline Intercept toIntercept(const RasterPoint& point, VertCube* cube = nullptr) {
        Intercept intercept(point.x, point.y, cube, point.sharpness);
        intercept.adjustedX = point.adjustedX;
        intercept.padBefore = point.padBefore;
        intercept.padAfter  = point.padAfter;
        intercept.isWrapped = point.isWrapped;

        return intercept;
    }

    inline MeshPointSourceRef meshSourceRefFor(const Intercept& intercept) {
        return { intercept.cube };
    }
}
