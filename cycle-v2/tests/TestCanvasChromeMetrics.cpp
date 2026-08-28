#include <catch2/catch_test_macros.hpp>

#include "UI/CanvasChromeMetrics.h"

using namespace CycleV2;

TEST_CASE("Canvas chrome uses one restrained semantic corner scale",
        "[cycle-v2][canvas][chrome][metrics]") {
    REQUIRE(CanvasChromeMetrics::microCornerRadius == 2.f);
    REQUIRE(CanvasChromeMetrics::insetCornerRadius == 3.f);
    REQUIRE(CanvasChromeMetrics::controlCornerRadius == 4.f);
    REQUIRE(CanvasChromeMetrics::tileCornerRadius == 5.f);
    REQUIRE(CanvasChromeMetrics::panelCornerRadius == 6.f);

    REQUIRE(CanvasChromeMetrics::microCornerRadius
            < CanvasChromeMetrics::insetCornerRadius);
    REQUIRE(CanvasChromeMetrics::insetCornerRadius
            < CanvasChromeMetrics::controlCornerRadius);
    REQUIRE(CanvasChromeMetrics::controlCornerRadius
            < CanvasChromeMetrics::tileCornerRadius);
    REQUIRE(CanvasChromeMetrics::tileCornerRadius
            < CanvasChromeMetrics::panelCornerRadius);
}

TEST_CASE("Canvas chrome reserves its strongest generic stroke for focus",
        "[cycle-v2][canvas][chrome][metrics]") {
    REQUIRE(CanvasChromeMetrics::restingBorderWidth == 1.f);
    REQUIRE(CanvasChromeMetrics::activeBorderWidth == 1.5f);
    REQUIRE(CanvasChromeMetrics::focusRingWidth == 2.f);

    REQUIRE(CanvasChromeMetrics::restingBorderWidth
            < CanvasChromeMetrics::activeBorderWidth);
    REQUIRE(CanvasChromeMetrics::activeBorderWidth
            < CanvasChromeMetrics::focusRingWidth);
}
