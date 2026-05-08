#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/EnvelopeMesh.h"
#include "../src/Curve/Rasterization/Facades/EnvRasterizerFacade.h"
#include "../src/Curve/Rasterization/Policies/EnvelopePlaybackPolicy.h"
#include "../src/Curve/Rasterization/Policies/EnvelopeRenderTimingPolicy.h"
#include "../src/Curve/Rasterization/Policies/EnvelopeReleasePolicy.h"

namespace {
    std::vector<Intercept> makeIntercepts() {
        return {
            Intercept(0.10f, 0.20f, nullptr, 0.30f),
            Intercept(0.35f, 0.75f, nullptr, 0.40f),
            Intercept(0.70f, 0.40f, nullptr, 0.80f),
            Intercept(0.92f, 0.62f, nullptr, 0.50f),
        };
    }

    Rasterization::EnvelopePaddingContext makeLoopContext() {
        Rasterization::EnvelopePaddingContext context;
        context.loopIndex = 1;
        context.sustainIndex = 3;
        context.loopLength = 0.57f;
        context.canLoop = true;
        context.hasReleaseCurve = false;

        return context;
    }
}

TEST_CASE("EnvRasterizerFacade evaluates envelope marker indices", "[rasterization][env][facade]") {
    EnvelopeMesh mesh("EnvelopeMarkerMesh");
    VertCube loopCube;
    VertCube sustainCube;

    auto intercepts = makeIntercepts();
    intercepts[1].cube = &loopCube;
    intercepts[3].cube = &sustainCube;

    mesh.loopCubes.insert(&loopCube);
    mesh.sustainCubes.insert(&sustainCube);

    auto result = Rasterization::EnvRasterizerFacade().evaluateMarkers(intercepts, &mesh, 1);

    REQUIRE(result.loopIndex == 1);
    REQUIRE(result.sustainIndex == 3);
}

TEST_CASE("EnvRasterizerFacade falls back to last envelope intercept when sustain is unmarked", "[rasterization][env][facade]") {
    EnvelopeMesh mesh("EnvelopeMarkerMesh");
    auto intercepts = makeIntercepts();

    auto result = Rasterization::EnvRasterizerFacade().evaluateMarkers(intercepts, &mesh, 1);

    REQUIRE(result.loopIndex == -1);
    REQUIRE(result.sustainIndex == 3);
}

TEST_CASE("EnvRasterizerFacade applies unipolar sustain floor point", "[rasterization][env][facade]") {
    auto intercepts = makeIntercepts();

    Rasterization::EnvelopeSustainPointContext context;
    context.sustainIndex = 1;
    context.addFloorPoint = true;

    bool needsResorting = Rasterization::EnvRasterizerFacade().applySustainPoint(intercepts, context);

    REQUIRE(needsResorting);
    REQUIRE(intercepts.size() == 5);
    REQUIRE(intercepts.back().cube == nullptr);
    REQUIRE(intercepts.back().x == Catch::Approx(0.3501f));
    REQUIRE(intercepts.back().y == Catch::Approx(0.75f));
    REQUIRE(intercepts.back().shp == Catch::Approx(1.f));
}

TEST_CASE("EnvRasterizerFacade skips sustain floor point for terminal sustain", "[rasterization][env][facade]") {
    auto intercepts = makeIntercepts();

    Rasterization::EnvelopeSustainPointContext context;
    context.sustainIndex = 3;
    context.addFloorPoint = true;

    bool needsResorting = Rasterization::EnvRasterizerFacade().applySustainPoint(intercepts, context);

    REQUIRE_FALSE(needsResorting);
    REQUIRE(intercepts.size() == 4);
}

TEST_CASE("EnvRasterizerFacade builds display padding for looping envelopes", "[rasterization][env][facade]") {
    auto intercepts = makeIntercepts();
    std::vector<Curve> curves;

    bool sampleable = Rasterization::EnvRasterizerFacade().buildDisplayPadding(
            intercepts,
            curves,
            makeLoopContext());

    REQUIRE(sampleable);
    REQUIRE(curves.size() == 7);
    REQUIRE(curves.front().a.x == Catch::Approx(-0.10f));
    REQUIRE(curves.front().c.x == Catch::Approx(-0.05f));
    REQUIRE(curves.back().b.x == Catch::Approx(intercepts[2].x + 0.57f));
    REQUIRE(curves.back().c.x == Catch::Approx(intercepts[3].x + 0.57f));
}

TEST_CASE("EnvRasterizerFacade builds release render padding with terminal tail", "[rasterization][env][facade]") {
    auto intercepts = makeIntercepts();
    std::vector<Curve> curves;

    auto context = makeLoopContext();
    context.state = Rasterization::EnvelopePaddingContext::Releasing;
    context.hasReleaseCurve = true;

    bool sampleable = Rasterization::EnvRasterizerFacade().buildRenderPadding(
            intercepts,
            curves,
            context);

    REQUIRE(sampleable);
    REQUIRE(curves.size() == 8);
    REQUIRE(curves.back().a.x == Catch::Approx(intercepts.back().x + 0.001f));
    REQUIRE(curves.back().b.x == Catch::Approx(intercepts.back().x + 0.002f));
    REQUIRE(curves.back().c.x == Catch::Approx(intercepts.back().x + 0.003f));
}

TEST_CASE("EnvelopePlaybackPolicy resolves release state and loop boundaries", "[rasterization][env][facade]") {
    auto intercepts = makeIntercepts();

    Rasterization::EnvelopePlaybackContext context;
    context.loopIndex = 1;
    context.sustainIndex = 2;

    Rasterization::EnvelopePlaybackPolicy policy;

    REQUIRE_FALSE(policy.hasReleaseCurve(intercepts, 3));
    REQUIRE(policy.hasReleaseCurve(intercepts, 2));
    REQUIRE(policy.loopLength(intercepts, context) == Catch::Approx(0.35f));
    REQUIRE(policy.boundary(intercepts, context) == Catch::Approx(0.70f));

    context.releasing = true;
    REQUIRE(policy.boundary(intercepts, context) == Catch::Approx(intercepts.back().x));
}

TEST_CASE("EnvelopeReleasePolicy resolves release intercept and scale", "[rasterization][env][facade]") {
    auto intercepts = makeIntercepts();

    Rasterization::EnvelopeReleaseContext context;
    context.bipolar = false;
    context.sustainIndex = 1;

    Rasterization::EnvelopeReleasePolicy policy;

    REQUIRE(policy.releaseIndex(context) == 2);

    auto release = policy.start(intercepts, context, 0.75f, 0.25f);

    REQUIRE(release.index == 2);
    REQUIRE(release.position == Catch::Approx(0.70f));
    REQUIRE(release.scale == Catch::Approx(1.5f));

    context.bipolar = true;
    REQUIRE(policy.releaseIndex(context) == 1);
}

TEST_CASE("EnvelopeRenderTimingPolicy applies tempo and scale before partitioning", "[rasterization][env][facade]") {
    MeshLibrary::EnvProps props;
    props.active = true;
    props.tempoSync = true;
    props.scale = 2.f;

    Rasterization::EnvelopeRenderTimingContext context;
    context.numSamples = 2048;
    context.deltaX = 0.01;
    context.tempoScale = 2.f;
    context.loopLength = 0.25f;
    context.props = &props;

    auto timing = Rasterization::EnvelopeRenderTimingPolicy().prepare(context);

    REQUIRE(timing.effectiveDelta == Catch::Approx(0.0025));
    REQUIRE(timing.maxSamplesPerBuffer == 90);

    props.tempoSync = false;
    props.scale = 1.f;
    context.loopLength = -1.f;

    timing = Rasterization::EnvelopeRenderTimingPolicy().prepare(context);

    REQUIRE(timing.effectiveDelta == Catch::Approx(0.01));
    REQUIRE(timing.maxSamplesPerBuffer == 50);
}
