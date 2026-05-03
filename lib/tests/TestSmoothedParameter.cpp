#include <catch2/catch_test_macros.hpp>

#include "../src/Audio/SmoothedParameter.h"

TEST_CASE("SmoothedParameter initial value initializes ramp state", "[audio]") {
    SmoothedParameter parameter(0.375f);

    REQUIRE(parameter.getTargetValue() == 0.375f);
    REQUIRE(parameter.getCurrentValue() == 0.375f);
    REQUIRE(parameter.getPastCurrentValue() == 0.375f);
    REQUIRE_FALSE(parameter.hasRamp());
}
