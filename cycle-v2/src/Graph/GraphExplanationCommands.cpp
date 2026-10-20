#include "Graph/GraphCommandDispatcher.h"

namespace CycleV2 {

void GraphDocument::applyExplanation(const ExplanationEdit& edit, bool forward) {
    const juce::String& text = forward ? edit.after : edit.before;
    if (edit.nodeId.isEmpty()) {
        presetPresentation.description = text;
    } else if (Node* node = currentGraph.findNodeForEditing(edit.nodeId)) {
        node->rationale = text;
    }
}

void GraphDocument::recordExplanation(ExplanationEdit edit) {
    applyExplanation(edit, true);
    const uint64_t newStateId = nextStateId++;
    undoHistory.push_back({ std::move(edit), currentStateId, newStateId });
    redoHistory.clear();
    if (undoHistory.size() > maximumHistoryDepth) {
        undoHistory.erase(undoHistory.begin());
    }
    GraphChangeSet change;
    change.explanationsChanged = true;
    publishChangeAtState(std::move(change), newStateId);
}

GraphEditResult GraphCommandDispatcher::setNodeRationale(
        const juce::String& nodeId,
        const juce::String& text) {
    GraphEditResult result;
    result.nodeId = nodeId;
    result.changed = false;
    const Node* node = document.graph().findNode(nodeId);
    if (node == nullptr) {
        result.code = GraphEditCode::MissingNode;
    } else if (hasTransientEdit() || compoundActive || text.length() > maximumExplanationLength) {
        result.code = GraphEditCode::ValidationRejected;
    } else if (node->rationale != text) {
        document.recordExplanation({ nodeId, node->rationale, text });
        result.changed = true;
        result.changes.explanationsChanged = true;
    }
    return result;
}

GraphEditResult GraphCommandDispatcher::setPresetDescription(const juce::String& text) {
    GraphEditResult result;
    result.changed = false;
    if (hasTransientEdit() || compoundActive || text.length() > maximumExplanationLength) {
        result.code = GraphEditCode::ValidationRejected;
    } else if (document.presentation().description != text) {
        document.recordExplanation({ {}, document.presentation().description, text });
        result.changed = true;
        result.changes.explanationsChanged = true;
    }
    return result;
}

}
