#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/Rasterization/Policies/Curves/CurveResolutionPolicy.h"

TEST_CASE("CurveResolutionPolicy applies shared curve resolution", "[rasterization][curves]") {
    std::vector<Curve> curves;

    for (int i = 0; i < 9; ++i) {
        curves.emplace_back(
                Intercept(float(i), 0.f),
                Intercept(float(i) + 0.5f, 0.f),
                Intercept(float(i) + 1.f, 0.f));
    }

    Rasterization::CurveResolutionPolicy::Context context;
    context.lowResCurves = true;

    Rasterization::CurveResolutionPolicy().apply(curves, context);

    for (const auto& curve : curves) {
        REQUIRE(curve.resIndex == Curve::resolutions - 1);
    }
}
