#include "TrimeshGuideAttachmentMenu.h"
#include "TrimeshGuideAttachmentTarget.h"

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

    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::GuideCurve) {
            continue;
        }

        ++guideNumber;
        bool attached {};

        for (const auto& edge : graph.getEdges()) {
            if (edge.isProcessingAttachment()
                    && edge.attachmentType == AttachmentType::GuideCurve
                    && edge.sourceNodeId == node.id
                    && edge.destNodeId == meshNodeId
                    && std::find(targets.begin(), targets.end(), edge.destPortId)
                            != targets.end()) {
                attached = true;
                break;
            }
        }

        items.push_back({
                firstGuideMenuId + guideNumber - 1,
                String(guideNumber),
                node.id,
                false,
                attached
        });
    }

    return items;
}

}
