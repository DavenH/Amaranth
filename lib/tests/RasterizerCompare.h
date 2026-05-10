#pragma once

#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Support/LegacyMeshRasterizer.h"

namespace RasterizerCompare {
    using Catch::Approx;

    struct Snapshot {
        std::vector<float> waveX;
        std::vector<float> waveY;
        std::vector<float> diffX;
        std::vector<float> slope;
        std::vector<Curve> curves;
        std::vector<Intercept> intercepts;
        std::vector<ColorPoint> colorPoints;
        int zeroIndex {};
        int oneIndex {};
        bool sampleable {};
    };

    inline std::vector<float> copyBuffer(Buffer<float> buffer) {
        std::vector<float> values;
        values.reserve(buffer.size());

        for (int i = 0; i < buffer.size(); ++i) {
            values.emplace_back(buffer[i]);
        }

        return values;
    }

    template<typename Rasterizer>
    inline Snapshot capture(Rasterizer& rasterizer) {
        Snapshot snapshot;

        snapshot.waveX       = copyBuffer(rasterizer.getWaveX());
        snapshot.waveY       = copyBuffer(rasterizer.getWaveY());
        snapshot.diffX       = copyBuffer(rasterizer.getDiffX());
        snapshot.slope       = copyBuffer(rasterizer.getSlopes());
        snapshot.curves      = rasterizer.getCurves();
        snapshot.intercepts  = rasterizer.getRasterizerData().intercepts;
        snapshot.colorPoints = rasterizer.getColorPoints();
        snapshot.zeroIndex   = rasterizer.getZeroIndex();
        snapshot.oneIndex    = rasterizer.getOneIndex();
        snapshot.sampleable  = rasterizer.samplerView().isSampleable();

        return snapshot;
    }

    inline void requireNear(float actual, float expected, float tolerance = 1e-5f) {
        REQUIRE(actual == Approx(expected).margin(tolerance));
    }

    inline void requireBufferNear(
            const std::vector<float>& actual,
            const std::vector<float>& expected,
            float tolerance = 1e-5f) {
        REQUIRE(actual.size() == expected.size());

        for (int i = 0; i < (int) actual.size(); ++i) {
            INFO("index=" << i << " actual=" << actual[i] << " expected=" << expected[i]);
            requireNear(actual[i], expected[i], tolerance);
        }
    }

    inline void requireInterceptNear(
            const Intercept& actual,
            const Intercept& expected,
            float tolerance = 1e-5f) {
        requireNear(actual.x, expected.x, tolerance);
        requireNear(actual.y, expected.y, tolerance);
        requireNear(actual.shp, expected.shp, tolerance);
        requireNear(actual.adjustedX, expected.adjustedX, tolerance);
        REQUIRE(actual.padBefore == expected.padBefore);
        REQUIRE(actual.padAfter == expected.padAfter);
        REQUIRE(actual.isWrapped == expected.isWrapped);
        REQUIRE(actual.cube == expected.cube);
    }

    inline void requireVertexNear(
            const Vertex2& actual,
            const Vertex2& expected,
            float tolerance = 1e-5f) {
        requireNear(actual.x, expected.x, tolerance);
        requireNear(actual.y, expected.y, tolerance);
    }

    inline void requireColorPointNear(
            const ColorPoint& actual,
            const ColorPoint& expected,
            float tolerance = 1e-5f) {
        REQUIRE(actual.num == expected.num);
        REQUIRE(actual.cube == expected.cube);
        requireVertexNear(actual.before, expected.before, tolerance);
        requireVertexNear(actual.mid, expected.mid, tolerance);
        requireVertexNear(actual.after, expected.after, tolerance);
    }

    inline void requireCurveNear(
            const Curve& actual,
            const Curve& expected,
            float tolerance = 1e-5f) {
        requireInterceptNear(actual.a, expected.a, tolerance);
        requireInterceptNear(actual.b, expected.b, tolerance);
        requireInterceptNear(actual.c, expected.c, tolerance);
        REQUIRE(actual.resIndex == expected.resIndex);
        REQUIRE(actual.curveRes == expected.curveRes);
        REQUIRE(actual.waveIdx == expected.waveIdx);
    }

    inline void requireSnapshotNear(
            const Snapshot& actual,
            const Snapshot& expected,
            float tolerance = 1e-5f) {
        REQUIRE(actual.sampleable == expected.sampleable);
        REQUIRE(actual.zeroIndex == expected.zeroIndex);
        REQUIRE(actual.oneIndex == expected.oneIndex);

        requireBufferNear(actual.waveX, expected.waveX, tolerance);
        requireBufferNear(actual.waveY, expected.waveY, tolerance);
        requireBufferNear(actual.diffX, expected.diffX, tolerance);
        requireBufferNear(actual.slope, expected.slope, tolerance);

        REQUIRE(actual.intercepts.size() == expected.intercepts.size());
        for (int i = 0; i < (int) actual.intercepts.size(); ++i) {
            INFO("intercept=" << i);
            requireInterceptNear(actual.intercepts[i], expected.intercepts[i], tolerance);
        }

        REQUIRE(actual.colorPoints.size() == expected.colorPoints.size());
        for (int i = 0; i < (int) actual.colorPoints.size(); ++i) {
            INFO("colorPoint=" << i);
            requireColorPointNear(actual.colorPoints[i], expected.colorPoints[i], tolerance);
        }

        REQUIRE(actual.curves.size() == expected.curves.size());
        for (int i = 0; i < (int) actual.curves.size(); ++i) {
            INFO("curve=" << i);
            requireCurveNear(actual.curves[i], expected.curves[i], tolerance);
        }
    }
}
