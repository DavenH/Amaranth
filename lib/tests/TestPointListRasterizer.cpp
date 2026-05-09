#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Curve.h"
#include "../src/Curve/Rasterizer2D.h"
#include "../src/Curve/Rasterization/Interpolation/LinearPointInterpolator.h"
#include "../src/Curve/Rasterization/Pipelines/PointListRasterizationPipeline.h"
#include "../src/Curve/Rasterization/Sources/PointListSource.h"

namespace {
    struct CurveTableScope {
        CurveTableScope() {
            if (refCount++ == 0) {
                Curve::calcTable();
            }
        }

        ~CurveTableScope() {
            if (--refCount == 0) {
                Curve::deleteTable();
            }
        }

        inline static int refCount = 0;
    };

    std::vector<Intercept> makePointList() {
        std::vector<Intercept> points {
            Intercept(0.85f, 0.35f, nullptr, 0.60f),
            Intercept(0.05f, 0.20f, nullptr, 0.25f),
            Intercept(0.35f, 0.80f, nullptr, 1.00f),
            Intercept(0.60f, 0.45f, nullptr, 0.40f),
        };

        for (auto& point : points) {
            point.adjustedX = point.x;
        }

        return points;
    }

    void requireFinite(Buffer<float> buffer) {
        for (int i = 0; i < buffer.size(); ++i) {
            INFO("index=" << i << " value=" << buffer[i]);
            REQUIRE(std::isfinite(buffer[i]));
        }
    }

    std::vector<float> copyBuffer(Buffer<float> buffer) {
        std::vector<float> values;
        values.reserve(buffer.size());

        for (int i = 0; i < buffer.size(); ++i) {
            values.emplace_back(buffer[i]);
        }

        return values;
    }
}

TEST_CASE("PointListSource sorts and exposes external raster points", "[rasterization][pointlist]") {
    std::vector<Intercept> points = makePointList();
    Rasterization::PointListSource source(points);

    REQUIRE(source.size() == 4);
    source.sortByX();

    REQUIRE(source.interceptAt(0).x == Catch::Approx(0.05f));
    REQUIRE(source.interceptAt(3).x == Catch::Approx(0.85f));

    Rasterization::RasterPoint point = source.pointAt(2);
    REQUIRE(point.x == Catch::Approx(source.interceptAt(2).x));
    REQUIRE(point.y == Catch::Approx(source.interceptAt(2).y));
    REQUIRE(point.source.type == Rasterization::RasterPointSource::Type::ExternalPoint);
    REQUIRE(point.source.marker == 2);
}

TEST_CASE("LinearPointInterpolator interpolates point-list intercepts", "[rasterization][pointlist]") {
    Intercept a(0.2f, 0.1f, nullptr, 0.25f);
    Intercept b(0.6f, 0.9f, nullptr, 0.75f);
    a.adjustedX = 0.3f;
    b.adjustedX = 0.7f;

    Intercept mid = Rasterization::LinearPointInterpolator().interpolate(a, b, 0.5f);

    REQUIRE(mid.x == Catch::Approx(0.4f));
    REQUIRE(mid.y == Catch::Approx(0.5f));
    REQUIRE(mid.shp == Catch::Approx(0.5f));
    REQUIRE(mid.adjustedX == Catch::Approx(0.5f));
}

TEST_CASE("Rasterizer2D rasterizes non-cyclic point lists through PointListSource", "[rasterization][pointlist]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();

    Rasterizer2D rasterizer(points, false);
    rasterizer.calcCrossPoints();

    REQUIRE_FALSE(rasterizer.getWaveX().empty());
    REQUIRE(rasterizer.isSampleable());
    requireFinite(rasterizer.getWaveX());
    requireFinite(rasterizer.getWaveY());

    REQUIRE(points.front().x == Catch::Approx(0.05f));
    REQUIRE(points.back().x == Catch::Approx(0.85f));
}

TEST_CASE("Rasterizer2D rasterizes cyclic point lists through PointListSource", "[rasterization][pointlist]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();

    Rasterizer2D rasterizer(points, true);
    rasterizer.calcCrossPoints();

    REQUIRE_FALSE(rasterizer.getWaveX().empty());
    REQUIRE(rasterizer.isSampleable());
    requireFinite(rasterizer.getWaveX());
    requireFinite(rasterizer.getWaveY());

    REQUIRE(rasterizer.getWaveX().front() < 0.f);
    REQUIRE(rasterizer.getWaveX().back() > 1.f);
}

TEST_CASE("PointListRasterizationPipeline matches non-cyclic Rasterizer2D output", "[rasterization][pointlist][pipeline]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> referencePoints = makePointList();
    std::vector<Intercept> pipelinePoints = makePointList();

    Rasterizer2D reference(referencePoints, false);
    reference.calcCrossPoints();

    Rasterization::RasterizationRequest request;
    request.cyclic = false;
    Rasterization::PointListRasterizationPipeline pipeline;
    const auto& output = pipeline.render(pipelinePoints, request);

    REQUIRE(output.sampleable);
    REQUIRE(output.waveform.waveX.size() == reference.getWaveX().size());
    REQUIRE(output.waveform.waveY.size() == reference.getWaveY().size());

    for (int i = 0; i < output.waveform.waveX.size(); ++i) {
        INFO("index=" << i);
        REQUIRE(output.waveform.waveX[i] == Catch::Approx(reference.getWaveX()[i]).margin(1e-5f));
        REQUIRE(output.waveform.waveY[i] == Catch::Approx(reference.getWaveY()[i]).margin(1e-5f));
    }
}

TEST_CASE("PointListRasterizationPipeline matches cyclic Rasterizer2D output", "[rasterization][pointlist][pipeline]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> referencePoints = makePointList();
    std::vector<Intercept> pipelinePoints = makePointList();

    Rasterizer2D reference(referencePoints, true);
    reference.calcCrossPoints();

    Rasterization::RasterizationRequest request;
    request.cyclic = true;
    Rasterization::PointListRasterizationPipeline pipeline;
    const auto& output = pipeline.render(pipelinePoints, request);

    REQUIRE(output.sampleable);
    REQUIRE(output.waveform.waveX.size() == reference.getWaveX().size());
    REQUIRE(output.waveform.waveY.size() == reference.getWaveY().size());
    REQUIRE(output.waveform.waveX.front() < 0.f);
    REQUIRE(output.waveform.waveX.back() > 1.f);

    for (int i = 0; i < output.waveform.waveX.size(); ++i) {
        INFO("index=" << i);
        REQUIRE(output.waveform.waveX[i] == Catch::Approx(reference.getWaveX()[i]).margin(1e-5f));
        REQUIRE(output.waveform.waveY[i] == Catch::Approx(reference.getWaveY()[i]).margin(1e-5f));
    }
}

TEST_CASE("PointListRasterizationPipeline marks empty and single-point inputs unsampleable", "[rasterization][pointlist][pipeline]") {
    CurveTableScope curveTableScope;
    Rasterization::PointListRasterizationPipeline pipeline;
    Rasterization::RasterizationRequest request;

    std::vector<Intercept> emptyPoints;
    REQUIRE_FALSE(pipeline.render(emptyPoints, request).sampleable);

    std::vector<Intercept> singlePoint { Intercept(0.5f, 0.5f) };
    REQUIRE_FALSE(pipeline.render(singlePoint, request).sampleable);
}

TEST_CASE("Rasterizer2D updates a partial waveform after a point edit", "[rasterization][pointlist]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();

    Rasterizer2D rasterizer(points, false);
    rasterizer.calcCrossPoints();

    std::vector<float> before = copyBuffer(rasterizer.getWaveY());

    Intercept edited(0.35f, 0.25f, nullptr, 0.85f);
    edited.adjustedX = edited.x;
    rasterizer.updateCurve(2, edited);

    std::vector<float> after = copyBuffer(rasterizer.getWaveY());
    REQUIRE(before.size() == after.size());

    int changed = 0;
    for (int i = 0; i < (int) before.size(); ++i) {
        if (before[i] != Catch::Approx(after[i]).margin(1e-6f)) {
            ++changed;
        }
    }

    REQUIRE(changed > 0);
    requireFinite(rasterizer.getWaveX());
    requireFinite(rasterizer.getWaveY());
}
