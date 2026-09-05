#pragma once

#include <cstdint>
#include <vector>

#include <JuceHeader.h>

#include "Graph/GraphRenderSemanticResolver.h"
#include "Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

struct SignalProbePreviewTileCacheAccess {
    Image* image {};
    Rectangle<int> logicalBounds;
    bool hit {};
};

struct SignalProbePreviewTileCacheStats {
    uint64_t hits {};
    uint64_t misses {};
};

class SignalProbePreviewTileCache {
public:
    void clear();
    void beginFrame();
    SignalProbePreviewTileCacheAccess access(
            const GraphPreviewResult::SignalProbePreview& preview,
            NodeRenderSemantic semantic,
            Rectangle<int> logicalBounds,
            float physicalScale);
    void draw(Graphics& graphics, const SignalProbePreviewTileCacheAccess& access) const;
    SignalProbePreviewTileCacheStats endFrame();

private:
    struct Entry {
        String probeId;
        GraphPreviewResult::SignalProbePreview previewSnapshot;
        NodeRenderSemantic semanticSnapshot;
        Rectangle<int> logicalBounds;
        uint64_t paintGeneration {};
        float physicalScale {};
        Image image;

        bool matches(
                const GraphPreviewResult::SignalProbePreview& preview,
                NodeRenderSemantic semantic,
                Rectangle<int> bounds,
                float scale) const;
    };

    void replaceEntry(
            Entry& entry,
            const GraphPreviewResult::SignalProbePreview& preview,
            NodeRenderSemantic semantic,
            Rectangle<int> logicalBounds,
            float physicalScale);

    std::vector<Entry> entries;
    uint64_t paintGeneration {};
    SignalProbePreviewTileCacheStats frameStats;
};

}
