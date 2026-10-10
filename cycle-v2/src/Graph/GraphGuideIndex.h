#pragma once

#include <optional>
#include <unordered_map>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphGuideIndex {
public:
    std::optional<size_t> resourceIndex(const String& guideId) const;
    std::optional<size_t> heatmapIndex(const String& assetId) const;
    std::optional<size_t> assignmentIndex(
            const String& nodeId,
            const TrimeshCubeComponentGuideTarget& target) const;

    int usageCount(const String& guideId) const;
    const std::vector<String>& targetNodeIds(const String& guideId) const;
    const std::vector<String>& guideIdsForTargetNode(const String& nodeId) const;

    void addResource(const String& guideId, size_t index);
    void addHeatmap(const String& assetId, size_t index);
    void rebuildResources(const std::vector<GuideCurveResource>& resources);
    void rebuildHeatmaps(const std::vector<GuideHeatmapAssetPtr>& heatmaps);
    void rebuildAssignments(const std::vector<GuideCurveAssignment>& assignments);

private:
    struct StringHash {
        size_t operator()(const String& value) const {
            return (size_t) value.hashCode64();
        }
    };

    struct TargetAddress {
        String nodeId;
        TrimeshCubeComponentGuideTarget target;

        bool operator==(const TargetAddress& other) const {
            return nodeId == other.nodeId && target == other.target;
        }
    };

    struct TargetAddressHash {
        size_t operator()(const TargetAddress& value) const;
    };

    std::unordered_map<String, size_t, StringHash> resourceIndexes;
    std::unordered_map<String, size_t, StringHash> heatmapIndexes;
    std::unordered_map<TargetAddress, size_t, TargetAddressHash> assignmentIndexes;
    std::unordered_map<String, int, StringHash> usageCounts;
    std::unordered_map<String, std::vector<String>, StringHash> targetNodes;
    std::unordered_map<String, std::vector<String>, StringHash> targetNodeGuides;
};

}
