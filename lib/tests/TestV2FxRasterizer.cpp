#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Array/VecOps.h"
#include "../src/Curve/V2/Runtime/V2FxRasterizer.h"
#include "../src/Curve/V2/Stages/V2CurveBuilderStages.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

void appendFxTestCube(Mesh& mesh, float phaseOffset) {
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
            + 0.05f
            + (timePole ? 0.25f : 0.0f)
            + (redPole ? 0.2f : 0.0f)
            + (bluePole ? 0.1f : 0.0f);
        vert->values[Vertex::Amp] = 0.1f + (redPole ? 0.45f : 0.0f) + (bluePole ? 0.3f : 0.0f);
        vert->values[Vertex::Curve] = 0.3f;
    }
}

void populateFxTestMesh(Mesh& mesh) {
    appendFxTestCube(mesh, 0.0f);
    appendFxTestCube(mesh, 0.16f);
}

void prepareFxRasterizer(V2FxRasterizer& rasterizer, const Mesh& mesh) {
    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);
    rasterizer.setMeshSnapshot(&mesh);

    V2FxControlSnapshot controls;
    controls.morph = MorphPosition(0.35f, 0.4f, 0.7f);
    controls.scaling = MeshRasterizer::Unipolar;
    controls.cyclic = false;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    rasterizer.updateControlData(controls);
}
}

TEST_CASE("V2FxRasterizer renderAudio requires prepare and mesh", "[curve][v2][fx]") {
    V2FxRasterizer rasterizer;

    V2RenderRequest request;
    request.numSamples = 64;
    request.deltaX = 1.0 / 64.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> outputMemory(64);
    Buffer<float> output = outputMemory.withSize(64);

    V2RenderResult result;
    REQUIRE_FALSE(rasterizer.renderAudio(request, output, result));
    REQUIRE_FALSE(result.rendered);
}

TEST_CASE("V2FxRasterizer renders deterministic output for fixed controls", "[curve][v2][fx][determinism]") {
    ScopedMesh scoped("v2-fx-determinism");
    populateFxTestMesh(scoped.mesh);

    V2FxRasterizer rasterizer;
    prepareFxRasterizer(rasterizer, scoped.mesh);

    V2RenderRequest request;
    request.numSamples = 96;
    request.deltaX = 1.0 / 96.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> firstMemory(96);
    ScopedAlloc<float> secondMemory(96);
    ScopedAlloc<float> diffMemory(96);
    Buffer<float> first = firstMemory.withSize(96);
    Buffer<float> second = secondMemory.withSize(96);
    Buffer<float> diff = diffMemory.withSize(96);

    V2RenderResult firstResult;
    V2RenderResult secondResult;
    REQUIRE(rasterizer.renderAudio(request, first, firstResult));
    REQUIRE(rasterizer.renderAudio(request, second, secondResult));
    REQUIRE(firstResult.samplesWritten == first.size());
    REQUIRE(secondResult.samplesWritten == second.size());

    VecOps::sub(first, second, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 == 0.0f);
    REQUIRE(maxAbs == 0.0f);
}

TEST_CASE("V2FxRasterizer supports linear and cyclic modes", "[curve][v2][fx][modes]") {
    ScopedMesh scoped("v2-fx-modes");
    populateFxTestMesh(scoped.mesh);

    V2FxRasterizer rasterizer;
    prepareFxRasterizer(rasterizer, scoped.mesh);

    V2RenderRequest request;
    request.numSamples = 80;
    request.deltaX = 1.0 / 80.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> linearMemory(80);
    ScopedAlloc<float> cyclicMemory(80);
    Buffer<float> linear = linearMemory.withSize(80);
    Buffer<float> cyclic = cyclicMemory.withSize(80);

    V2RenderResult linearResult;
    REQUIRE(rasterizer.renderAudio(request, linear, linearResult));
    REQUIRE(linearResult.samplesWritten == linear.size());

    V2FxControlSnapshot controls;
    controls.morph = MorphPosition(0.35f, 0.4f, 0.7f);
    controls.scaling = MeshRasterizer::Unipolar;
    controls.cyclic = true;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    rasterizer.updateControlData(controls);

    V2RenderResult cyclicResult;
    REQUIRE(rasterizer.renderAudio(request, cyclic, cyclicResult));
    REQUIRE(cyclicResult.samplesWritten == cyclic.size());
}

TEST_CASE("V2FxRasterizer intercept extraction is morph-invariant like legacy FX path", "[curve][v2][fx][parity]") {
    ScopedMesh scoped("v2-fx-morph-invariant");
    populateFxTestMesh(scoped.mesh);

    V2FxRasterizer rasterizer;
    prepareFxRasterizer(rasterizer, scoped.mesh);

    V2RenderRequest request;
    request.numSamples = 64;
    request.deltaX = 1.0 / 64.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> aMem(64);
    ScopedAlloc<float> bMem(64);
    ScopedAlloc<float> diffMem(64);
    Buffer<float> a = aMem.withSize(64);
    Buffer<float> b = bMem.withSize(64);
    Buffer<float> diff = diffMem.withSize(64);

    V2RenderResult aResult;
    REQUIRE(rasterizer.renderAudio(request, a, aResult));
    REQUIRE(aResult.samplesWritten == a.size());

    V2FxControlSnapshot controls;
    controls.morph = MorphPosition(0.91f, 0.02f, 0.11f);
    controls.scaling = MeshRasterizer::Unipolar;
    controls.cyclic = false;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    rasterizer.updateControlData(controls);

    V2RenderResult bResult;
    REQUIRE(rasterizer.renderAudio(request, b, bResult));
    REQUIRE(bResult.samplesWritten == b.size());

    VecOps::sub(a, b, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 == 0.0f);
    REQUIRE(maxAbs == 0.0f);
}

TEST_CASE("V2FxRasterizer curve builder can reproduce legacy fixed FX padding anchors", "[curve][v2][fx][padding]") {
    std::vector<Intercept> intercepts;
    intercepts.emplace_back(0.0f, 0.2f);
    intercepts.emplace_back(0.4f, 0.5f);
    intercepts.emplace_back(0.9f, 0.8f);

    V2DefaultCurveBuilderStage curveBuilder;
    V2CurveBuilderContext context;
    context.paddingPolicy = V2CurveBuilderContext::PaddingPolicy::FxLegacyFixed;
    context.interpolateCurves = false;

    std::vector<Curve> curves;
    int curveCount = 0;
    REQUIRE(curveBuilder.run(intercepts, static_cast<int>(intercepts.size()), curves, curveCount, context));
    REQUIRE(curveCount == 5);

    REQUIRE(curves[0].a.x == -1.0f);
    REQUIRE(curves[0].b.x == -0.5f);
    REQUIRE(curves[0].c.x == intercepts[0].x);

    REQUIRE(curves[curveCount - 1].a.x == intercepts[2].x);
    REQUIRE(curves[curveCount - 1].b.x == 1.5f);
    REQUIRE(curves[curveCount - 1].c.x == 2.0f);
}
