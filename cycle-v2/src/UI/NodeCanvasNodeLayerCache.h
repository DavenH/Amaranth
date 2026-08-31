#pragma once

#include <cstdint>
#include <vector>

#include <JuceHeader.h>

#include "Graph/NodeGraph.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

struct NodeCanvasNodeLayerCacheAccess {
    Image* image {};
    Rectangle<int> logicalBounds;
    bool hit {};
};

struct NodeCanvasNodeLayerCacheStats {
    uint64_t hits {};
    uint64_t misses {};
};

class NodeCanvasNodeLayerCache {
public:
    void clear();
    void beginFrame();
    NodeCanvasNodeLayerCacheAccess access(
            const Node& node,
            Rectangle<int> logicalBounds,
            uint64_t viewportRevision,
            uint64_t previewResourceFingerprint,
            uint64_t renderContextFingerprint,
            const NodePreviewResult* runtimePreview,
            bool selected,
            float physicalScale);
    void draw(Graphics& graphics, const NodeCanvasNodeLayerCacheAccess& access) const;
    NodeCanvasNodeLayerCacheStats endFrame();

private:
    struct Entry {
        String nodeId;
        Node nodeSnapshot;
        NodePreviewResult runtimePreviewSnapshot;
        Rectangle<int> logicalBounds;
        uint64_t viewportRevision {};
        uint64_t previewResourceFingerprint {};
        uint64_t renderContextFingerprint {};
        uint64_t paintGeneration {};
        float physicalScale {};
        bool selected {};
        bool hasRuntimePreview {};
        Image image;

        bool matches(
                const Node& node,
                Rectangle<int> bounds,
                uint64_t currentViewportRevision,
                uint64_t resourceFingerprint,
                uint64_t contextFingerprint,
                const NodePreviewResult* runtimePreview,
                bool currentlySelected,
                float scale) const;
    };

    void replaceEntry(
            Entry& entry,
            const Node& node,
            Rectangle<int> logicalBounds,
            uint64_t viewportRevision,
            uint64_t previewResourceFingerprint,
            uint64_t renderContextFingerprint,
            const NodePreviewResult* runtimePreview,
            bool selected,
            float physicalScale);

    std::vector<Entry> entries;
    uint64_t paintGeneration {};
    NodeCanvasNodeLayerCacheStats frameStats;
};

}
