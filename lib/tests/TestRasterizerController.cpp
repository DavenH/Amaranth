#include <catch2/catch_test_macros.hpp>

#include "../src/Curve/Rasterization/RasterizerController.h"

using namespace Rasterization;

TEST_CASE("RasterizerController dispatches optional providers", "[rasterization][controller]") {
    RasterizerController controller;
    std::vector<Intercept> intercepts;
    std::vector<Curve> curves;

    REQUIRE_FALSE(controller.calcCrossPoints());
    REQUIRE_FALSE(controller.clean(RasterizerRuntime()));
    REQUIRE_FALSE(controller.pad(intercepts, curves));

    bool calculated = false;
    bool cleaned = false;
    bool padded = false;
    bool processed = false;
    bool assigned = false;
    bool offsetUpdated = false;

    controller.setCrossPointProvider([&]() {
        calculated = true;
    });
    controller.setCleanupProvider([&](RasterizerRuntime) {
        cleaned = true;
    });
    controller.setPaddingProvider([&](std::vector<Intercept>&, std::vector<Curve>&) {
        padded = true;
    });
    controller.setProcessInterceptsProvider([&](std::vector<Intercept>&) {
        processed = true;
    });
    controller.setMeshAssignmentProvider([&](Mesh*) {
        assigned = true;
    });
    controller.setOffsetSeedsProvider([&](int layerSize, int tableSize) {
        offsetUpdated = layerSize == 3 && tableSize == 9;
    });

    REQUIRE(controller.calcCrossPoints());
    REQUIRE(controller.clean(RasterizerRuntime()));
    REQUIRE(controller.pad(intercepts, curves));
    REQUIRE(controller.processIntercepts(intercepts));
    REQUIRE(controller.updateOffsetSeeds(3, 9));
    controller.meshAssigned(nullptr);

    REQUIRE(calculated);
    REQUIRE(cleaned);
    REQUIRE(padded);
    REQUIRE(processed);
    REQUIRE(assigned);
    REQUIRE(offsetUpdated);

    controller.resetProviders();

    REQUIRE_FALSE(controller.calcCrossPoints());
    REQUIRE_FALSE(controller.clean(RasterizerRuntime()));
    REQUIRE_FALSE(controller.pad(intercepts, curves));
    REQUIRE_FALSE(controller.processIntercepts(intercepts));
    REQUIRE_FALSE(controller.updateOffsetSeeds(3, 9));
}

TEST_CASE("RasterizerController exposes optional dimension providers", "[rasterization][controller]") {
    RasterizerController controller;

    REQUIRE_FALSE(controller.hasNumDimensionsProvider());
    REQUIRE_FALSE(controller.hasCrossSectionAvailabilityProvider());
    REQUIRE_FALSE(controller.hasPrimaryViewDimensionProvider());

    controller.setNumDimensionsProvider([]() {
        return 2;
    });
    controller.setCrossSectionAvailabilityProvider([]() {
        return true;
    });
    controller.setPrimaryViewDimensionProvider([]() {
        return 4;
    });

    REQUIRE(controller.hasNumDimensionsProvider());
    REQUIRE(controller.hasCrossSectionAvailabilityProvider());
    REQUIRE(controller.hasPrimaryViewDimensionProvider());
    REQUIRE(controller.numDimensions() == 2);
    REQUIRE(controller.hasEnoughCubesForCrossSection());
    REQUIRE(controller.primaryViewDimension() == 4);
}
