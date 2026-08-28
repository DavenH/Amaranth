#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphNodeFactory.h"
#include "UI/NodeCanvasNodeLayerCache.h"

using namespace CycleV2;

TEST_CASE("Node layer cache reuses only complete presentation keys",
        "[cycle-v2][canvas][performance][cache]") {
    NodeCanvasNodeLayerCache cache;
    Node node = GraphNodeFactory().createNode(NodeKind::Delay, "delay", { 20.f, 30.f });
    const Rectangle<int> bounds { 10, 20, 180, 140 };
    NodePreviewResult preview {
            "delay",
            PreviewModuleRole::Waveform,
            { 0.1f, 0.2f },
            { 0.3f },
            0,
            0,
            PortDomain::TimeSignal
    };
    preview.contentRevision = 1;
    const auto access = [&](
            uint64_t viewportRevision,
            uint64_t resourceFingerprint,
            uint64_t contextFingerprint,
            bool selected,
            float physicalScale = 1.f,
            const NodePreviewResult* runtimePreview = nullptr) {
        return cache.access(
                node,
                bounds,
                viewportRevision,
                resourceFingerprint,
                contextFingerprint,
                runtimePreview,
                selected,
                physicalScale);
    };

    cache.beginFrame();
    const NodeCanvasNodeLayerCacheAccess first = access(2, 3, 4, false);
    REQUIRE_FALSE(first.hit);
    Graphics cachedGraphics(*first.image);
    cachedGraphics.fillAll(Colours::red);
    Image output(Image::ARGB, 220, 200, true);
    Graphics outputGraphics(output);
    cache.draw(outputGraphics, first);
    REQUIRE(output.getPixelAt(bounds.getCentreX(), bounds.getCentreY()) == Colours::red);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    REQUIRE(access(2, 3, 4, false).hit);
    REQUIRE(cache.endFrame().hits == 1);

    node.parameters.front().value = "0.75";
    cache.beginFrame();
    REQUIRE_FALSE(access(2, 3, 4, false).hit);
    REQUIRE(cache.endFrame().misses == 1);

    cache.beginFrame();
    REQUIRE_FALSE(access(2, 3, 4, true).hit);
    REQUIRE_FALSE(access(5, 3, 4, true).hit);
    REQUIRE_FALSE(access(5, 7, 4, true).hit);
    REQUIRE_FALSE(access(5, 7, 8, true).hit);
    REQUIRE_FALSE(access(5, 7, 8, true, 2.f).hit);
    REQUIRE_FALSE(access(5, 7, 8, true, 2.f, &preview).hit);
    preview.secondary.front() = 0.9f;
    preview.contentRevision = 2;
    REQUIRE_FALSE(access(5, 7, 8, true, 2.f, &preview).hit);
    REQUIRE(cache.endFrame().misses == 7);

    cache.beginFrame();
    REQUIRE(cache.endFrame().hits == 0);
    cache.beginFrame();
    REQUIRE_FALSE(access(5, 7, 8, true, 2.f, &preview).hit);
    REQUIRE(cache.endFrame().misses == 1);
}

TEST_CASE("Node layer cache invalidates only the changed runtime preview",
        "[cycle-v2][canvas][performance][cache]") {
    NodeCanvasNodeLayerCache cache;
    Node first = GraphNodeFactory().createNode(NodeKind::Delay, "first", { 20.f, 30.f });
    Node second = GraphNodeFactory().createNode(NodeKind::Reverb, "second", { 240.f, 30.f });
    NodePreviewResult firstPreview { "first", PreviewModuleRole::Waveform, { 0.2f } };
    NodePreviewResult secondPreview { "second", PreviewModuleRole::ReverbSpectrogram, { 0.4f } };
    firstPreview.contentRevision = 1;
    secondPreview.contentRevision = 2;

    const auto access = [&](const Node& node, const NodePreviewResult& preview) {
        return cache.access(node, node.bounds.toNearestInt(), 1, 2, 3, &preview, false, 1.f);
    };

    cache.beginFrame();
    REQUIRE_FALSE(access(first, firstPreview).hit);
    REQUIRE_FALSE(access(second, secondPreview).hit);
    REQUIRE(cache.endFrame().misses == 2);

    firstPreview.primary.front() = 0.8f;
    firstPreview.contentRevision = 3;
    cache.beginFrame();
    REQUIRE_FALSE(access(first, firstPreview).hit);
    REQUIRE(access(second, secondPreview).hit);
    const NodeCanvasNodeLayerCacheStats stats = cache.endFrame();
    REQUIRE(stats.misses == 1);
    REQUIRE(stats.hits == 1);
}
