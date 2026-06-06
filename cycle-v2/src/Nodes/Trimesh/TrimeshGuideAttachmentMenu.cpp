#include "TrimeshGuideAttachmentMenu.h"

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

    const String target = targetPortId(vertexIndex, parameterField);
    int guideNumber {};

    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::GuideCurve) {
            continue;
        }

        ++guideNumber;
        bool attached {};

        for (const auto& edge : graph.getEdges()) {
            if (edge.attachment
                    && edge.sourceNodeId == node.id
                    && edge.destNodeId == meshNodeId
                    && edge.destPortId == target) {
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

String TrimeshGuideAttachmentMenu::targetPortId(
        int vertexIndex,
        const String& parameterField) {
    return "guide.vertex." + String(vertexIndex) + "." + parameterField;
}

}
