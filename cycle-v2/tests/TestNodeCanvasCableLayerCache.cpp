#include <catch2/catch_test_macros.hpp>

#include "UI/NodeCanvasCableLayerCache.h"

using namespace CycleV2;

namespace {

NodeSceneEdge makeSceneEdge(int edgeIndex = 4, float xOffset = 0.f) {
    NodeSceneEdge edge;
    edge.edgeIndex = edgeIndex;
    edge.source = { 20.f + xOffset, 40.f };
    edge.destination = { 80.f + xOffset, 120.f };
    edge.cablePath.startNewSubPath(edge.source);
    edge.cablePath.cubicTo(
            { 35.f + xOffset, 40.f },
            { 65.f + xOffset, 120.f },
            edge.destination);
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

    cache.beginFrame({ 0, 0, 220, 180 }, 1.f);
    const NodeCanvasCableLayerCacheAccess first = access();
    REQUIRE_FALSE(first.hit);
    Graphics cachedGraphics(*first.image);
    cachedGraphics.fillAll(Colours::red);
    const NodeCanvasCableLayerCacheFrame firstFrame = cache.endFrame();
    REQUIRE(firstFrame.spriteStats.misses == 1);
    REQUIRE_FALSE(firstFrame.compositeHit);
    Image output(Image::ARGB, 220, 180, true);
    Graphics outputGraphics(output);
    cache.drawComposite(outputGraphics, firstFrame);
    REQUIRE(output.getPixelAt(bounds.getCentreX(), bounds.getCentreY()) == Colours::red);

    cache.beginFrame({ 0, 0, 220, 180 }, 1.f);
    REQUIRE(access().hit);
    const NodeCanvasCableLayerCacheFrame hitFrame = cache.endFrame();
    REQUIRE(hitFrame.spriteStats.hits == 1);
    REQUIRE(hitFrame.compositeHit);

    cache.beginFrame({ 0, 0, 220, 180 }, 1.f);
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
    const NodeCanvasCableLayerCacheFrame changedFrame = cache.endFrame();
    REQUIRE(changedFrame.spriteStats.misses == 6);
    REQUIRE_FALSE(changedFrame.compositeHit);

    cache.beginFrame({ 0, 0, 220, 180 }, 2.f);
    REQUIRE_FALSE(access(0.72f, 2.f).hit);
    const NodeCanvasCableLayerCacheFrame scaledFrame = cache.endFrame();
    REQUIRE(scaledFrame.spriteStats.misses == 1);
    REQUIRE_FALSE(scaledFrame.compositeHit);

    cache.beginFrame({ 0, 0, 220, 180 }, 2.f);
    const NodeCanvasCableLayerCacheFrame emptyFrame = cache.endFrame();
    REQUIRE(emptyFrame.spriteStats.hits == 0);
    REQUIRE_FALSE(emptyFrame.compositeHit);
    cache.beginFrame({ 0, 0, 220, 180 }, 2.f);
    REQUIRE_FALSE(access(0.72f, 2.f).hit);
    REQUIRE(cache.endFrame().spriteStats.misses == 1);
}

TEST_CASE("Cable composite cache invalidates ordered visible membership",
        "[cycle-v2][canvas][performance][cache]") {
    NodeCanvasCableLayerCache cache;
    NodeSceneEdge first = makeSceneEdge(1, 0.f);
    NodeSceneEdge second = makeSceneEdge(2, 120.f);
    const Rectangle<int> firstBounds { 0, 20, 100, 120 };
    const Rectangle<int> secondBounds { 120, 20, 100, 120 };
    NodeCableStyle style { Colours::cyan, false, false, false, false };
    const Rectangle<int> visibleBounds { 0, 0, 240, 180 };
    const auto access = [&](const NodeSceneEdge& edge, Rectangle<int> bounds, Colour colour) {
        const NodeCanvasCableLayerCacheAccess result = cache.access(
                edge,
                style,
                bounds,
                0.58f,
                1.f);
        if (!result.hit) {
            Graphics spriteGraphics(*result.image);
            spriteGraphics.fillAll(colour);
        }
        return result;
    };

    cache.beginFrame(visibleBounds, 1.f);
    REQUIRE_FALSE(access(first, firstBounds, Colours::red).hit);
    REQUIRE_FALSE(access(second, secondBounds, Colours::blue).hit);
    const NodeCanvasCableLayerCacheFrame initial = cache.endFrame();
    REQUIRE_FALSE(initial.compositeHit);
    Image initialOutput(Image::ARGB, 240, 180, true);
    Graphics initialGraphics(initialOutput);
    cache.drawComposite(initialGraphics, initial);
    REQUIRE(initialOutput.getPixelAt(
            firstBounds.getCentreX(),
            firstBounds.getCentreY()) == Colours::red);
    REQUIRE(initialOutput.getPixelAt(
            secondBounds.getCentreX(),
            secondBounds.getCentreY()) == Colours::blue);

    cache.beginFrame(visibleBounds, 1.f);
    REQUIRE(access(first, firstBounds, Colours::red).hit);
    REQUIRE(access(second, secondBounds, Colours::blue).hit);
    REQUIRE(cache.endFrame().compositeHit);

    cache.beginFrame(visibleBounds, 1.f);
    REQUIRE(access(second, secondBounds, Colours::blue).hit);
    REQUIRE(access(first, firstBounds, Colours::red).hit);
    REQUIRE_FALSE(cache.endFrame().compositeHit);

    cache.beginFrame(visibleBounds, 1.f);
    REQUIRE(access(first, firstBounds, Colours::red).hit);
    const NodeCanvasCableLayerCacheFrame removed = cache.endFrame();
    REQUIRE_FALSE(removed.compositeHit);
    Image removedOutput(Image::ARGB, 240, 180, true);
    Graphics removedGraphics(removedOutput);
    cache.drawComposite(removedGraphics, removed);
    REQUIRE(removedOutput.getPixelAt(
            firstBounds.getCentreX(),
            firstBounds.getCentreY()) == Colours::red);
    REQUIRE(removedOutput.getPixelAt(
            secondBounds.getCentreX(),
            secondBounds.getCentreY()) == Colours::transparentBlack);

    cache.beginFrame({ 0, 0, 80, 180 }, 1.f);
    REQUIRE(access(first, firstBounds, Colours::red).hit);
    const NodeCanvasCableLayerCacheFrame clipped = cache.endFrame();
    REQUIRE_FALSE(clipped.compositeHit);
    REQUIRE(clipped.compositeBounds == Rectangle<int>(0, 20, 80, 120));
}
