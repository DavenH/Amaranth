#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Nodes/Curve/Model/FlatCurveMeshState.h"

using namespace CycleV2;

TEST_CASE("Flat curve mesh state serializes ordered vertex triples", "[cycle-v2][curve]") {
    const std::vector<FlatCurveVertexState> vertices {
            { 1.f, 0.75f, 0.25f },
            { 0.f, 0.1f, 1.f }
    };

    const juce::String serialized = FlatCurveMeshState::serialize(vertices);
    const auto parsed = FlatCurveMeshState::parse(serialized);

    REQUIRE(parsed.size() == 2);
    REQUIRE(parsed[0].x == 0.f);
    REQUIRE(parsed[0].y == Catch::Approx(0.1f));
    REQUIRE(parsed[0].curve == 1.f);
    REQUIRE(parsed[1].x == 1.f);
    REQUIRE(parsed[1].y == Catch::Approx(0.75f));
    REQUIRE(parsed[1].curve == Catch::Approx(0.25f));
}
