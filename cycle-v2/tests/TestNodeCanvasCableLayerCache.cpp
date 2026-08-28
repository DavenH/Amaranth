#include <catch2/catch_test_macros.hpp>

#include "UI/NodeCanvasCableLayerCache.h"

using namespace CycleV2;

namespace {

NodeSceneEdge makeSceneEdge() {
    NodeSceneEdge edge;
    edge.edgeIndex = 4;
    edge.source = { 20.f, 40.f };
    edge.destination = { 180.f, 120.f };
    edge.cablePath.startNewSubPath(edge.source);
    edge.cablePath.cubicTo({ 70.f, 40.f }, { 130.f, 120.f }, edge.destination);
    return edge;
}

}

TEST_CASE("Cable layer cache reuses only complete presentation keys",
        "[cycle-v2][canvas][performance][cache]") {
    NodeCanvasCableLayerCache cache;
    NodeSceneEdge edge = makeSceneEdge();
    NodeCableStyle style { Colours::cyan, false, false, false, false };
    const Rectangle<int> bounds { 0, 20, 200, 120 };
    const auto access = [&](float zoom = 0.58f, float scale = 1.f) {
        return cache.access(edge, style, bounds, zoom, scale);
    };

    cache.beginFrame();
    const NodeCanvasCableLayerCacheAccess first = access();
    REQUIRE_FALSE(first.hit);
    Graphics cachedGraphics(*first.image);
    cachedGraphics.fillAll(Colours::red);
    Image output(Image::ARGB, 220, 180, true);
    Graphics outputGraphics(output);
    cache.draw(outputGraphics, first);
    REQUIRE(output.getPixelAt(bounds.getCentreX(), bounds.getCentreY()) == Colours::red);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    REQUIRE(access().hit);
    REQUIRE(cache.endFrame().hits == 1);

    cache.beginFrame();
    style.selected = true;
    REQUIRE_FALSE(access().hit);
    edge.destination.x += 1.f;
    REQUIRE_FALSE(access().hit);
    edge.cablePath.lineTo(edge.destination);
    REQUIRE_FALSE(access().hit);
    edge.destinationPortLike = false;
    REQUIRE_FALSE(access().hit);
    edge.destinationBundleIncludesYellow = false;
    REQUIRE_FALSE(access().hit);
    REQUIRE_FALSE(access(0.72f).hit);
    REQUIRE_FALSE(access(0.72f, 2.f).hit);
    REQUIRE(cache.endFrame().misses == 7);

    cache.beginFrame();
    REQUIRE(cache.endFrame().hits == 0);
    cache.beginFrame();
    REQUIRE_FALSE(access(0.72f, 2.f).hit);
    REQUIRE(cache.endFrame().misses == 1);
}
