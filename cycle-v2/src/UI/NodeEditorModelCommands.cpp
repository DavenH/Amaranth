#include "UI/NodeEditorModelCommands.h"

#include "Runtime/FingerprintBuilder.h"
#include "UI/NodeEditorHost.h"

namespace CycleV2 {

NodeEditorModelCommands::NodeEditorModelCommands(
        GraphDocument& targetDocument,
        GraphCommandDispatcher& targetCommands,
        NodeEditorPresentation& targetPresentation) :
        document(targetDocument)
    ,   commands(targetCommands)
    ,   presentation(targetPresentation) {
}

bool NodeEditorModelCommands::begin(const String& nodeId, bool downstreamFeedback) {
    const Node* node = document.graph().findNode(nodeId);
    if (node == nullptr || activeNodeId.isNotEmpty()) {
        return false;
    }
    if (!presentation.beginNodeEditorGesture(
                nodeId, commands, document, downstreamFeedback)) {
        return false;
    }
    activeNodeId = nodeId;
    durableBaseRevision = node->model != nullptr ? node->model->revision() : 0;
    return true;
}

bool NodeEditorModelCommands::publish(const String& nodeId, NodeModelStatePtr model) {
    if (model == nullptr || (activeNodeId.isNotEmpty() && activeNodeId != nodeId)) {
        return false;
    }
    const bool discrete = activeNodeId.isEmpty();
    if (discrete && !begin(nodeId, false)) {
        return false;
    }
    const auto result = commands.replaceNodeModel(
            nodeId, durableBaseRevision, std::move(model));
    if (!result.succeeded()) {
        presentation.cancelNodeEditorGesture(activeNodeId, commands);
        presentation.rebindNodeEditor();
        activeNodeId = {};
        return false;
    }
    if (result.changed) {
        const uint64_t fingerprint = FingerprintBuilder()
                .add(commands.editingGraph().getRevision())
                .value();
        presentation.recordNodeEditorMovement(nodeId, "model", fingerprint, std::nullopt);
        presentation.repaintNodeEditor(false);
    }
    if (discrete) {
        finish();
    }
    return true;
}

void NodeEditorModelCommands::finish() {
    if (activeNodeId.isEmpty()) {
        return;
    }
    presentation.finishNodeEditorGesture(activeNodeId, commands, document);
    activeNodeId = {};
}

}
