#include <catch2/catch_test_macros.hpp>

#include "RasterizerParityHarness.h"

using namespace RasterizerParityHarness;

namespace {
void populateHarnessMesh(Mesh& mesh) {
    appendCube(mesh, makeCubeFromPoles(
        0.10f,
        0.28f,
        0.19f,
        0.09f,
        0.12f,
        0.34f,
        0.21f,
        0.40f));

    appendCube(mesh, makeCubeFromPoles(
        0.31f,
        0.22f,
        0.17f,
        0.07f,
        0.18f,
        0.29f,
        0.16f,
        0.36f));
}
}

TEST_CASE("Graphic parity facade compares legacy and v2 sample outputs", "[curve][v2][parity][harness]") {
    ScopedMesh scoped("graphic-parity-harness");
    populateHarnessMesh(scoped.mesh);

    GraphicRenderConfig config;
    config.morph = MorphPosition(0.35f, 0.55f, 0.72f);
    config.scaling = MeshRasterizer::Unipolar;
    config.cyclic = false;
    config.interpolateCurves = true;
    config.lowResolution = false;
    config.integralSampling = false;
    config.minX = 0.0f;
    config.maxX = 1.0f;
    config.primaryDimension = Vertex::Time;
    config.sampleCount = 128;

    GraphicParityFacade facade;
    GraphicParityResult result;

    REQUIRE(facade.run(scoped.mesh, config, result));

    REQUIRE(result.legacy.rendered);
    REQUIRE(result.v2.rendered);
    REQUIRE(result.legacy.intercepts.size() > 1);
    REQUIRE(result.v2.intercepts.size() > 1);
    REQUIRE(result.legacy.intercepts.size() == result.v2.intercepts.size());
    REQUIRE(result.legacy.samples.size() == static_cast<size_t>(config.sampleCount));
    REQUIRE(result.v2.samples.size() == static_cast<size_t>(config.sampleCount));

    REQUIRE(result.maxAbsDiff < 0.0025f);
    REQUIRE(result.l2Diff < 0.01f);
}

TEST_CASE("Graphic parity harness accepts explicit per-vertex cube specifications", "[curve][v2][parity][harness]") {
    ScopedMesh scoped("graphic-parity-explicit-cube");

    CubeVertexSpecs cubeSpecs{};
    for (int i = 0; i < static_cast<int>(VertCube::numVerts); ++i) {
        bool timePole = false;
        bool redPole = false;
        bool bluePole = false;
        VertCube::getPoles(i, timePole, redPole, bluePole);

        Vertex spec;
        spec.values[Vertex::Time] = timePole ? 1.0f : 0.0f;
        spec.values[Vertex::Red] = redPole ? 1.0f : 0.0f;
        spec.values[Vertex::Blue] = bluePole ? 1.0f : 0.0f;
        spec.values[Vertex::Phase] = 0.08f
            + (timePole ? 0.24f : 0.0f)
            + (redPole ? 0.13f : 0.0f)
            + (bluePole ? 0.11f : 0.0f);
        spec.values[Vertex::Amp] = 0.20f + (redPole ? 0.25f : 0.0f) + (bluePole ? 0.18f : 0.0f);
        spec.values[Vertex::Curve] = 0.5f;

        cubeSpecs[static_cast<size_t>(i)] = spec;
    }

    appendCube(scoped.mesh, cubeSpecs);
    appendCube(scoped.mesh, makeCubeFromPoles(0.26f, 0.20f, 0.12f, 0.06f, 0.14f, 0.30f, 0.17f, 0.45f));

    GraphicRenderConfig config;
    config.morph = MorphPosition(0.22f, 0.61f, 0.44f);
    config.sampleCount = 96;

    GraphicParityFacade facade;
    GraphicParityResult result;

    REQUIRE(facade.run(scoped.mesh, config, result));
    REQUIRE(result.sampleDiff.size() == static_cast<size_t>(config.sampleCount));
    REQUIRE(result.legacy.curveCount > 0);
    REQUIRE(result.v2.curveCount > 0);
}
