#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/V2/Runtime/V2RasterizerGraph.h"
#include "../src/Curve/V2/Runtime/V2RasterizerWorkspace.h"
#include "../src/Curve/V2/Stages/V2InterpolatorStages.h"
#include "../src/Curve/V2/Stages/V2PositionerStages.h"
#include "../src/Curve/V2/State/V2EnvStateMachine.h"
#include "../src/Curve/Mesh.h"
#include "../src/Curve/VertCube.h"

namespace {
void populateUnitCubeMesh(Mesh& mesh) {
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

        vert->values[Vertex::Phase] = 0.1f
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
}

TEST_CASE("V2 interpolator stages extract intercepts", "[curve][v2][raster]") {
    Mesh mesh("v2-test-mesh");
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

    mesh.destroy();
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
