#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../CycleState.h"
#include "../Rasterization/Facades/VoiceRasterizerFacade.h"

namespace {
    using Catch::Approx;
}

TEST_CASE("VoiceRasterizerFacade resolves wrapped voice point positions", "[cycle][rasterization][voice]") {
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

    auto result = Cycle::Rasterization::VoiceRasterizerFacade().resolvePoint(
            context,
            true,
            &a,
            &b,
            &vertex);

    REQUIRE(result.intersects);
    REQUIRE(result.phase == Approx(0.35f));
    REQUIRE(vertex.values[Vertex::Amp] == Approx(0.50f));
}

TEST_CASE("VoiceRasterizerFacade builds chained padding without caller-side policy construction", "[cycle][rasterization][voice]") {
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

    int paddingSize = Cycle::Rasterization::VoiceRasterizerFacade().buildChainedPadding(
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
