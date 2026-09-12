#include "Graph/GraphDocument.h"

namespace CycleV2 {

GraphDocument::GraphDocument(NodeGraph initialGraph) :
        currentGraph(std::move(initialGraph)) {}

GraphDocument GraphDocument::openOrDefault(
        const juce::File& source,
        NodeGraph fallback) {
    if (source.existsAsFile()) {
        NodeGraph loaded = GraphSerializer().fromJsonString(source.loadFileAsString());
        if (!loaded.getNodes().empty()) {
            GraphDocument document(std::move(loaded));
            document.currentFile = source;
            return document;
        }
    }
    return GraphDocument(std::move(fallback));
}

bool GraphDocument::save(const juce::File& destination) {
    if (destination == juce::File()) {
        return false;
    }
    destination.getParentDirectory().createDirectory();
    if (!destination.replaceWithText(toJson(), false, false, "\n")) {
        return false;
    }

    currentFile = destination;
    savedStateId = currentStateId;
    return true;
}

bool GraphDocument::load(const juce::File& source) {
    if (!source.existsAsFile() || !loadJson(source.loadFileAsString())) {
        return false;
    }

    currentFile = source;
    savedStateId = currentStateId;
    return true;
}

bool GraphDocument::loadJson(const juce::String& json, bool recordUndo) {
    NodeGraph candidate = GraphSerializer().fromJsonString(json);
    if (candidate.getNodes().empty()) {
        return false;
    }

    if (recordUndo) {
        recordBeforeChange(currentGraph);
    }
    currentGraph = std::move(candidate);
    GraphChangeSet change;
    change.topologyChanged = true;
    change.layoutChanged = true;
    publishChange(std::move(change));
    return true;
}

juce::String GraphDocument::toJson() const {
    return GraphSerializer().toJsonString(currentGraph);
}

bool GraphDocument::undo() {
    if (undoHistory.empty()) {
        return false;
    }

    HistoryEntry entry = std::move(undoHistory.back());
    undoHistory.pop_back();
    if (auto* graph = std::get_if<NodeGraph>(&entry.edit)) {
        const uint64_t targetStateId = entry.beforeStateId;
        redoHistory.push_back({ currentGraph, entry.beforeStateId, entry.afterStateId });
        return restoreGraph(std::move(*graph), targetStateId);
    }

    auto& delta = std::get<GraphDelta>(entry.edit);
    delta.applyInverse(currentGraph);
    const GraphChangeSet change = delta.change();
    const uint64_t targetStateId = entry.beforeStateId;
    redoHistory.push_back(std::move(entry));
    publishChangeAtState(change, targetStateId);
    return true;
}

bool GraphDocument::redo() {
    if (redoHistory.empty()) {
        return false;
    }

    HistoryEntry entry = std::move(redoHistory.back());
    redoHistory.pop_back();
    if (auto* graph = std::get_if<NodeGraph>(&entry.edit)) {
        const uint64_t targetStateId = entry.afterStateId;
        undoHistory.push_back({ currentGraph, entry.beforeStateId, entry.afterStateId });
        return restoreGraph(std::move(*graph), targetStateId);
    }

    auto& delta = std::get<GraphDelta>(entry.edit);
    delta.applyForward(currentGraph);
    const GraphChangeSet change = delta.change();
    const uint64_t targetStateId = entry.afterStateId;
    undoHistory.push_back(std::move(entry));
    publishChangeAtState(change, targetStateId);
    return true;
}

void GraphDocument::recordExternalChange(NodeGraph beforeGraph, GraphChangeSet change) {
    recordBeforeChange(std::move(beforeGraph));
    publishChange(std::move(change));
}

void GraphDocument::recordBeforeChange(NodeGraph graph) {
    const uint64_t newStateId = nextStateId++;
    undoHistory.push_back({ std::move(graph), currentStateId, newStateId });
    pendingStateId = newStateId;
    redoHistory.clear();
    if (undoHistory.size() > maximumHistoryDepth) {
        undoHistory.erase(undoHistory.begin());
    }
}

void GraphDocument::recordDelta(GraphDelta delta) {
    if (delta.empty()) {
        return;
    }
    const uint64_t newStateId = nextStateId++;
    undoHistory.push_back({ std::move(delta), currentStateId, newStateId });
    pendingStateId = newStateId;
    redoHistory.clear();
    if (undoHistory.size() > maximumHistoryDepth) {
        undoHistory.erase(undoHistory.begin());
    }
}

void GraphDocument::publishChange(GraphChangeSet change) {
    const uint64_t stateId = pendingStateId.has_value()
            ? *pendingStateId
            : nextStateId++;
    pendingStateId.reset();
    publishChangeAtState(std::move(change), stateId);
}

void GraphDocument::publishChangeAtState(GraphChangeSet change, uint64_t stateId) {
    latestChange = std::move(change);
    currentStateId = stateId;
    ++documentRevision;
    if (listener) {
        listener(documentRevision, latestChange);
    }
}

bool GraphDocument::restoreGraph(NodeGraph graph, uint64_t stateId) {
    currentGraph = std::move(graph);
    GraphChangeSet change;
    change.topologyChanged = true;
    change.layoutChanged = true;
    publishChangeAtState(std::move(change), stateId);
    return true;
}

}
