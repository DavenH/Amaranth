#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Curve/Rasterization/Rasterizer/GraphicMeshRasterizer.h>

using Catch::Approx;

TEST_CASE("Shared graphic rasterizer configures explicit render bounds", "[rasterization][graphic]") {
    Rasterization::GraphicRasterizer rasterizer(true, 0.125f);

    const auto& request = rasterizer.getRequest();
    REQUIRE(request.cyclic);
    REQUIRE(request.xMinimum == Approx(-0.125f));
    REQUIRE(request.xMaximum == Approx(1.125f));
}

TEST_CASE("Shared graphic rasterizer restores scoped render state", "[rasterization][graphic]") {
    Rasterization::GraphicRasterizer rasterizer(false, 0.f);
    rasterizer.setNoiseSeed(17);
    rasterizer.setMorphPosition(MorphPosition(0.25f, 0.50f, 0.75f));

    Rasterization::GraphicRasterizer::RenderState saved;
    {
        auto scopedState = rasterizer.preserveState(saved);
        auto batchState = Rasterization::GraphicRasterizer::createAnalysisRenderState(
                Rasterization::GraphicRasterizer::Scaling::HalfBipolar,
                MorphPosition(0.75f, 0.25f, 0.50f));
        rasterizer.restoreStateFrom(batchState);

        REQUIRE(rasterizer.getRequest().scalingMode == Rasterization::PointScalingMode::HalfBipolar);
    }

    REQUIRE(rasterizer.getNoiseSeed() == 17);
    REQUIRE(rasterizer.getMorphPosition().time.getTargetValue() == Approx(0.25f));
    REQUIRE(rasterizer.getMorphPosition().red.getTargetValue() == Approx(0.50f));
    REQUIRE(rasterizer.getMorphPosition().blue.getTargetValue() == Approx(0.75f));
}
