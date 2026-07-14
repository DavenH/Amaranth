#include <catch2/catch_test_macros.hpp>

#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Rasterizer/EnvelopeGridRasterizer.h>

TEST_CASE("Shared envelope grid rasterizer preserves E3 request semantics", "[rasterization][envelope-grid]") {
    Rasterization::EnvelopeGridRasterizer rasterizer;
    const auto& request = rasterizer.getRequest();

    REQUIRE(request.lowResCurves);
    REQUIRE_FALSE(request.cyclic);
    REQUIRE_FALSE(request.calcDepthDimensions);
    REQUIRE(request.scalingMode == Rasterization::PointScalingMode::HalfBipolar);
    REQUIRE(request.xMinimum == 0.f);
    REQUIRE(request.xMaximum == 10.f);
}

TEST_CASE("Shared envelope grid rasterizer clears columns for an absent mesh", "[rasterization][envelope-grid]") {
    constexpr int columnSize = 8;
    ScopedAlloc<Float32> memory(columnSize * 2);
    Buffer<float> first = memory.place(columnSize);
    Buffer<float> second = memory.place(columnSize);
    first.set(1.f);
    second.set(1.f);

    std::vector<Column> columns { Column(first), Column(second) };
    Rasterization::EnvelopeGridRasterizer rasterizer;
    rasterizer.renderGrid(nullptr, columns, columnSize, Vertex::Time);

    REQUIRE(columns[0].sum() == 0.f);
    REQUIRE(columns[1].sum() == 0.f);
}
