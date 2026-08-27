#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "UI/NodeCanvasNodeLayerCache.h"

using namespace CycleV2;

TEST_CASE("Node layer cache reuses only complete presentation keys",
        "[cycle-v2][canvas][performance][cache]") {
    NodeCanvasNodeLayerCache cache;
    Node node = GraphNodeFactory().createNode(NodeKind::Delay, "delay", { 20.f, 30.f });
    const Rectangle<int> bounds { 10, 20, 180, 140 };
    const auto access = [&](
            uint64_t presentationRevision,
            uint64_t viewportRevision,
            uint64_t resourceFingerprint,
            uint64_t contextFingerprint,
            bool selected,
            float physicalScale = 1.f) {
        return cache.access(
                node,
                bounds,
                presentationRevision,
                viewportRevision,
                resourceFingerprint,
                contextFingerprint,
                selected,
                physicalScale);
    };

    cache.beginFrame();
    const NodeCanvasNodeLayerCacheAccess first = access(1, 2, 3, 4, false);
    REQUIRE_FALSE(first.hit);
    Graphics cachedGraphics(*first.image);
    cachedGraphics.fillAll(Colours::red);
    Image output(Image::ARGB, 220, 200, true);
    Graphics outputGraphics(output);
    cache.draw(outputGraphics, first);
    REQUIRE(output.getPixelAt(bounds.getCentreX(), bounds.getCentreY()) == Colours::red);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    REQUIRE(access(1, 2, 3, 4, false).hit);
    REQUIRE(cache.endFrame().hits == 1);

    node.parameters.front().value = "0.75";
    cache.beginFrame();
    REQUIRE_FALSE(access(1, 2, 3, 4, false).hit);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    REQUIRE_FALSE(access(1, 2, 3, 4, true).hit);
    REQUIRE_FALSE(access(1, 5, 3, 4, true).hit);
    REQUIRE_FALSE(access(6, 5, 3, 4, true).hit);
    REQUIRE_FALSE(access(6, 5, 7, 4, true).hit);
    REQUIRE_FALSE(access(6, 5, 7, 8, true).hit);
    REQUIRE_FALSE(access(6, 5, 7, 8, true, 2.f).hit);
    REQUIRE(cache.endFrame().misses == 6);

    cache.beginFrame();
    REQUIRE(cache.endFrame().hits == 0);
    cache.beginFrame();
    REQUIRE_FALSE(access(6, 5, 7, 8, true, 2.f).hit);
    REQUIRE(cache.endFrame().misses == 1);
}
