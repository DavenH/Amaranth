#pragma once

#include <cstdint>
#include <vector>

#include <JuceHeader.h>

#include "UI/NodeCableRenderer.h"

namespace CycleV2 {

struct NodeCanvasCableLayerCacheAccess {
    Image* image {};
    Rectangle<int> logicalBounds;
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
};

class NodeCanvasCableLayerCache {
public:
    void beginFrame(Rectangle<int> visibleBounds, float physicalScale);
    NodeCanvasCableLayerCacheAccess access(
            const NodeSceneEdge& edge,
            const NodeCableStyle& style,
            Rectangle<int> logicalBounds,
            float zoom,
            float physicalScale);
    NodeCanvasCableLayerCacheFrame endFrame();
    void drawComposite(
            Graphics& graphics,
            const NodeCanvasCableLayerCacheFrame& frame) const;

private:
    struct Entry {
        int edgeIndex { -1 };
        Point<float> source;
        Point<float> destination;
        Path cablePath;
        NodeCableStyle styleSnapshot;
        Rectangle<int> logicalBounds;
        uint64_t paintGeneration {};
        float zoom {};
        float physicalScale {};
        bool destinationPortLike { true };
        bool modulationBundle {};
        bool destinationBundleIncludesYellow { true };
        Image image;

        bool matches(
                const NodeSceneEdge& edge,
                const NodeCableStyle& style,
                Rectangle<int> bounds,
                float currentZoom,
                float scale) const;
    };

    void replaceEntry(
            Entry& entry,
            const NodeSceneEdge& sceneEdge,
            const NodeCableStyle& style,
            Rectangle<int> logicalBounds,
            float zoom,
            float physicalScale);
    void rebuildComposite(Rectangle<int> bounds);
    void drawEntry(Graphics& graphics, const Entry& entry) const;
    Rectangle<int> frameCompositeBounds() const;
    bool compositeMatches(Rectangle<int> bounds) const;

    std::vector<Entry> entries;
    std::vector<size_t> frameEntryIndices;
    std::vector<int> compositeEdgeIndices;
    Rectangle<int> frameVisibleBounds;
    Rectangle<int> compositeBounds;
    Image compositeImage;
    uint64_t paintGeneration {};
    float framePhysicalScale {};
    float compositePhysicalScale {};
    bool compositeInitialized {};
    NodeCanvasCableLayerCacheStats frameStats;
};

}
