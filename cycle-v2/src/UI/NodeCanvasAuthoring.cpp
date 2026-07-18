#include "NodeCanvasAuthoring.h"

#include "NodeViewModule.h"

#include <unordered_map>
#include <unordered_set>

namespace CycleV2 {

namespace {

bool operationLayoutSupported(NodeKind kind) {
    return NodeViewModuleRegistry::instance()
            .moduleFor(kind)
            .capabilities()
            .operationLayoutControl;
}

bool outputSideControlSupported(NodeKind kind) {
    return NodeViewModuleRegistry::instance()
            .moduleFor(kind)
            .capabilities()
            .outputSideControl;
}

bool hasEditor(NodeKind kind) {
    const auto& capabilities = NodeViewModuleRegistry::instance().moduleFor(kind).capabilities();
    return capabilities.previewable || capabilities.hostedEditor;
}

String parameterValue(const Node& node, const String& parameterId) {
    for (const auto& parameter : node.parameters) {
        if (parameter.id == parameterId) {
            return parameter.value;
        }
    }

    return {};
}

}

NodeCanvasAuthoring::NodeCanvasAuthoring(
        GraphDocument& documentToUse,
        GraphCommandDispatcher& commandsToUse,
        GraphPresentationModel& presentationToUse,
        NodeEditorCommands& editorCommandsToUse) :
        document(documentToUse)
    ,   commands(commandsToUse)
    ,   presentation(presentationToUse)
    ,   editorCommands(editorCommandsToUse) {
}

void NodeCanvasAuthoring::setSession(NodeCanvasAuthoringSession sessionToUse) {
    authoringSession = std::move(sessionToUse);
}

void NodeCanvasAuthoring::selectNode(const String& nodeId) {
    authoringSession.selectedNodeId = nodeId;
    authoringSession.selectedEdgeIndex = -1;
}

void NodeCanvasAuthoring::selectEdge(int edgeIndex) {
    authoringSession.selectedNodeId = {};
    authoringSession.selectedEdgeIndex = edgeIndex;
}

bool NodeCanvasAuthoring::clearSelection() {
    if (authoringSession.selectedNodeId.isEmpty()
            && authoringSession.expandedNodeId.isEmpty()
            && authoringSession.selectedEdgeIndex < 0) {
        return false;
    }

    clearDocumentSelection();
    return true;
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::saveGraph(const File& file) {
    const bool saved = document.save(file);
    return handledResult(saved, saved ? "Saved " + file.getFileName() : "Save failed", { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::loadGraph(const File& file) {
    if (!document.load(file)) {
        return handledResult(false, "Open failed", { true });
    }

    clearDocumentSelection();
    refreshPresentation();
    return restoredDocumentResult("Opened " + file.getFileName());
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::saveSnapshot(const File& file) {
    const bool saved = document.save(file);
    return handledResult(saved, saved ? "Saved snapshot" : "Save failed", { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::loadSnapshot(const File& file) {
    if (!file.existsAsFile()) {
        return handledResult(false, "No snapshot", { true });
    }
    if (!document.load(file)) {
        return handledResult(false, "Load failed", { true });
    }

    clearDocumentSelection();
    refreshPresentation();
    return restoredDocumentResult("Loaded snapshot");
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::undo() {
    if (!document.undo()) {
        return handledResult(false, "Nothing to undo", { true });
    }

    return restoreGraphXml({}, "Undo");
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::redo() {
    if (!document.redo()) {
        return handledResult(false, "Nothing to redo", { true });
    }

    return restoreGraphXml({}, "Redo");
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::restoreGraphXml(
        const String& xml,
        const String& statusMessage) {
    if (xml.isNotEmpty() && !document.loadXml(xml, false)) {
        return handledResult(false, "Restore failed", { true });
    }

    clearDocumentSelection();
    refreshPresentation();
    return restoredDocumentResult(statusMessage);
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::openEditor(const String& nodeId) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || !hasEditor(node->kind)) {
        return {};
    }

    selectNode(nodeId);
    authoringSession.expandedNodeId = nodeId;
    return handledResult(true, "Opened editor: " + nodeId, { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::addNode(NodeKind kind, Point<float> position) {
    const auto edit = commands.addNode(kind, position);
    if (!edit.succeeded()) {
        return graphEditResult(edit, {}, {});
    }

    authoringSession.expandedNodeId = {};
    return graphEditResult(edit, "Node added: " + edit.nodeId, edit.nodeId, { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::moveNode(
        const String& nodeId,
        Point<float> position) {
    if (findNode(nodeId) == nullptr) {
        return {};
    }

    return graphEditResult(
            commands.moveNode(nodeId, position),
            "Node moved: " + nodeId,
            nodeId,
            { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::toggleSignalProbe(
        int edgeIndex,
        float tapPosition) {
    if (edgeIndex < 0 || edgeIndex >= (int) document.graph().getEdges().size()) {
        return {};
    }
    return graphEditResult(
            commands.toggleSignalProbe((size_t) edgeIndex, tapPosition),
            "Signal probe toggled",
            {},
            { true, false, true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::removeSignalProbe(const String& probeId) {
    return graphEditResult(
            commands.removeSignalProbe(probeId),
            "Signal probe removed",
            {},
            { true, false, true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::reattachSignalProbe(
        const String& probeId,
        int edgeIndex,
        float tapPosition) {
    if (edgeIndex < 0 || edgeIndex >= (int) document.graph().getEdges().size()) {
        return {};
    }
    return graphEditResult(
            commands.reattachSignalProbe(probeId, (size_t) edgeIndex, tapPosition),
            "Signal probe reattached",
            {},
            { true, false, true });
}

void NodeCanvasAuthoring::beginNodeMoveGesture() {
    commands.beginCompoundEdit();
}

bool NodeCanvasAuthoring::resizeNodeDuringGesture(
        const String& nodeId,
        Rectangle<float> bounds) {
    return commands.resizeNode(nodeId, bounds).succeeded();
}

void NodeCanvasAuthoring::commitNodeMoveGesture() {
    commands.commitCompoundEdit();
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::connectPorts(
        const PortAddress& source,
        const PortAddress& destination) {
    const auto edit = commands.connect(source, destination);
    return graphEditResult(
            edit,
            "Connected " + source.nodeId + "." + source.portId
                    + " -> " + destination.nodeId + "." + destination.portId,
            destination.nodeId,
            { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::deleteNode(const String& nodeId) {
    const auto edit = commands.removeNode(nodeId);
    if (!edit.succeeded()) {
        return graphEditResult(edit, {}, {});
    }

    const bool editorClosed = authoringSession.expandedNodeId == nodeId;
    clearDocumentSelection();
    return graphEditResult(edit, "Node deleted: " + nodeId, {}, { true, editorClosed, true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::deleteEdge(int edgeIndex) {
    if (edgeIndex < 0) {
        return {};
    }

    const auto edit = commands.removeEdgeAt((size_t) edgeIndex);
    return graphEditResult(edit, "Edge deleted: " + String(edgeIndex), {}, { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::setNodeParameter(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    if (findNode(nodeId) == nullptr || parameterId.isEmpty()) {
        return {};
    }

    return graphEditResult(
            commands.setNodeParameter(
                    nodeId,
                    parameterId,
                    label.isEmpty() ? parameterId : label,
                    value),
            "Parameter set: " + nodeId + "." + parameterId,
            nodeId,
            { true, true });
}

bool NodeCanvasAuthoring::getNodeParameter(
        const String& nodeId,
        const String& parameterId,
        String& value) const {
    const Node* node = findNode(nodeId);
    if (node == nullptr) {
        return false;
    }

    for (const auto& parameter : node->parameters) {
        if (parameter.id == parameterId) {
            value = parameter.value;
            return true;
        }
    }

    return false;
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::setTrimeshMorph(
        const String& nodeId,
        const String& axis,
        float value) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh
            || (axis != "yellow" && axis != "red" && axis != "blue")) {
        return {};
    }

    authoringSession.expandedNodeId = nodeId;
    if (!editorCommands.beginTrimeshMorphEdit(nodeId, axis, jlimit(0.f, 1.f, value))) {
        return handledResult(false, {}, { false, true, false });
    }

    editorCommands.endTrimeshMorphEdit();
    selectNode(nodeId);
    return handledResult(true, "Morph set: " + nodeId + "." + axis, { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::setTrimeshPrimaryAxis(
        const String& nodeId,
        const String& axis) {
    if (findNode(nodeId) == nullptr) {
        return {};
    }

    authoringSession.expandedNodeId = nodeId;
    const bool changed = editorCommands.setTrimeshPrimaryAxisValue(nodeId, axis);
    return handledResult(changed, changed ? "Primary axis: " + axis : String(), { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::toggleTrimeshLink(
        const String& nodeId,
        const String& axis) {
    if (findNode(nodeId) == nullptr) {
        return {};
    }

    authoringSession.expandedNodeId = nodeId;
    const bool changed = editorCommands.toggleTrimeshLinkAxisValue(nodeId, axis);
    return handledResult(changed, changed ? "Link toggled: " + axis : String(), { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::selectTrimeshVertex(
        const String& nodeId,
        int vertexIndex) {
    if (findNode(nodeId) == nullptr) {
        return {};
    }

    authoringSession.expandedNodeId = nodeId;
    const bool selected = editorCommands.selectTrimeshVertexIndex(nodeId, vertexIndex);
    return handledResult(selected, {}, { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::setTrimeshVertexParameter(
        const String& nodeId,
        const String& parameterId,
        float value) {
    if (findNode(nodeId) == nullptr) {
        return {};
    }

    authoringSession.expandedNodeId = nodeId;
    if (!editorCommands.beginTrimeshVertexParameterEdit(
            nodeId,
            parameterId,
            jlimit(0.f, 1.f, value))) {
        return handledResult(false, {}, { false, true, false });
    }

    editorCommands.endTrimeshVertexParameterEdit();
    selectNode(nodeId);
    return handledResult(true, "Vertex parameter set: " + parameterId, { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::spliceSelectedNodeIntoEdge(int edgeIndex) {
    const Node* node = findNode(authoringSession.selectedNodeId);
    const auto& edges = document.graph().getEdges();
    if (node == nullptr || edgeIndex < 0 || edgeIndex >= (int) edges.size()) {
        return {};
    }

    const String selectedNodeId = node->id;
    const Edge originalEdge = edges[(size_t) edgeIndex];
    commands.beginCompoundEdit();
    const auto edit = commands.spliceNodeIntoEdge((size_t) edgeIndex, selectedNodeId);

    if (!edit.succeeded()) {
        commands.cancelCompoundEdit();
        const String status = edit.code == GraphEditCode::ValidationRejected
                ? "Cable insert incompatible"
                : String();
        return graphEditResult(edit, status, selectedNodeId);
    }

    spaceNodesAfterSplice(originalEdge.sourceNodeId, edit.nodeId, originalEdge.destNodeId);
    commands.commitCompoundEdit();

    authoringSession.expandedNodeId = {};
    return graphEditResult(
            edit,
            "Inserted node into cable",
            edit.nodeId,
            { true, true, false });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::cycleOperationPortLayout(const String& nodeId) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || !operationLayoutSupported(node->kind)
            || node->inputs.size() < 2 || node->outputs.empty()) {
        return {};
    }

    const OperationPortLayout layout = operationPortLayout(*node);
    const auto edit = commands.editNodePresentation(nodeId, [layout](Node& edited) {
        switch (layout) {
            case OperationPortLayout::Side:
                edited.inputs[0].side = PortSide::Left;
                edited.inputs[1].side = PortSide::Top;
                break;
            case OperationPortLayout::Uptack:
                edited.inputs[0].side = PortSide::Top;
                edited.inputs[1].side = PortSide::Bottom;
                break;
            case OperationPortLayout::Vertical:
                edited.inputs[0].side = PortSide::Left;
                edited.inputs[1].side = PortSide::Bottom;
                break;
            case OperationPortLayout::Tee:
                edited.inputs[0].side = PortSide::Left;
                edited.inputs[1].side = PortSide::Left;
                break;
        }
        edited.outputs[0].side = PortSide::Right;
    });

    return graphEditResult(edit, {}, nodeId, { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::cycleMeshOutputSide(const String& nodeId) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || !outputSideControlSupported(node->kind) || node->outputs.empty()) {
        return {};
    }

    const PortSide side = nextOutputSide(*node);
    const auto edit = commands.editNodePresentation(nodeId, [side](Node& edited) {
        edited.outputs[0].side = side;
    });
    return graphEditResult(edit, {}, nodeId, { true });
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::cycleVoiceDomain(const String& nodeId) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::VoiceContext) {
        return {};
    }

    const String domain = VoiceContextCompactEditor::nextDomain(*node);
    return setVoiceContextParameter(
            nodeId,
            "domain",
            "Start Domain",
            domain,
            "Voice start domain: " + domain);
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::applyVoiceContextEdit(
        const String& nodeId,
        const VoiceContextEdit& edit) {
    switch (edit.control) {
        case VoiceContextEdit::Control::Domain:
            return setVoiceContextParameter(nodeId, "domain", "Start Domain", edit.value,
                    "Voice start domain: " + edit.value);
        case VoiceContextEdit::Control::Octave:
            return setVoiceContextParameter(nodeId, "octave", "Octave", edit.value,
                    "Octave: " + edit.value);
        case VoiceContextEdit::Control::Pitch:
            return setVoiceContextParameter(nodeId, "pitch", "Pitch", edit.value,
                    "Pitch: " + edit.value);
        case VoiceContextEdit::Control::Portamento:
            return setVoiceContextParameter(nodeId, "portamento", "Portamento", edit.value,
                    edit.value == "1" ? "Portamento on" : "Portamento off");
        case VoiceContextEdit::Control::Oversampling:
            return setVoiceContextParameter(nodeId, "oversampling", "Oversampling", edit.value,
                    "Oversampling: " + edit.value);
    }

    return {};
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::setTransformMode(
        const String& nodeId,
        TransformMode mode) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || !TransformCompactEditor::supports(node->kind)) {
        return {};
    }

    const String value = TransformCompactEditor::parameterValue(mode);
    if (parameterValue(*node, "mode") == value) {
        return handledResult(true, TransformCompactEditor::status(node->kind, mode), { true });
    }

    commands.beginCompoundEdit();
    const auto parameterEdit = commands.setNodeParameter(nodeId, "mode", "Mode", value);
    if (!parameterEdit.succeeded()) {
        commands.cancelCompoundEdit();
        return graphEditResult(parameterEdit, {}, {});
    }

    const NodeKind kind = node->kind;
    const auto presentationEdit = commands.editNodePresentation(nodeId, [kind, mode](Node& edited) {
        edited.subtitle = TransformCompactEditor::subtitle(kind, mode);
    });
    if (!presentationEdit.succeeded()) {
        commands.cancelCompoundEdit();
        return graphEditResult(presentationEdit, {}, {});
    }

    commands.commitCompoundEdit();
    selectNode(nodeId);
    refreshPresentation();
    authoringSession.statusMessage = TransformCompactEditor::status(kind, mode);
    return { true, true, true, GraphEditCode::Connected, nodeId, { true, false, true } };
}

NodeCanvasAuthoring::OperationPortLayout NodeCanvasAuthoring::operationPortLayout(const Node& node) {
    if (node.inputs.size() < 2) {
        return OperationPortLayout::Side;
    }

    const PortSide first = node.inputs[0].side;
    const PortSide second = node.inputs[1].side;
    if (first == PortSide::Left && second == PortSide::Bottom) {
        return OperationPortLayout::Tee;
    }
    if (first == PortSide::Top && second == PortSide::Bottom) {
        return OperationPortLayout::Vertical;
    }
    if (first == PortSide::Left && second == PortSide::Top) {
        return OperationPortLayout::Uptack;
    }

    return OperationPortLayout::Side;
}

NodeCanvasAuthoring::OperationPortLayout NodeCanvasAuthoring::nextOperationPortLayout(
        OperationPortLayout layout) {
    switch (layout) {
        case OperationPortLayout::Side:     return OperationPortLayout::Uptack;
        case OperationPortLayout::Uptack:   return OperationPortLayout::Vertical;
        case OperationPortLayout::Vertical: return OperationPortLayout::Tee;
        case OperationPortLayout::Tee:      return OperationPortLayout::Side;
    }

    return OperationPortLayout::Side;
}

PortSide NodeCanvasAuthoring::nextOutputSide(const Node& node) {
    const PortSide side = node.outputs.empty() ? PortSide::Right : node.outputs.front().side;
    switch (side) {
        case PortSide::Right:  return PortSide::Bottom;
        case PortSide::Bottom: return PortSide::Top;
        case PortSide::Top:    return PortSide::Right;
        case PortSide::Left:   return PortSide::Right;
    }

    return PortSide::Right;
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::graphEditResult(
        const GraphEditResult& edit,
        const String& statusMessage,
        const String& selectedNodeId,
        NodeCanvasAuthoringEffects effects) {
    NodeCanvasAuthoringResult result;
    result.handled = true;
    result.succeeded = edit.succeeded();
    result.graphChanged = edit.succeeded() && edit.changed;
    result.graphEditCode = edit.code;
    result.nodeId = edit.nodeId;
    result.effects = effects;

    if (!edit.succeeded()) {
        if (statusMessage.isNotEmpty()) {
            authoringSession.statusMessage = statusMessage;
        }
        return result;
    }

    if (selectedNodeId.isNotEmpty()) {
        selectNode(selectedNodeId);
    } else {
        authoringSession.selectedEdgeIndex = -1;
    }
    refreshPresentation();
    authoringSession.statusMessage = statusMessage;
    return result;
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::handledResult(
        bool succeeded,
        const String& statusMessage,
        NodeCanvasAuthoringEffects effects) {
    authoringSession.statusMessage = statusMessage;
    return { true, succeeded, false, GraphEditCode::Connected, {}, effects };
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::restoredDocumentResult(
        const String& statusMessage) {
    authoringSession.statusMessage = statusMessage;
    return {
            true,
            true,
            true,
            GraphEditCode::Connected,
            {},
            { true, true, true }
    };
}

NodeCanvasAuthoringResult NodeCanvasAuthoring::setVoiceContextParameter(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value,
        const String& statusMessage) {
    const Node* node = findNode(nodeId);
    if (node == nullptr || node->kind != NodeKind::VoiceContext) {
        return {};
    }
    if (parameterValue(*node, parameterId) == value) {
        return handledResult(true, statusMessage, { true });
    }

    commands.beginCompoundEdit();
    const auto parameterEdit = commands.setNodeParameter(nodeId, parameterId, label, value);
    if (!parameterEdit.succeeded()) {
        commands.cancelCompoundEdit();
        return graphEditResult(parameterEdit, {}, {});
    }

    if (parameterId == "domain") {
        const auto presentationEdit = commands.editNodePresentation(nodeId, [value](Node& edited) {
            edited.subtitle = value == "spectral" ? "spectral start" : "waveform start";
        });
        if (!presentationEdit.succeeded()) {
            commands.cancelCompoundEdit();
            return graphEditResult(presentationEdit, {}, {});
        }
    }

    commands.commitCompoundEdit();
    selectNode(nodeId);
    refreshPresentation();
    authoringSession.statusMessage = statusMessage;
    return { true, true, true, GraphEditCode::Connected, nodeId, { true, false, true } };
}

void NodeCanvasAuthoring::refreshPresentation() {
    presentation.refresh(document.graph(), document.revision(), document.lastChange());
}

void NodeCanvasAuthoring::clearDocumentSelection() {
    authoringSession.selectedNodeId = {};
    authoringSession.expandedNodeId = {};
    authoringSession.selectedEdgeIndex = -1;
    authoringSession.spliceTargetEdgeIndex = -1;
}

void NodeCanvasAuthoring::spaceNodesAfterSplice(
        const String& upstreamNodeId,
        const String& insertedNodeId,
        const String& downstreamNodeId) {
    constexpr float neighbourSpacing = 56.f;
    const Node* upstreamNode = findNode(upstreamNodeId);
    const Node* insertedNode = findNode(insertedNodeId);
    if (upstreamNode == nullptr || insertedNode == nullptr) {
        return;
    }

    const float desiredInsertedLeft = upstreamNode->bounds.getRight() + neighbourSpacing;
    if (insertedNode->bounds.getX() < desiredInsertedLeft) {
        commands.resizeNode(insertedNodeId, insertedNode->bounds.withX(desiredInsertedLeft));
    }

    insertedNode = findNode(insertedNodeId);
    const Node* downstreamNode = findNode(downstreamNodeId);
    if (insertedNode == nullptr || downstreamNode == nullptr) {
        return;
    }

    const float desiredDownstreamLeft = insertedNode->bounds.getRight() + neighbourSpacing;
    const float xOffset = desiredDownstreamLeft - downstreamNode->bounds.getX();
    if (xOffset <= 0.f) {
        return;
    }

    std::unordered_map<std::string, std::vector<String>> adjacency;
    for (const auto& edge : document.graph().getEdges()) {
        adjacency[edge.sourceNodeId.toStdString()].push_back(edge.destNodeId);
    }

    std::vector<String> downstreamNodeIds { downstreamNodeId };
    std::unordered_set<std::string> visited { downstreamNodeId.toStdString() };
    for (size_t index = 0; index < downstreamNodeIds.size(); ++index) {
        const auto found = adjacency.find(downstreamNodeIds[index].toStdString());
        if (found == adjacency.end()) {
            continue;
        }

        for (const auto& candidate : found->second) {
            if (candidate == insertedNodeId || !visited.insert(candidate.toStdString()).second) {
                continue;
            }

            downstreamNodeIds.push_back(candidate);
        }
    }

    commands.translateNodes(downstreamNodeIds, { xOffset, 0.f });
}

const Node* NodeCanvasAuthoring::findNode(const String& nodeId) const {
    return document.graph().findNode(nodeId);
}

}
