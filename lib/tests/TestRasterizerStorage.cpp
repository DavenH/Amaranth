#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Rasterization/State/RasterizerStorage.h"

using namespace Rasterization;

TEST_CASE("RasterizerStorage groups mutable rasterizer state", "[rasterization][storage]") {
    RasterizerStorage storage;

    storage.intercepts.intercepts.emplace_back(0.25f, 0.5f);
    storage.intercepts.frontPadding.emplace_back(-0.25f, 0.5f);
    storage.intercepts.backPadding.emplace_back(1.25f, 0.5f);
    storage.curves.guideCurveRegions.emplace_back();
    storage.snapshot.rasterizerData.zeroIndex = 3;
    storage.waveform.waveform.zeroIndex = 7;
    storage.waveform.waveform.oneIndex = 11;

    REQUIRE(storage.intercepts.intercepts.size() == 1);
    REQUIRE(storage.intercepts.frontPadding.size() == 1);
    REQUIRE(storage.intercepts.backPadding.size() == 1);
    REQUIRE(storage.curves.guideCurveRegions.size() == 1);
    REQUIRE(storage.snapshot.rasterizerData.zeroIndex == 3);
    REQUIRE(storage.waveform.waveform.zeroIndex == 7);
    REQUIRE(storage.waveform.waveform.oneIndex == 11);

    storage.clear();

    REQUIRE(storage.intercepts.intercepts.empty());
    REQUIRE(storage.intercepts.frontPadding.empty());
    REQUIRE(storage.intercepts.backPadding.empty());
    REQUIRE(storage.curves.curves.empty());
    REQUIRE(storage.curves.guideCurveRegions.empty());
    REQUIRE(storage.waveform.waveform.waveX.empty());
    REQUIRE(storage.waveform.waveform.zeroIndex == 0);
    REQUIRE(storage.waveform.waveform.oneIndex == 0);
}
