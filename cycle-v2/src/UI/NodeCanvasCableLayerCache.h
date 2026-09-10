#pragma once

#include <cstdint>
#include <vector>

#include <JuceHeader.h>

#include "UI/NodeCableRenderer.h"

namespace CycleV2 {

struct NodeCanvasCableLayerCacheAccess {
    Image* image {};
    Rectangle<float> logicalBounds;
    bool hit {};
};

struct NodeCanvasCableLayerCacheStats {
    uint64_t hits {};
    uint64_t misses {};
};

struct NodeCanvasCableLayerCacheFrame {
    NodeCanvasCableLayerCacheStats spriteStats;
    Image* compositeImage {};
    Rectangle<int> compositeBounds;
    bool compositeHit {};
    bool drawEntries {};
};

class NodeCanvasCableLayerCache {
public:
    void beginFrame(Rectangle<int> visibleBounds, float physicalScale);
    NodeCanvasCableLayerCacheAccess access(
            const NodeSceneEdge& edge,
            const NodeCableStyle& style,
            Rectangle<float> logicalBounds,
            float zoom,
            float physicalScale);
    NodeCanvasCableLayerCacheFrame endFrame();
    void drawComposite(
            Graphics& graphics,
            const NodeCanvasCableLayerCacheFrame& frame) const;

private:
    struct Entry {
        int edgeIndex { -1 };
        uint64_t geometryFingerprint {};
        NodeCableStyle styleSnapshot;
        Rectangle<float> logicalBounds;
        uint64_t paintGeneration {};
        uint64_t imageRevision {};
        float zoom {};
        float physicalScale {};
        bool destinationPortLike { true };
        bool modulationBundle {};
        bool destinationBundleIncludesYellow { true };
        bool sourceEndpointVisible { true };
        bool destinationEndpointVisible { true };
        Image image;

        bool matches(
                const NodeSceneEdge& edge,
                const NodeCableStyle& style,
                Rectangle<float> bounds,
                float currentZoom,
                float scale) const;
    };

    void replaceEntry(
            Entry& entry,
            const NodeSceneEdge& sceneEdge,
            const NodeCableStyle& style,
            Rectangle<float> logicalBounds,
            float zoom,
            float physicalScale);
    void rebuildComposite(Rectangle<int> bounds);
    void drawEntry(Graphics& graphics, const Entry& entry) const;
    Rectangle<int> frameCompositeBounds() const;
    bool compositeMatches(Rectangle<int> bounds) const;
    bool pendingLayoutMatches(Rectangle<int> bounds) const;
    void rememberPendingLayout(Rectangle<int> bounds);

    std::vector<Entry> entries;
    std::vector<size_t> frameEntryIndices;
    std::vector<int> compositeEdgeIndices;
    std::vector<Rectangle<float>> compositeEntryBounds;
    std::vector<uint64_t> compositeEntryRevisions;
    std::vector<int> pendingEdgeIndices;
    std::vector<Rectangle<float>> pendingEntryBounds;
    std::vector<uint64_t> pendingEntryRevisions;
    Rectangle<int> frameVisibleBounds;
    Rectangle<int> compositeBounds;
    Rectangle<int> pendingBounds;
    Image compositeImage;
    uint64_t paintGeneration {};
    float framePhysicalScale {};
    float compositePhysicalScale {};
    bool compositeInitialized {};
    bool pendingLayoutInitialized {};
    NodeCanvasCableLayerCacheStats frameStats;
};

}
