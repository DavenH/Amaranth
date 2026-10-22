#include <algorithm>

#include "Graph/GraphMeshGuideRemap.h"
#include "Graph/NodeGraph.h"

#include <Curve/Mesh/MorphDimensions.h>

namespace CycleV2 {

void remapMeshGuideAssignments(NodeGraph& graph, const String& nodeId,
        const std::vector<int>& retainedCubeIndices, GraphEditResult& result,
        const MorphDimensions* retainedDimensions) {
    const auto& assignments = graph.getGuideAssignments();
    for (size_t index = 0; index < assignments.size(); ++index) {
        const auto& assignment = assignments[index];
        if (assignment.targetNodeId != nodeId) {
            continue;
        }
        const int axis = assignment.target.field == GuideCurveField::Red ? Vertex::Red
                : assignment.target.field == GuideCurveField::Blue ? Vertex::Blue : -1;
        const auto cell = std::find(retainedCubeIndices.begin(), retainedCubeIndices.end(),
                assignment.target.cubeIndex);
        const bool removed = (retainedDimensions != nullptr && axis >= 0
                        && !retainedDimensions->active(axis))
                || cell == retainedCubeIndices.end();
        const int newIndex = (int) std::distance(retainedCubeIndices.begin(), cell);
        if (removed || newIndex != assignment.target.cubeIndex) {
            result.changes.removedGuideAssignments.push_back({ index, assignment });
            if (!removed) {
                auto remapped = assignment;
                remapped.target.cubeIndex = newIndex;
                result.changes.addedGuideAssignments.push_back(std::move(remapped));
            }
        }
    }
    std::reverse(result.changes.removedGuideAssignments.begin(),
            result.changes.removedGuideAssignments.end());
    for (const auto& assignment : result.changes.removedGuideAssignments) {
        graph.removeGuideAssignment(nodeId, assignment.assignment.target);
    }
    for (const auto& assignment : result.changes.addedGuideAssignments) {
        graph.assignGuideCurve(assignment);
    }
    result.changes.guidesChanged = !result.changes.removedGuideAssignments.empty();
    result.changes.guidePresentationChanged = result.changes.guidesChanged;
}

}
