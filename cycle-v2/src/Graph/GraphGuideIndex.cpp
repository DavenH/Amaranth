#include <unordered_set>

#include "Graph/GraphGuideIndex.h"
#include "Graph/InteractionComplexityDiagnostics.h"

#include "Nodes/Guide/GuideHeatmapAsset.h"

namespace CycleV2 {

namespace {

template<typename Map, typename Key>
std::optional<size_t> findIndex(const Map& indexes, const Key& key) {
    const auto found = indexes.find(key);
    return found != indexes.end() ? std::optional<size_t>(found->second) : std::nullopt;
}

}

std::optional<size_t> GraphGuideIndex::resourceIndex(const String& guideId) const {
    return findIndex(resourceIndexes, guideId);
}

std::optional<size_t> GraphGuideIndex::heatmapIndex(const String& assetId) const {
    return findIndex(heatmapIndexes, assetId);
}

std::optional<size_t> GraphGuideIndex::assignmentIndex(
        const String& nodeId,
        const TrimeshCubeComponentGuideTarget& target) const {
    return findIndex(assignmentIndexes, TargetAddress { nodeId, target });
}

int GraphGuideIndex::usageCount(const String& guideId) const {
    const auto found = usageCounts.find(guideId);
    return found != usageCounts.end() ? found->second : 0;
}

const std::vector<String>& GraphGuideIndex::targetNodeIds(const String& guideId) const {
    static const std::vector<String> empty;
    const auto found = targetNodes.find(guideId);
    return found != targetNodes.end() ? found->second : empty;
}

const std::vector<String>& GraphGuideIndex::guideIdsForTargetNode(const String& nodeId) const {
    static const std::vector<String> empty;
    const auto found = targetNodeGuides.find(nodeId);
    return found != targetNodeGuides.end() ? found->second : empty;
}

void GraphGuideIndex::addResource(const String& guideId, size_t index) {
    resourceIndexes[guideId] = index;
}

void GraphGuideIndex::addHeatmap(const String& assetId, size_t index) {
    heatmapIndexes[assetId] = index;
}

void GraphGuideIndex::rebuildResources(const std::vector<GuideCurveResource>& resources) {
    resourceIndexes.clear();
    resourceIndexes.reserve(resources.size());
    for (size_t index = 0; index < resources.size(); ++index) {
        resourceIndexes[resources[index].id] = index;
    }
}

void GraphGuideIndex::rebuildHeatmaps(const std::vector<GuideHeatmapAssetPtr>& heatmaps) {
    heatmapIndexes.clear();
    heatmapIndexes.reserve(heatmaps.size());
    for (size_t index = 0; index < heatmaps.size(); ++index) {
        heatmapIndexes[heatmaps[index]->id()] = index;
    }
}

void GraphGuideIndex::rebuildAssignments(
        const std::vector<GuideCurveAssignment>& assignments) {
    InteractionComplexityDiagnostics::recordAssignmentLinearScan();
    assignmentIndexes.clear();
    usageCounts.clear();
    targetNodes.clear();
    targetNodeGuides.clear();
    assignmentIndexes.reserve(assignments.size());

    using StringSet = std::unordered_set<String, StringHash>;
    std::unordered_map<String, StringSet, StringHash> targetNodesByGuide;
    std::unordered_map<String, StringSet, StringHash> guidesByTargetNode;
    for (size_t index = 0; index < assignments.size(); ++index) {
        const GuideCurveAssignment& assignment = assignments[index];
        assignmentIndexes[{ assignment.targetNodeId, assignment.target }] = index;
        ++usageCounts[assignment.guideId];
        if (targetNodesByGuide[assignment.guideId]
                .insert(assignment.targetNodeId).second) {
            targetNodes[assignment.guideId].push_back(assignment.targetNodeId);
        }
        if (guidesByTargetNode[assignment.targetNodeId]
                .insert(assignment.guideId).second) {
            targetNodeGuides[assignment.targetNodeId].push_back(assignment.guideId);
        }
    }
}

size_t GraphGuideIndex::TargetAddressHash::operator()(const TargetAddress& value) const {
    size_t result = (size_t) value.nodeId.hashCode64();
    result ^= (size_t) value.target.cubeIndex + 0x9e3779b9U
            + (result << 6U) + (result >> 2U);
    result ^= (size_t) value.target.field + 0x9e3779b9U
            + (result << 6U) + (result >> 2U);
    return result;
}

}
