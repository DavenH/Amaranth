#include "NodeEditorHost.h"

#include "NodeParameterValue.h"
#include "../Runtime/FingerprintBuilder.h"
#include "../Nodes/Effect2D/CurveNodeModels.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshMeshState.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"

namespace CycleV2 {

namespace {

var editorStateWithSelectedVertex(const Node& node, int vertexId) {
    auto result = std::make_unique<DynamicObject>();
    if (const auto* current = node.editorState.getDynamicObject()) {
        for (const auto& property : current->getProperties()) {
            result->setProperty(property.name, property.value);
        }
    }
    result->setProperty("selectedVertexId", vertexId);
    return var(result.release());
}

}

NodeEditorCommandService::NodeEditorCommandService(
        Component& ownerToUse,
        GraphDocument& documentToUse,
        GraphCommandDispatcher& commandsToUse,
        NodeEditorPresentation& presentationToUse,
        NodeEditorResources& resourcesToUse) :
        owner        (&ownerToUse)
    ,   document     (documentToUse)
    ,   commands     (commandsToUse)
    ,   presentation (presentationToUse)
    ,   resources    (resourcesToUse) {
}

const Node* NodeEditorCommandService::findNode(const String& nodeId) const {
    return commands.editingGraph().findNode(nodeId);
}

bool NodeEditorCommandService::beginNodeParameterEdit(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        float value) {
    if (findNode(nodeId) == nullptr) {
        return false;
    }
    activeParameterNodeId = nodeId;
    activeParameterId = parameterId;
    activeParameterLabel = label;
    activeParameterField = parameterId;
    activeParameterFingerprint = 0;
    activeParameterChanged = false;
    presentation.selectEditedNode(nodeId);
    commands.beginTransientEdit();
    return updateNodeParameterEditValue(value);
}

bool NodeEditorCommandService::updateNodeParameterEditValue(float value) {
    if (activeParameterNodeId.isEmpty() || activeParameterId.isEmpty()) {
        return false;
    }
    const auto result = commands.setNodeParameter(
            activeParameterNodeId,
            activeParameterId,
            activeParameterLabel,
            String(value, 6));
    if (!result.succeeded()) {
        return false;
    }
    if (!result.changed) {
        return true;
    }
    activeParameterFingerprint = static_cast<uint64_t>(String(value, 9).hashCode64());
    activeParameterChanged = true;
    presentation.recordNodeEditorMovement(
            activeParameterNodeId,
            activeParameterId,
            activeParameterFingerprint);
    presentation.repaintNodeEditor(false);
    return true;
}

bool NodeEditorCommandService::beginNodeParameterPairEdit(
        const String& nodeId,
        const String& firstParameterId,
        const String& firstLabel,
        float firstValue,
        const String& secondParameterId,
        const String& secondLabel,
        float secondValue) {
    if (findNode(nodeId) == nullptr) {
        return false;
    }
    activeParameterNodeId = nodeId;
    activeParameterId = firstParameterId;
    activeParameterLabel = firstLabel;
    secondaryParameterId = secondParameterId;
    secondaryParameterLabel = secondLabel;
    activeParameterField = firstParameterId + "+" + secondParameterId;
    activeParameterFingerprint = 0;
    activeParameterChanged = false;
    presentation.selectEditedNode(nodeId);
    commands.beginTransientEdit();
    return updateNodeParameterPairEditValues(firstValue, secondValue);
}

bool NodeEditorCommandService::updateNodeParameterPairEditValues(
        float firstValue,
        float secondValue) {
    if (activeParameterNodeId.isEmpty() || activeParameterId.isEmpty()
            || secondaryParameterId.isEmpty()) {
        return false;
    }
    const auto first = commands.setNodeParameter(
            activeParameterNodeId,
            activeParameterId,
            activeParameterLabel,
            String(firstValue, 6));
    const auto second = commands.setNodeParameter(
            activeParameterNodeId,
            secondaryParameterId,
            secondaryParameterLabel,
            String(secondValue, 6));
    if (!first.succeeded() || !second.succeeded()) {
        return false;
    }
    if (!first.changed && !second.changed) {
        return true;
    }
    const uint64_t fingerprint = FingerprintBuilder(
            static_cast<uint64_t>(String(firstValue, 9).hashCode64()))
            .add(String(secondValue, 9))
            .value();
    activeParameterFingerprint = fingerprint;
    activeParameterChanged = true;
    presentation.recordNodeEditorMovement(
            activeParameterNodeId,
            activeParameterField,
            fingerprint);
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endNodeParameterEdit() {
    if (activeParameterNodeId.isEmpty()) {
        return;
    }
    commands.commitTransientEdit();
    if (activeParameterChanged) {
        if (presentation.probeRefreshMode() == ProbeRefreshMode::LiveLatest) {
            presentation.commitNodeEditorLocalState(
                    activeParameterNodeId,
                    activeParameterField,
                    activeParameterFingerprint,
                    document.revision());
        } else {
            presentation.refreshNodeEditorPresentation();
        }
    }
    presentation.rebindNodeEditor();
    activeParameterNodeId = {};
    activeParameterId = {};
    activeParameterLabel = {};
    secondaryParameterId = {};
    secondaryParameterLabel = {};
    activeParameterField = {};
    activeParameterFingerprint = 0;
    activeParameterChanged = false;
}

bool NodeEditorCommandService::setNodeParameterValue(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        float value) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    const ParameterDefinition* definition =
            NodeDefinitionRegistry::instance().findParameter(node->kind, parameterId);
    const String serializedValue = definition != nullptr
                    && definition->type == ParameterType::Boolean
            ? String(value >= 0.5f ? 1 : 0)
            : String(value, 6);
    const auto result = commands.setNodeParameter(
            nodeId, parameterId, label, serializedValue);
    if (!result.succeeded()) {
        return false;
    }
    presentation.selectEditedNode(nodeId);
    presentation.refreshNodeEditorPresentation();
    presentation.rebindNodeEditor();
    presentation.repaintNodeEditor(false);
    return true;
}

bool NodeEditorCommandService::publishCurveState(
        const String& nodeId,
        NodeModelStatePtr model,
        const std::vector<NodeParameter>& controls) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    const auto result = commands.publishCurveState({
            nodeId,
            node->model != nullptr ? node->model->revision() : 0,
            model,
            controls
    });
    if (!result.succeeded() || !result.changed) {
        return result.succeeded();
    }
    if (curveTransactionActive) {
        curvePublicationPending = true;
        curvePublicationNodeId = nodeId;
        curvePublicationFingerprint = FingerprintBuilder()
                .add(model->schemaId())
                .add(model->revision())
                .value();
    } else {
        presentation.scheduleNodeEditorRefresh();
    }
    presentation.repaintNodeEditor(true);
    return true;
}

void NodeEditorCommandService::beginCurveTransaction() {
    curveTransactionActive = true;
    curvePublicationPending = false;
    curvePublicationNodeId = {};
    curvePublicationFingerprint = 0;
    commands.beginTransientEdit();
}

void NodeEditorCommandService::commitCurveTransaction() {
    commands.commitTransientEdit();
    curveTransactionActive = false;
    if (!curvePublicationPending) {
        return;
    }
    curvePublicationPending = false;
    if (presentation.probeRefreshMode() == ProbeRefreshMode::LiveLatest) {
        presentation.commitNodeEditorLocalState(
                curvePublicationNodeId,
                "curve",
                curvePublicationFingerprint,
                document.revision());
    } else {
        presentation.scheduleNodeEditorRefresh();
    }
    curvePublicationNodeId = {};
    curvePublicationFingerprint = 0;
    presentation.repaintNodeEditor(true);
}

bool NodeEditorCommandService::setTrimeshPrimaryAxisValue(
        const String& nodeId,
        const String& axis) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    if (nodeParameterValue(*node, "primaryAxis", "yellow") == axis) {
        return true;
    }
    const auto result = commands.setNodeParameter(nodeId, "primaryAxis", "Primary Axis", axis);
    if (!result.succeeded()) {
        return false;
    }
    presentation.selectEditedNode(nodeId);
    presentation.refreshNodeEditorPresentation();
    presentation.setNodeEditorStatus("Primary axis: " + axis);
    presentation.repaintNodeEditor(true);
    return true;
}

bool NodeEditorCommandService::toggleTrimeshLinkAxisValue(
        const String& nodeId,
        const String& axis) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    const String parameterId = "link." + axis;
    const String defaultValue = axis == "yellow" ? "1" : "0";
    const bool enabled = nodeParameterValue(*node, parameterId, defaultValue).getIntValue() != 0;
    const auto result = commands.setNodeParameter(
            nodeId, parameterId, "Link " + axis, enabled ? "0" : "1");
    if (!result.succeeded()) {
        return false;
    }
    presentation.selectEditedNode(nodeId);
    presentation.refreshNodeEditorPresentation();
    presentation.setNodeEditorStatus("Link " + axis + (enabled ? " disabled" : " enabled"));
    presentation.repaintNodeEditor(true);
    return true;
}

bool NodeEditorCommandService::beginTrimeshMorphEdit(
        const String& nodeId,
        const String& parameterId,
        float value) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    activeMorphNodeId = nodeId;
    activeMorphParameterId = parameterId;
    activeMorphFingerprint = 0;
    activeMorphChanged = false;
    presentation.selectEditedNode(nodeId);
    commands.beginTransientEdit();
    return updateTrimeshMorphEditValue(value);
}

bool NodeEditorCommandService::updateTrimeshMorphEditValue(float value) {
    const Node* node = findNode(activeMorphNodeId);
    if (node == nullptr || activeMorphParameterId.isEmpty()) {
        return false;
    }
    const String label = activeMorphParameterId.substring(0, 1).toUpperCase()
            + activeMorphParameterId.substring(1);
    const auto result = commands.setNodeParameter(
            activeMorphNodeId, activeMorphParameterId, label, String(value, 3));
    if (!result.succeeded()) {
        return false;
    }
    if (!result.changed) {
        return true;
    }
    activeMorphFingerprint = static_cast<uint64_t>(String(value, 9).hashCode64());
    activeMorphChanged = true;
    presentation.recordNodeEditorMovement(
            activeMorphNodeId,
            activeMorphParameterId,
            activeMorphFingerprint);
    presentation.rebindNodeEditorTransient();
    presentation.setNodeEditorStatus("Morph " + label + " = " + String(value, 2));
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endTrimeshMorphEdit() {
    if (activeMorphNodeId.isEmpty()) {
        return;
    }
    const Node* node = findNode(activeMorphNodeId);
    const String primaryAxis = node != nullptr
            ? nodeParameterValue(*node, "primaryAxis", "yellow")
            : String();
    const bool primaryAxisOnly = activeMorphParameterId == primaryAxis;
    commands.commitTransientEdit();
    if (activeMorphChanged && (primaryAxisOnly
            || presentation.probeRefreshMode() == ProbeRefreshMode::LiveLatest)) {
        presentation.commitNodeEditorLocalState(
                activeMorphNodeId,
                activeMorphParameterId,
                activeMorphFingerprint,
                document.revision());
    } else if (activeMorphChanged) {
        presentation.refreshNodeEditorPresentation();
    }
    activeMorphNodeId = {};
    activeMorphParameterId = {};
    activeMorphFingerprint = 0;
    activeMorphChanged = false;
}

bool NodeEditorCommandService::beginTrimeshVertexParameterEdit(
        const String& nodeId,
        const String& parameterId,
        float value) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return false;
    }
    int vertexIndex = (int) node->editorState.getProperty("selectedVertexId", -1);
    commands.beginTransientEdit();
    if (vertexIndex < 0) {
        vertexIndex = widget->resolvedSelectedVertexIndexForNode(*node);
        if (vertexIndex >= 0) {
            commands.setNodeEditorState(
                    nodeId, editorStateWithSelectedVertex(*node, vertexIndex));
        }
    }
    if (vertexIndex < 0) {
        commands.cancelTransientEdit();
        return false;
    }
    activeVertexNodeId = nodeId;
    activeVertexParameterId = parameterId;
    activeVertexWidget = widget;
    activeVertexIndex = vertexIndex;
    presentation.selectEditedNode(nodeId);
    return updateTrimeshVertexParameterEditValue(value);
}

bool NodeEditorCommandService::updateTrimeshVertexParameterEditValue(float value) {
    const Node* node = findNode(activeVertexNodeId);
    if (node == nullptr || activeVertexWidget == nullptr
            || activeVertexParameterId.isEmpty() || activeVertexIndex < 0) {
        return false;
    }
    String label = activeVertexParameterId.fromFirstOccurrenceOf(".", false, false);
    if (label.isEmpty()) {
        label = activeVertexParameterId;
    }
    if (!activeVertexWidget->setVertexParameter(
            activeVertexIndex,
            activeVertexParameterId,
            value)) {
        return false;
    }
    const uint64_t modelRevision = node->model != nullptr ? node->model->revision() : 0;
    const auto result = commands.replaceNodeModel(
            activeVertexNodeId,
            modelRevision,
            TrimeshNodeModelState::copyOf(
                    activeVertexWidget->currentMesh(), modelRevision + 1));
    if (!result.succeeded()) {
        return false;
    }
    if (!result.changed) {
        return true;
    }
    const uint64_t fingerprint = FingerprintBuilder()
            .add(activeVertexParameterId)
            .add(modelRevision + 1)
            .value();
    presentation.recordNodeEditorMovement(
            activeVertexNodeId,
            activeVertexParameterId,
            fingerprint);
    presentation.setNodeEditorStatus(
            "Vertex #" + String(activeVertexIndex) + " " + label + " = " + String(value, 2));
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endTrimeshVertexParameterEdit() {
    if (activeVertexNodeId.isEmpty()) {
        return;
    }
    if (findNode(activeVertexNodeId) != nullptr && activeVertexWidget != nullptr) {
        commands.commitTransientEdit();
    } else {
        commands.cancelTransientEdit();
    }
    presentation.flushNodeEditorRefresh();
    activeVertexNodeId = {};
    activeVertexParameterId = {};
    activeVertexWidget = nullptr;
    activeVertexIndex = -1;
}

void NodeEditorCommandService::persistTrimeshMeshEdits(
        const String& nodeId,
        bool gestureComplete) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return;
    }

    if (activeMeshNodeId != nodeId) {
        if (activeMeshNodeId.isNotEmpty()) {
            commands.cancelTransientEdit();
        }
        commands.beginTransientEdit();
        activeMeshNodeId = nodeId;
        activeMeshChanged = false;
        presentation.selectEditedNode(nodeId);
    }

    const int selectedVertex = widget->selectedVertexIndexForPanel();
    const uint64_t modelRevision = node->model != nullptr ? node->model->revision() : 0;
    const auto topology = commands.replaceNodeModel(
            nodeId,
            modelRevision,
            TrimeshNodeModelState::copyOf(widget->currentMesh(), modelRevision + 1));
    auto editor = std::make_unique<DynamicObject>();
    editor->setProperty("selectedVertexId", selectedVertex);
    const auto selection = commands.setNodeEditorState(nodeId, var(editor.release()));
    if (!topology.succeeded() || !selection.succeeded()) {
        commands.cancelTransientEdit();
        activeMeshNodeId = {};
        activeMeshChanged = false;
        return;
    }

    if (topology.changed || selection.changed) {
        activeMeshChanged = true;
        const uint64_t fingerprint = FingerprintBuilder()
                .add(modelRevision + 1)
                .add(static_cast<uint64_t>(selectedVertex))
                .value();
        presentation.recordNodeEditorMovement(nodeId, "mesh", fingerprint);
        presentation.repaintNodeEditor(true);
    }

    if (!gestureComplete) {
        return;
    }

    commands.commitTransientEdit();
    if (activeMeshChanged) {
        presentation.flushNodeEditorRefresh();
    }
    activeMeshNodeId = {};
    activeMeshChanged = false;
}

bool NodeEditorCommandService::showTrimeshGuideAttachmentMenu(
        const String& nodeId,
        const String& parameterField,
        Rectangle<int> targetScreenArea) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || owner == nullptr) {
        return false;
    }
    const int vertexIndex = widget->resolvedSelectedVertexIndexForNode(*node);
    const auto items = TrimeshGuideAttachmentMenu::itemsFor(
            document.graph(), nodeId, vertexIndex, parameterField);
    PopupMenu menu;
    for (const auto& item : items) {
        menu.addItem(item.menuId, item.label, true, item.attached);
    }
    auto safeOwner = owner;
    menu.showMenuAsync(
            PopupMenu::Options().withTargetScreenArea(targetScreenArea),
            [this, safeOwner, nodeId, vertexIndex, parameterField, items](int menuId) {
                if (safeOwner == nullptr || menuId == 0) {
                    return;
                }
                GraphEditResult result { GraphEditCode::ValidationRejected, {}, {} };
                if (menuId == TrimeshGuideAttachmentMenu::newGuideMenuId) {
                    result = commands.createAndAttachGuideCurve(
                            nodeId, vertexIndex, parameterField,
                            presentation.nodeEditorCreationPosition());
                } else {
                    for (const auto& item : items) {
                        if (item.menuId == menuId) {
                            result = commands.attachGuideCurve(
                                    item.guideNodeId, nodeId, vertexIndex, parameterField);
                            break;
                        }
                    }
                }
                if (!result.succeeded()) {
                    presentation.setNodeEditorStatus("Guide attachment failed");
                    presentation.repaintNodeEditor(false);
                    return;
                }
                presentation.selectEditedNode(result.nodeId.isEmpty() ? nodeId : result.nodeId);
                presentation.refreshNodeEditorPresentation();
                presentation.rebindNodeEditor();
                presentation.setNodeEditorStatus("Guide " + parameterField + " attached");
                presentation.repaintNodeEditor(false);
            });
    return true;
}

bool NodeEditorCommandService::selectTrimeshVertexIndex(
        const String& nodeId,
        int vertexIndex) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    if ((int) node->editorState.getProperty("selectedVertexId", -1) == vertexIndex) {
        return true;
    }
    const auto result = commands.setNodeEditorState(
            nodeId, editorStateWithSelectedVertex(*node, vertexIndex));
    if (!result.succeeded()) {
        return false;
    }
    presentation.refreshNodeEditorPresentation();
    presentation.selectEditedNode(nodeId);
    presentation.setNodeEditorStatus("Selected vertex #" + String(vertexIndex));
    return true;
}

}
