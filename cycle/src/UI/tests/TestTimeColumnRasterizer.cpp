#include <catch2/catch_test_macros.hpp>

#include "../VisualDsp/TimeColumnRasterizer.h"

TEST_CASE("TimeColumnRasterizer varies guide noise seeds by layer", "[cycle][visual-dsp]") {
    using Cycle::Rasterization::TimeColumnRasterizer;

    REQUIRE(TimeColumnRasterizer::noiseSeedForColumnLayer(0, 0) !=
            TimeColumnRasterizer::noiseSeedForColumnLayer(0, 1));
    REQUIRE(TimeColumnRasterizer::noiseSeedForColumnLayer(8, 0) !=
            TimeColumnRasterizer::noiseSeedForColumnLayer(8, 1));
    REQUIRE(TimeColumnRasterizer::noiseSeedForColumnLayer(8, 1) ==
            TimeColumnRasterizer::noiseSeedForColumnLayer(8, 1));
}
