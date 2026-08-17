#include "TrimeshGuideAttachmentMenu.h"
#include "TrimeshGuideAttachmentTarget.h"

#include <algorithm>

namespace CycleV2 {

std::vector<TrimeshGuideAttachmentMenuItem> TrimeshGuideAttachmentMenu::itemsFor(
        const NodeGraph& graph,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) {
    std::vector<TrimeshGuideAttachmentMenuItem> items;
    items.push_back({
            newGuideMenuId,
            "new...",
            {},
            true,
            false
    });

    const Node* meshNode = graph.findNode(meshNodeId);
    const auto targets = meshNode != nullptr
            ? TrimeshGuideAttachmentTarget::cubePortIdsForVertex(
                    *meshNode,
                    vertexIndex,
                    parameterField)
            : std::vector<String>();
    int guideNumber {};
    for (const auto& guide : graph.getGuideCurves()) {
        ++guideNumber;
        bool attached {};
        for (const auto& targetPortId : targets) {
            const auto target = TrimeshGuideAttachmentTarget::parse(targetPortId);
            if (target.isValid()) {
                const auto field = TrimeshGuideAttachmentTarget::guideField(target.field);
                const auto assignment = std::find_if(
                        graph.getGuideAssignments().begin(),
                        graph.getGuideAssignments().end(),
                        [&](const GuideCurveAssignment& candidate) {
                            return candidate.guideId == guide.id
                                    && candidate.targets(
                                            meshNodeId,
                                            { target.cubeIndex, field });
                        });
                if (assignment != graph.getGuideAssignments().end()) {
                    attached = true;
                    break;
                }
            }
        }

        items.push_back({
                firstGuideMenuId + guideNumber - 1,
                guide.shortLabel.isEmpty() ? String(guideNumber) : guide.shortLabel,
                guide.id,
                false,
                attached
        });
    }

    return items;
}

}
