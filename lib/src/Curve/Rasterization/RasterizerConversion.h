#pragma once

#include "../Intercept.h"

class VertCube;

namespace Rasterization {
    struct RasterPointSource {
        enum class Type {
            None,
            MeshCube,
            EnvelopeMarker,
            FxVertex,
            ExternalPoint
        };

        Type type {};
        int marker {};

        static RasterPointSource none() { return { Type::None, 0 }; }
        static RasterPointSource meshCube(int marker = 0) { return { Type::MeshCube, marker }; }
        static RasterPointSource envelopeMarker(int marker) { return { Type::EnvelopeMarker, marker }; }
        static RasterPointSource fxVertex(int marker) { return { Type::FxVertex, marker }; }
        static RasterPointSource externalPoint(int marker = 0) { return { Type::ExternalPoint, marker }; }
    };

    inline bool operator==(const RasterPointSource& a, const RasterPointSource& b) {
        return a.type == b.type && a.marker == b.marker;
    }

    inline bool operator!=(const RasterPointSource& a, const RasterPointSource& b) {
        return !(a == b);
    }

    struct RasterPoint {
        float x {};
        float y {};
        float sharpness {};
        float adjustedX {};
        bool padBefore {};
        bool padAfter {};
        bool isWrapped {};
        RasterPointSource source;
    };

    inline bool operator==(const RasterPoint& a, const RasterPoint& b) {
        return a.x == b.x
            && a.y == b.y
            && a.sharpness == b.sharpness
            && a.adjustedX == b.adjustedX
            && a.padBefore == b.padBefore
            && a.padAfter == b.padAfter
            && a.isWrapped == b.isWrapped
            && a.source == b.source;
    }

    inline bool operator!=(const RasterPoint& a, const RasterPoint& b) {
        return !(a == b);
    }

    struct MeshPointSourceRef {
        VertCube* cube {};
    };

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
