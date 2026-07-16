#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Nodes/Effect2D/Effect2DMeshState.h"

using namespace CycleV2;

TEST_CASE("Effect 2D mesh state serializes ordered vertex triples", "[cycle-v2][effect2d]") {
    const std::vector<Effect2DVertexState> vertices {
            { 1.f, 0.75f, 0.25f },
            { 0.f, 0.1f, 1.f }
    };

    const juce::String serialized = Effect2DMeshState::serialize(vertices);
    const auto parsed = Effect2DMeshState::parse(serialized);

    REQUIRE(parsed.size() == 2);
    REQUIRE(parsed[0].x == 0.f);
    REQUIRE(parsed[0].y == Catch::Approx(0.1f));
    REQUIRE(parsed[0].curve == 1.f);
    REQUIRE(parsed[1].x == 1.f);
    REQUIRE(parsed[1].y == Catch::Approx(0.75f));
    REQUIRE(parsed[1].curve == Catch::Approx(0.25f));
}

TEST_CASE("Effect 2D mesh state skips malformed triples", "[cycle-v2][effect2d]") {
    const auto parsed = Effect2DMeshState::parse("0,0,1;bad;0.5,0.75;1,1,1");

    REQUIRE(parsed.size() == 2);
    REQUIRE(parsed.front().x == 0.f);
    REQUIRE(parsed.back().x == 1.f);
}
