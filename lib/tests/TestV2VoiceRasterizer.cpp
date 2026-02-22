#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Array/VecOps.h"
#include "../src/Curve/V2/Runtime/V2VoiceRasterizer.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

void appendVoiceTestCube(Mesh& mesh, float phaseOffset) {
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
            + (timePole ? 0.2f : 0.0f)
            + (redPole ? 0.15f : 0.0f)
            + (bluePole ? 0.1f : 0.0f);
        vert->values[Vertex::Amp] = 0.2f + (redPole ? 0.3f : 0.0f) + (bluePole ? 0.15f : 0.0f);
        vert->values[Vertex::Curve] = 0.4f;
    }
}

void populateVoiceTestMesh(Mesh& mesh) {
    appendVoiceTestCube(mesh, 0.0f);
    appendVoiceTestCube(mesh, 0.18f);
}

void appendVoiceChainingCube(Mesh& mesh, int cubeIndex, float phaseBase, float ampBase) {
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

        float phaseMod = (timePole ? 0.31f : -0.04f) + (redPole ? 0.09f : 0.0f) + (bluePole ? -0.06f : 0.0f);
        float ampMod = (timePole ? 0.22f : 0.03f) + (redPole ? 0.11f : -0.02f) + (bluePole ? 0.08f : -0.01f);

        vert->values[Vertex::Phase] = phaseBase + 0.17f * static_cast<float>(cubeIndex) + phaseMod;
        vert->values[Vertex::Amp] = ampBase + 0.05f * static_cast<float>(cubeIndex) + ampMod;
        vert->values[Vertex::Curve] = 0.2f + 0.12f * static_cast<float>((cubeIndex + i) % 4);
    }
}

void populateVoiceChainingMesh(Mesh& mesh) {
    appendVoiceChainingCube(mesh, 0, -0.18f, 0.08f);
    appendVoiceChainingCube(mesh, 1, 0.12f, 0.10f);
    appendVoiceChainingCube(mesh, 2, 0.48f, 0.14f);
    appendVoiceChainingCube(mesh, 3, 0.86f, 0.18f);
}

float absf(float x) {
    return x >= 0.0f ? x : -x;
}

float maxAdjacentStep(Buffer<float> data) {
    if (data.size() < 2) {
        return 0.0f;
    }

    float maxStep = 0.0f;
    for (int i = 1; i < data.size(); ++i) {
        float step = absf(data[i] - data[i - 1]);
        if (step > maxStep) {
            maxStep = step;
        }
    }

    return maxStep;
}

void prepareVoiceRasterizer(V2VoiceRasterizer& rasterizer, const Mesh& mesh) {
    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);
    rasterizer.setMeshSnapshot(&mesh);

    V2VoiceControlSnapshot controls;
    controls.morph = MorphPosition(0.2f, 0.5f, 0.75f);
    controls.scaling = MeshRasterizer::Bipolar;
    controls.cyclic = true;
    controls.minX = 0.0f;
    controls.maxX = 1.0f;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    rasterizer.updateControlData(controls);
    rasterizer.resetPhase(0.0);
}
}

TEST_CASE("V2VoiceRasterizer renderAudio requires prepare and mesh", "[curve][v2][voice]") {
    V2VoiceRasterizer rasterizer;

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

TEST_CASE("V2VoiceRasterizer block continuity matches single long render", "[curve][v2][voice][continuity]") {
    ScopedMesh scoped("v2-voice-continuity");
    populateVoiceTestMesh(scoped.mesh);

    V2VoiceRasterizer splitRender;
    V2VoiceRasterizer fullRender;
    prepareVoiceRasterizer(splitRender, scoped.mesh);
    prepareVoiceRasterizer(fullRender, scoped.mesh);

    V2RenderRequest splitRequest;
    splitRequest.numSamples = 64;
    splitRequest.deltaX = 0.01;
    splitRequest.tempoScale = 1.0f;
    splitRequest.scale = 1;

    V2RenderRequest fullRequest = splitRequest;
    fullRequest.numSamples = 128;

    ScopedAlloc<float> aMemory(64);
    ScopedAlloc<float> bMemory(64);
    ScopedAlloc<float> fullMemory(128);
    ScopedAlloc<float> diffAMemory(64);
    ScopedAlloc<float> diffBMemory(64);

    Buffer<float> a = aMemory.withSize(64);
    Buffer<float> b = bMemory.withSize(64);
    Buffer<float> full = fullMemory.withSize(128);
    Buffer<float> diffA = diffAMemory.withSize(64);
    Buffer<float> diffB = diffBMemory.withSize(64);

    V2RenderResult resultA;
    V2RenderResult resultB;
    V2RenderResult resultFull;
    REQUIRE(splitRender.renderAudio(splitRequest, a, resultA));
    REQUIRE(splitRender.renderAudio(splitRequest, b, resultB));
    REQUIRE(fullRender.renderAudio(fullRequest, full, resultFull));

    VecOps::sub(a, full.section(0, 64), diffA);
    VecOps::sub(b, full.section(64, 64), diffB);
    float l2A = diffA.normL2();
    float l2B = diffB.normL2();

    REQUIRE(l2A == 0.0f);
    REQUIRE(l2B == 0.0f);
}

TEST_CASE("V2VoiceRasterizer deterministic with phase reset", "[curve][v2][voice][determinism]") {
    ScopedMesh scoped("v2-voice-determinism");
    populateVoiceTestMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    V2RenderRequest request;
    request.numSamples = 96;
    request.deltaX = 0.008;
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
    rasterizer.resetPhase(0.0);
    REQUIRE(rasterizer.renderAudio(request, second, secondResult));

    VecOps::sub(first, second, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 == 0.0f);
    REQUIRE(maxAbs == 0.0f);
}

TEST_CASE("V2VoiceRasterizer phase wraps in cyclic mode", "[curve][v2][voice][phase]") {
    ScopedMesh scoped("v2-voice-wrap");
    populateVoiceTestMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    V2RenderRequest request;
    request.numSamples = 400;
    request.deltaX = 0.01;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> outputMemory(400);
    Buffer<float> output = outputMemory.withSize(400);

    V2RenderResult result;
    REQUIRE(rasterizer.renderAudio(request, output, result));

    double phase = rasterizer.getPhaseForTesting();
    REQUIRE(phase >= 0.0);
    REQUIRE(phase < 1.0);
}

TEST_CASE("V2VoiceRasterizer chaining interpolation stays continuous across cycle boundaries", "[curve][v2][voice][chaining][continuity]") {
    ScopedMesh scoped("v2-voice-chaining");
    populateVoiceChainingMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    V2VoiceControlSnapshot controls;
    controls.morph = MorphPosition(0.47f, 0.63f, 0.39f);
    controls.scaling = MeshRasterizer::Bipolar;
    controls.cyclic = true;
    controls.minX = 0.0f;
    controls.maxX = 1.0f;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    rasterizer.updateControlData(controls);
    rasterizer.resetPhase(0.0);

    V2RenderRequest request;
    request.numSamples = 128;
    request.deltaX = 1.0 / 128.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> cycleAMemory(128);
    ScopedAlloc<float> cycleBMemory(128);
    ScopedAlloc<float> diffMemory(128);

    Buffer<float> cycleA = cycleAMemory.withSize(128);
    Buffer<float> cycleB = cycleBMemory.withSize(128);
    Buffer<float> diff = diffMemory.withSize(128);

    V2RenderResult resultA;
    V2RenderResult resultB;
    REQUIRE(rasterizer.renderAudio(request, cycleA, resultA));
    REQUIRE(rasterizer.renderAudio(request, cycleB, resultB));
    REQUIRE(resultA.samplesWritten == cycleA.size());
    REQUIRE(resultB.samplesWritten == cycleB.size());

    VecOps::sub(cycleA, cycleB, diff);
    float cycleL2 = diff.normL2();
    REQUIRE(cycleL2 >= 0.0f);

    float boundaryStep = absf(cycleB[0] - cycleA[cycleA.size() - 1]);
    float internalMax = maxAdjacentStep(cycleA);

    float continuityScale = jmax(internalMax, 1e-3f);
    REQUIRE(boundaryStep <= continuityScale * 1.25f + 1e-4f);
}

TEST_CASE("V2VoiceRasterizer chaining remains smooth across varying mesh positions", "[curve][v2][voice][chaining][mesh]") {
    ScopedMesh scoped("v2-voice-chaining-morph");
    populateVoiceChainingMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);
    rasterizer.resetPhase(0.0);

    V2RenderRequest request;
    request.numSamples = 128;
    request.deltaX = 1.0 / 128.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> prevMemory(128);
    ScopedAlloc<float> currMemory(128);
    ScopedAlloc<float> diffMemory(128);
    Buffer<float> previous = prevMemory.withSize(128);
    Buffer<float> current = currMemory.withSize(128);
    Buffer<float> diff = diffMemory.withSize(128);

    bool havePrevious = false;
    float maxBoundaryStep = 0.0f;
    float maxInternalStep = 0.0f;
    float maxBlockDelta = 0.0f;

    for (int block = 0; block < 6; ++block) {
        for (auto* vert : scoped.mesh.getVerts()) {
            float phaseOffset = 0.07f * static_cast<float>(block + 1);
            float ampDirection = vert->values[Vertex::Time] > 0.5f ? 1.0f : -1.0f;
            float ampOffset = (block % 2 == 0 ? 0.12f : -0.10f) * ampDirection;

            vert->values[Vertex::Phase] += phaseOffset;
            vert->values[Vertex::Amp] = jlimit(0.0f, 1.0f, vert->values[Vertex::Amp] + ampOffset);
        }

        V2RenderResult result;
        REQUIRE(rasterizer.renderAudio(request, current, result));
        REQUIRE(result.samplesWritten == current.size());

        float currentInternal = maxAdjacentStep(current);
        if (currentInternal > maxInternalStep) {
            maxInternalStep = currentInternal;
        }

        if (havePrevious) {
            float boundaryStep = absf(current[0] - previous[previous.size() - 1]);
            if (boundaryStep > maxBoundaryStep) {
                maxBoundaryStep = boundaryStep;
            }

            VecOps::sub(current, previous, diff);
            float deltaL2 = diff.normL2();
            if (deltaL2 > maxBlockDelta) {
                maxBlockDelta = deltaL2;
            }
        }

        current.copyTo(previous);
        havePrevious = true;
    }

    float continuityScale = jmax(maxInternalStep, 1e-3f);
    REQUIRE(maxBoundaryStep <= continuityScale * 1.5f + 1e-4f);
}
