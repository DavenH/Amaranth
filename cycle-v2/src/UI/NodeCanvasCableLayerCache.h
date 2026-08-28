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

class NodeCanvasCableLayerCache {
public:
    void beginFrame();
    NodeCanvasCableLayerCacheAccess access(
            const NodeSceneEdge& edge,
            const NodeCableStyle& style,
            Rectangle<int> logicalBounds,
            float zoom,
            float physicalScale);
    void draw(Graphics& graphics, const NodeCanvasCableLayerCacheAccess& access) const;
    NodeCanvasCableLayerCacheStats endFrame();

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

    std::vector<Entry> entries;
    uint64_t paintGeneration {};
    NodeCanvasCableLayerCacheStats frameStats;
};

}
