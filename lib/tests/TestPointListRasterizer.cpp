#include <cmath>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Curve.h"
#include "../src/Curve/Rasterization/Rasterizer/Rasterizer2D.h"
#include "../src/Curve/Rasterization/Builders/CurveWaveformBuilder.h"

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

    void requireBufferNear(const char* name, Buffer<float> actual, Buffer<float> expected) {
        REQUIRE(actual.size() == expected.size());
        for (int i = 0; i < actual.size(); ++i) {
            INFO("buffer=" << name << " index=" << i);
            REQUIRE(actual[i] == Catch::Approx(expected[i]).margin(1e-4f));
        }
    }

    void requireEquivalentResults(
            const Rasterization::RenderResult& actual,
            const Rasterization::RenderResult& expected) {
        requireBufferNear("waveX", actual.waveform.waveX, expected.waveform.waveX);
        requireBufferNear("waveY", actual.waveform.waveY, expected.waveform.waveY);
        requireBufferNear("diffX", actual.waveform.diffX, expected.waveform.diffX);
        requireBufferNear("slope", actual.waveform.slope, expected.waveform.slope);
        requireBufferNear("area", actual.waveform.area, expected.waveform.area);
        REQUIRE(actual.waveform.zeroIndex == expected.waveform.zeroIndex);
        REQUIRE(actual.waveform.oneIndex == expected.waveform.oneIndex);
        REQUIRE(actual.sampleable == expected.sampleable);

        ScopedAlloc<float> actualMemory;
        ScopedAlloc<float> expectedMemory;
        actualMemory.ensureSize(48);
        expectedMemory.ensureSize(48);
        Buffer<float> actualSamples = actualMemory.place(48);
        Buffer<float> expectedSamples = expectedMemory.place(48);
        Rasterization::WaveformSampler::samplePerfectly(
                actual.waveform,
                0.01,
                actualSamples,
                0.05);
        Rasterization::WaveformSampler::samplePerfectly(
                expected.waveform,
                0.01,
                expectedSamples,
                0.05);
        requireBufferNear("integrated samples", actualSamples, expectedSamples);
    }
}

TEST_CASE("Rasterizer2D rasterizes non-cyclic point lists through curve waveform pipeline", "[rasterization][pointlist]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();

    Rasterizer2D rasterizer(points, false);
    rasterizer.updateWaveform();

    REQUIRE_FALSE(rasterizer.getWaveX().empty());
    REQUIRE(rasterizer.isSampleable());
    requireFinite(rasterizer.getWaveX());
    requireFinite(rasterizer.getWaveY());

    REQUIRE(points.front().x == Catch::Approx(0.05f));
    REQUIRE(points.back().x == Catch::Approx(0.85f));
}

TEST_CASE("Rasterizer2D rasterizes cyclic point lists through curve waveform pipeline", "[rasterization][pointlist]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();

    Rasterizer2D rasterizer(points, true);
    rasterizer.updateWaveform();

    REQUIRE_FALSE(rasterizer.getWaveX().empty());
    REQUIRE(rasterizer.isSampleable());
    requireFinite(rasterizer.getWaveX());
    requireFinite(rasterizer.getWaveY());

    REQUIRE(rasterizer.getWaveX().front() < 0.f);
    REQUIRE(rasterizer.getWaveX().back() > 1.f);
}

TEST_CASE("CurveWaveformBuilder matches non-cyclic Rasterizer2D output", "[rasterization][pointlist][pipeline]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> referencePoints = makePointList();
    std::vector<Intercept> pipelinePoints = makePointList();

    Rasterizer2D reference(referencePoints, false);
    reference.updateWaveform();

    Rasterization::RasterizationRequest request;
    request.cyclic = false;
    Rasterization::CurveWaveformBuilder builder;
    Rasterization::RenderResult output;
    builder.renderIntercepts(pipelinePoints, output, request);

    REQUIRE(output.sampleable);
    REQUIRE(output.waveform.waveX.size() == reference.getWaveX().size());
    REQUIRE(output.waveform.waveY.size() == reference.getWaveY().size());

    for (int i = 0; i < output.waveform.waveX.size(); ++i) {
        INFO("index=" << i);
        REQUIRE(output.waveform.waveX[i] == Catch::Approx(reference.getWaveX()[i]).margin(1e-5f));
        REQUIRE(output.waveform.waveY[i] == Catch::Approx(reference.getWaveY()[i]).margin(1e-5f));
    }
}

TEST_CASE("CurveWaveformBuilder matches cyclic Rasterizer2D output", "[rasterization][pointlist][pipeline]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> referencePoints = makePointList();
    std::vector<Intercept> pipelinePoints = makePointList();

    Rasterizer2D reference(referencePoints, true);
    reference.updateWaveform();

    Rasterization::RasterizationRequest request;
    request.cyclic = true;
    Rasterization::CurveWaveformBuilder builder;
    Rasterization::RenderResult output;
    builder.renderIntercepts(pipelinePoints, output, request);

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

TEST_CASE("CurveWaveformBuilder marks empty and single-point inputs unsampleable", "[rasterization][pointlist][pipeline]") {
    CurveTableScope curveTableScope;
    Rasterization::CurveWaveformBuilder builder;
    Rasterization::RasterizationRequest request;
    Rasterization::RenderResult output;

    std::vector<Intercept> emptyPoints;
    REQUIRE_FALSE(builder.renderIntercepts(emptyPoints, output, request).sampleable);

    std::vector<Intercept> singlePoint { Intercept(0.5f, 0.5f) };
    REQUIRE_FALSE(builder.renderIntercepts(singlePoint, output, request).sampleable);
}

TEST_CASE("Rasterizer2D updates a partial waveform after a point edit", "[rasterization][pointlist]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();

    Rasterizer2D rasterizer(points, false);
    rasterizer.updateWaveform();

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

TEST_CASE("Rasterizer2D incremental edits preserve every full-bake invariant",
        "[rasterization][pointlist][incremental]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();
    Rasterizer2D incremental(points, false);
    incremental.updateWaveform();
    Intercept edited = points[1];
    edited.y = 0.15f;
    incremental.updateCurve(1 + incremental.getPaddingSize(), edited);

    REQUIRE(incremental.getDiagnostics().fullBakeCount == 1);
    REQUIRE(incremental.getDiagnostics().affectedRangeBakeCount == 1);

    std::vector<Intercept> rebuiltPoints = points;
    Rasterizer2D rebuilt(rebuiltPoints, false);
    rebuilt.updateWaveform();
    requireEquivalentResults(incremental.result(), rebuilt.result());
}

TEST_CASE("Rasterizer2D sharpness extremes remain coherent with a full bake",
        "[rasterization][pointlist][incremental]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points = makePointList();
    Rasterizer2D incremental(points, false);
    incremental.updateWaveform();

    Intercept edited = points[2];
    edited.x = 0.70f;
    edited.adjustedX = edited.x;
    edited.y = 0.95f;
    edited.shp = 0.01f;
    incremental.updateCurve(2 + incremental.getPaddingSize(), edited);

    std::vector<Intercept> rebuiltPoints = points;
    Rasterizer2D rebuilt(rebuiltPoints, false);
    rebuilt.updateWaveform();
    requireEquivalentResults(incremental.result(), rebuilt.result());
}

TEST_CASE("Rasterizer2D boundary-adjacent edits preserve crossings and integrated samples",
        "[rasterization][pointlist][incremental]") {
    CurveTableScope curveTableScope;

    for (const auto& edit : std::vector<std::pair<int, Intercept>> {
                 { 0, Intercept(0.01f, 0.65f, nullptr, 0.05f) },
                 { 3, Intercept(0.98f, 0.10f, nullptr, 0.95f) } }) {
        std::vector<Intercept> points = makePointList();
        Rasterizer2D incremental(points, false);
        incremental.updateWaveform();
        Intercept edited = edit.second;
        edited.adjustedX = edited.x;
        incremental.updateCurve(edit.first + incremental.getPaddingSize(), edited);

        std::vector<Intercept> rebuiltPoints = points;
        Rasterizer2D rebuilt(rebuiltPoints, false);
        rebuilt.updateWaveform();
        requireEquivalentResults(incremental.result(), rebuilt.result());
    }
}

TEST_CASE("Rasterizer2D detects a local resolution-layout transition",
        "[rasterization][pointlist][incremental]") {
    CurveTableScope curveTableScope;
    std::vector<Intercept> points;
    for (int i = 0; i < 12; ++i) {
        points.emplace_back(0.10f + 0.0155f * float(i), 0.2f + 0.05f * float(i));
    }
    for (auto& point : points) {
        point.adjustedX = point.x;
    }

    Rasterizer2D incremental(points, false);
    incremental.updateWaveform();
    std::vector<int> initialResolutionIndices;
    for (const auto& curve : incremental.result().curves) {
        initialResolutionIndices.emplace_back(curve.resIndex);
    }

    int editedPointIndex = -1;
    Intercept edited;
    for (int pointIndex = 1; pointIndex < (int) points.size() - 1 && editedPointIndex < 0;
            ++pointIndex) {
        for (int step = 1; step < 10; ++step) {
            std::vector<Intercept> candidatePoints = points;
            const float portion = float(step) * 0.1f;
            candidatePoints[pointIndex].x = candidatePoints[pointIndex - 1].x
                    + portion * (candidatePoints[pointIndex + 1].x
                               - candidatePoints[pointIndex - 1].x);
            candidatePoints[pointIndex].adjustedX = candidatePoints[pointIndex].x;
            Rasterizer2D candidate(candidatePoints, false);
            candidate.updateWaveform();

            for (int curveIndex = 0; curveIndex < (int) candidate.result().curves.size(); ++curveIndex) {
                if (candidate.result().curves[curveIndex].resIndex
                        != initialResolutionIndices[curveIndex]) {
                    editedPointIndex = pointIndex;
                    edited = candidatePoints[pointIndex];
                    break;
                }
            }

            if (editedPointIndex >= 0) {
                break;
            }
        }
    }

    REQUIRE(editedPointIndex >= 0);
    edited.adjustedX = edited.x;
    incremental.updateCurve(editedPointIndex + incremental.getPaddingSize(), edited);

    std::vector<Intercept> rebuiltPoints = points;
    Rasterizer2D rebuilt(rebuiltPoints, false);
    rebuilt.updateWaveform();

    REQUIRE(incremental.getDiagnostics().fullBakeCount == 2);
    REQUIRE(incremental.getDiagnostics().affectedRangeBakeCount == 0);
    requireEquivalentResults(incremental.result(), rebuilt.result());
}
