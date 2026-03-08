#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "../src/Curve/V2/Orchestration/V2RasterizerPipeline.h"
#include "../src/Curve/V2/Sampling/V2WaveSampling.h"
#include "../src/Curve/V2/Stages/V2CurveBuilderStages.h"
#include "../src/Curve/V2/Stages/V2InterpolatorStages.h"
#include "../src/Curve/V2/Stages/V2PositionerStages.h"
#include "../src/Curve/V2/Stages/V2WaveBuilderStages.h"
#include "../src/Curve/V2/State/V2EnvStateMachine.h"
#include "../src/Array/VecOps.h"
#include "../src/Curve/IDeformer.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
struct ScopedMesh {
    explicit ScopedMesh(const String& name) : mesh(name) {}
    ~ScopedMesh() { mesh.destroy(); }
    Mesh mesh;
};

void appendUnitCubeMesh(Mesh& mesh, float phaseOffset) {
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
            + (redPole ? 0.3f : 0.0f)
            + (bluePole ? 0.05f : 0.0f);
        vert->values[Vertex::Amp] = 0.2f
            + (timePole ? 0.1f : 0.0f)
            + (redPole ? 0.1f : 0.0f)
            + (bluePole ? 0.2f : 0.0f);
        vert->values[Vertex::Curve] = 0.4f;
    }
}

void populateUnitCubeMesh(Mesh& mesh) {
    appendUnitCubeMesh(mesh, 0.0f);
}

class StubInterpolatorStage :
        public V2InterpolatorStage {
public:
    bool run(
        const V2InterpolatorContext&,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override {
        outIntercepts.clear();
        outIntercepts.emplace_back(0.75f, 0.8f);
        outIntercepts.emplace_back(-0.25f, 0.2f);
        outCount = static_cast<int>(outIntercepts.size());
        return true;
    }
};

class FixedInterpolatorStage :
        public V2InterpolatorStage {
public:
    explicit FixedInterpolatorStage(std::vector<Intercept> fixed) :
            fixedIntercepts(std::move(fixed))
    {}

    bool run(
        const V2InterpolatorContext&,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override {
        outIntercepts = fixedIntercepts;
        outCount = static_cast<int>(outIntercepts.size());
        return outCount > 0;
    }

private:
    std::vector<Intercept> fixedIntercepts;
};

class OverflowInterpolatorStage :
        public V2InterpolatorStage {
public:
    explicit OverflowInterpolatorStage(int count) :
            count(count)
    {}

    bool run(
        const V2InterpolatorContext&,
        std::vector<Intercept>& outIntercepts,
        int& outCount) noexcept override {
        outIntercepts.clear();
        for (int i = 0; i < count; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(jmax(1, count - 1));
            outIntercepts.emplace_back(x, x);
        }
        outCount = static_cast<int>(outIntercepts.size());
        return outCount > 0;
    }

private:
    int count;
};

class ConstantDeformer :
        public IDeformer {
public:
    explicit ConstantDeformer(float value) :
            value(value)
    {}

    float getTableValue(int, float, const NoiseContext&) override {
        return value;
    }

    void sampleDownAddNoise(int, Buffer<float> dest, const NoiseContext&) override {
        dest.add(value);
    }

    Buffer<Float32> getTable(int) override {
        return Buffer<Float32>();
    }

    int getTableDensity(int) override {
        return 0;
    }

private:
    float value{0.0f};
};

class TestRasterizer :
        public V2RasterizerPipeline {
public:
    TestRasterizer(
        V2InterpolatorStage* interp,
        V2PositionerStage* pos,
        V2CurveBuilderStage* curve = nullptr,
        V2WaveBuilderStage* wave = nullptr) {
        setInterpolator(interp);
        setPositioner(pos);
        if (curve != nullptr) { setCurveBuilder(curve); }
        if (wave != nullptr) { setWaveBuilder(wave); }
    }

    using V2RasterizerPipeline::workspace;
    using V2RasterizerPipeline::runInterceptStages;
    using V2RasterizerPipeline::buildInterceptArtifacts;
    using V2RasterizerPipeline::buildCurveAndWaveArtifacts;
    using V2RasterizerPipeline::buildWaveArtifactsFromCurves;
    using V2RasterizerPipeline::buildAllArtifacts;

    bool renderIntercepts(V2RasterArtifacts&) noexcept override { return false; }
    bool renderWaveform(V2RasterArtifacts&) noexcept override { return false; }

protected:
    bool sampleArtifacts(
        const V2RasterArtifacts&,
        const V2RenderRequest&,
        Buffer<float>,
        V2RenderResult&) noexcept override { return false; }
};

V2RenderResult renderFromFixedIntercepts(
    const std::vector<Intercept>& intercepts,
    Buffer<float> output,
    bool interpolateCurves = true,
    bool cyclic = false) {
    V2CapacitySpec capacities;
    capacities.maxIntercepts = 64;
    capacities.maxCurves = 64;
    capacities.maxWavePoints = 1024;
    capacities.maxDeformRegions = 0;

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;

    V2PositionerStage* positionerStage = cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                                : static_cast<V2PositionerStage*>(&positioner);
    TestRasterizer rasterizer(&interpolator, positionerStage, &curveBuilder, &waveBuilder);
    rasterizer.workspace.prepare(capacities);

    V2InterpolatorContext interpolatorContext;

    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.cyclic = cyclic;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.interpolateCurves = interpolateCurves;

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = interpolateCurves;

    V2RasterArtifacts artifacts;
    if (! rasterizer.buildAllArtifacts(interpolatorContext, positionerContext, curveBuilderContext, waveBuilderContext, artifacts)) {
        return V2RenderResult{};
    }

    int wavePointCount = artifacts.waveBuffers.waveX.size();
    LinearPhasePolicy policy(0.0, 1.0 / output.size());
    int sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
    return V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, output, sampleIndex);
}

void makeReferenceSamples(
        const V2WaveBuffers& waveBuffers,
        int wavePointCount,
        int zeroIndex,
        Buffer<float> output,
        double deltaX) {
    V2WaveBuffers sampledWaveBuffers;
    waveBuffers.assignSized(sampledWaveBuffers, wavePointCount);

    Buffer<float> waveX = sampledWaveBuffers.waveX;
    Buffer<float> waveY = sampledWaveBuffers.waveY;
    Buffer<float> slope = sampledWaveBuffers.slope;

    if (waveX.size() < 2 || waveY.size() < 2 || slope.size() < 1 || output.empty() || deltaX <= 0.0) {
        output.zero();
        return;
    }

    int segCount = slope.size();
    int segment = jlimit(0, segCount - 1, zeroIndex);
    double phase = 0.0;

    for (int i = 0; i < output.size(); ++i) {
        while (segment < segCount - 1 && phase >= waveX[segment + 1]) {
            ++segment;
        }

        double clamped = phase;
        if (phase < waveX[0]) {
            clamped = waveX[0];
            segment = 0;
        } else if (phase > waveX[segCount]) {
            clamped = waveX[segCount];
            segment = segCount - 1;
        }

        output[i] = static_cast<float>((clamped - waveX[segment]) * slope[segment] + waveY[segment]);
        if (output[i] != output[i]) {
            output[i] = 0.0f;
        }
        phase += deltaX;
    }
}

bool renderAndBuildReferenceFromFixedIntercepts(
    const std::vector<Intercept>& intercepts,
    Buffer<float> output,
    Buffer<float> expected,
    bool interpolateCurves = true,
    bool cyclic = false) {
    V2CapacitySpec capacities;
    capacities.maxIntercepts = 64;
    capacities.maxCurves = 64;
    capacities.maxWavePoints = 1024;
    capacities.maxDeformRegions = 0;

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;

    V2PositionerStage* positionerStage = cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                                : static_cast<V2PositionerStage*>(&positioner);
    TestRasterizer rasterizer(&interpolator, positionerStage, &curveBuilder, &waveBuilder);
    rasterizer.workspace.prepare(capacities);

    V2InterpolatorContext interpolatorContext;

    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.cyclic = cyclic;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.interpolateCurves = interpolateCurves;

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = interpolateCurves;

    double deltaX = 1.0 / output.size();

    output.zero();
    expected.zero();

    V2RasterArtifacts artifacts;
    if (! rasterizer.buildAllArtifacts(interpolatorContext, positionerContext, curveBuilderContext, waveBuilderContext, artifacts)) {
        return false;
    }

    int wavePointCount = artifacts.waveBuffers.waveX.size();
    LinearPhasePolicy policy(0.0, deltaX);
    int sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
    V2RenderResult result = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, output, sampleIndex);

    if (! result.rendered || result.samplesWritten != output.size()) {
        return false;
    }

    int interceptCount = 0;
    if (! rasterizer.runInterceptStages(interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    int curveCount = 0;
    if (! curveBuilder.run(
            rasterizer.workspace.intercepts,
            interceptCount,
            rasterizer.workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    int refWavePointCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    if (! waveBuilder.run(
            rasterizer.workspace.curves,
            curveCount,
            rasterizer.workspace.waveBuffers,
            refWavePointCount,
            zeroIndex,
            oneIndex,
            waveBuilderContext)) {
        return false;
    }

    makeReferenceSamples(
        rasterizer.workspace.waveBuffers,
        refWavePointCount,
        zeroIndex,
        expected,
        deltaX);
    return true;
}
}


TEST_CASE("V2 interpolator stages extract intercepts", "[curve][v2][raster]") {
    ScopedMesh scoped("v2-test-mesh");
    Mesh& mesh = scoped.mesh;
    populateUnitCubeMesh(mesh);

    V2InterpolatorContext context;
    context.mesh = &mesh;
    context.morph = MorphPosition(0.5f, 0.25f, 0.75f);
    context.wrapPhases = false;

    std::vector<Intercept> intercepts;
    int count = 0;

    V2TrilinearInterpolatorStage trilinear;
    V2BilinearInterpolatorStage bilinear;
    V2SimpleInterpolatorStage simple;

    REQUIRE(trilinear.run(context, intercepts, count));
    REQUIRE(count == 1);

    REQUIRE(bilinear.run(context, intercepts, count));
    REQUIRE(count == 1);

    REQUIRE(simple.run(context, intercepts, count));
    REQUIRE(count == 1);
}

TEST_CASE("V2 positioner stages normalize order and scaling", "[curve][v2][raster]") {
    std::vector<Intercept> intercepts = {
        Intercept(1.2f, 0.25f),
        Intercept(-0.1f, 0.75f),
        Intercept(0.4f, 0.5f)
    };
    int count = static_cast<int>(intercepts.size());

    V2PositionerContext linearContext;
    linearContext.scaling = V2ScalingType::Bipolar;
    linearContext.minX = 0.0f;
    linearContext.maxX = 1.0f;

    V2LinearPositionerStage linear;
    REQUIRE(linear.run(intercepts, count, linearContext));
    REQUIRE(intercepts[0].x >= 0.0f);
    REQUIRE(intercepts[count - 1].x <= 1.0f);
    REQUIRE(intercepts[0].x < intercepts[1].x);
    REQUIRE(intercepts[1].x < intercepts[2].x);
    REQUIRE(intercepts[0].y >= -1.0f);
    REQUIRE(intercepts[2].y <= 1.0f);

    V2PositionerContext cyclicContext;
    cyclicContext.scaling = V2ScalingType::Unipolar;
    cyclicContext.minX = 0.0f;
    cyclicContext.maxX = 1.0f;

    intercepts = { Intercept(1.2f, 0.2f), Intercept(-0.15f, 0.8f) };
    count = static_cast<int>(intercepts.size());

    V2CyclicPositionerStage cyclic;
    REQUIRE(cyclic.run(intercepts, count, cyclicContext));
    REQUIRE(intercepts[0].x >= 0.0f);
    REQUIRE(intercepts[1].x <= 1.0f);
}

TEST_CASE("V2 composite positioner composes reusable stages", "[curve][v2][raster][positioner]") {
    std::vector<Intercept> baseline = {
        Intercept(1.2f, 0.25f),
        Intercept(-0.1f, 0.75f),
        Intercept(0.4f, 0.5f)
    };
    std::vector<Intercept> composed = baseline;
    int baselineCount = static_cast<int>(baseline.size());
    int composedCount = static_cast<int>(composed.size());

    V2PositionerContext context;
    context.scaling = V2ScalingType::Bipolar;
    context.minX = 0.0f;
    context.maxX = 1.0f;

    V2LinearPositionerStage linear;
    REQUIRE(linear.run(baseline, baselineCount, context));

    V2ClampOrWrapPositionerStage clampStage(false);
    V2ApplyScalingPositionerStage scalingStage;
    V2SortAndOrderPositionerStage orderStage;
    V2CompositePositionerStage composedStage;
    REQUIRE(composedStage.addStage(&clampStage));
    REQUIRE(composedStage.addStage(&scalingStage));
    REQUIRE(composedStage.addStage(&orderStage));
    REQUIRE(composedStage.run(composed, composedCount, context));

    REQUIRE(baselineCount == composedCount);
    for (int i = 0; i < baselineCount; ++i) {
        REQUIRE(std::abs(composed[i].x - baseline[i].x) < 1e-6f);
        REQUIRE(std::abs(composed[i].y - baseline[i].y) < 1e-6f);
    }
}

TEST_CASE("V2 point-path positioner stage can be inserted into composed pipeline", "[curve][v2][raster][positioner][path]") {
    ScopedMesh scoped("v2-point-path-positioner");
    auto* cube = new VertCube(&scoped.mesh);
    scoped.mesh.addCube(cube);

    for (int d = 0; d < Vertex::numElements; ++d) {
        cube->deformerAt(d) = -1;
        cube->dfrmGainAt(d) = 0.0f;
    }

    cube->deformerAt(Vertex::Red) = 0;
    cube->dfrmGainAt(Vertex::Red) = 0.1f;
    cube->deformerAt(Vertex::Amp) = 1;
    cube->dfrmGainAt(Vertex::Amp) = 0.2f;
    cube->deformerAt(Vertex::Phase) = 2;
    cube->dfrmGainAt(Vertex::Phase) = 0.2f;
    cube->deformerAt(Vertex::Curve) = 3;
    cube->dfrmGainAt(Vertex::Curve) = 0.1f;

    std::vector<Intercept> intercepts = { Intercept(0.95f, 0.75f, cube, 0.4f) };
    int count = static_cast<int>(intercepts.size());

    short vertOffsets[4] = { 0, 0, 0, 0 };
    short phaseOffsets[4] = { 0, 0, 0, 0 };
    ConstantDeformer deformer(1.0f);

    V2PositionerContext context;
    context.scaling = V2ScalingType::Bipolar;
    context.cyclic = true;
    context.minX = 0.0f;
    context.maxX = 1.0f;
    context.morph = MorphPosition(0.5f, 0.5f, 0.5f);
    context.pointPath = V2PositionerContext::PointPathContext(
        &deformer,
        123,
        vertOffsets,
        phaseOffsets,
        true);

    V2ClampOrWrapPositionerStage clampStage(true);
    V2ApplyScalingPositionerStage scalingStage;
    V2PointPathPositionerStage pointPathStage;
    V2SortAndOrderPositionerStage orderStage;
    V2CompositePositionerStage composedStage;
    REQUIRE(composedStage.addStage(&clampStage));
    REQUIRE(composedStage.addStage(&scalingStage));
    REQUIRE(composedStage.addStage(&pointPathStage));
    REQUIRE(composedStage.addStage(&orderStage));
    REQUIRE(composedStage.run(intercepts, count, context));

    REQUIRE(count == 1);
    REQUIRE(intercepts[0].x >= context.minX);
    REQUIRE(intercepts[0].x <= context.maxX);
    REQUIRE(intercepts[0].y > 0.5f);
    REQUIRE(intercepts[0].shp > 0.4f);
    REQUIRE(intercepts[0].isWrapped);
}

TEST_CASE("V2 point-path stage supports noOffsetAtEnds suppression and legacy time progress", "[curve][v2][raster][positioner][path][parity]") {
    ScopedMesh scoped("v2-point-path-legacy-progress");
    auto* cube = new VertCube(&scoped.mesh);
    scoped.mesh.addCube(cube);

    for (int i = 0; i < static_cast<int>(VertCube::numVerts); ++i) {
        bool timePole = false;
        bool redPole = false;
        bool bluePole = false;
        VertCube::getPoles(i, timePole, redPole, bluePole);

        auto* vert = cube->lineVerts[i];
        vert->values[Vertex::Time] = timePole ? 1.0f : 0.0f;
        vert->values[Vertex::Red] = redPole ? 1.0f : 0.0f;
        vert->values[Vertex::Blue] = bluePole ? 1.0f : 0.0f;
        vert->values[Vertex::Phase] = 0.35f + (redPole ? 0.10f : -0.05f);
        vert->values[Vertex::Amp] = 0.55f + (bluePole ? 0.15f : -0.10f);
        vert->values[Vertex::Curve] = 0.40f;
    }

    for (int d = 0; d < Vertex::numElements; ++d) {
        cube->deformerAt(d) = -1;
        cube->dfrmGainAt(d) = 0.0f;
    }

    cube->deformerAt(Vertex::Amp) = 0;
    cube->dfrmGainAt(Vertex::Amp) = 0.25f;
    cube->deformerAt(Vertex::Phase) = 0;
    cube->dfrmGainAt(Vertex::Phase) = 0.25f;
    cube->deformerAt(Vertex::Curve) = 0;
    cube->dfrmGainAt(Vertex::Curve) = 0.10f;

    ConstantDeformer deformer(1.0f);
    short seeds[1] = { 0 };

    std::vector<Intercept> legacyProgress = { Intercept(0.45f, 0.60f, cube, 0.30f) };
    std::vector<Intercept> directProgress = legacyProgress;
    legacyProgress[0].adjustedX = legacyProgress[0].x;
    directProgress[0].adjustedX = directProgress[0].x;
    int legacyCount = 1;
    int directCount = 1;

    V2PointPathPositionerStage stage;

    V2PositionerContext legacyContext;
    legacyContext.scaling = V2ScalingType::Bipolar;
    legacyContext.cyclic = true;
    legacyContext.minX = 0.0f;
    legacyContext.maxX = 1.0f;
    legacyContext.interpolationDimension = Vertex::Time;
    legacyContext.morph = MorphPosition(0.37f, 0.42f, 0.58f);
    legacyContext.pointPath = V2PositionerContext::PointPathContext(
        &deformer,
        0,
        seeds,
        seeds,
        true,
        false,
        true);

    V2PositionerContext directContext = legacyContext;
    directContext.pointPath = V2PositionerContext::PointPathContext(
        &deformer,
        0,
        seeds,
        seeds,
        true,
        false,
        false);

    REQUIRE(stage.run(legacyProgress, legacyCount, legacyContext));
    REQUIRE(stage.run(directProgress, directCount, directContext));

    REQUIRE(std::abs(legacyProgress[0].x - directProgress[0].x) < 1e-6f);
    REQUIRE(std::abs(legacyProgress[0].y - directProgress[0].y) < 1e-6f);
    REQUIRE(std::abs(legacyProgress[0].shp - directProgress[0].shp) < 1e-6f);

    std::vector<Intercept> suppressed = { Intercept(0.45f, 0.60f, cube, 0.30f) };
    suppressed[0].adjustedX = suppressed[0].x;
    int suppressedCount = 1;
    V2PositionerContext suppressedContext = legacyContext;
    suppressedContext.morph = MorphPosition(0.0f, 0.42f, 0.58f);
    suppressedContext.pointPath = V2PositionerContext::PointPathContext(
        &deformer,
        0,
        seeds,
        seeds,
        true,
        true,
        true);

    REQUIRE(stage.run(suppressed, suppressedCount, suppressedContext));
    REQUIRE(std::abs(suppressed[0].y - 0.60f) < 1e-6f);
    REQUIRE(std::abs(suppressed[0].shp - 0.30f) < 1e-6f);
    REQUIRE(std::abs(suppressed[0].x - 0.45f) < 1e-6f);
}

TEST_CASE("V2 wave builder supports component deformer and decoupled deform-region output", "[curve][v2][raster][wave][deformer]") {
    ScopedMesh scoped("v2-wave-component-path");
    auto* cube = new VertCube(&scoped.mesh);
    scoped.mesh.addCube(cube);
    cube->getCompDfrm() = 0;
    cube->dfrmGainAt(Vertex::Time) = 0.5f;

    Intercept i0(0.0f, 0.25f, cube, 0.0f);
    Intercept i1(0.5f, 0.75f, cube, 0.8f);
    Intercept i2(1.0f, 0.25f, cube, 0.0f);

    std::vector<Curve> curves;
    curves.emplace_back(i0, i1, i2);
    curves.emplace_back(i1, i2, Intercept(1.25f, 0.25f, cube, 0.0f));

    V2DefaultWaveBuilderStage waveBuilder;
    ConstantDeformer deformer(0.5f);
    short seeds[1] = { 0 };

    V2CapacitySpec caps;
    caps.maxIntercepts = 8;
    caps.maxCurves = 8;
    caps.maxWavePoints = 1024;
    caps.maxDeformRegions = 8;
    V2RasterizerWorkspace workspace;
    workspace.prepare(caps);

    int waveCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    V2WaveBuilderContext directContext(
        true,
        V2WaveBuilderContext::ComponentPathContext(
            &deformer,
            0,
            seeds,
            seeds,
            true,
            false,
            false,
            0.5f,
            0,
            0,
            &workspace.deformRegions));

    REQUIRE(waveBuilder.run(
        curves,
        static_cast<int>(curves.size()),
        workspace.waveBuffers,
        waveCount,
        zeroIndex,
        oneIndex,
        directContext));

    REQUIRE(waveCount > 1);
    REQUIRE(workspace.deformRegions.empty());

    float directSum = workspace.waveBuffers.waveY.withSize(waveCount).sum();

    V2WaveBuilderContext decoupledContext(
        true,
        V2WaveBuilderContext::ComponentPathContext(
            &deformer,
            0,
            seeds,
            seeds,
            true,
            true,
            false,
            0.5f,
            0,
            0,
            &workspace.deformRegions));

    REQUIRE(waveBuilder.run(
        curves,
        static_cast<int>(curves.size()),
        workspace.waveBuffers,
        waveCount,
        zeroIndex,
        oneIndex,
        decoupledContext));

    REQUIRE(! workspace.deformRegions.empty());
    REQUIRE(workspace.deformRegions[0].deformChan == 0);
    REQUIRE(workspace.deformRegions[0].amplitude > 0.0f);
    REQUIRE(workspace.waveBuffers.waveY.withSize(waveCount).sum() < directSum);
}

TEST_CASE("V2 sampler stage applies decoupled deform regions at sample time", "[curve][v2][raster][sampler][decoupled]") {
    ConstantDeformer deformer(0.5f);

    ScopedAlloc<float> memory(16);
    Buffer<float> waveX(memory.place(3), 3);
    Buffer<float> waveY(memory.place(3), 3);
    Buffer<float> slope(memory.place(2), 2);
    Buffer<float> output(memory.place(4), 4);

    waveX[0] = 0.0f;
    waveX[1] = 0.5f;
    waveX[2] = 1.0f;
    waveY.zero();
    slope.zero();
    output.zero();

    std::vector<V2DeformRegion> deformRegions;
    V2DeformRegion region;
    region.deformChan = 0;
    region.amplitude = 1.0f;
    region.start = Intercept(0.0f, 0.0f);
    region.end = Intercept(1.0f, 0.0f);
    deformRegions.emplace_back(region);

    V2DecoupledDeformContext deform;
    deform.path = &deformer;
    deform.deformRegions = &deformRegions;

    V2WaveBuffers waveBuffers(waveX, waveY, Buffer<float>(), slope);

    LinearPhasePolicy policy(0.0, 0.25);
    int sampleIndex = 0;
    V2RenderResult result = V2WaveSampling::sampleBlock(policy, waveBuffers, 3, output, sampleIndex, deform);
    REQUIRE(result.rendered);
    REQUIRE(result.samplesWritten == output.size());
    for (int i = 0; i < output.size(); ++i) {
        REQUIRE(std::abs(output[i] - 0.5f) < 1e-6f);
    }
}

TEST_CASE("V2 rasterizer graph runs interpolation and positioning with workspace", "[curve][v2][raster]") {
    V2CapacitySpec capacities;
    capacities.maxIntercepts = 16;
    capacities.maxCurves = 16;
    capacities.maxWavePoints = 64;
    capacities.maxDeformRegions = 0;

    StubInterpolatorStage interpolator;
    V2LinearPositionerStage positioner;
    TestRasterizer rasterizer(&interpolator, &positioner);
    rasterizer.workspace.prepare(capacities);
    rasterizer.workspace.intercepts.emplace_back(99.0f, 99.0f); // should be cleared by pipeline

    V2InterpolatorContext interpolatorContext;
    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    int interceptCount = 0;
    REQUIRE(rasterizer.runInterceptStages(interpolatorContext, positionerContext, interceptCount));
    REQUIRE(interceptCount == 2);
    REQUIRE(rasterizer.workspace.intercepts[0].x < rasterizer.workspace.intercepts[1].x);
}

TEST_CASE("V2 envelope golden characterization: Normal to Looping to Releasing", "[curve][v2][env][golden]") {
    enum class Action {
        NoteOn,
        LoopTransition,
        NoteOff,
        ConsumeRelease
    };

    struct GoldenStep {
        Action action;
        V2EnvMode expectedMode;
        bool expectedReleasePending;
    };

    const GoldenStep fixture[] = {
        { Action::NoteOn, V2EnvMode::Normal, false },
        { Action::LoopTransition, V2EnvMode::Looping, false },
        { Action::NoteOff, V2EnvMode::Releasing, true },
        { Action::ConsumeRelease, V2EnvMode::Releasing, false }
    };

    V2EnvStateMachine machine;

    for (const auto& step : fixture) {
        switch (step.action) {
            case Action::NoteOn:
                machine.noteOn();
                break;
            case Action::LoopTransition:
                REQUIRE(machine.transitionToLooping(true, true));
                break;
            case Action::NoteOff:
                REQUIRE(machine.noteOff(true));
                break;
            case Action::ConsumeRelease:
                REQUIRE(machine.consumeReleaseTrigger());
                break;
        }

        REQUIRE(machine.getMode() == step.expectedMode);
        REQUIRE(machine.isReleasePending() == step.expectedReleasePending);
    }
}

TEST_CASE("V2 rasterizer graph renders first end-to-end waveform buffer", "[curve][v2][raster][e2e]") {
    ScopedMesh scoped("v2-e2e-mesh");
    Mesh& mesh = scoped.mesh;
    populateUnitCubeMesh(mesh);
    appendUnitCubeMesh(mesh, 0.15f);

    V2CapacitySpec capacities;
    capacities.maxIntercepts = 32;
    capacities.maxCurves = 64;
    capacities.maxWavePoints = 512;
    capacities.maxDeformRegions = 0;

    V2TrilinearInterpolatorStage interpolator;
    V2LinearPositionerStage positioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;

    TestRasterizer rasterizer(&interpolator, &positioner, &curveBuilder, &waveBuilder);
    rasterizer.workspace.prepare(capacities);

    V2InterpolatorContext interpolatorContext;
    interpolatorContext.mesh = &mesh;
    interpolatorContext.morph = MorphPosition(0.3f, 0.2f, 0.7f);
    interpolatorContext.wrapPhases = false;

    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.interpolateCurves = true;

    V2WaveBuilderContext waveBuilderContext;

    ScopedAlloc<float> outMemory(128);
    Buffer<float> output = outMemory.withSize(128);
    output.zero();

    V2RasterArtifacts artifacts;
    REQUIRE(rasterizer.buildAllArtifacts(interpolatorContext, positionerContext, curveBuilderContext, waveBuilderContext, artifacts));

    int wavePointCount = artifacts.waveBuffers.waveX.size();
    LinearPhasePolicy policy(0.0, 1.0 / output.size());
    int sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
    V2RenderResult result = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, output, sampleIndex);

    REQUIRE(result.rendered);
    REQUIRE(result.samplesWritten == output.size());
    REQUIRE(output.sum() > 0.0f);
}

TEST_CASE("V2 rendered output matches sawtooth control points sample-by-sample", "[curve][v2][raster][shape][strict]") {
    std::vector<Intercept> sawIntercepts = {
        Intercept(0.00f, 0.00f, nullptr, 1.0f),
        Intercept(0.33f, 0.33f, nullptr, 1.0f),
        Intercept(0.66f, 0.66f, nullptr, 1.0f),
        Intercept(0.99f, 0.00f, nullptr, 1.0f)
    };

    ScopedAlloc<float> outMemory(256);
    Buffer<float> output = outMemory.withSize(256);
    output.zero();

    ScopedAlloc<float> expectedMemory(output.size());
    ScopedAlloc<float> diffMemory(output.size());
    Buffer<float> expected = expectedMemory.withSize(output.size());
    Buffer<float> diff = diffMemory.withSize(output.size());

    REQUIRE(renderAndBuildReferenceFromFixedIntercepts(sawIntercepts, output, expected, false, false));
    VecOps::sub(output, expected, diff);

    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 < 1e-5f);
    REQUIRE(maxAbs < 1e-6f);
}

TEST_CASE("V2 rendered output matches square-like control points sample-by-sample", "[curve][v2][raster][shape][strict]") {
    std::vector<Intercept> squareIntercepts = {
        Intercept(0.00f, 0.90f, nullptr, 1.0f),
        Intercept(0.49f, 0.90f, nullptr, 1.0f),
        Intercept(0.50f, 0.10f, nullptr, 1.0f),
        Intercept(0.99f, 0.10f, nullptr, 1.0f)
    };

    ScopedAlloc<float> outMemory(256);
    Buffer<float> output = outMemory.withSize(256);
    output.zero();

    ScopedAlloc<float> expectedMemory(output.size());
    ScopedAlloc<float> diffMemory(output.size());
    Buffer<float> expected = expectedMemory.withSize(output.size());
    Buffer<float> diff = diffMemory.withSize(output.size());

    REQUIRE(renderAndBuildReferenceFromFixedIntercepts(squareIntercepts, output, expected, false, false));
    VecOps::sub(output, expected, diff);

    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 < 1e-5f);
    REQUIRE(maxAbs < 1e-6f);
}

TEST_CASE("V2 graph rejects intercept overflow beyond prepared capacity", "[curve][v2][raster][capacity]") {
    V2CapacitySpec capacities;
    capacities.maxIntercepts = 4;
    capacities.maxCurves = 8;
    capacities.maxWavePoints = 64;
    capacities.maxDeformRegions = 0;

    OverflowInterpolatorStage interpolator(5);
    V2LinearPositionerStage positioner;
    TestRasterizer rasterizer(&interpolator, &positioner);
    rasterizer.workspace.prepare(capacities);

    V2InterpolatorContext interpolatorContext;
    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    int interceptCount = 0;
    REQUIRE_FALSE(rasterizer.runInterceptStages(interpolatorContext, positionerContext, interceptCount));
    REQUIRE(interceptCount == 5);
}

TEST_CASE("V2 render path keeps workspace capacities stable across repeated calls", "[curve][v2][raster][capacity]") {
    std::vector<Intercept> intercepts = {
        Intercept(0.00f, 0.10f, nullptr, 1.0f),
        Intercept(0.33f, 0.70f, nullptr, 1.0f),
        Intercept(0.66f, 0.20f, nullptr, 1.0f),
        Intercept(0.99f, 0.80f, nullptr, 1.0f)
    };

    V2CapacitySpec capacities;
    capacities.maxIntercepts = 16;
    capacities.maxCurves = 32;
    capacities.maxWavePoints = 256;
    capacities.maxDeformRegions = 0;

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    TestRasterizer rasterizer(&interpolator, &positioner, &curveBuilder, &waveBuilder);
    rasterizer.workspace.prepare(capacities);

    V2InterpolatorContext interpolatorContext;

    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.interpolateCurves = false;

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = false;

    ScopedAlloc<float> outMemory(128);
    Buffer<float> output = outMemory.withSize(128);

    size_t interceptCapacity = rasterizer.workspace.intercepts.capacity();
    size_t curveCapacity = rasterizer.workspace.curves.capacity();
    size_t deformCapacity = rasterizer.workspace.deformRegions.capacity();

    for (int i = 0; i < 8; ++i) {
        output.zero();
        V2RasterArtifacts artifacts;
        REQUIRE(rasterizer.buildAllArtifacts(interpolatorContext, positionerContext, curveBuilderContext, waveBuilderContext, artifacts));

        int wavePointCount = artifacts.waveBuffers.waveX.size();
        LinearPhasePolicy policy(0.0, 1.0 / output.size());
        int sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
        V2RenderResult result = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, output, sampleIndex);
        REQUIRE(result.rendered);
        REQUIRE(result.samplesWritten == output.size());
    }

    REQUIRE(rasterizer.workspace.intercepts.capacity() == interceptCapacity);
    REQUIRE(rasterizer.workspace.curves.capacity() == curveCapacity);
    REQUIRE(rasterizer.workspace.deformRegions.capacity() == deformCapacity);
}

TEST_CASE("V2 graph render matches independent reference sampling of built wave", "[curve][v2][raster][oracle]") {
    std::vector<Intercept> intercepts = {
        Intercept(0.00f, 0.00f, nullptr, 1.0f),
        Intercept(0.25f, 0.75f, nullptr, 1.0f),
        Intercept(0.50f, 0.20f, nullptr, 1.0f),
        Intercept(0.75f, 0.90f, nullptr, 1.0f),
        Intercept(0.99f, 0.10f, nullptr, 1.0f)
    };

    V2CapacitySpec capacities;
    capacities.maxIntercepts = 24;
    capacities.maxCurves = 48;
    capacities.maxWavePoints = 512;
    capacities.maxDeformRegions = 0;

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    TestRasterizer rasterizer(&interpolator, &positioner, &curveBuilder, &waveBuilder);
    rasterizer.workspace.prepare(capacities);

    V2InterpolatorContext interpolatorContext;

    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    V2CurveBuilderContext curveBuilderContext;
    curveBuilderContext.interpolateCurves = false;

    V2WaveBuilderContext waveBuilderContext;
    waveBuilderContext.interpolateCurves = false;

    ScopedAlloc<float> graphOutputMemory(256);
    ScopedAlloc<float> expectedOutputMemory(256);
    ScopedAlloc<float> diffMemory(256);
    Buffer<float> graphOutput = graphOutputMemory.withSize(256);
    Buffer<float> expectedOutput = expectedOutputMemory.withSize(256);
    Buffer<float> diff = diffMemory.withSize(256);
    graphOutput.zero();
    expectedOutput.zero();

    double deltaX = 1.0 / graphOutput.size();

    V2RasterArtifacts artifacts;
    REQUIRE(rasterizer.buildAllArtifacts(interpolatorContext, positionerContext, curveBuilderContext, waveBuilderContext, artifacts));

    int wavePointCount = artifacts.waveBuffers.waveX.size();
    LinearPhasePolicy policy(0.0, deltaX);
    int sampleIndex = jlimit(0, wavePointCount - 2, artifacts.zeroIndex);
    V2RenderResult graphResult = V2WaveSampling::sampleBlock(policy, artifacts.waveBuffers, wavePointCount, graphOutput, sampleIndex);

    REQUIRE(graphResult.rendered);
    REQUIRE(graphResult.samplesWritten == graphOutput.size());

    int interceptCount = 0;
    REQUIRE(rasterizer.runInterceptStages(interpolatorContext, positionerContext, interceptCount));

    int curveCount = 0;
    REQUIRE(curveBuilder.run(
        rasterizer.workspace.intercepts,
        interceptCount,
        rasterizer.workspace.curves,
        curveCount,
        curveBuilderContext));

    int refWavePointCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    REQUIRE(waveBuilder.run(
        rasterizer.workspace.curves,
        curveCount,
        rasterizer.workspace.waveBuffers,
        refWavePointCount,
        zeroIndex,
        oneIndex,
        waveBuilderContext));

    makeReferenceSamples(
        rasterizer.workspace.waveBuffers,
        refWavePointCount,
        zeroIndex,
        expectedOutput,
        deltaX);

    VecOps::sub(graphOutput, expectedOutput, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 < 1e-5f);
    REQUIRE(maxAbs < 1e-6f);
}

TEST_CASE("V2 render output is deterministic for fixed pipeline input", "[curve][v2][raster][determinism]") {
    std::vector<Intercept> intercepts = {
        Intercept(0.00f, 0.00f, nullptr, 1.0f),
        Intercept(0.19f, 0.86f, nullptr, 1.0f),
        Intercept(0.41f, 0.14f, nullptr, 1.0f),
        Intercept(0.64f, 0.77f, nullptr, 1.0f),
        Intercept(0.99f, 0.03f, nullptr, 1.0f)
    };

    ScopedAlloc<float> firstMemory(256);
    ScopedAlloc<float> secondMemory(256);
    ScopedAlloc<float> diffMemory(256);
    Buffer<float> first = firstMemory.withSize(256);
    Buffer<float> second = secondMemory.withSize(256);
    Buffer<float> diff = diffMemory.withSize(256);

    V2RenderResult firstResult = renderFromFixedIntercepts(intercepts, first, false, false);
    V2RenderResult secondResult = renderFromFixedIntercepts(intercepts, second, false, false);

    REQUIRE(firstResult.rendered);
    REQUIRE(secondResult.rendered);
    REQUIRE(firstResult.samplesWritten == first.size());
    REQUIRE(secondResult.samplesWritten == second.size());

    VecOps::sub(first, second, diff);
    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 == 0.0f);
    REQUIRE(maxAbs == 0.0f);
}
