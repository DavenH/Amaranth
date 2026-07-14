#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Rasterization/Policies/Voice/VoicePolicies.h>

using Catch::Approx;

TEST_CASE("Shared voice point policy interpolates and wraps oscillator phase", "[rasterization][voice]") {
    Vertex a(0.20f, 0.25f);
    Vertex b(0.80f, 0.75f);
    Vertex output;
    a.values[Vertex::Time] = 0.f;
    a.values[Vertex::Phase] = 0.20f;
    b.values[Vertex::Time] = 1.f;
    b.values[Vertex::Phase] = 0.80f;

    Rasterization::VoicePointPositionPolicy::Context context;
    context.voiceTime = 0.5f;
    context.oscPhase = 0.85f;
    const auto result = Rasterization::VoicePointPositionPolicy().resolve(
            context,
            true,
            &a,
            &b,
            &output);

    REQUIRE(result.intersects);
    REQUIRE(result.phase == Approx(0.35f));
}

TEST_CASE("Shared voice chaining state rotates complete intercept windows", "[rasterization][voice]") {
    Rasterization::VoiceCycleState state;
    state.callCount = 1;
    state.backIcpts = {
            Intercept(0.10f, -0.50f),
            Intercept(0.42f, 0.25f)
    };
    std::vector<Intercept> current { Intercept(0.20f, -0.25f) };
    bool needsResorting = true;
    Rasterization::VoiceChainingPolicy policy(&needsResorting);

    policy.beginCall(state, current);

    REQUIRE_FALSE(needsResorting);
    REQUIRE(current.size() == 2);
    REQUIRE(current.front().x == Approx(0.10f));
    REQUIRE(state.backIcpts.empty());
}
