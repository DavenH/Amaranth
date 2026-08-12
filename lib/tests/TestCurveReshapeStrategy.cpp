#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Inter/CurveReshapeStrategy.h>

TEST_CASE("Curve reshape follows the gesture direction toward its controlling intercept",
        "[interaction][curve][reshape]") {
    constexpr float controlY = 0.7f;
    constexpr float zoom = 1.f;
    constexpr float dragScale = 1.f;
    constexpr float curveScale = 0.9f;

    const float upwardToward = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.3f, 0.4f, controlY, zoom, dragScale, curveScale);
    const float upwardAway = CurveReshapeStrategy::sharpnessDelta(
            0.8f, 0.8f, 0.9f, controlY, zoom, dragScale, curveScale);
    const float downwardToward = CurveReshapeStrategy::sharpnessDelta(
            0.9f, 0.9f, 0.8f, controlY, zoom, dragScale, curveScale);
    const float downwardAway = CurveReshapeStrategy::sharpnessDelta(
            0.4f, 0.4f, 0.3f, controlY, zoom, dragScale, curveScale);

    REQUIRE(upwardToward > 0.f);
    REQUIRE(upwardAway < 0.f);
    REQUIRE(downwardToward > 0.f);
    REQUIRE(downwardAway < 0.f);
}

TEST_CASE("Curve reshape reverses and preserves scale", "[interaction][curve][reshape]") {
    const float toward = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.3f, 0.4f, 0.7f, 4.f, 2.f, 0.9f);
    const float reverse = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.4f, 0.3f, 0.7f, 4.f, 2.f, 0.9f);
    const float stationary = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.4f, 0.4f, 0.7f, 4.f, 2.f, 0.9f);

    REQUIRE(toward == Catch::Approx(0.1f));
    REQUIRE(reverse == Catch::Approx(-toward));
    REQUIRE(stationary == 0.f);
}

TEST_CASE("Curve reshape keeps sharpening after crossing its control",
        "[interaction][curve][reshape]") {
    const float beforeCrossing = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.5f, 0.65f, 0.6f, 1.f, 1.f, 0.9f);
    const float afterCrossing = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.65f, 0.8f, 0.6f, 1.f, 1.f, 0.9f);
    const float reverseAfterCrossing = CurveReshapeStrategy::sharpnessDelta(
            0.3f, 0.8f, 0.7f, 0.6f, 1.f, 1.f, 0.9f);

    REQUIRE(beforeCrossing > 0.f);
    REQUIRE(afterCrossing > 0.f);
    REQUIRE(reverseAfterCrossing < 0.f);
    REQUIRE(CurveReshapeStrategy::applySharpnessDelta(0.95f, afterCrossing) == 1.f);
}

TEST_CASE("Curve reshape clamps retained sharpness", "[interaction][curve][reshape]") {
    REQUIRE(CurveReshapeStrategy::applySharpnessDelta(0.9f, 0.2f) == 1.f);
    REQUIRE(CurveReshapeStrategy::applySharpnessDelta(0.1f, -0.2f) == 0.f);
    REQUIRE(CurveReshapeStrategy::applySharpnessDelta(0.4f, 0.f) == 0.4f);
}
