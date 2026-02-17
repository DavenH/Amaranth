#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/V2/Runtime/V2RasterizerGraph.h"
#include "../src/Curve/V2/Runtime/V2RasterizerWorkspace.h"
#include "../src/Curve/V2/Stages/V2CurveBuilderStages.h"
#include "../src/Curve/V2/Stages/V2InterpolatorStages.h"
#include "../src/Curve/V2/Stages/V2PositionerStages.h"
#include "../src/Curve/V2/Stages/V2SamplerStages.h"
#include "../src/Curve/V2/Stages/V2WaveBuilderStages.h"
#include "../src/Curve/V2/State/V2EnvStateMachine.h"
#include "../src/Array/VecOps.h"
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

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    V2PositionerStage* positionerStage = cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                                : static_cast<V2PositionerStage*>(&positioner);
    V2RasterizerGraph graph(&interpolator, positionerStage, &curveBuilder, &waveBuilder, &sampler);

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

    V2SamplerContext samplerContext;
    samplerContext.request.numSamples = output.size();
    samplerContext.request.deltaX = 1.0 / output.size();
    samplerContext.request.tempoScale = 1.0f;
    samplerContext.request.scale = 1;

    return graph.render(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        samplerContext,
        output);
}

void makeReferenceSamples(
    Buffer<float> waveX,
    Buffer<float> waveY,
    Buffer<float> slope,
    int zeroIndex,
    Buffer<float> output,
    double deltaX) {
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

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2CyclicPositionerStage cyclicPositioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    V2PositionerStage* positionerStage = cyclic ? static_cast<V2PositionerStage*>(&cyclicPositioner)
                                                : static_cast<V2PositionerStage*>(&positioner);
    V2RasterizerGraph graph(&interpolator, positionerStage, &curveBuilder, &waveBuilder, &sampler);

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

    V2SamplerContext samplerContext;
    samplerContext.request.numSamples = output.size();
    samplerContext.request.deltaX = 1.0 / output.size();
    samplerContext.request.tempoScale = 1.0f;
    samplerContext.request.scale = 1;

    output.zero();
    expected.zero();

    V2RenderResult result = graph.render(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        samplerContext,
        output);

    if (! result.rendered || result.samplesWritten != output.size()) {
        return false;
    }

    int interceptCount = 0;
    if (! graph.runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount)) {
        return false;
    }

    int curveCount = 0;
    if (! curveBuilder.run(
            workspace.intercepts,
            interceptCount,
            workspace.curves,
            curveCount,
            curveBuilderContext)) {
        return false;
    }

    int wavePointCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    if (! waveBuilder.run(
            workspace.curves,
            curveCount,
            workspace.waveX,
            workspace.waveY,
            workspace.diffX,
            workspace.slope,
            wavePointCount,
            zeroIndex,
            oneIndex,
            waveBuilderContext)) {
        return false;
    }

    makeReferenceSamples(
        workspace.waveX.withSize(wavePointCount),
        workspace.waveY.withSize(wavePointCount),
        workspace.slope.withSize(wavePointCount - 1),
        zeroIndex,
        expected,
        samplerContext.request.deltaX);
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

TEST_CASE("V2 rasterizer graph runs interpolation and positioning with workspace", "[curve][v2][raster]") {
    V2CapacitySpec capacities;
    capacities.maxIntercepts = 16;
    capacities.maxCurves = 16;
    capacities.maxWavePoints = 64;
    capacities.maxDeformRegions = 0;

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);
    workspace.intercepts.emplace_back(99.0f, 99.0f); // should be cleared by graph

    StubInterpolatorStage interpolator;
    V2LinearPositionerStage positioner;
    V2RasterizerGraph graph(&interpolator, &positioner);

    V2InterpolatorContext interpolatorContext;
    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    int interceptCount = 0;
    REQUIRE(graph.runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount));
    REQUIRE(interceptCount == 2);
    REQUIRE(workspace.intercepts[0].x < workspace.intercepts[1].x);
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

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);

    V2TrilinearInterpolatorStage interpolator;
    V2LinearPositionerStage positioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;

    V2RasterizerGraph graph(&interpolator, &positioner, &curveBuilder, &waveBuilder, &sampler);

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

    V2SamplerContext samplerContext;
    samplerContext.request.numSamples = output.size();
    samplerContext.request.deltaX = 1.0 / output.size();
    samplerContext.request.tempoScale = 1.0f;
    samplerContext.request.scale = 1;

    V2RenderResult result = graph.render(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        samplerContext,
        output);

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

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);

    OverflowInterpolatorStage interpolator(5);
    V2LinearPositionerStage positioner;
    V2RasterizerGraph graph(&interpolator, &positioner);

    V2InterpolatorContext interpolatorContext;
    V2PositionerContext positionerContext;
    positionerContext.scaling = V2ScalingType::Unipolar;
    positionerContext.minX = 0.0f;
    positionerContext.maxX = 1.0f;

    int interceptCount = 0;
    REQUIRE_FALSE(graph.runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount));
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

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;
    V2RasterizerGraph graph(&interpolator, &positioner, &curveBuilder, &waveBuilder, &sampler);

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

    V2SamplerContext samplerContext;
    samplerContext.request.numSamples = output.size();
    samplerContext.request.deltaX = 1.0 / output.size();
    samplerContext.request.tempoScale = 1.0f;
    samplerContext.request.scale = 1;

    size_t interceptCapacity = workspace.intercepts.capacity();
    size_t curveCapacity = workspace.curves.capacity();
    size_t deformStartCapacity = workspace.deformRegionStarts.capacity();
    size_t deformEndCapacity = workspace.deformRegionEnds.capacity();

    for (int i = 0; i < 8; ++i) {
        output.zero();
        V2RenderResult result = graph.render(
            workspace,
            interpolatorContext,
            positionerContext,
            curveBuilderContext,
            waveBuilderContext,
            samplerContext,
            output);
        REQUIRE(result.rendered);
        REQUIRE(result.samplesWritten == output.size());
    }

    REQUIRE(workspace.intercepts.capacity() == interceptCapacity);
    REQUIRE(workspace.curves.capacity() == curveCapacity);
    REQUIRE(workspace.deformRegionStarts.capacity() == deformStartCapacity);
    REQUIRE(workspace.deformRegionEnds.capacity() == deformEndCapacity);
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

    V2RasterizerWorkspace workspace;
    workspace.prepare(capacities);

    FixedInterpolatorStage interpolator(intercepts);
    V2LinearPositionerStage positioner;
    V2DefaultCurveBuilderStage curveBuilder;
    V2DefaultWaveBuilderStage waveBuilder;
    V2LinearSamplerStage sampler;
    V2RasterizerGraph graph(&interpolator, &positioner, &curveBuilder, &waveBuilder, &sampler);

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

    V2SamplerContext samplerContext;
    samplerContext.request.numSamples = graphOutput.size();
    samplerContext.request.deltaX = 1.0 / graphOutput.size();
    samplerContext.request.tempoScale = 1.0f;
    samplerContext.request.scale = 1;

    V2RenderResult graphResult = graph.render(
        workspace,
        interpolatorContext,
        positionerContext,
        curveBuilderContext,
        waveBuilderContext,
        samplerContext,
        graphOutput);

    REQUIRE(graphResult.rendered);
    REQUIRE(graphResult.samplesWritten == graphOutput.size());

    int interceptCount = 0;
    REQUIRE(graph.runInterceptStages(workspace, interpolatorContext, positionerContext, interceptCount));

    int curveCount = 0;
    REQUIRE(curveBuilder.run(
        workspace.intercepts,
        interceptCount,
        workspace.curves,
        curveCount,
        curveBuilderContext));

    int wavePointCount = 0;
    int zeroIndex = 0;
    int oneIndex = 0;
    REQUIRE(waveBuilder.run(
        workspace.curves,
        curveCount,
        workspace.waveX,
        workspace.waveY,
        workspace.diffX,
        workspace.slope,
        wavePointCount,
        zeroIndex,
        oneIndex,
        waveBuilderContext));

    makeReferenceSamples(
        workspace.waveX.withSize(wavePointCount),
        workspace.waveY.withSize(wavePointCount),
        workspace.slope.withSize(wavePointCount - 1),
        zeroIndex,
        expectedOutput,
        samplerContext.request.deltaX);

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
