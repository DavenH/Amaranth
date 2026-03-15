#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

#include <App/SingletonAccessor.h>
#include <Array/ScopedAlloc.h>
#include <Array/VecOps.h>
#include <Curve/Mesh.h>
#include <Curve/V2/Rasterizers/V2VoiceRasterizer.h>
#include <Curve/V2/Sampling/V2WaveSampling.h>
#include <Curve/VertCube.h>
#include <Curve/VoiceMeshRasterizer.h>

#include "../src/Curve/CycleState.h"
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

void wrapMeshPhasesToUnitRange(Mesh& mesh) {
    for (auto* vert : mesh.getVerts()) {
        float& phase = vert->values[Vertex::Phase];

        while (phase >= 1.0f) {
            phase -= 1.0f;
        }

        while (phase < 0.0f) {
            phase += 1.0f;
        }
    }
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

struct InvalidSampleSummary {
    int invalidCount{0};
    int firstInvalidIndex{-1};
};

struct CurveInvalidSummary {
    int invalidCurveCount{0};
    int firstInvalidCurve{-1};
    float ax{0.0f};
    float ay{0.0f};
    float bx{0.0f};
    float by{0.0f};
    float cx{0.0f};
    float cy{0.0f};
    int resIndex{0};
};

InvalidSampleSummary scanInvalidSamples(const char* label, Buffer<float> buffer) {
    InvalidSampleSummary summary;

    for (int i = 0; i < buffer.size(); ++i) {
        if (! std::isfinite(buffer[i])) {
            ++summary.invalidCount;
            if (summary.firstInvalidIndex < 0) {
                summary.firstInvalidIndex = i;
            }
        }
    }

    INFO(label << " invalidSamples=" << summary.invalidCount
         << " firstInvalidIndex=" << summary.firstInvalidIndex);
    return summary;
}

CurveInvalidSummary scanCurveTransforms(const char* label, const std::vector<Curve>& curves) {
    CurveInvalidSummary summary;

    for (int curveIndex = 0; curveIndex < static_cast<int>(curves.size()); ++curveIndex) {
        const Curve& curve = curves[curveIndex];
        int res = Curve::resolution >> curve.resIndex;

        for (int i = 0; i < res; ++i) {
            if (! std::isfinite(curve.transformX[i]) || ! std::isfinite(curve.transformY[i])) {
                ++summary.invalidCurveCount;
                if (summary.firstInvalidCurve < 0) {
                    summary.firstInvalidCurve = curveIndex;
                    summary.ax = curve.a.x;
                    summary.ay = curve.a.y;
                    summary.bx = curve.b.x;
                    summary.by = curve.b.y;
                    summary.cx = curve.c.x;
                    summary.cy = curve.c.y;
                    summary.resIndex = curve.resIndex;
                }
                break;
            }
        }
    }
    return summary;
}

bool findFiniteRange(Buffer<float> buffer, int& first, int& last) {
    first = -1;
    last = -1;

    for (int i = 0; i < buffer.size(); ++i) {
        if (std::isfinite(buffer[i])) {
            first = i;
            break;
        }
    }

    for (int i = buffer.size() - 1; i >= 0; --i) {
        if (std::isfinite(buffer[i])) {
            last = i;
            break;
        }
    }

    return first >= 0 && last > first;
}

int sanitizeInvalidSamples(const char* label, Buffer<float> buffer) {
    int invalidCount = 0;
    for (int i = 0; i < buffer.size(); ++i) {
        if (! std::isfinite(buffer[i])) {
            ++invalidCount;
            buffer[i] = 0.0f;
        }
    }

    INFO(label << " invalidSamples=" << invalidCount);
    return invalidCount;
}

struct LegacyVoicePhaseDiagnostics {
    int overlapCount{0};
    int invalidPhaseCount{0};
    int negativePhaseCount{0};
    int overOnePhaseCount{0};
    float minPhase{std::numeric_limits<float>::infinity()};
    float maxPhase{-std::numeric_limits<float>::infinity()};
};

LegacyVoicePhaseDiagnostics collectLegacyVoicePhaseDiagnostics(
    const Mesh& mesh,
    const MorphPosition& morph,
    float advancement) {
    LegacyVoicePhaseDiagnostics diagnostics;
    VertCube::ReductionData reduct;
    float voiceTime = jmin(1.f, morph.time + advancement);

    for (auto* cube : mesh.getCubes()) {
        cube->getInterceptsFast(Vertex::Time, reduct, MorphPosition(voiceTime, morph.red, morph.blue));

        if (! reduct.pointOverlaps) {
            continue;
        }

        ++diagnostics.overlapCount;

        float aPhase = reduct.v0.values[Vertex::Phase];
        float bPhase = reduct.v1.values[Vertex::Phase];

        diagnostics.minPhase = jmin(diagnostics.minPhase, jmin(aPhase, bPhase));
        diagnostics.maxPhase = jmax(diagnostics.maxPhase, jmax(aPhase, bPhase));

        float phases[2] = { aPhase, bPhase };
        for (float phase : phases) {
            if (! std::isfinite(phase)) {
                ++diagnostics.invalidPhaseCount;
                continue;
            }

            if (phase < 0.0f) {
                ++diagnostics.negativePhaseCount;
            }

            if (phase > 1.0f) {
                ++diagnostics.overOnePhaseCount;
            }
        }
    }

    INFO("legacyVoice overlaps=" << diagnostics.overlapCount
         << " invalidPhases=" << diagnostics.invalidPhaseCount
         << " negativePhases=" << diagnostics.negativePhaseCount
         << " overOnePhases=" << diagnostics.overOnePhaseCount
         << " minPhase=" << diagnostics.minPhase
         << " maxPhase=" << diagnostics.maxPhase
         << " advancement=" << advancement);

    return diagnostics;
}

void prepareLegacyVoiceRasterizer(
    VoiceMeshRasterizer& rasterizer,
    CycleState& state,
    Mesh& mesh,
    const MorphPosition& morph) {
    rasterizer.setState(&state);
    rasterizer.setMesh(&mesh);
    rasterizer.setMorphPosition(morph);
    rasterizer.setScalingMode(MeshRasterizer::Bipolar);
    rasterizer.setWrapsEnds(true);
    rasterizer.setInterpolatesCurves(true);
    rasterizer.setLowresCurves(false);
    rasterizer.setLimits(0.0f, 1.0f);
}

bool primeLegacyVoiceRasterizer(
    VoiceMeshRasterizer& rasterizer,
    float oscPhase = 0.0f) {
    rasterizer.calcCrossPointsChaining(oscPhase);
    rasterizer.calcCrossPointsChaining(oscPhase);
    return rasterizer.isSampleable();
}

struct LegacyVoiceChainingOracleState {
    Intercept frontA{-0.05f, 0.0f};
    Intercept frontB{-0.10f, 0.0f};
    Intercept frontC{-0.15f, 0.0f};
    Intercept frontD{-0.25f, 0.0f};
    Intercept frontE{-0.35f, 0.0f};
    std::vector<Intercept> icpts;
    std::vector<Intercept> backIcpts;
    int callCount{0};
    float advancement{0.0f};
};

void restrictInterceptsLegacyOracle(std::vector<Intercept>& intercepts, float minX, float maxX) {
    if (intercepts.empty()) {
        return;
    }

    for (auto& intercept : intercepts) {
        intercept.adjustedX = jlimit(minX, maxX, intercept.adjustedX);
    }

    for (int i = 1; i < static_cast<int>(intercepts.size()); ++i) {
        Intercept& a = intercepts[i - 1];
        Intercept& b = intercepts[i];
        if (b.adjustedX < a.adjustedX && b.isWrapped == a.isWrapped) {
            b.adjustedX = a.adjustedX + 0.0001f;
        }
    }

    for (auto& intercept : intercepts) {
        intercept.x = intercept.adjustedX;
    }

    if (intercepts.back().x >= maxX) {
        for (int i = static_cast<int>(intercepts.size()) - 1; i >= 1; --i) {
            Intercept& left = intercepts[i - 1];
            Intercept& right = intercepts[i];
            if (left.x >= right.x) {
                left.x = right.x - 0.0001f;
            }
        }
    }

    for (int i = 1; i < static_cast<int>(intercepts.size()); ++i) {
        Intercept& left = intercepts[i - 1];
        Intercept& right = intercepts[i];
        if (right.x <= left.x) {
            right.x = left.x + 0.0001f;
        }
    }
}

void runLegacyVoiceChainingOracleStep(
    const Mesh& mesh,
    const MorphPosition& morph,
    float minX,
    float maxX,
    float oscPhase,
    float minLineLength,
    LegacyVoiceChainingOracleState& state) {
    if (mesh.getNumCubes() == 0) {
        return;
    }

    if (state.callCount > 0) {
        std::swap(state.backIcpts, state.icpts);
        state.backIcpts.clear();
    }

    VertCube::ReductionData reduct;

    for (auto* cube : mesh.getCubes()) {
        float voiceTime = jmin(1.0f, morph.time + state.advancement);
        cube->getInterceptsFast(Vertex::Time, reduct, MorphPosition(voiceTime, morph.red, morph.blue));

        Vertex* a = &reduct.v0;
        Vertex* b = &reduct.v1;
        Vertex* v = &reduct.v;

        if (! reduct.pointOverlaps) {
            continue;
        }

        if (a->values[Vertex::Phase] > 1.0f && b->values[Vertex::Phase] > 1.0f) {
            a->values[Vertex::Phase] -= 1.0f;
            b->values[Vertex::Phase] -= 1.0f;
        }

        if ((a->values[Vertex::Phase] > 1.0f) != (b->values[Vertex::Phase] > 1.0f)) {
            float icpt = a->values[Vertex::Time]
                + (1.0f - a->values[Vertex::Phase])
                    / ((a->values[Vertex::Phase] - b->values[Vertex::Phase])
                       / (a->values[Vertex::Time] - b->values[Vertex::Time]));
            if (icpt > voiceTime) {
                a->values[Vertex::Phase] -= 1.0f;
                b->values[Vertex::Phase] -= 1.0f;
            }
        }

        VertCube::vertexAt(voiceTime, Vertex::Time, a, b, v);

        float phase = v->values[Vertex::Phase] + oscPhase;
        while (phase >= 1.0f) {
            phase -= 1.0f;
        }
        while (phase < 0.0f) {
            phase += 1.0f;
        }

        Intercept intercept(phase, 2.0f * v->values[Vertex::Amp] - 1.0f, cube, 0.0f);
        intercept.shp = v->values[Vertex::Curve];
        intercept.adjustedX = intercept.x;
        state.backIcpts.push_back(intercept);
    }

    std::sort(state.backIcpts.begin(), state.backIcpts.end());
    restrictInterceptsLegacyOracle(state.backIcpts, minX, maxX);

    if (state.backIcpts.size() < 2 || state.icpts.size() < 2) {
        ++state.callCount;
        return;
    }

    if (state.callCount == 0) {
        state.advancement = minLineLength * 1.1f;
    }

    ++state.callCount;
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
    controls.scaling = V2ScalingType::Bipolar;
    controls.cyclic = true;
    controls.minX = 0.0f;
    controls.maxX = 1.0f;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    rasterizer.updateControlData(controls);
    rasterizer.resetPhase(0.0);
}
}

TEST_CASE("V2VoiceRasterizer renderBlock requires prepare and mesh", "[curve][v2][voice][contract]") {
    V2VoiceRasterizer rasterizer;

    V2RenderRequest request;
    request.numSamples = 64;
    request.deltaX = 1.0 / 64.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> outputMemory(64);
    Buffer<float> output = outputMemory.withSize(64);

    V2RenderResult result;
    REQUIRE_FALSE(rasterizer.renderBlock(request, output, result));
    REQUIRE_FALSE(result.rendered);
}

TEST_CASE("V2VoiceRasterizer block continuity matches single long render", "[curve][v2][voice][continuity][contract]") {
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
    REQUIRE(splitRender.renderBlock(splitRequest, a, resultA));
    REQUIRE(splitRender.renderBlock(splitRequest, b, resultB));
    REQUIRE(fullRender.renderBlock(fullRequest, full, resultFull));

    VecOps::sub(a, full.section(0, 64), diffA);
    VecOps::sub(b, full.section(64, 64), diffB);
    float l2A = diffA.normL2();
    float l2B = diffB.normL2();

    REQUIRE(l2A == 0.0f);
    REQUIRE(l2B == 0.0f);
}

TEST_CASE("V2VoiceRasterizer deterministic with phase reset", "[curve][v2][voice][determinism][contract]") {
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
    REQUIRE(rasterizer.renderBlock(request, first, firstResult));
    rasterizer.resetPhase(0.0);
    REQUIRE(rasterizer.renderBlock(request, second, secondResult));

    VecOps::sub(first, second, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 == 0.0f);
    REQUIRE(maxAbs == 0.0f);
}

TEST_CASE("V2VoiceRasterizer phase wraps in cyclic mode", "[curve][v2][voice][phase][contract]") {
    ScopedMesh scoped("v2-voice-wrap");
    populateVoiceTestMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    V2RenderRequest request;
    request.numSamples = 400;
    request.deltaX = 1.0 / 400.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> outputMemory(400);
    Buffer<float> output = outputMemory.withSize(400);

    V2RenderResult result;
    REQUIRE(rasterizer.renderBlock(request, output, result));

    double phase = rasterizer.getPhaseForTesting();
    REQUIRE(phase >= -0.5);
    REQUIRE(phase <= 0.5);
}

TEST_CASE("V2VoiceRasterizer applies non-zero minLineLength advancement on subsequent intercept renders", "[curve][v2][voice][chaining][advancement][contract]") {
    ScopedMesh scoped("v2-voice-min-line-advancement");
    populateVoiceChainingMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    V2VoiceControlSnapshot controls;
    controls.morph = MorphPosition(0.23f, 0.61f, 0.44f);
    controls.scaling = V2ScalingType::Bipolar;
    controls.cyclic = true;
    controls.minX = 0.0f;
    controls.maxX = 1.0f;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.minLineLength = 0.01f;
    rasterizer.updateControlData(controls);
    rasterizer.resetPhase(0.0);

    V2RasterArtifacts first;
    V2RasterArtifacts second;
    REQUIRE(rasterizer.renderIntercepts(first));
    REQUIRE(first.intercepts != nullptr);
    std::vector<Intercept> firstSnapshot(first.intercepts->begin(), first.intercepts->end());
    REQUIRE(rasterizer.renderIntercepts(second));
    REQUIRE(second.intercepts != nullptr);
    REQUIRE(firstSnapshot.size() == second.intercepts->size());

    bool changed = false;
    for (size_t i = 0; i < firstSnapshot.size(); ++i) {
        if (std::abs(firstSnapshot[i].x - (*second.intercepts)[i].x) > 1e-6f
                || std::abs(firstSnapshot[i].y - (*second.intercepts)[i].y) > 1e-6f) {
            changed = true;
            break;
        }
    }

    REQUIRE(changed);
}

TEST_CASE("V2VoiceRasterizer chaining interpolation stays continuous across cycle boundaries", "[curve][v2][voice][chaining][continuity][contract]") {
    ScopedMesh scoped("v2-voice-chaining");
    populateVoiceChainingMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    V2VoiceControlSnapshot controls;
    controls.morph = MorphPosition(0.47f, 0.63f, 0.39f);
    controls.scaling = V2ScalingType::Bipolar;
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
    REQUIRE(rasterizer.renderBlock(request, cycleA, resultA));
    REQUIRE(rasterizer.renderBlock(request, cycleB, resultB));
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

TEST_CASE("V2VoiceRasterizer chaining remains smooth across varying mesh positions", "[curve][v2][voice][chaining][mesh][contract]") {
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
        REQUIRE(rasterizer.renderBlock(request, current, result));
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

TEST_CASE("V2VoiceRasterizer chaining intercepts match legacy oracle sequencing", "[curve][v2][voice][chaining][parity][legacy]") {
    ScopedMesh scoped("v2-voice-chaining-parity");
    populateVoiceChainingMesh(scoped.mesh);

    V2VoiceRasterizer rasterizer;
    prepareVoiceRasterizer(rasterizer, scoped.mesh);

    LegacyVoiceChainingOracleState legacy;
    std::vector<Intercept> previousV2Intercepts;

    for (int block = 0; block < 6; ++block) {
        MorphPosition morph(
            0.47f,
            0.63f + 0.02f * static_cast<float>(block),
            0.39f - 0.015f * static_cast<float>(block));

        V2VoiceControlSnapshot controls;
        controls.morph = morph;
        controls.scaling = V2ScalingType::Bipolar;
        controls.cyclic = true;
        controls.minX = 0.0f;
        controls.maxX = 1.0f;
        controls.interpolateCurves = true;
        controls.lowResolution = false;
        controls.integralSampling = false;
        rasterizer.updateControlData(controls);

        runLegacyVoiceChainingOracleStep(scoped.mesh, morph, controls.minX, controls.maxX, 0.0f, 0.0f, legacy);

        V2RasterArtifacts v2Artifacts;
        REQUIRE(rasterizer.renderIntercepts(v2Artifacts));
        REQUIRE(legacy.backIcpts.size() == v2Artifacts.intercepts->size());

        for (int i = 0; i < static_cast<int>(v2Artifacts.intercepts->size()); ++i) {
            REQUIRE((*v2Artifacts.intercepts)[i].x == Catch::Approx(legacy.backIcpts[i].x).margin(1e-6f));
            REQUIRE((*v2Artifacts.intercepts)[i].y == Catch::Approx(legacy.backIcpts[i].y).margin(1e-6f));
            REQUIRE((*v2Artifacts.intercepts)[i].shp == Catch::Approx(legacy.backIcpts[i].shp).margin(1e-6f));
            REQUIRE((*v2Artifacts.intercepts)[i].adjustedX == Catch::Approx(legacy.backIcpts[i].adjustedX).margin(1e-6f));
        }

        if (block > 0) {
            REQUIRE(static_cast<int>(legacy.icpts.size()) == static_cast<int>(previousV2Intercepts.size()));
            for (int i = 0; i < static_cast<int>(legacy.icpts.size()); ++i) {
                REQUIRE(legacy.icpts[i].x == Catch::Approx(previousV2Intercepts[i].x).margin(1e-6f));
                REQUIRE(legacy.icpts[i].y == Catch::Approx(previousV2Intercepts[i].y).margin(1e-6f));
            }
        }

        previousV2Intercepts.assign(
            v2Artifacts.intercepts->begin(),
            v2Artifacts.intercepts->end());
    }
}

TEST_CASE("Legacy VoiceMeshRasterizer chaining inputs stay phase-valid", "[curve][legacy][voice][contract]") {
    ScopedMesh scoped("legacy-voice-phase-contract");
    populateVoiceChainingMesh(scoped.mesh);
    wrapMeshPhasesToUnitRange(scoped.mesh);

    MorphPosition morph(0.47f, 0.63f, 0.39f);
    LegacyVoicePhaseDiagnostics initial = collectLegacyVoicePhaseDiagnostics(scoped.mesh, morph, 0.0f);

    REQUIRE(initial.overlapCount > 0);
    REQUIRE(initial.invalidPhaseCount == 0);
    REQUIRE(initial.negativePhaseCount == 0);

    CycleState state;
    VoiceMeshRasterizer legacy(nullptr);
    prepareLegacyVoiceRasterizer(legacy, state, scoped.mesh, morph);
    legacy.calcCrossPointsChaining(0.0f);

    LegacyVoicePhaseDiagnostics primed = collectLegacyVoicePhaseDiagnostics(scoped.mesh, morph, state.advancement);
    REQUIRE(primed.overlapCount > 0);
    REQUIRE(primed.invalidPhaseCount == 0);
    REQUIRE(primed.negativePhaseCount == 0);
}

TEST_CASE("Legacy VoiceMeshRasterizer primed chained render remains finite", "[curve][legacy][voice][contract]") {
    ScopedMesh scoped("legacy-voice-finite-contract");
    populateVoiceChainingMesh(scoped.mesh);
    wrapMeshPhasesToUnitRange(scoped.mesh);

    MorphPosition morph(0.47f, 0.63f, 0.39f);
    CycleState state;
    VoiceMeshRasterizer legacy(nullptr);
    prepareLegacyVoiceRasterizer(legacy, state, scoped.mesh, morph);
    REQUIRE(primeLegacyVoiceRasterizer(legacy, 0.0f));
    CurveInvalidSummary curveSummary = scanCurveTransforms("legacyVoiceCurves", legacy.getCurves());
    INFO("legacyVoiceCurves invalidCurves=" << curveSummary.invalidCurveCount
         << " firstInvalidCurve=" << curveSummary.firstInvalidCurve
         << " a=(" << curveSummary.ax << "," << curveSummary.ay << ")"
         << " b=(" << curveSummary.bx << "," << curveSummary.by << ")"
         << " c=(" << curveSummary.cx << "," << curveSummary.cy << ")"
         << " resIndex=" << curveSummary.resIndex);
    REQUIRE(curveSummary.invalidCurveCount == 0);

    Buffer<float> legacyWaveX = legacy.getWaveX();
    Buffer<float> legacyWaveY = legacy.getWaveY();
    Buffer<float> legacySlope = legacy.getSlopes().withSize(jmax(0, legacyWaveX.size() - 1));
    int legacyWaveCount = legacyWaveX.size();
    REQUIRE(legacyWaveCount > 1);
    REQUIRE(scanInvalidSamples("legacyVoiceWaveX", legacyWaveX).invalidCount == 0);
    REQUIRE(scanInvalidSamples("legacyVoiceWaveY", legacyWaveY).invalidCount == 0);
    REQUIRE(scanInvalidSamples("legacyVoiceSlope", legacySlope).invalidCount == 0);

    int start = 0;
    int end = 0;
    REQUIRE(findFiniteRange(legacyWaveX, start, end));

    constexpr int numSamples = 128;
    ScopedAlloc<float> outputMemory(numSamples);
    Buffer<float> output = outputMemory.withSize(numSamples);
    double phaseStart = legacyWaveX[start];
    double phaseEnd = jmin(1.0, static_cast<double>(legacyWaveX[end]));
    double delta = (phaseEnd - phaseStart) / static_cast<double>(numSamples - 1);
    int index = jlimit(0, legacyWaveCount - 2, legacy.getZeroIndex());

    for (int i = 0; i < numSamples; ++i) {
        double phase = phaseStart + static_cast<double>(i) * delta;
        output[i] = legacy.sampleAt(phase, index);
    }

    InvalidSampleSummary summary = scanInvalidSamples("legacyVoice", output);
    REQUIRE(summary.invalidCount == 0);
}

TEST_CASE("V2VoiceRasterizer legacy oracle primed chained render stays close to VoiceMeshRasterizer", "[curve][v2][voice][parity][legacy]") {
    ScopedMesh scoped("v2-voice-legacy-render-parity");
    populateVoiceChainingMesh(scoped.mesh);
    wrapMeshPhasesToUnitRange(scoped.mesh);

    MorphPosition morph(0.47f, 0.63f, 0.39f);

    V2VoiceRasterizer v2;
    prepareVoiceRasterizer(v2, scoped.mesh);

    V2VoiceControlSnapshot controls;
    controls.morph = morph;
    controls.scaling = V2ScalingType::Bipolar;
    controls.cyclic = true;
    controls.minX = 0.0f;
    controls.maxX = 1.0f;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.integralSampling = false;
    v2.updateControlData(controls);

    V2RasterArtifacts primedArtifacts;
    REQUIRE(v2.renderIntercepts(primedArtifacts));

    constexpr int numSamples = 128;

    ScopedAlloc<float> memory(3 * numSamples);
    Buffer<float> v2Output = memory.place(numSamples);
    Buffer<float> legacyOutput = memory.place(numSamples);
    Buffer<float> diff = memory.place(numSamples);

    V2RasterArtifacts artifacts;
    REQUIRE(v2.renderArtifacts(artifacts));
    REQUIRE(artifacts.waveBuffers.waveX.size() > 1);

    CycleState state;
    VoiceMeshRasterizer legacy(nullptr);
    prepareLegacyVoiceRasterizer(legacy, state, scoped.mesh, morph);
    REQUIRE(primeLegacyVoiceRasterizer(legacy, 0.0f));

    Buffer<float> legacyWaveX = legacy.getWaveX();
    int legacyWaveCount = legacyWaveX.size();
    int v2WaveCount = artifacts.waveBuffers.waveX.size();
    REQUIRE(legacyWaveCount > 1);

    int legacyStart = 0;
    int legacyEnd = 0;
    int v2Start = 0;
    int v2End = 0;
    REQUIRE(findFiniteRange(legacyWaveX, legacyStart, legacyEnd));
    REQUIRE(findFiniteRange(artifacts.waveBuffers.waveX, v2Start, v2End));

    float phaseStart = jmax(0.0f, jmax(legacyWaveX[legacyStart], artifacts.waveBuffers.waveX[v2Start]));
    float phaseEnd = jmin(1.0f, jmin(legacyWaveX[legacyEnd], artifacts.waveBuffers.waveX[v2End]));
    REQUIRE(phaseEnd > phaseStart);

    double commonDelta = (phaseEnd - phaseStart) / static_cast<double>(numSamples - 1);
    int legacyIndex = jlimit(0, legacyWaveCount - 2, legacy.getZeroIndex());
    int v2Index = jlimit(0, v2WaveCount - 2, artifacts.zeroIndex);

    for (int i = 0; i < numSamples; ++i) {
        double phase = phaseStart + static_cast<double>(i) * commonDelta;
        legacyOutput[i] = legacy.sampleAt(phase, legacyIndex);
        v2Output[i] = V2WaveSampling::sampleAtPhase(phase, artifacts.waveBuffers, v2WaveCount, v2Index);
        REQUIRE(std::isfinite(v2Output[i]));
    }

    sanitizeInvalidSamples("legacyVoice", legacyOutput);
    REQUIRE(sanitizeInvalidSamples("v2Voice", v2Output) == 0);

    VecOps::sub(legacyOutput, v2Output, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maximum = diff.max();

    INFO("voice render parity l2=" << l2 << " maxAbs=" << maximum);
    REQUIRE(l2 <= 2.0f);
    REQUIRE(maximum <= 0.25f);
}
