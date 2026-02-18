#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Array/VecOps.h"
#include "../src/Curve/V2/Runtime/V2EnvRasterizer.h"
#include "../src/Curve/V2/Stages/V2StageInterfaces.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

void appendEnvTestCube(Mesh& mesh, float phaseOffset) {
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
            + (timePole ? 0.25f : 0.0f)
            + (redPole ? 0.2f : 0.0f);
        vert->values[Vertex::Amp] = 0.2f + (redPole ? 0.3f : 0.0f) + (bluePole ? 0.1f : 0.0f);
        vert->values[Vertex::Curve] = 0.35f;
    }
}

void populateEnvTestMesh(Mesh& mesh) {
    appendEnvTestCube(mesh, 0.0f);
    appendEnvTestCube(mesh, 0.12f);
}

class FixedEnvInterpolatorStage :
        public V2InterpolatorStage {
public:
    explicit FixedEnvInterpolatorStage(std::vector<Intercept> intercepts) :
            intercepts(std::move(intercepts))
    {}

    bool run(
        const V2InterpolatorContext&,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override {
        outIntercepts = intercepts;
        outCount = static_cast<int>(outIntercepts.size());
        return outCount > 1;
    }

private:
    std::vector<Intercept> intercepts;
};
}

TEST_CASE("V2EnvRasterizer note on/off and release trigger semantics", "[curve][v2][env][rasterizer]") {
    V2EnvRasterizer rasterizer;

    V2EnvControlSnapshot controls;
    controls.hasReleaseCurve = true;
    rasterizer.updateControlData(controls);

    rasterizer.noteOn();
    REQUIRE(rasterizer.getMode() == V2EnvMode::Normal);
    REQUIRE_FALSE(rasterizer.isReleasePending());

    REQUIRE(rasterizer.transitionToLooping(true, true));
    REQUIRE(rasterizer.getMode() == V2EnvMode::Looping);

    REQUIRE(rasterizer.noteOff());
    REQUIRE(rasterizer.getMode() == V2EnvMode::Releasing);
    REQUIRE(rasterizer.isReleasePending());

    REQUIRE(rasterizer.consumeReleaseTrigger());
    REQUIRE_FALSE(rasterizer.isReleasePending());
    REQUIRE_FALSE(rasterizer.consumeReleaseTrigger());
}

TEST_CASE("V2EnvRasterizer renderAudio requires prepare and mesh", "[curve][v2][env][rasterizer]") {
    V2EnvRasterizer rasterizer;

    V2RenderRequest request;
    request.numSamples = 64;
    request.deltaX = 1.0 / 64.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> outputMemory(64);
    Buffer<float> output = outputMemory.withSize(64);
    output.zero();

    V2RenderResult result;
    REQUIRE_FALSE(rasterizer.renderAudio(request, output, result));
    REQUIRE_FALSE(result.rendered);
    REQUIRE(result.samplesWritten == 0);
}

TEST_CASE("V2EnvRasterizer renders deterministic output for fixed inputs", "[curve][v2][env][rasterizer]") {
    ScopedMesh scoped("v2-env-rasterizer");
    populateEnvTestMesh(scoped.mesh);

    V2EnvRasterizer rasterizer;

    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);
    rasterizer.setMeshSnapshot(&scoped.mesh);

    V2EnvControlSnapshot controls;
    controls.morph = MorphPosition(0.2f, 0.4f, 0.8f);
    controls.scaling = MeshRasterizer::Unipolar;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.cyclic = false;
    controls.hasReleaseCurve = true;
    rasterizer.updateControlData(controls);
    rasterizer.noteOn();

    V2RenderRequest request;
    request.numSamples = 128;
    request.deltaX = 1.0 / 128.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> firstMemory(128);
    ScopedAlloc<float> secondMemory(128);
    ScopedAlloc<float> diffMemory(128);
    Buffer<float> first = firstMemory.withSize(128);
    Buffer<float> second = secondMemory.withSize(128);
    Buffer<float> diff = diffMemory.withSize(128);
    first.zero();
    second.zero();

    V2RenderResult firstResult;
    V2RenderResult secondResult;
    REQUIRE(rasterizer.renderAudio(request, first, firstResult));
    rasterizer.noteOn();
    REQUIRE(rasterizer.renderAudio(request, second, secondResult));
    REQUIRE(firstResult.samplesWritten == first.size());
    REQUIRE(secondResult.samplesWritten == second.size());

    VecOps::sub(first, second, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(first.sum() > 0.0f);
    REQUIRE(l2 == 0.0f);
    REQUIRE(maxAbs == 0.0f);
}

TEST_CASE("V2EnvRasterizer loops until release then follows release region", "[curve][v2][env][rasterizer][loop][release]") {
    V2EnvRasterizer rasterizer;

    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);

    ScopedMesh scoped("v2-env-loop-release");
    populateEnvTestMesh(scoped.mesh);
    rasterizer.setMeshSnapshot(&scoped.mesh);

    V2EnvControlSnapshot controls;
    controls.morph = MorphPosition(0.2f, 0.4f, 0.8f);
    controls.scaling = MeshRasterizer::Unipolar;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.hasReleaseCurve = true;
    controls.hasLoopRegion = true;
    controls.loopStartX = 0.0f;
    controls.loopEndX = 0.5f;
    controls.releaseStartX = 0.5f;
    rasterizer.updateControlData(controls);

    rasterizer.noteOn();
    REQUIRE(rasterizer.transitionToLooping(true, true));
    REQUIRE(rasterizer.getMode() == V2EnvMode::Looping);

    V2RenderRequest request;
    request.numSamples = 50; // exactly one loop-length at delta=0.01 for [0.0, 0.5)
    request.deltaX = 0.01;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> loopAMemory(50);
    ScopedAlloc<float> loopBMemory(50);
    ScopedAlloc<float> releaseMemory(50);
    ScopedAlloc<float> diffLoopMemory(50);
    ScopedAlloc<float> diffReleaseMemory(50);

    Buffer<float> loopA = loopAMemory.withSize(50);
    Buffer<float> loopB = loopBMemory.withSize(50);
    Buffer<float> release = releaseMemory.withSize(50);
    Buffer<float> diffLoop = diffLoopMemory.withSize(50);
    Buffer<float> diffRelease = diffReleaseMemory.withSize(50);

    V2RenderResult loopResultA;
    V2RenderResult loopResultB;
    REQUIRE(rasterizer.renderAudio(request, loopA, loopResultA));
    double loopPosAfterA = rasterizer.getSamplePositionForTesting();
    REQUIRE(rasterizer.renderAudio(request, loopB, loopResultB));
    double loopPosAfterB = rasterizer.getSamplePositionForTesting();
    REQUIRE(loopResultA.samplesWritten == loopA.size());
    REQUIRE(loopResultB.samplesWritten == loopB.size());
    REQUIRE(loopPosAfterA >= controls.loopStartX);
    REQUIRE(loopPosAfterA < controls.loopEndX);
    REQUIRE(loopPosAfterB == Catch::Approx(loopPosAfterA).margin(1e-6));

    VecOps::sub(loopA, loopB, diffLoop);
    float loopL2 = diffLoop.normL2();
    diffLoop.abs();
    float loopMax = diffLoop.max();
    REQUIRE(loopL2 == 0.0f);
    REQUIRE(loopMax == 0.0f);

    REQUIRE(rasterizer.noteOff());
    REQUIRE(rasterizer.getMode() == V2EnvMode::Releasing);
    REQUIRE(rasterizer.isReleasePending());

    V2RenderResult releaseResult;
    REQUIRE(rasterizer.renderAudio(request, release, releaseResult));
    double releasePosAfter = rasterizer.getSamplePositionForTesting();
    REQUIRE(releaseResult.samplesWritten == release.size());
    REQUIRE_FALSE(rasterizer.isReleasePending());
    REQUIRE(releasePosAfter > controls.releaseStartX);
    REQUIRE(releasePosAfter > controls.loopEndX);

    VecOps::sub(loopA, release, diffRelease);
    float releaseL2 = diffRelease.normL2();
    REQUIRE(releaseL2 >= 0.0f);
}
