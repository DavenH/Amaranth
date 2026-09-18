#include <algorithm>

#include "Nodes/Guide/GuideGraphEditor.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideHeatmapAsset.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"

namespace CycleV2 {

GraphEditResult GuideGraphEditor::createGuideCurve(NodeGraph& graph) const {
    int nextNumber = 1;
    while (graph.findGuideCurve("guide" + String(nextNumber)) != nullptr) {
        ++nextNumber;
    }

    GuideCurveResource guide;
    guide.id = "guide" + String(nextNumber);
    guide.shortLabel = "G" + String(nextNumber);
    guide.colourIndex = nextNumber - 1;
    guide.shelfOrder = (int) graph.getGuideCurves().size();
    guide.model = createDefaultGuideCurveModel();
    if (!graph.addGuideCurve(std::move(guide))) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }
    GraphEditResult result { GraphEditCode::Connected, "guide" + String(nextNumber), {} };
    result.changes.guidePresentationChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::duplicateGuideCurve(NodeGraph& graph, const String& guideId) const {
    const GuideCurveResource* source = graph.findGuideCurve(guideId);
    if (source == nullptr) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    const GuideCurveResource sourceCopy = *source;

    const GraphEditResult created = createGuideCurve(graph);
    if (!created.succeeded()) {
        return created;
    }

    GuideCurveResource* duplicate = graph.findGuideCurveForEditing(created.nodeId);
    jassert(duplicate != nullptr);
    if (duplicate == nullptr) {
        return { GraphEditCode::MissingNode, created.nodeId, {} };
    }
    duplicate->name = sourceCopy.name.isEmpty() ? "Copy" : sourceCopy.name + " Copy";
    duplicate->enabled = sourceCopy.enabled;
    duplicate->noise = sourceCopy.noise;
    duplicate->dcOffset = sourceCopy.dcOffset;
    duplicate->phase = sourceCopy.phase;
    duplicate->model = sourceCopy.model;
    duplicate->heatmapAssetId = sourceCopy.heatmapAssetId;
    duplicate->revision = 1;
    graph.markChanged();
    return created;
}

GraphEditResult GuideGraphEditor::reorderGuideCurve(
        NodeGraph& graph,
        const String& guideId,
        int shelfOrder) const {
    if (graph.findGuideCurve(guideId) == nullptr) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    if (!graph.moveGuideCurve(guideId, shelfOrder)) {
        return { GraphEditCode::Connected, guideId, {}, {}, false };
    }
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.guidePresentationChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::removeGuideCurve(NodeGraph& graph, const String& guideId) const {
    const std::vector<String> consumers = graph.guideTargetNodeIds(guideId);
    if (!graph.removeGuideCurve(guideId)) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.nodeIds = consumers;
    result.changes.guidesChanged = !consumers.empty();
    result.changes.guidePresentationChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::renameGuideCurve(
        NodeGraph& graph,
        const String& guideId,
        const String& name) const {
    GuideCurveResource* guide = graph.findGuideCurveForEditing(guideId);
    const String trimmedName = name.trim();
    if (guide == nullptr || trimmedName.isEmpty()) {
        return { GraphEditCode::InvalidParameterValue, guideId, {} };
    }
    if (guide->name == trimmedName) {
        return { GraphEditCode::Connected, guideId, {}, {}, false };
    }
    guide->name = trimmedName;
    ++guide->revision;
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.guidePresentationChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::replaceGuideCurve(
        NodeGraph& graph,
        const String& guideId,
        NodeModelStatePtr model,
        const std::vector<NodeParameter>& controls) const {
    GuideCurveResource* guide = graph.findGuideCurveForEditing(guideId);
    if (guide == nullptr || model == nullptr) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    const bool modelChanged = guide->model == nullptr
            || !guide->model->equals(*model);

    for (const auto& control : controls) {
        if (control.id == "enabled") {
            guide->enabled = control.value.getIntValue() != 0;
        } else if (control.id == "noise") {
            guide->noise = jlimit(0.f, 1.f, (float) control.value.getDoubleValue());
        } else if (control.id == "dcOffset") {
            guide->dcOffset = jlimit(0.f, 1.f, (float) control.value.getDoubleValue());
        } else if (control.id == "phase") {
            guide->phase = jlimit(0.f, 1.f, (float) control.value.getDoubleValue());
        }
    }
    guide->model = std::move(model);
    ++guide->revision;
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.nodeIds = graph.guideTargetNodeIds(guideId);
    result.changes.guidesChanged = !result.changes.nodeIds.empty();
    result.changes.guidePresentationChanged = true;
    result.changes.modelChanged = modelChanged;
    return result;
}

GraphEditResult GuideGraphEditor::setGuideHeatmap(
        NodeGraph& graph,
        const String& guideId,
        GuideHeatmapAssetPtr asset) const {
    GuideCurveResource* guide = graph.findGuideCurveForEditing(guideId);
    if (guide == nullptr) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    if (asset == nullptr || !graph.addGuideHeatmap(asset)) {
        return { GraphEditCode::InvalidTypedSnapshot, guideId, {} };
    }
    if (guide->heatmapAssetId == asset->id()) {
        return { GraphEditCode::Connected, guideId, {}, {}, false };
    }
    guide->heatmapAssetId = asset->id();
    ++guide->revision;
    graph.removeUnreferencedGuideHeatmaps();
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.nodeIds = graph.guideTargetNodeIds(guideId);
    result.changes.guidesChanged = !result.changes.nodeIds.empty();
    result.changes.guidePresentationChanged = true;
    result.changes.modelChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::clearGuideHeatmap(NodeGraph& graph, const String& guideId) const {
    GuideCurveResource* guide = graph.findGuideCurveForEditing(guideId);
    if (guide == nullptr) {
        return { GraphEditCode::MissingNode, guideId, {} };
    }
    if (guide->heatmapAssetId.isEmpty()) {
        return { GraphEditCode::Connected, guideId, {}, {}, false };
    }
    guide->heatmapAssetId.clear();
    ++guide->revision;
    graph.removeUnreferencedGuideHeatmaps();
    graph.markChanged();
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.nodeIds = graph.guideTargetNodeIds(guideId);
    result.changes.guidesChanged = !result.changes.nodeIds.empty();
    result.changes.guidePresentationChanged = true;
    result.changes.modelChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::assignGuideCurveToMeshComponent(
        NodeGraph& graph,
        const String& guideId,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) const {
    const GuideCurveResource* guide = graph.findGuideCurve(guideId);
    const Node* meshNode = graph.findNode(meshNodeId);
    if (guide == nullptr || meshNode == nullptr) {
        return { GraphEditCode::MissingNode, {}, {} };
    }
    if ((meshNode->kind != NodeKind::TrilinearMesh
                    && meshNode->kind != NodeKind::Envelope)
            || std::find(
                    TrimeshGuideAttachmentTarget::fields().begin(),
                    TrimeshGuideAttachmentTarget::fields().end(),
                    parameterField) == TrimeshGuideAttachmentTarget::fields().end()) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }

    const auto targets = MeshGuideAttachmentTarget::cubeTargetsForSelection(
            *meshNode, vertexIndex, parameterField);
    if (targets.empty()) {
        return { GraphEditCode::ValidationRejected, {}, {} };
    }
    for (const auto& componentTarget : targets) {
        const GuideCurveAssignment* existing = graph.guideAssignmentForTarget(
                meshNodeId, componentTarget);
        const GuideCurveTargetKind targetKind = meshNode->kind == NodeKind::Envelope
                ? GuideCurveTargetKind::EnvelopeCubeComponent
                : GuideCurveTargetKind::TrimeshCubeComponent;
        if ((existing == nullptr || existing->guideId != guideId)
                && !graph.assignGuideCurve({ guideId, meshNodeId, componentTarget, targetKind })) {
            return { GraphEditCode::ValidationRejected, {}, {} };
        }
    }
    GraphEditResult result { GraphEditCode::Connected, guideId, {} };
    result.changes.nodeIds.push_back(meshNodeId);
    result.changes.guidesChanged = true;
    result.changes.guidePresentationChanged = true;
    return result;
}

GraphEditResult GuideGraphEditor::detachGuideCurveFromMeshComponent(
        NodeGraph& graph,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) const {
    const Node* meshNode = graph.findNode(meshNodeId);
    if (meshNode == nullptr || (meshNode->kind != NodeKind::TrilinearMesh
                    && meshNode->kind != NodeKind::Envelope)) {
        return { GraphEditCode::MissingNode, meshNodeId, {} };
    }

    const auto targets = MeshGuideAttachmentTarget::cubeTargetsForSelection(
            *meshNode, vertexIndex, parameterField);
    if (targets.empty()) {
        return { GraphEditCode::ValidationRejected, meshNodeId, {} };
    }

    bool detached {};
    for (const auto& target : targets) {
        detached = graph.removeGuideAssignment(meshNodeId, target) || detached;
    }
    GraphEditResult result {
            detached ? GraphEditCode::Connected : GraphEditCode::ValidationRejected,
            meshNodeId,
            {}
    };
    if (detached) {
        result.changes.nodeIds.push_back(meshNodeId);
        result.changes.guidesChanged = true;
        result.changes.guidePresentationChanged = true;
    }
    return result;
}

GraphEditResult GuideGraphEditor::createGuideCurveAndAssignToMeshComponent(
        NodeGraph& graph,
        const String& meshNodeId,
        int vertexIndex,
        const String& parameterField) const {
    const auto created = createGuideCurve(graph);
    if (!created.succeeded()) {
        return created;
    }
    GraphEditResult result = assignGuideCurveToMeshComponent(
            graph, created.nodeId, meshNodeId, vertexIndex, parameterField);
    result.changes.guidePresentationChanged = result.succeeded();
    return result;
}

}
