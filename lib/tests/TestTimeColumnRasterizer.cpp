#include <catch2/catch_test_macros.hpp>

#include <Array/ScopedAlloc.h>
#include <Curve/Rasterization/Rasterizer/TimeColumnRenderer.h>

TEST_CASE("Shared time column rasterizer varies guide seeds by column and layer", "[rasterization][time-column]") {
    using Rasterization::TimeColumnRasterizer;

    REQUIRE(TimeColumnRasterizer::noiseSeedForLayer(0) !=
            TimeColumnRasterizer::noiseSeedForLayer(1));
    REQUIRE(TimeColumnRasterizer::noiseSeedForLayer(8) ==
            TimeColumnRasterizer::noiseSeedForColumnLayer(0, 8));
    REQUIRE(TimeColumnRasterizer::noiseSeedForColumnLayer(8, 0) !=
            TimeColumnRasterizer::noiseSeedForColumnLayer(8, 1));
}

TEST_CASE("Shared time column rasterizer clears columns without active meshes", "[rasterization][time-column]") {
    constexpr int columnSize = 8;
    ScopedAlloc<Float32> memory(columnSize * 4);
    Buffer<float> columnBuffer = memory.place(columnSize);
    Buffer<float> sumBuffer = memory.place(columnSize);
    Buffer<float> zoomProgress = memory.place(columnSize);
    Buffer<float> layerBuffer = memory.place(columnSize);
    columnBuffer.set(1.f);
    sumBuffer.set(1.f);
    zoomProgress.ramp();

    std::vector<Column> columns { Column(columnBuffer) };
    std::vector<Rasterization::TimeColumnRasterizer::Layer> layers { { nullptr, false, 0.5f } };
    Rasterization::GraphicRasterizer rasterizer(false, 0.f);

    Rasterization::TimeColumnRasterizer::Context context;
    context.layers = &layers;
    context.rasterizer = &rasterizer;
    context.columns = &columns;
    context.zoomProgress = zoomProgress;
    context.layerBuffer = layerBuffer;
    context.sumBuffer = sumBuffer;
    context.numColumns = 1;

    Rasterization::TimeColumnRasterizer().render(context);

    REQUIRE(columns.front().sum() == 0.f);
}
