#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentMenu.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"

#include <algorithm>

namespace CycleV2 {

std::vector<TrimeshGuideAttachmentMenuItem> TrimeshGuideAttachmentMenu::itemsFor(
        const NodeGraph& graph,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) {
    std::vector<TrimeshGuideAttachmentMenuItem> items;
    const Node* meshNode = graph.findNode(meshNodeId);
    const auto targets = meshNode != nullptr
            ? TrimeshGuideAttachmentTarget::cubeTargetsForVertex(
                    *meshNode,
                    vertexIndex,
                    parameterField)
            : std::vector<TrimeshCubeComponentGuideTarget>();
    const bool anyAttached = std::any_of(targets.begin(), targets.end(), [&](const auto& target) {
        return std::any_of(
                graph.getGuideAssignments().begin(),
                graph.getGuideAssignments().end(),
                [&](const GuideCurveAssignment& assignment) {
                    return assignment.targets(meshNodeId, target);
                });
    });
    if (anyAttached) {
        items.push_back({ detachGuideMenuId, "detach", {}, false, true, false });
    }
    items.push_back({ newGuideMenuId, "new...", {}, true, false, false });
    int guideNumber {};
    for (const auto& guide : graph.getGuideCurves()) {
        ++guideNumber;
        bool attached {};
        for (const auto& target : targets) {
            const auto assignment = std::find_if(
                    graph.getGuideAssignments().begin(),
                    graph.getGuideAssignments().end(),
                    [&](const GuideCurveAssignment& candidate) {
                        return candidate.guideId == guide.id
                                && candidate.targets(meshNodeId, target);
                    });
            if (assignment != graph.getGuideAssignments().end()) {
                attached = true;
                break;
            }
        }

        items.push_back({
                firstGuideMenuId + guideNumber - 1,
                guide.shortLabel.isEmpty() ? String(guideNumber) : guide.shortLabel,
                guide.id,
                false,
                false,
                attached
        });
    }

    return items;
}

}
