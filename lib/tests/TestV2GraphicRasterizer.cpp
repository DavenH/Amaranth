#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Curve/V2/Runtime/V2GraphicRasterizer.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

void appendGraphicTestCube(Mesh& mesh, float phaseOffset) {
    auto* cube = new VertCube(&mesh);
    mesh.addCube(cube);

    for (int i = 0; i < static_cast<int>(VertCube::numVerts); ++i) {
        bool timePole = false;
        bool redPole = false;
        bool bluePole = false;
        VertCube::getPoles(i, timePole, redPole, bluePole);

        auto* vert = cube->lineVerts[i];
        vert->values[Vertex::Time] = timePole ? 1.0f : 0.0f;
        vert->values[Vertex::Red] = redPole ? 1.0f : 0.0f;
        vert->values[Vertex::Blue] = bluePole ? 1.0f : 0.0f;

        vert->values[Vertex::Phase] = phaseOffset
            + 0.1f
            + (timePole ? 0.3f : 0.0f)
            + (redPole ? 0.2f : 0.0f);
        vert->values[Vertex::Amp] = 0.15f + (redPole ? 0.25f : 0.0f) + (bluePole ? 0.2f : 0.0f);
        vert->values[Vertex::Curve] = 0.45f;
    }
}

void populateGraphicTestMesh(Mesh& mesh) {
    appendGraphicTestCube(mesh, 0.0f);
    appendGraphicTestCube(mesh, 0.17f);
}
}

TEST_CASE("V2GraphicRasterizer renders after prepare and control update", "[curve][v2][graphic]") {
    ScopedMesh scoped("v2-graphic");
    populateGraphicTestMesh(scoped.mesh);

    V2GraphicRasterizer rasterizer;

    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);
    rasterizer.setMeshSnapshot(&scoped.mesh);

    V2GraphicControlSnapshot controls;
    controls.morph = MorphPosition(0.25f, 0.5f, 0.75f);
    controls.scaling = V2ScalingType::Unipolar;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.cyclic = false;
    rasterizer.updateControlData(controls);

    ScopedAlloc<float> outputMemory(128);
    Buffer<float> output = outputMemory.withSize(128);
    output.zero();

    V2GraphicRequest request;
    request.numSamples = output.size();
    request.interpolateCurves = true;
    request.lowResolution = false;

    V2GraphicResult result;
    REQUIRE(rasterizer.renderGraphic(request, output, result));
    REQUIRE(result.rendered);
    REQUIRE(result.pointsWritten == output.size());
    REQUIRE(output.sum() > 0.0f);
}

TEST_CASE("V2GraphicRasterizer rejects rendering when not prepared", "[curve][v2][graphic]") {
    ScopedMesh scoped("v2-graphic-unprepared");
    populateGraphicTestMesh(scoped.mesh);

    V2GraphicRasterizer rasterizer;
    rasterizer.setMeshSnapshot(&scoped.mesh);

    V2GraphicRequest request;
    request.numSamples = 64;

    ScopedAlloc<float> outputMemory(64);
    Buffer<float> output = outputMemory.withSize(64);

    V2GraphicResult result;
    REQUIRE_FALSE(rasterizer.renderGraphic(request, output, result));
    REQUIRE_FALSE(result.rendered);
    REQUIRE(result.pointsWritten == 0);
}

TEST_CASE("V2GraphicRasterizer supports cyclic and linear positioning modes", "[curve][v2][graphic]") {
    ScopedMesh scoped("v2-graphic-cyclic");
    populateGraphicTestMesh(scoped.mesh);

    V2GraphicRasterizer rasterizer;

    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);
    rasterizer.setMeshSnapshot(&scoped.mesh);

    V2GraphicRequest request;
    request.numSamples = 96;
    request.interpolateCurves = false;
    request.lowResolution = true;

    ScopedAlloc<float> linearMemory(96);
    ScopedAlloc<float> cyclicMemory(96);
    Buffer<float> linear = linearMemory.withSize(96);
    Buffer<float> cyclic = cyclicMemory.withSize(96);
    linear.zero();
    cyclic.zero();

    V2GraphicControlSnapshot controls;
    controls.morph = MorphPosition(0.4f, 0.6f, 0.3f);
    controls.scaling = V2ScalingType::Unipolar;
    controls.interpolateCurves = false;
    controls.lowResolution = true;
    controls.cyclic = false;
    rasterizer.updateControlData(controls);

    V2GraphicResult linearResult;
    REQUIRE(rasterizer.renderGraphic(request, linear, linearResult));
    REQUIRE(linearResult.pointsWritten == linear.size());

    controls.cyclic = true;
    rasterizer.updateControlData(controls);

    V2GraphicResult cyclicResult;
    REQUIRE(rasterizer.renderGraphic(request, cyclic, cyclicResult));
    REQUIRE(cyclicResult.pointsWritten == cyclic.size());
}
