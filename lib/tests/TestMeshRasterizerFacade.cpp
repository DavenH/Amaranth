#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Curve.h"
#include "../src/Curve/Rasterization/Facades/MeshRasterizerFacade.h"

TEST_CASE("MeshRasterizerFacade applies shared curve resolution policy", "[rasterization][facade]") {
    std::vector<Curve> curves;

    for (int i = 0; i < 9; ++i) {
        curves.emplace_back(
                Intercept(float(i), 0.f),
                Intercept(float(i) + 0.5f, 0.f),
                Intercept(float(i) + 1.f, 0.f));
    }

    Rasterization::CurveResolutionPolicy::Context context;
    context.lowResCurves = true;

    Rasterization::MeshRasterizerFacade facade;
    facade.applyCurveResolution(curves, context);

    for (const auto& curve : curves) {
        REQUIRE(curve.resIndex == Curve::resolutions - 1);
    }
}

TEST_CASE("MeshRasterizerFacade can skip snapshot publication", "[rasterization][facade]") {
    RasterizerData data;
    data.zeroIndex = 11;
    data.oneIndex = 13;

    std::vector<Intercept> intercepts {
        Intercept(0.f, 0.25f),
        Intercept(1.f, 0.75f),
    };

    Rasterization::RasterizerSnapshotSource source;
    source.intercepts = &intercepts;
    source.waveform.zeroIndex = 1;
    source.waveform.oneIndex = 2;

    Rasterization::MeshRasterizerFacade facade;
    facade.skipSnapshot(data, source);

    REQUIRE(data.intercepts.empty());
    REQUIRE(data.zeroIndex == 11);
    REQUIRE(data.oneIndex == 13);
}
