#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/Mesh/Mesh.h"
#include "Support/LegacyMeshRasterizer.h"
#include "../src/Curve/Rasterization/Policies/Curves/CurvePolicies.h"
#include "../src/Curve/Mesh/VertCube.h"
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

    std::unique_ptr<Mesh, MeshDeleter> createWaveformBakeMesh() {
        std::unique_ptr<Mesh, MeshDeleter> mesh(new Mesh("WaveformBakeMesh"));

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

    void configureRasterizer(MeshRasterizer& rasterizer, Mesh* mesh) {
        rasterizer.setMesh(mesh);
        rasterizer.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
        rasterizer.setDims(Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue));
    }
}

TEST_CASE("CurveResolutionPolicy applies low-resolution shortcut", "[rasterization][waveform]") {
    std::vector<Curve> curves;

    for (int i = 0; i < 9; ++i) {
        curves.emplace_back(
                Intercept(float(i), 0.f),
                Intercept(float(i) + 0.5f, 0.f),
                Intercept(float(i) + 1.f, 0.f));
        curves.back().setShouldInterpolate(true);
    }

    Rasterization::CurveResolutionPolicy::Context context;
    context.lowResCurves = true;
    Rasterization::CurveResolutionPolicy().apply(curves, context);

    for (const auto& curve : curves) {
        REQUIRE(curve.resIndex == Curve::resolutions - 1);
    }
}

TEST_CASE("Waveform bake path keeps MeshRasterizer wave snapshots deterministic", "[rasterization][waveform]") {
    CurveTableScope curveTableScope;
    auto mesh = createWaveformBakeMesh();

    MeshRasterizer first("WaveformBakeA");
    MeshRasterizer second("WaveformBakeB");
    configureRasterizer(first, mesh.get());
    configureRasterizer(second, mesh.get());

    first.calcCrossPoints();
    first.makeCopy();
    second.calcCrossPoints();
    second.makeCopy();

    REQUIRE(first.getDiffX().back() == 0.f);
    REQUIRE(first.getSlopes().back() == 0.f);
    RasterizerCompare::requireSnapshotNear(
            RasterizerCompare::capture(second),
            RasterizerCompare::capture(first));
}
