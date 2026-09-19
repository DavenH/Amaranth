#include "Graph/GraphGuideValidator.h"

#include "Graph/GraphValidationTypes.h"
#include "Nodes/Guide/GuideAttachmentTarget.h"

namespace CycleV2 {

namespace {

void addInvalidAttachmentIssue(
        std::vector<GraphValidationIssue>& issues,
        const String& message) {
    issues.push_back({ GraphValidationCode::InvalidAttachmentDestination, message });
}

}

void GraphGuideValidator::validate(
        const NodeGraph& graph,
        std::vector<GraphValidationIssue>& issues) const {
    for (const auto& assignment : graph.getGuideAssignments()) {
        const GuideCurveResource* guide = graph.findGuideCurve(assignment.guideId);
        const Node* target = graph.findNode(assignment.targetNodeId);
        const bool validTarget = target != nullptr
                && (assignment.targetKind == GuideCurveTargetKind::EnvelopeCubeComponent
                        ? target->kind == NodeKind::Envelope
                        : target->kind == NodeKind::TrilinearMesh)
                && GuideAttachmentTarget::isValid(*target, assignment.target);
        if (guide == nullptr || target == nullptr || !validTarget) {
            addInvalidAttachmentIssue(
                    issues,
                    "Guide assignment references an invalid resource or cube component");
        }
    }

    for (const auto& guide : graph.getGuideCurves()) {
        if (guide.revision < 1
                || (guide.heatmapAssetId.isNotEmpty()
                        && graph.findGuideHeatmap(guide.heatmapAssetId) == nullptr)) {
            addInvalidAttachmentIssue(
                    issues,
                    "Guide resource references an invalid heatmap asset");
        }
    }
}

}
