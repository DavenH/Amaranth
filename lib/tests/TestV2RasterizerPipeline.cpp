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

void makeExpectedPiecewiseLinear(
    const std::vector<Intercept>& intercepts,
    Buffer<float> output,
    double deltaX) {
    int segIndex = 0;
    int segCount = static_cast<int>(intercepts.size()) - 1;

    for (int i = 0; i < output.size(); ++i) {
        float x = static_cast<float>(i * deltaX);

        while (segIndex < segCount - 1 && x > intercepts[segIndex + 1].x) {
            ++segIndex;
        }

        const Intercept& a = intercepts[segIndex];
        const Intercept& b = intercepts[jmin(segIndex + 1, segCount)];

        float dx = b.x - a.x;
        float t = 0.0f;
        if (dx > 1e-9f) {
            t = (x - a.x) / dx;
        }

        t = jlimit(0.0f, 1.0f, t);
        output[i] = a.y + t * (b.y - a.y);
    }
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

    V2RenderResult result = renderFromFixedIntercepts(sawIntercepts, output, false, false);
    REQUIRE(result.rendered);
    REQUIRE(result.samplesWritten == output.size());

    ScopedAlloc<float> expectedMemory(output.size());
    ScopedAlloc<float> diffMemory(output.size());
    Buffer<float> expected = expectedMemory.withSize(output.size());
    Buffer<float> diff = diffMemory.withSize(output.size());

    makeExpectedPiecewiseLinear(sawIntercepts, expected, 1.0 / output.size());
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

    V2RenderResult result = renderFromFixedIntercepts(squareIntercepts, output, false, false);
    REQUIRE(result.rendered);
    REQUIRE(result.samplesWritten == output.size());

    ScopedAlloc<float> expectedMemory(output.size());
    ScopedAlloc<float> diffMemory(output.size());
    Buffer<float> expected = expectedMemory.withSize(output.size());
    Buffer<float> diff = diffMemory.withSize(output.size());

    makeExpectedPiecewiseLinear(squareIntercepts, expected, 1.0 / output.size());
    VecOps::sub(output, expected, diff);

    float l2 = diff.normL2();
    diff.abs();
    float maxAbs = diff.max();

    REQUIRE(l2 < 1e-5f);
    REQUIRE(maxAbs < 1e-6f);
}
