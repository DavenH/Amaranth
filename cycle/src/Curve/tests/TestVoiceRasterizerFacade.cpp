#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../CycleState.h"
#include "../Rasterization/Policies/Voice/VoiceChainedPaddingPolicy.h"
#include "../Rasterization/Policies/Voice/VoiceChainingPolicy.h"
#include "../Rasterization/Policies/Voice/VoiceCurveResolutionPolicy.h"
#include "../Rasterization/Policies/Voice/VoicePointPositionPolicy.h"
#include "../Rasterization/Pipelines/VoiceSlicePipeline.h"
#include <Curve/Mesh.h>
#include <Curve/Rasterization/Sources/MeshCubeSource.h>
#include <Curve/VertCube.h>

namespace {
    using Catch::Approx;

    void setCubeAsVoicePoint(
            VertCube* cube,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        for (int i = 0; i < (int) VertCube::numVerts; ++i) {
            bool time, red, blue;
            VertCube::getPoles(i, time, red, blue);

            Vertex* vertex = cube->getVertex(i);
            vertex->values[Vertex::Time]  = time ? 1.f : 0.f;
            vertex->values[Vertex::Red]   = red  ? 1.f : 0.f;
            vertex->values[Vertex::Blue]  = blue ? 1.f : 0.f;
            vertex->values[Vertex::Phase] = time ? highPhase : lowPhase;
            vertex->values[Vertex::Amp]   = time ? highAmp   : lowAmp;
            vertex->values[Vertex::Curve] = sharpness;
        }
    }

    VertCube* addVoiceCube(
            Mesh& mesh,
            float lowPhase,
            float highPhase,
            float lowAmp,
            float highAmp,
            float sharpness) {
        auto* cube = new VertCube(&mesh);
        setCubeAsVoicePoint(cube, lowPhase, highPhase, lowAmp, highAmp, sharpness);
        mesh.addCube(cube);

        return cube;
    }
}

TEST_CASE("VoicePointPositionPolicy resolves wrapped voice point positions", "[cycle][rasterization][voice]") {
    Vertex a(0.20f, 0.25f);
    Vertex b(0.80f, 0.75f);
    Vertex vertex;

    a.values[Vertex::Time] = 0.0f;
    a.values[Vertex::Phase] = 0.20f;
    a.values[Vertex::Amp] = 0.25f;
    b.values[Vertex::Time] = 1.0f;
    b.values[Vertex::Phase] = 0.80f;
    b.values[Vertex::Amp] = 0.75f;

    Cycle::Rasterization::VoicePointPositionPolicy::Context context;
    context.voiceTime = 0.5f;
    context.oscPhase = 0.85f;

    auto result = Cycle::Rasterization::VoicePointPositionPolicy().resolve(
            context,
            true,
            &a,
            &b,
            &vertex);

    REQUIRE(result.intersects);
    REQUIRE(result.phase == Approx(0.35f));
    REQUIRE(vertex.values[Vertex::Amp] == Approx(0.50f));
}

TEST_CASE("VoiceChainedPaddingPolicy builds chained padding", "[cycle][rasterization][voice]") {
    std::vector<Intercept> intercepts {
        Intercept(0.10f, -0.50f, nullptr, 0.20f),
        Intercept(0.42f, 0.25f, nullptr, 0.35f),
        Intercept(0.78f, -0.10f, nullptr, 0.50f),
    };

    std::vector<Intercept> nextIntercepts {
        Intercept(0.12f, -0.40f, nullptr, 0.25f),
        Intercept(0.46f, 0.40f, nullptr, 0.45f),
        Intercept(0.86f, 0.10f, nullptr, 0.65f),
    };

    CycleState state;
    state.frontE = Intercept(-0.75f, -0.10f);
    state.frontD = Intercept(-0.55f, -0.20f);
    state.frontC = Intercept(-0.35f, -0.30f);
    state.frontB = Intercept(-0.12f, -0.40f);
    state.frontA = Intercept(-0.04f, -0.50f);

    std::vector<Curve> curves;

    int paddingSize = Cycle::Rasterization::VoiceChainedPaddingPolicy().build(
            intercepts,
            nextIntercepts,
            state,
            curves,
            0.05f);

    REQUIRE(paddingSize == 2);
    REQUIRE(curves.size() == 10);
    REQUIRE(state.frontA.x == Approx(-0.22f));
    REQUIRE(state.frontB.x == Approx(-0.58f));
}

TEST_CASE("VoiceCurveResolutionPolicy applies voice endpoint resolution", "[cycle][rasterization][voice]") {
    std::vector<Intercept> intercepts {
        Intercept(-0.10f, -0.50f),
        Intercept(0.10f, -0.25f),
        Intercept(0.45f, 0.25f),
        Intercept(0.80f, -0.10f),
        Intercept(1.10f, 0.20f),
    };

    std::vector<Curve> curves {
        Curve(intercepts[0], intercepts[1], intercepts[2]),
        Curve(intercepts[1], intercepts[2], intercepts[3]),
        Curve(intercepts[2], intercepts[3], intercepts[4]),
    };

    Cycle::Rasterization::VoiceCurveResolutionPolicy().apply(curves);

    REQUIRE(curves.front().resIndex == curves[1].resIndex);
    REQUIRE(curves.back().resIndex == curves[curves.size() - 2].resIndex);
}

TEST_CASE("VoiceChainingPolicy rotates and publishes chained intercept windows", "[cycle][rasterization][voice]") {
    CycleState state;
    state.callCount = 1;
    state.backIcpts = {
        Intercept(0.10f, -0.50f),
        Intercept(0.42f, 0.25f),
    };

    std::vector<Intercept> currentIntercepts {
        Intercept(0.20f, -0.25f),
    };

    bool needsResorting = true;
    Cycle::Rasterization::VoiceChainingPolicy policy(&needsResorting);

    policy.beginCall(state, currentIntercepts);

    REQUIRE_FALSE(needsResorting);
    REQUIRE(currentIntercepts.size() == 2);
    REQUIRE(currentIntercepts[0].x == Approx(0.10f));
    REQUIRE(state.backIcpts.empty());

    Cycle::Rasterization::VoiceSlicePipeline::Output output;
    output.intercepts = {
        Intercept(0.60f, 0.10f),
        Intercept(0.30f, 0.40f),
    };

    needsResorting = true;
    bool canPublish = policy.publishNextIntercepts(
            output,
            state,
            [](std::vector<Intercept>& intercepts) {
                for (auto& intercept : intercepts) {
                    intercept.adjustedX = intercept.x;
                }
            });

    REQUIRE(canPublish);
    REQUIRE_FALSE(needsResorting);
    REQUIRE(state.backIcpts.size() == 2);
    REQUIRE(state.backIcpts[0].x == Approx(0.30f));
    REQUIRE(policy.canBuildChainedCurves(state, currentIntercepts));
}

TEST_CASE("VoiceSlicePipeline builds voice slice intercepts", "[cycle][rasterization][voice]") {
    Mesh mesh("VoiceSlicePipelineMesh");
    addVoiceCube(mesh, 0.20f, 0.80f, 0.25f, 0.75f, 0.35f);
    addVoiceCube(mesh, 0.10f, 0.40f, 0.10f, 0.30f, 0.55f);

    ::Rasterization::MeshCubeSource source(&mesh);
    VertCube::ReductionData reductionData;

    auto output = Cycle::Rasterization::VoiceSlicePipeline().render(
            source,
            MorphPosition(0.50f, 0.50f, 0.50f),
            0.f,
            0.85f,
            [](Intercept&, const MorphPosition&) {},
            reductionData);

    REQUIRE(output.sampleable);
    REQUIRE(output.intercepts.size() == 2);
    REQUIRE(output.intercepts[0].x == Approx(0.10f));
    REQUIRE(output.intercepts[0].y == Approx(-0.60f));
    REQUIRE(output.intercepts[0].shp == Approx(0.55f));
    REQUIRE(output.intercepts[1].x == Approx(0.35f));
    REQUIRE(output.intercepts[1].y == Approx(0.f));
    REQUIRE(output.intercepts[1].shp == Approx(0.35f));

    mesh.destroy();
}
