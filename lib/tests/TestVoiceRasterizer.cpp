#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Curve.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/VoiceRasterizer.h>

namespace {
    using Catch::Approx;

    struct CurveTableScope {
        CurveTableScope() {
            Curve::calcTable();
        }

        ~CurveTableScope() {
            Curve::deleteTable();
        }
    };

    void setCubeAsVoicePoint(
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
            vertex->values[Vertex::Time] = time ? 1.f : 0.f;
            vertex->values[Vertex::Red] = red ? 1.f : 0.f;
            vertex->values[Vertex::Blue] = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp] = time ? highAmp : lowAmp;
            vertex->values[Vertex::Curve] = sharpness;
        }
    }

    void addVoiceCube(
            Mesh& mesh,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        auto* cube = new VertCube(&mesh);
        setCubeAsVoicePoint(cube, lowPhase, highPhase, lowAmp, highAmp, sharpness);
        mesh.addCube(cube);
    }
}

TEST_CASE("Shared voice rasterizer builds chained slice intercepts", "[rasterization][voice]") {
    CurveTableScope curveTable;
    Mesh mesh("VoiceSliceMesh");
    addVoiceCube(mesh, 0.20f, 0.80f, 0.25f, 0.75f, 0.35f);
    addVoiceCube(mesh, 0.10f, 0.40f, 0.10f, 0.30f, 0.55f);

    Rasterization::VoiceCycleState state;
    Rasterization::VoiceRasterizer rasterizer;
    rasterizer.setMesh(&mesh);
    rasterizer.setState(&state);
    rasterizer.setMorphPosition(MorphPosition(0.50f, 0.50f, 0.50f));

    rasterizer.updateChainedWaveform(0.85f);
    rasterizer.updateChainedWaveform(0.85f);

    const auto& intercepts = rasterizer.snapshotView().intercepts();
    REQUIRE(intercepts.size() == 2);
    REQUIRE(intercepts[0].x == Approx(0.10f));
    REQUIRE(intercepts[0].y == Approx(-0.60f));
    REQUIRE(intercepts[0].shp == Approx(0.55f));
    REQUIRE(intercepts[1].x == Approx(0.35f));
    REQUIRE(intercepts[1].y == Approx(0.f));
    REQUIRE(intercepts[1].shp == Approx(0.35f));

    mesh.destroy();
}
