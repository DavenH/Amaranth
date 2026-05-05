#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Obj/MorphPosition.h"

TEST_CASE("MorphPosition resetTime pins current and target values", "[morph]") {
    MorphPosition position(0.75f, 0.25f, 0.5f);

    position.resetTime();

    REQUIRE(position.time.getCurrentValue() == Catch::Approx(0.f));
    REQUIRE(position.time.getTargetValue() == Catch::Approx(0.f));
    REQUIRE(position.red.getCurrentValue() == Catch::Approx(0.25f));
    REQUIRE(position.blue.getCurrentValue() == Catch::Approx(0.5f));
}
