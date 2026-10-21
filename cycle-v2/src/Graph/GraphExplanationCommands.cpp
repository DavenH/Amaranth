#include "Graph/GraphCommandDispatcher.h"

namespace CycleV2 {

juce::String* GraphDocument::explanationTextForEditing(const ExplanationTarget& target) {
    switch (target.scope) {
        case ExplanationScope::Preset:
            return target.id.isEmpty() ? &presetPresentation.description : nullptr;
        case ExplanationScope::Node:
            if (Node* node = currentGraph.findNodeForEditing(target.id)) {
                return &node->rationale;
            }
            break;
        case ExplanationScope::Group:
            if (NodeGroup* group = currentGraph.findNodeGroupForEditing(target.id)) {
                return &group->rationale;
            }
            break;
    }
    return nullptr;
}

void GraphDocument::applyExplanation(const ExplanationEdit& edit, bool forward) {
    if (const auto* text = std::get_if<ExplanationTextEdit>(&edit.value)) {
        if (juce::String* current = explanationTextForEditing(edit.target)) {
            *current = forward ? text->after : text->before;
        }
    } else if (NodeGroup* group = currentGraph.findNodeGroupForEditing(edit.target.id)) {
        const auto& placement = std::get<InformationCardPlacementEdit>(edit.value);
        group->informationCardOffset = forward ? placement.after : placement.before;
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

GraphEditResult GraphCommandDispatcher::setExplanationText(
        const ExplanationTarget& target, const juce::String& text) {
    GraphEditResult result;
    result.nodeId = target.id;
    result.changed = false;
    juce::String* current = document.explanationTextForEditing(target);
    if (current == nullptr) {
        result.code = target.scope == ExplanationScope::Node
                ? GraphEditCode::MissingNode : GraphEditCode::ValidationRejected;
    } else if (hasTransientEdit() || compoundActive || text.length() > maximumExplanationLength) {
        result.code = GraphEditCode::ValidationRejected;
    } else if (*current != text) {
        document.recordExplanation({ target, GraphDocument::ExplanationTextEdit { *current, text } });
        result.changed = true;
        result.changes.explanationsChanged = true;
    }
    return result;
}

GraphEditResult GraphCommandDispatcher::setNodeRationale(
        const juce::String& nodeId, const juce::String& text) {
    return setExplanationText({ ExplanationScope::Node, nodeId }, text);
}

GraphEditResult GraphCommandDispatcher::setPresetDescription(const juce::String& text) {
    return setExplanationText({ ExplanationScope::Preset, {} }, text);
}

GraphEditResult GraphCommandDispatcher::setGroupInformationCard(
        const juce::String& groupId, std::optional<juce::Point<float>> offset) {
    GraphEditResult result;
    result.nodeId = groupId;
    result.changed = false;
    const NodeGroup* group = document.graph().findNodeGroup(groupId);
    if (group == nullptr || hasTransientEdit() || compoundActive
            || (offset.has_value() && !isValidInformationCardOffset(offset->x, offset->y))) {
        result.code = GraphEditCode::ValidationRejected;
    } else if (group->informationCardOffset != offset) {
        document.recordExplanation({ { ExplanationScope::Group, groupId },
                GraphDocument::InformationCardPlacementEdit { group->informationCardOffset, offset } });
        result.changed = true;
        result.changes.explanationsChanged = true;
    }
    return result;
}

}
