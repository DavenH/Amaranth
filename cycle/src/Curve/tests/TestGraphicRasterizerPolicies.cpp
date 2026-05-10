#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <App/MeshLibrary.h>
#include <Curve/Vertex.h>

#include "../Rasterization/Policies/Graphic/GraphicPolicies.h"

namespace {
    using Catch::Approx;
}

TEST_CASE("GraphicAxisPolicy exposes current morph axis as primary dimension", "[cycle][rasterization][graphic]") {
    Cycle::Rasterization::GraphicAxisPolicy policy;

    REQUIRE(policy.primaryViewDimension(Vertex::Time) == Vertex::Time);
    REQUIRE(policy.primaryViewDimension(Vertex::Red) == Vertex::Red);
    REQUIRE(policy.primaryViewDimension(Vertex::Blue) == Vertex::Blue);
}

TEST_CASE("GraphicMorphPositionPolicy applies scratch position for graphic layers", "[cycle][rasterization][graphic]") {
    Cycle::Rasterization::GraphicMorphPositionPolicy::Context context;
    context.panelMorph.time = 0.25f;
    context.panelMorph.red = 0.5f;
    context.panelMorph.blue = 0.75f;
    context.layerGroup = LayerGroups::GroupSpect;
    context.currentMorphAxis = Vertex::Red;
    context.scratchChannel = 2;
    context.scratchPosition = 0.875f;

    MorphPosition morph = Cycle::Rasterization::GraphicMorphPositionPolicy().resolve(context);

    REQUIRE(morph.time.getTargetValue() == Approx(0.875f));
    REQUIRE(morph.red.getTargetValue() == Approx(0.5f));
    REQUIRE(morph.blue.getTargetValue() == Approx(0.75f));
}

TEST_CASE("GraphicMorphPositionPolicy preserves time layer morph when axis is not time", "[cycle][rasterization][graphic]") {
    Cycle::Rasterization::GraphicMorphPositionPolicy::Context context;
    context.panelMorph.time = 0.25f;
    context.layerGroup = LayerGroups::GroupTime;
    context.currentMorphAxis = Vertex::Red;
    context.scratchChannel = 3;
    context.scratchPosition = 0.9f;

    MorphPosition morph = Cycle::Rasterization::GraphicMorphPositionPolicy().resolve(context);

    REQUIRE(morph.time.getTargetValue() == Approx(0.25f));
}
