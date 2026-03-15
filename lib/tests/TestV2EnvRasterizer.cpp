#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "../src/Array/ScopedAlloc.h"
#include "../src/Array/VecOps.h"
#include "../src/Curve/EnvelopeMesh.h"
#include "../src/Curve/EnvRasterizer.h"
#include "../src/Curve/V2/Rasterizers/V2EnvRasterizer.h"
#include "../src/Curve/V2/Stages/V2StageInterfaces.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"
#include "TestDefs.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

struct ScopedEnvelopeMesh {
    explicit ScopedEnvelopeMesh(const String& name) : mesh(name) {}
    ~ScopedEnvelopeMesh() { mesh.destroy(); }
    EnvelopeMesh mesh;
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

void prepareLegacyEnvForRuntime(EnvRasterizer& legacy, EnvelopeMesh& mesh) {
    legacy.ensureParamSize(1);
    legacy.setMesh(&mesh);
    legacy.setMorphPosition(MorphPosition(0.2f, 0.4f, 0.8f));
    legacy.setScalingMode(MeshRasterizer::Unipolar);
    legacy.setInterpolatesCurves(true);
    legacy.setLowresCurves(false);
    legacy.setWrapsEnds(false);
    legacy.setLimits(0.0f, 10.0f);
    legacy.calcCrossPoints();
    legacy.makeCopy();
    legacy.setNoteOn();
}

float maxAbs(Buffer<float> buffer) {
    if (buffer.empty()) {
        return 0.0f;
    }

    float minValue = 0.0f;
    float maxValue = 0.0f;
    buffer.minmax(minValue, maxValue);
    return jmax(maxValue, -minValue);
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

void requireFiniteBuffer(const char* label, Buffer<float> buffer) {
    REQUIRE(sanitizeInvalidSamples(label, buffer) == 0);
}

void requireCloseToLegacy(
    const char* label,
    Buffer<float> diff,
    float l2Tolerance,
    float maxTolerance) {
    float l2 = diff.normL2();
    float maximum = maxAbs(diff);

    INFO(label << " l2=" << l2 << " maxAbs=" << maximum);
    REQUIRE(l2 <= l2Tolerance);
    REQUIRE(maximum <= maxTolerance);
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

TEST_CASE("V2EnvRasterizer note on/off and release trigger semantics", "[curve][v2][env][rasterizer][contract]") {
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

TEST_CASE("V2EnvRasterizer renderBlock requires prepare and mesh", "[curve][v2][env][rasterizer][contract]") {
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
    REQUIRE_FALSE(rasterizer.renderBlock(request, output, result));
    REQUIRE_FALSE(result.rendered);
    REQUIRE(result.samplesWritten == 0);
}

TEST_CASE("V2EnvRasterizer renders deterministic output for fixed inputs", "[curve][v2][env][rasterizer][contract]") {
    ScopedMesh scoped("v2-env-rasterizer");
    populateEnvTestMesh(scoped.mesh);

    V2EnvRasterizer rasterizer;

    V2PrepareSpec prepare;
    rasterizer.prepare(prepare);
    rasterizer.setMeshSnapshot(&scoped.mesh);

    V2EnvControlSnapshot controls;
    controls.morph = MorphPosition(0.2f, 0.4f, 0.8f);
    controls.scaling = V2ScalingType::Unipolar;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.hasReleaseCurve = true;
    rasterizer.updateControlData(controls);
    rasterizer.noteOn();

    V2RenderRequest request;
    request.numSamples = 128;
    request.deltaX = 1.0 / 128.0;
    request.tempoScale = 1.0f;
    request.scale = 1;

    ScopedAlloc<float> memory(384);
    Buffer<float> first = memory.place(128);
    Buffer<float> second = memory.place(128);
    Buffer<float> diff = memory.place(128);
    first.zero();
    second.zero();

    V2RenderResult firstResult;
    V2RenderResult secondResult;
    REQUIRE(rasterizer.renderBlock(request, first, firstResult));

    rasterizer.noteOn();
    REQUIRE(rasterizer.renderBlock(request, second, secondResult));
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

TEST_CASE("V2EnvRasterizer loops until release then follows release region", "[curve][v2][env][rasterizer][loop][release][contract]") {
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
    controls.scaling = V2ScalingType::Unipolar;
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

    ScopedAlloc<float> memory(250);

    Buffer<float> loopA = memory.place(50);
    Buffer<float> loopB = memory.place(50);
    Buffer<float> release = memory.place(50);
    Buffer<float> diffLoop = memory.place(50);
    Buffer<float> diffRelease = memory.place(50);

    V2RenderResult loopResultA;
    V2RenderResult loopResultB;

    REQUIRE(rasterizer.renderBlock(request, loopA, loopResultA));
    double loopPosAfterA = rasterizer.getSamplePositionForTesting();

    REQUIRE(rasterizer.renderBlock(request, loopB, loopResultB));
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
    REQUIRE(rasterizer.renderBlock(request, release, releaseResult));

    double releasePosAfter = rasterizer.getSamplePositionForTesting();
    REQUIRE(releaseResult.samplesWritten == release.size());
    REQUIRE_FALSE(rasterizer.isReleasePending());
    REQUIRE(releasePosAfter > controls.releaseStartX);
    REQUIRE(releasePosAfter > controls.loopEndX);

    VecOps::sub(loopA, release, diffRelease);
    float releaseL2 = diffRelease.normL2();
    REQUIRE(releaseL2 >= 0.0f);
}

TEST_CASE("V2EnvRasterizer rendered ADSR-like envelope loops at sustain and releases after noteOff", "[curve][v2][env][rendered][adsr][contract]") {
    DEBUG_CLEAR();

    std::vector<Intercept> intercepts = {
        Intercept(0.00f, 0.00f), // attack start
        Intercept(0.12f, 1.00f), // attack peak
        Intercept(0.30f, 0.60f), // decay to sustain
        Intercept(0.50f, 0.60f), // sustain region end
        Intercept(0.90f, 0.05f)  // release tail
    };
    FixedEnvInterpolatorStage fixedInterpolator(intercepts);

    V2EnvRasterizer rasterizer;
    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 32;
    prepare.capacities.maxCurves = 64;
    prepare.capacities.maxWavePoints = 512;
    prepare.capacities.maxDeformRegions = 0;
    rasterizer.prepare(prepare);
    rasterizer.setInterpolatorForTesting(&fixedInterpolator);

    V2EnvControlSnapshot controls;
    controls.scaling = V2ScalingType::Unipolar;
    controls.minX = 0.0f;
    controls.maxX = 1.0f;
    controls.hasReleaseCurve = true;
    controls.hasLoopRegion = true;
    controls.loopStartX = 0.30f;
    controls.loopEndX = 0.50f;
    controls.releaseStartX = 0.50f;
    rasterizer.updateControlData(controls);
    rasterizer.noteOn();

    V2RenderRequest attackRequest;
    attackRequest.numSamples = 30;
    attackRequest.deltaX = 0.01;
    attackRequest.tempoScale = 1.0f;
    attackRequest.scale = 1;

    ScopedAlloc<float> memory(2048);

    V2RasterArtifacts attackArtifacts;
    REQUIRE(rasterizer.renderArtifacts(attackArtifacts));
    REQUIRE(attackArtifacts.intercepts != nullptr);
    REQUIRE(attackArtifacts.waveBuffers.waveX.size() > 1);

    int icptCount = static_cast<int>(attackArtifacts.intercepts->size());
    int waveCount = attackArtifacts.waveBuffers.waveX.size();
    Buffer<float> icptX = memory.place(icptCount);
    Buffer<float> icptY = memory.place(icptCount);
    Buffer<float> waveX = memory.place(waveCount);
    Buffer<float> waveY = memory.place(waveCount);

    for (int i = 0; i < icptCount; ++i) {
        icptX[i] = (*attackArtifacts.intercepts)[i].x;
        icptY[i] = (*attackArtifacts.intercepts)[i].y;
    }
    attackArtifacts.waveBuffers.waveX.copyTo(waveX);
    attackArtifacts.waveBuffers.waveY.copyTo(waveY);
    DEBUG_VIEW(icptX, "v2_env_adsr_icpt_x");
    DEBUG_VIEW(icptY, "v2_env_adsr_icpt_y");
    DEBUG_VIEW(waveX, "v2_env_adsr_wave_x");
    DEBUG_VIEW(waveY, "v2_env_adsr_wave_y");

    Buffer<float> attack = memory.place(30);
    V2RenderResult attackResult;
    REQUIRE(rasterizer.renderBlock(attackRequest, attack, attackResult));
    REQUIRE(attackResult.samplesWritten == attack.size());
    DEBUG_VIEW(attack, "v2_env_adsr_attack");
    REQUIRE(attack[10] > attack[0]);   // attack rise
    REQUIRE(attack[29] < attack[10]);  // decay toward sustain

    REQUIRE(rasterizer.transitionToLooping(true, true));
    REQUIRE(rasterizer.getMode() == V2EnvMode::Looping);

    V2RenderRequest loopRequest;
    loopRequest.numSamples = 20;
    loopRequest.deltaX = 0.01;
    loopRequest.tempoScale = 1.0f;
    loopRequest.scale = 1;

    Buffer<float> loopA = memory.place(20);
    Buffer<float> loopB = memory.place(20);
    Buffer<float> loopDiff = memory.place(20);

    V2RenderResult loopResultA;
    V2RenderResult loopResultB;
    REQUIRE(rasterizer.renderBlock(loopRequest, loopA, loopResultA));
    REQUIRE(rasterizer.renderBlock(loopRequest, loopB, loopResultB));
    DEBUG_VIEW_OVERLAY(loopA, loopB, "v2_env_adsr_loopA_vs_loopB");

    float internalMaxStep = 0.0f;
    for (int i = 1; i < loopA.size(); ++i) {
        float step = loopA[i] - loopA[i - 1];
        if (step < 0.0f) {
            step = -step;
        }
        if (step > internalMaxStep) {
            internalMaxStep = step;
        }
    }

    float boundaryStep = loopB[0] - loopA[loopA.size() - 1];
    if (boundaryStep < 0.0f) {
        boundaryStep = -boundaryStep;
    }

    float continuityScale = jmax(internalMaxStep, 1e-3f);
    REQUIRE(boundaryStep <= continuityScale * 1.25f + 1e-4f);

    REQUIRE(rasterizer.noteOff());
    REQUIRE(rasterizer.getMode() == V2EnvMode::Releasing);

    Buffer<float> release = memory.place(20);
    Buffer<float> releaseDiff = memory.place(20);
    V2RenderResult releaseResult;
    REQUIRE(rasterizer.renderBlock(loopRequest, release, releaseResult));
    REQUIRE(releaseResult.samplesWritten == release.size());
    DEBUG_VIEW_OVERLAY(loopA, release, "v2_env_adsr_loopA_vs_release");

    VecOps::sub(loopA, loopB, loopDiff);
    VecOps::sub(loopA, release, releaseDiff);
    DEBUG_VIEW(loopDiff, "v2_env_adsr_loop_diff");
    DEBUG_VIEW(releaseDiff, "v2_env_adsr_release_diff");
    REQUIRE(releaseDiff.normL2() > 0.0f);  // release diverges from sustain loop
    REQUIRE(release[release.size() - 1] <= release[0]); // release moves downward
}

TEST_CASE("Legacy EnvRasterizer attack loop and release outputs remain finite", "[curve][legacy][env][contract]") {
    ScopedEnvelopeMesh scoped("legacy-env-contract");
    populateEnvTestMesh(scoped.mesh);
    REQUIRE(scoped.mesh.getNumCubes() >= 2);

    scoped.mesh.loopCubes.insert(scoped.mesh.getCubes()[0]);
    scoped.mesh.sustainCubes.insert(scoped.mesh.getCubes()[1]);

    EnvRasterizer legacy(nullptr, nullptr, "legacy-env-contract");
    prepareLegacyEnvForRuntime(legacy, scoped.mesh);
    CurveInvalidSummary curveSummary = scanCurveTransforms("legacyEnvCurves", legacy.getCurves());
    INFO("legacyEnvCurves invalidCurves=" << curveSummary.invalidCurveCount
         << " firstInvalidCurve=" << curveSummary.firstInvalidCurve
         << " a=(" << curveSummary.ax << "," << curveSummary.ay << ")"
         << " b=(" << curveSummary.bx << "," << curveSummary.by << ")"
         << " c=(" << curveSummary.cx << "," << curveSummary.cy << ")"
         << " resIndex=" << curveSummary.resIndex);
    REQUIRE(curveSummary.invalidCurveCount == 0);

    Buffer<float> legacyWaveX = legacy.getWaveX();
    Buffer<float> legacyWaveY = legacy.getWaveY();
    Buffer<float> legacySlope = legacy.getSlopes().withSize(jmax(0, legacyWaveX.size() - 1));
    REQUIRE(scanInvalidSamples("legacyEnvWaveX", legacyWaveX).invalidCount == 0);
    REQUIRE(scanInvalidSamples("legacyEnvWaveY", legacyWaveY).invalidCount == 0);
    REQUIRE(scanInvalidSamples("legacyEnvSlope", legacySlope).invalidCount == 0);

    MeshLibrary::EnvProps props{};
    props.active = true;
    props.dynamic = false;
    props.tempoSync = false;
    props.global = false;
    props.logarithmic = false;
    props.scale = 1;
    props.tempoScale = 1.0f;

    constexpr int numSamples = 96;
    constexpr double deltaX = 0.01;

    ScopedAlloc<float> memory(3 * numSamples);
    Buffer<float> attack = memory.place(numSamples);
    Buffer<float> loop = memory.place(numSamples);
    Buffer<float> release = memory.place(numSamples);

    REQUIRE(legacy.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, props, 1.0f));
    legacy.getRenderBuffer().withSize(numSamples).copyTo(attack);

    REQUIRE(legacy.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, props, 1.0f));
    legacy.getRenderBuffer().withSize(numSamples).copyTo(loop);

    legacy.setNoteOff();
    REQUIRE(legacy.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, props, 1.0f));
    legacy.getRenderBuffer().withSize(numSamples).copyTo(release);

    InvalidSampleSummary attackSummary = scanInvalidSamples("legacyAttack", attack);
    InvalidSampleSummary loopSummary = scanInvalidSamples("legacyLoop", loop);
    InvalidSampleSummary releaseSummary = scanInvalidSamples("legacyRelease", release);

    REQUIRE(attackSummary.invalidCount == 0);
    REQUIRE(loopSummary.invalidCount == 0);
    REQUIRE(releaseSummary.invalidCount == 0);
}

TEST_CASE("V2EnvRasterizer legacy oracle rendered output stays close to EnvRasterizer", "[curve][v2][env][parity][legacy]") {
    DEBUG_CLEAR();

    ScopedEnvelopeMesh scoped("env-legacy-v2-parity");
    populateEnvTestMesh(scoped.mesh);
    INFO("mesh cubes: " << scoped.mesh.getNumCubes());
    REQUIRE(scoped.mesh.getNumCubes() >= 2);

    scoped.mesh.loopCubes.insert(scoped.mesh.getCubes()[0]);
    scoped.mesh.sustainCubes.insert(scoped.mesh.getCubes()[1]);

    EnvRasterizer legacy(nullptr, nullptr, "legacy-env-parity");
    prepareLegacyEnvForRuntime(legacy, scoped.mesh);

    const std::vector<Intercept>& legacyIcpts = legacy.getRastData().intercepts;
    REQUIRE(legacyIcpts.size() >= 2);

    int loopIndex = -1;
    int sustainIndex = -1;
    legacy.getIndices(loopIndex, sustainIndex);
    REQUIRE(sustainIndex >= 0);
    REQUIRE(sustainIndex < static_cast<int>(legacyIcpts.size()));

    bool hasLoopRegion = loopIndex >= 0 && sustainIndex > loopIndex;
    bool hasReleaseCurve = sustainIndex < static_cast<int>(legacyIcpts.size()) - 1;
    int releaseIndex = hasReleaseCurve
        ? jmin(static_cast<int>(legacyIcpts.size()) - 1, sustainIndex + 1)
        : sustainIndex;

    V2EnvRasterizer v2;
    V2PrepareSpec prepare;
    prepare.capacities.maxIntercepts = 64;
    prepare.capacities.maxCurves = 128;
    prepare.capacities.maxWavePoints = 2048;
    prepare.capacities.maxDeformRegions = 0;
    v2.prepare(prepare);
    v2.setMeshSnapshot(&scoped.mesh);

    V2EnvControlSnapshot controls;
    controls.morph = MorphPosition(0.2f, 0.4f, 0.8f);
    controls.scaling = V2ScalingType::Unipolar;
    controls.interpolateCurves = true;
    controls.lowResolution = false;
    controls.hasReleaseCurve = hasReleaseCurve;
    controls.hasLoopRegion = hasLoopRegion;
    controls.loopStartX = hasLoopRegion ? legacyIcpts[loopIndex].x : 0.0f;
    controls.loopEndX = hasLoopRegion ? legacyIcpts[sustainIndex].x : 1.0f;
    controls.releaseStartX = legacyIcpts[releaseIndex].x;
    v2.updateControlData(controls);

    MeshLibrary::EnvProps props{};
    props.active = true;
    props.dynamic = false;
    props.tempoSync = false;
    props.global = false;
    props.logarithmic = false;
    props.scale = 1;
    props.tempoScale = 1.0f;

    constexpr int numSamples = 96;
    constexpr double deltaX = 0.01;

    ScopedAlloc<float> memory(9 * numSamples);
    Buffer<float> legacyAttack = memory.place(numSamples);
    Buffer<float> legacyLoop = memory.place(numSamples);
    Buffer<float> legacyRelease = memory.place(numSamples);
    Buffer<float> v2Attack = memory.place(numSamples);
    Buffer<float> v2Loop = memory.place(numSamples);
    Buffer<float> v2Release = memory.place(numSamples);
    Buffer<float> attackDiff = memory.place(numSamples);
    Buffer<float> loopDiff = memory.place(numSamples);
    Buffer<float> releaseDiff = memory.place(numSamples);
    v2.noteOn();

    REQUIRE(legacy.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, props, 1.0f));
    legacy.getRenderBuffer().withSize(numSamples).copyTo(legacyAttack);

    V2RenderRequest request;
    request.numSamples = numSamples;
    request.deltaX = deltaX;
    request.tempoScale = 1.0f;
    request.scale = 1;

    V2RenderResult v2AttackResult;
    REQUIRE(v2.renderBlock(request, v2Attack, v2AttackResult));
    REQUIRE(v2AttackResult.samplesWritten == numSamples);

    if (hasLoopRegion) {
        REQUIRE(v2.transitionToLooping(true, true));
    }

    REQUIRE(legacy.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, props, 1.0f));
    legacy.getRenderBuffer().withSize(numSamples).copyTo(legacyLoop);

    V2RenderResult v2LoopResult;
    REQUIRE(v2.renderBlock(request, v2Loop, v2LoopResult));
    REQUIRE(v2LoopResult.samplesWritten == numSamples);

    legacy.setNoteOff();
    if (hasReleaseCurve) {
        REQUIRE(v2.noteOff());
    } else {
        REQUIRE_FALSE(v2.noteOff());
    }

    REQUIRE(legacy.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, props, 1.0f));
    legacy.getRenderBuffer().withSize(numSamples).copyTo(legacyRelease);

    V2RenderResult v2ReleaseResult;
    REQUIRE(v2.renderBlock(request, v2Release, v2ReleaseResult));
    REQUIRE(v2ReleaseResult.samplesWritten > 0);

    sanitizeInvalidSamples("legacyAttack", legacyAttack);
    sanitizeInvalidSamples("legacyLoop", legacyLoop);
    sanitizeInvalidSamples("legacyRelease", legacyRelease);
    requireFiniteBuffer("v2Attack", v2Attack);
    requireFiniteBuffer("v2Loop", v2Loop);
    requireFiniteBuffer("v2Release", v2Release);

    VecOps::sub(legacyAttack, v2Attack, attackDiff);
    VecOps::sub(legacyLoop, v2Loop, loopDiff);
    VecOps::sub(legacyRelease, v2Release, releaseDiff);

    requireCloseToLegacy("attack", attackDiff, 4.00f, 0.45f);
    requireCloseToLegacy("loop", loopDiff, 4.00f, 0.45f);
    requireCloseToLegacy("release", releaseDiff, 4.00f, 0.45f);

    DEBUG_VIEW_OVERLAY(legacyAttack, v2Attack, "env_parity_attack_overlay");
    DEBUG_VIEW(attackDiff, "env_parity_attack_diff");
    DEBUG_VIEW_OVERLAY(legacyLoop, v2Loop, "env_parity_loop_overlay");
    DEBUG_VIEW(loopDiff, "env_parity_loop_diff");
    DEBUG_VIEW_OVERLAY(legacyRelease, v2Release, "env_parity_release_overlay");
    DEBUG_VIEW(releaseDiff, "env_parity_release_diff");
}
