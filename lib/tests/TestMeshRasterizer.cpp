#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/Curve.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/MeshRasterizer.h"
#include "../src/Curve/Rasterization/Pipelines/MeshSlicePipeline.h"
#include "../src/Curve/Rasterization/Sources/MeshCubeSource.h"
#include "../src/Curve/VertCube.h"
#include "RasterizerCompare.h"

namespace {
    struct MeshDeleter {
        void operator()(Mesh* mesh) const {
            if (mesh != nullptr) {
                mesh->destroy();
                delete mesh;
            }
        }
    };

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

    void setCubeAsConstantPoint(
            VertCube* cube,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            bool time, red, blue;
            VertCube::getPoles(i, time, red, blue);

            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Time]  = time ? 1.f : 0.f;
            vertex->values[Vertex::Red]   = red  ? 1.f : 0.f;
            vertex->values[Vertex::Blue]  = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp]   = time ? highAmp   : lowAmp;
            vertex->values[Vertex::Curve] = sharpness;
        }
    }

    std::unique_ptr<Mesh, MeshDeleter> createSyntheticWaveMesh() {
        std::unique_ptr<Mesh, MeshDeleter> mesh(new Mesh("SyntheticWave"));

        struct CubeData {
            float lowPhase, highPhase;
            float lowAmp, highAmp;
            float sharpness;
        };

        const CubeData cubes[] = {
            { 0.03f, 0.05f, 0.22f, 0.28f, 0.20f },
            { 0.31f, 0.35f, 0.68f, 0.62f, 1.00f },
            { 0.57f, 0.61f, 0.18f, 0.24f, 0.45f },
            { 0.93f, 0.97f, 0.81f, 0.76f, 0.25f },
        };

        for (const auto& cubeData : cubes) {
            auto* cube = new VertCube(mesh.get());
            setCubeAsConstantPoint(cube,
                                   cubeData.lowPhase,
                                   cubeData.highPhase,
                                   cubeData.lowAmp,
                                   cubeData.highAmp,
                                   cubeData.sharpness);
            mesh->addCube(cube);
        }

        return mesh;
    }

    void requireFinite(Buffer<float> buffer) {
        for (int i = 0; i < buffer.size(); ++i) {
            INFO("index=" << i << " value=" << buffer[i]);
            REQUIRE(std::isfinite(buffer[i]));
        }
    }

    void requireNonDecreasing(Buffer<float> buffer, float tolerance = 2e-3f) {
        for (int i = 1; i < buffer.size(); ++i) {
            INFO("index=" << i
                 << " previous=" << buffer[i - 1]
                 << " current=" << buffer[i]
                 << " tolerance=" << tolerance);
            REQUIRE(buffer[i] + tolerance >= buffer[i - 1]);
        }
    }

    void configureWaveRasterizer(MeshRasterizer& rasterizer, Mesh* mesh) {
        rasterizer.setMesh(mesh);
        rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
        rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue));
    }
}

TEST_CASE("MeshRasterizer creates finite monotonic wave buffers for a synthetic mesh", "[meshrasterizer][wave]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    rasterizer.setMesh(mesh.get());
    rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    rasterizer.calcCrossPoints();

    auto waveX = rasterizer.getWaveX();
    auto waveY = rasterizer.getWaveY();

    REQUIRE_FALSE(waveX.empty());
    REQUIRE(waveX.size() == waveY.size());
    REQUIRE(rasterizer.isSampleable());

    requireFinite(waveX);
    requireFinite(waveY);
    requireNonDecreasing(waveX);

    REQUIRE(waveX.front() < 0.f);
    REQUIRE(waveX.back() > 1.f);
    REQUIRE(rasterizer.getZeroIndex() >= 0);
    REQUIRE(rasterizer.getZeroIndex() < waveX.size() - 1);
    REQUIRE(rasterizer.getOneIndex() >= 0);
    REQUIRE(rasterizer.getOneIndex() < waveX.size() - 1);

}

TEST_CASE("MeshRasterizer can sample evenly after max-sharpness waveform generation", "[meshrasterizer][wave][sampling]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    rasterizer.setMesh(mesh.get());
    rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    rasterizer.calcCrossPoints();

    ScopedAlloc<Float32> sampleMemory;
    sampleMemory.ensureSize(128);
    Buffer<float> samples = sampleMemory.place(128);
    rasterizer.sampleEvenlyTo(samples);

    requireFinite(samples);

}

TEST_CASE("MeshRasterizer characterization snapshot is deterministic", "[meshrasterizer][characterization]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer first("SyntheticMeshRasterizerA");
    MeshRasterizer second("SyntheticMeshRasterizerB");
    configureWaveRasterizer(first, mesh.get());
    configureWaveRasterizer(second, mesh.get());

    first.calcCrossPoints();
    first.makeCopy();
    second.calcCrossPoints();
    second.makeCopy();

    RasterizerCompare::requireSnapshotNear(
            RasterizerCompare::capture(first),
            RasterizerCompare::capture(second));

    ScopedAlloc<Float32> firstMemory;
    ScopedAlloc<Float32> secondMemory;
    firstMemory.ensureSize(128);
    secondMemory.ensureSize(128);

    Buffer<float> firstSamples = firstMemory.place(128);
    Buffer<float> secondSamples = secondMemory.place(128);
    first.sampleEvenlyTo(firstSamples);
    second.sampleEvenlyTo(secondSamples);

    RasterizerCompare::requireBufferNear(
            RasterizerCompare::copyBuffer(firstSamples),
            RasterizerCompare::copyBuffer(secondSamples));
}

TEST_CASE("MeshRasterizer characterizes hidden-dimension color points", "[meshrasterizer][characterization][colorpoints]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    configureWaveRasterizer(rasterizer, mesh.get());
    rasterizer.calcCrossPoints();
    rasterizer.makeCopy();

    const auto& colorPoints = rasterizer.getColorPoints();
    REQUIRE(colorPoints.size() == mesh->getNumCubes() * 3);

    int timePoints = 0;
    int redPoints = 0;
    int bluePoints = 0;

    for (const auto& point : colorPoints) {
        INFO("hiddenDim=" << point.num);
        REQUIRE(point.cube != nullptr);
        REQUIRE(std::isfinite(point.before.x));
        REQUIRE(std::isfinite(point.before.y));
        REQUIRE(std::isfinite(point.mid.x));
        REQUIRE(std::isfinite(point.mid.y));
        REQUIRE(std::isfinite(point.after.x));
        REQUIRE(std::isfinite(point.after.y));

        if (point.num == Vertex::Time) {
            ++timePoints;
        } else if (point.num == Vertex::Red) {
            ++redPoints;
        } else if (point.num == Vertex::Blue) {
            ++bluePoints;
        } else {
            FAIL("unexpected hidden dimension");
        }
    }

    REQUIRE(timePoints == mesh->getNumCubes());
    REQUIRE(redPoints == mesh->getNumCubes());
    REQUIRE(bluePoints == mesh->getNumCubes());
    REQUIRE(rasterizer.getRastData().colorPoints.size() == colorPoints.size());
}

TEST_CASE("MeshSlicePipeline matches MeshRasterizer intercept and color-point slicing", "[meshrasterizer][pipeline][slice]") {
    CurveTableScope curveTableScope;
    auto mesh = createSyntheticWaveMesh();

    MeshRasterizer rasterizer("SyntheticMeshRasterizer");
    configureWaveRasterizer(rasterizer, mesh.get());
    rasterizer.calcCrossPoints();
    rasterizer.makeCopy();

    Rasterization::MeshSlicePipeline pipeline;
    Rasterization::MeshCubeSource source(mesh.get());
    Rasterization::RasterizationRequest request = rasterizer.createRasterizationRequest();
    const auto& output = pipeline.render(
            source,
            request,
            0.f,
            [](Intercept&, const MorphPosition&, bool) {});

    REQUIRE(output.sampleable);
    REQUIRE(output.intercepts.size() == rasterizer.getRastData().intercepts.size());

    for (int i = 0; i < (int) output.intercepts.size(); ++i) {
        INFO("intercept=" << i);
        RasterizerCompare::requireInterceptNear(output.intercepts[i], rasterizer.getRastData().intercepts[i]);
    }

    REQUIRE(output.colorPoints.size() == rasterizer.getRastData().colorPoints.size());

    for (int i = 0; i < (int) output.colorPoints.size(); ++i) {
        INFO("colorPoint=" << i);
        RasterizerCompare::requireColorPointNear(output.colorPoints[i], rasterizer.getRastData().colorPoints[i]);
    }
}

TEST_CASE("MeshRasterizer characterizes intercept restriction spacing", "[meshrasterizer][characterization][restriction]") {
    MeshRasterizer rasterizer("RestrictionRasterizer");

    vector<Intercept> intercepts {
        Intercept(0.2f, 0.1f),
        Intercept(0.2f, 0.2f),
        Intercept(0.1f, 0.3f),
        Intercept(1.2f, 0.4f),
    };

    for (auto& intercept : intercepts) {
        intercept.adjustedX = intercept.x;
    }

    rasterizer.restrictIntercepts(intercepts);

    REQUIRE(intercepts.front().x >= 0.f);
    REQUIRE(intercepts.back().x <= 1.f);

    for (int i = 1; i < (int) intercepts.size(); ++i) {
        INFO("index=" << i << " left=" << intercepts[i - 1].x << " right=" << intercepts[i].x);
        REQUIRE(intercepts[i].x > intercepts[i - 1].x);
        REQUIRE(intercepts[i].x - intercepts[i - 1].x >= Catch::Approx(0.0001f).margin(1e-6f));
    }
}
