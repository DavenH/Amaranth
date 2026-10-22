#include "App/CycleV2AutomationWorkspaceCommands.h"

#include "App/NodeAuthoringAutomation.h"

#include "App/CycleV2AutomationProtocol.h"
#include "UI/NodeWorkspace.h"

#include <utility>

namespace CycleV2 {

using namespace juce;
using namespace AutomationProtocol;

CycleV2AutomationWorkspaceCommands::CycleV2AutomationWorkspaceCommands(
        NodeWorkspace& targetWorkspace,
        SnapshotProvider snapshotProvider,
        PathResolver pathResolver) :
        workspace(targetWorkspace)
    ,   snapshot(std::move(snapshotProvider))
    ,   resolvePath(std::move(pathResolver)) {
}

var CycleV2AutomationWorkspaceCommands::setPreviewController(const var& commandValue) {
    const auto valid = [](const var& value) {
        return (value.isInt() || value.isInt64()) && (int64) value >= 0 && (int64) value <= 127;
    };
    if (!valid(commandValue["controller"]) || !valid(commandValue["value"])) {
        return failedResult("setPreviewController", "controller and value must be integers in [0, 127]");
    }
    const auto channel = commandValue.getProperty("channel", 1);
    if ((!channel.isInt() && !channel.isInt64()) || (int64) channel != 1) {
        return failedResult("setPreviewController", "Preview controllers use audition channel 1");
    }
    String error;
    if (!workspace.setPreviewControllerForAutomation(
            (int) commandValue["controller"], (int) commandValue["value"], error)) {
        return failedResult("setPreviewController", error);
    }
    return okResult("setPreviewController", workspace.inspectPreviewControllersForAutomation());
}

var CycleV2AutomationWorkspaceCommands::openNodeEditor(const var& commandValue) {
    const String command = stringProperty(commandValue, "command", "openNodeEditor");
    const String nodeId = stringProperty(commandValue, "nodeId");

    if (nodeId.isEmpty()) {
        return failedResult(command, "Missing nodeId");
    }

    if (!workspace.openNodeEditorForAutomation(nodeId)) {
        return failedResult(command, "Could not open editor for node: " + nodeId);
    }

    var data = workspace.inspectNodeControlsForAutomation(nodeId);
    return okResult(command, data);
}

var CycleV2AutomationWorkspaceCommands::addNode(const var& commandValue) {
    const String kind = stringProperty(commandValue, "kind");
    const Point<float> position {
            floatProperty(commandValue, "x", 0.f),
            floatProperty(commandValue, "y", 0.f)
    };
    String nodeId;

    if (kind.isEmpty()) {
        return failedResult("addNode", "Missing kind");
    }

    if (!workspace.addNodeForAutomation(kind, position, nodeId)) {
        return failedResult("addNode", "Could not add node kind: " + kind);
    }

    var data = makeObject();
    auto* object = objectFor(data);
    object->setProperty("nodeId", nodeId);
    object->setProperty("kind", kind);
    return okResult("addNode", data);
}

var CycleV2AutomationWorkspaceCommands::moveNode(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const Point<float> position {
            floatProperty(commandValue, "x", 0.f),
            floatProperty(commandValue, "y", 0.f)
    };

    if (nodeId.isEmpty()) {
        return failedResult("moveNode", "Missing nodeId");
    }

    if (!workspace.moveNodeForAutomation(nodeId, position)) {
        return failedResult("moveNode", "Could not move node: " + nodeId);
    }

    return okResult("moveNode", snapshot());
}

var CycleV2AutomationWorkspaceCommands::connectPorts(const var& commandValue) {
    const String sourceNodeId = stringProperty(commandValue, "sourceNodeId");
    const String sourcePortId = stringProperty(commandValue, "sourcePortId");
    const String destNodeId = stringProperty(commandValue, "destNodeId");
    const String destPortId = stringProperty(commandValue, "destPortId");

    if (sourceNodeId.isEmpty() || sourcePortId.isEmpty() || destNodeId.isEmpty() || destPortId.isEmpty()) {
        return failedResult("connectPorts", "Missing source or destination port address");
    }

    if (!workspace.connectPortsForAutomation(sourceNodeId, sourcePortId, destNodeId, destPortId)) {
        return failedResult("connectPorts", "Could not connect "
                + sourceNodeId + "." + sourcePortId + " -> " + destNodeId + "." + destPortId);
    }

    return okResult("connectPorts", snapshot());
}

var CycleV2AutomationWorkspaceCommands::deleteNode(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");

    if (nodeId.isEmpty()) {
        return failedResult("deleteNode", "Missing nodeId");
    }

    if (!workspace.deleteNodeForAutomation(nodeId)) {
        return failedResult("deleteNode", "Could not delete node: " + nodeId);
    }

    return okResult("deleteNode", snapshot());
}

var CycleV2AutomationWorkspaceCommands::deleteEdge(const var& commandValue) {
    const int edgeIndex = intProperty(commandValue, "edgeIndex", intProperty(commandValue, "index", -1));

    if (!workspace.deleteEdgeForAutomation(edgeIndex)) {
        return failedResult("deleteEdge", "Could not delete edge index: " + String(edgeIndex));
    }

    return okResult("deleteEdge", snapshot());
}

var CycleV2AutomationWorkspaceCommands::deleteGuideCurve(const var& commandValue) {
    const String guideId = stringProperty(commandValue, "guideId");
    if (guideId.isEmpty()) {
        return failedResult("deleteGuideCurve", "Missing guideId");
    }
    if (!workspace.deleteGuideCurveForAutomation(guideId)) {
        return failedResult("deleteGuideCurve", "Could not delete Guide: " + guideId);
    }
    return okResult("deleteGuideCurve", snapshot());
}

var CycleV2AutomationWorkspaceCommands::loadGuideHeatmap(const var& commandValue) {
    const String guideId = stringProperty(commandValue, "guideId");
    const File file = resolvePath(stringProperty(commandValue, "path"));
    if (guideId.isEmpty() || !file.existsAsFile()) {
        return failedResult("loadGuideHeatmap", "Missing Guide or image path");
    }
    if (!workspace.loadGuideHeatmapForAutomation(guideId, file)) {
        return failedResult("loadGuideHeatmap", "Could not load Guide heatmap");
    }
    return okResult("loadGuideHeatmap", snapshot());
}

var CycleV2AutomationWorkspaceCommands::clearGuideHeatmap(const var& commandValue) {
    const String guideId = stringProperty(commandValue, "guideId");
    if (guideId.isEmpty() || !workspace.clearGuideHeatmapForAutomation(guideId)) {
        return failedResult("clearGuideHeatmap", "Could not clear Guide heatmap");
    }
    return okResult("clearGuideHeatmap", snapshot());
}

var CycleV2AutomationWorkspaceCommands::undo() {
    if (!workspace.undoForAutomation()) {
        return failedResult("undo", "Nothing to undo");
    }
    return okResult("undo", snapshot());
}

var CycleV2AutomationWorkspaceCommands::redo() {
    return workspace.redoForAutomation() ? okResult("redo", snapshot())
            : failedResult("redo", "Nothing to redo");
}

var CycleV2AutomationWorkspaceCommands::applyNodeEdits(const var& command) {
    NodeAuthoringEdits request;
    String error;
    if (!decodeNodeAuthoringEdits(command, request, error)) {
        return failedResult("applyNodeEdits", error);
    }
    const auto result = workspace.applyNodeEditsForAutomation(request);
    return nodeAuthoringResult(result, workspace.documentRevisionForAutomation());
}

var CycleV2AutomationWorkspaceCommands::setGuideParameter(const var& commandValue) {
    const String guideId = stringProperty(commandValue, "guideId");
    const String parameterId = stringProperty(commandValue, "parameterId");
    const String value = stringProperty(commandValue, "value");
    if (guideId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("setGuideParameter", "Missing guideId or parameterId");
    }
    if (!workspace.setGuideParameterForAutomation(guideId, parameterId, value)) {
        return failedResult("setGuideParameter", "Could not edit Guide: " + guideId);
    }
    return okResult("setGuideParameter", snapshot());
}

var CycleV2AutomationWorkspaceCommands::setNodeParameter(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String parameterId = stringProperty(commandValue, "parameterId");
    const String label = stringProperty(commandValue, "label", parameterId);
    const String value = stringProperty(commandValue, "value");

    if (nodeId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("setNodeParameter", "Missing nodeId or parameterId");
    }

    if (!workspace.setNodeParameterForAutomation(nodeId, parameterId, label, value)) {
        return failedResult("setNodeParameter", "Could not set parameter: " + nodeId + "." + parameterId);
    }

    return okResult("setNodeParameter", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2AutomationWorkspaceCommands::inspectNodeControls(const var& commandValue) const {
    const String nodeId = stringProperty(commandValue, "nodeId");

    if (nodeId.isEmpty()) {
        return failedResult("inspectNodeControls", "Missing nodeId");
    }

    var data = workspace.inspectNodeControlsForAutomation(nodeId);
    if (const auto* object = objectFor(data); object == nullptr || !(bool) object->getProperty("resolved")) {
        return failedResult("inspectNodeControls", "Unknown node: " + nodeId);
    }

    return okResult("inspectNodeControls", data);
}

var CycleV2AutomationWorkspaceCommands::setMorphSlider(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String axis = stringProperty(commandValue, "axis", stringProperty(commandValue, "parameterId"));
    const auto* commandObject = objectFor(commandValue);
    const float value = commandObject == nullptr
            ? 0.f
            : (float) (double) commandObject->getProperty("value");

    if (nodeId.isEmpty()) {
        return failedResult("setMorphSlider", "Missing nodeId");
    }
    if (axis.isEmpty()) {
        return failedResult("setMorphSlider", "Missing axis");
    }

    if (!workspace.setMorphSliderForAutomation(nodeId, axis, value)) {
        return failedResult("setMorphSlider", "Could not set morph slider " + nodeId + "." + axis);
    }

    return okResult("setMorphSlider", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2AutomationWorkspaceCommands::setPrimaryAxis(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String axis = stringProperty(commandValue, "axis");

    if (nodeId.isEmpty() || axis.isEmpty()) {
        return failedResult("setPrimaryAxis", "Missing nodeId or axis");
    }

    if (!workspace.setPrimaryAxisForAutomation(nodeId, axis)) {
        return failedResult("setPrimaryAxis", "Could not set primary axis: " + nodeId + "." + axis);
    }

    return okResult("setPrimaryAxis", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2AutomationWorkspaceCommands::toggleLink(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String axis = stringProperty(commandValue, "axis");

    if (nodeId.isEmpty() || axis.isEmpty()) {
        return failedResult("toggleLink", "Missing nodeId or axis");
    }

    if (!workspace.toggleLinkForAutomation(nodeId, axis)) {
        return failedResult("toggleLink", "Could not toggle link: " + nodeId + "." + axis);
    }

    return okResult("toggleLink", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2AutomationWorkspaceCommands::selectVertex(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const int vertexIndex = intProperty(commandValue, "vertexIndex", intProperty(commandValue, "index", -1));

    if (nodeId.isEmpty() || vertexIndex < 0) {
        return failedResult("selectVertex", "Missing nodeId or vertexIndex");
    }

    if (!workspace.selectVertexForAutomation(nodeId, vertexIndex)) {
        return failedResult("selectVertex", "Could not select vertex: " + nodeId + "#" + String(vertexIndex));
    }

    return okResult("selectVertex", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2AutomationWorkspaceCommands::setVertexParameter(const var& commandValue) {
    const String nodeId = stringProperty(commandValue, "nodeId");
    const String parameterId = stringProperty(commandValue, "parameterId");
    const float value = floatProperty(commandValue, "value", 0.f);

    if (nodeId.isEmpty() || parameterId.isEmpty()) {
        return failedResult("setVertexParameter", "Missing nodeId or parameterId");
    }

    if (!workspace.setVertexParameterForAutomation(nodeId, parameterId, value)) {
        return failedResult("setVertexParameter", "Could not set vertex parameter: " + nodeId + "." + parameterId);
    }

    return okResult("setVertexParameter", workspace.inspectNodeControlsForAutomation(nodeId));
}

var CycleV2AutomationWorkspaceCommands::explanation(const var& commandValue) {
    const String command = stringProperty(commandValue, "command");
    const var scopeValue = commandValue["scope"];
    const String scope = scopeValue.toString();
    const bool hasNode = commandValue.hasProperty("nodeId");
    const bool hasGroup = commandValue.hasProperty("groupId");
    ExplanationTarget target;
    if (scopeValue.isString() && scope == "preset" && !hasNode && !hasGroup) {
        target = { ExplanationScope::Preset, {} };
    } else if (scopeValue.isString() && scope == "node" && hasNode && !hasGroup
            && commandValue["nodeId"].isString() && commandValue["nodeId"].toString().isNotEmpty()) {
        target = { ExplanationScope::Node, commandValue["nodeId"].toString() };
    } else if (scopeValue.isString() && scope == "group" && hasGroup && !hasNode
            && commandValue["groupId"].isString() && commandValue["groupId"].toString().isNotEmpty()) {
        target = { ExplanationScope::Group, commandValue["groupId"].toString() };
    } else {
        return failedResult(command, "Use scope preset without IDs, node with nodeId, or group with groupId");
    }
    if (workspace.inspectExplanationForAutomation(target).isVoid()) {
        return failedResult(command, "Unknown explanation target");
    }
    if (command == "setExplanation") {
        const var text = commandValue["text"];
        if (!text.isString() || !workspace.setExplanationForAutomation(target, text.toString())) {
            return failedResult(command, "Explanation must be text of at most 16384 characters; target must be editable");
        }
    } else if (command == "setGroupInformationCard") {
        const var visible = commandValue["visible"];
        if (target.scope != ExplanationScope::Group || !visible.isBool()) {
            return failedResult(command, "Information cards require group scope and a boolean visible value");
        }
        bool succeeded = false;
        if (commandValue.hasProperty("offset")) {
            const var offset = commandValue["offset"];
            const var x = offset["x"];
            const var y = offset["y"];
            const auto numeric = [](const var& value) {
                return value.isDouble() || value.isInt() || value.isInt64();
            };
            if (!(bool) visible || offset.getDynamicObject() == nullptr || !numeric(x) || !numeric(y)
                    || !isValidInformationCardOffset((double) x, (double) y)) {
                return failedResult(command, "A visible card offset requires finite x/y within the supported world range");
            }
            succeeded = workspace.setGroupInformationCardForAutomation(target.id,
                    Point<float>((float) x, (float) y));
        } else {
            succeeded = workspace.setGroupInformationCardVisibleForAutomation(target.id, (bool) visible);
        }
        if (!succeeded) {
            return failedResult(command, "Could not change information card placement");
        }
    } else if (command == "openExplanationEditor"
            && !workspace.openExplanationEditorForAutomation(target)) {
        return failedResult(command, "Could not open explanation editor");
    }
    return okResult(command, workspace.inspectExplanationForAutomation(target));
}

}
