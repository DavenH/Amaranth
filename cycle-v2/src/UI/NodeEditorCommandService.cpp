#include "NodeEditorHost.h"

#include "NodeParameterValue.h"
#include "../Nodes/Effect2D/CurveNodeModels.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshMeshState.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"

namespace CycleV2 {

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
    return document.graph().findNode(nodeId);
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
    presentation.selectEditedNode(nodeId);
    commands.beginCompoundEdit();
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
    presentation.scheduleNodeEditorRefresh();
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endNodeParameterEdit() {
    commands.commitCompoundEdit();
    presentation.flushNodeEditorRefresh();
    activeParameterNodeId = {};
    activeParameterId = {};
    activeParameterLabel = {};
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
    presentation.repaintNodeEditor(false);
    return true;
}

bool NodeEditorCommandService::publishCurveState(
        const String& nodeId,
        const String& snapshot,
        uint64_t revision,
        const std::vector<NodeParameter>& controls) {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }
    const auto result = commands.publishCurveState({
            nodeId,
            CurveNodeModelCodec::revisionFromParameters(node->parameters),
            revision,
            snapshot,
            controls
    });
    if (!result.succeeded() || !result.changed) {
        return result.succeeded();
    }
    if (curveTransactionActive) {
        curvePublicationPending = true;
    } else {
        presentation.scheduleNodeEditorRefresh();
    }
    presentation.repaintNodeEditor(true);
    return true;
}

void NodeEditorCommandService::beginCurveTransaction() {
    curveTransactionActive = true;
    curvePublicationPending = false;
    commands.beginCompoundEdit();
}

void NodeEditorCommandService::commitCurveTransaction() {
    commands.commitCompoundEdit();
    curveTransactionActive = false;
    if (!curvePublicationPending) {
        return;
    }
    curvePublicationPending = false;
    presentation.scheduleNodeEditorRefresh();
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
    presentation.selectEditedNode(nodeId);
    commands.beginCompoundEdit();
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
    presentation.scheduleNodeEditorRefresh();
    presentation.setNodeEditorStatus("Morph " + label + " = " + String(value, 2));
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endTrimeshMorphEdit() {
    commands.commitCompoundEdit();
    presentation.flushNodeEditorRefresh();
    activeMorphNodeId = {};
    activeMorphParameterId = {};
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
    int vertexIndex = nodeParameterValue(*node, "selectedVertexIndex", "-1").getIntValue();
    commands.beginCompoundEdit();
    if (vertexIndex < 0) {
        vertexIndex = widget->resolvedSelectedVertexIndexForNode(*node);
        if (vertexIndex >= 0) {
            commands.setNodeParameter(nodeId, "selectedVertexIndex", "Selected Vertex", String(vertexIndex));
        }
    }
    if (vertexIndex < 0) {
        commands.cancelCompoundEdit();
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
    const auto publishResult = commands.setNodeParameter(
            activeVertexNodeId,
            TrimeshMeshState::parameterId(),
            "Mesh Topology",
            activeVertexWidget->currentMeshState());
    if (!publishResult.succeeded()) {
        return false;
    }
    presentation.setNodeEditorStatus(
            "Vertex #" + String(activeVertexIndex) + " " + label + " = " + String(value, 2));
    presentation.repaintNodeEditor(false);
    return true;
}

void NodeEditorCommandService::endTrimeshVertexParameterEdit() {
    const Node* node = findNode(activeVertexNodeId);
    bool published {};
    if (node != nullptr && activeVertexWidget != nullptr) {
        const auto result = commands.setNodeParameter(
                activeVertexNodeId,
                TrimeshMeshState::parameterId(),
                "Mesh Topology",
                activeVertexWidget->currentMeshState());
        published = result.succeeded();
    }
    if (published) {
        commands.commitCompoundEdit();
    } else {
        commands.cancelCompoundEdit();
    }
    presentation.flushNodeEditorRefresh();
    activeVertexNodeId = {};
    activeVertexParameterId = {};
    activeVertexWidget = nullptr;
    activeVertexIndex = -1;
}

void NodeEditorCommandService::persistTrimeshMeshEdits(const String& nodeId) {
    const Node* node = findNode(nodeId);
    TrimeshWidget* widget = node != nullptr ? resources.trimeshWidget(*node) : nullptr;
    if (node == nullptr || widget == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return;
    }
    commands.beginCompoundEdit();
    commands.setNodeParameter(
            nodeId,
            TrimeshMeshState::parameterId(),
            "Mesh Topology",
            widget->currentMeshState());
    commands.setNodeParameter(
            nodeId,
            "selectedVertexIndex",
            "Selected Vertex",
            String(widget->selectedVertexIndexForPanel()));
    commands.commitCompoundEdit();
    presentation.scheduleNodeEditorRefresh();
    presentation.repaintNodeEditor(false);
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
    if (nodeParameterValue(*node, "selectedVertexIndex", "-1").getIntValue() == vertexIndex) {
        return true;
    }
    const auto result = commands.setNodeParameter(
            nodeId, "selectedVertexIndex", "Selected Vertex", String(vertexIndex));
    if (!result.succeeded()) {
        return false;
    }
    presentation.refreshNodeEditorPresentation();
    presentation.selectEditedNode(nodeId);
    presentation.setNodeEditorStatus("Selected vertex #" + String(vertexIndex));
    return true;
}

}
