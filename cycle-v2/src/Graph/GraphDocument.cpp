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
    dirty = false;
    return true;
}

bool GraphDocument::load(const juce::File& source) {
    if (!source.existsAsFile() || !loadJson(source.loadFileAsString())) {
        return false;
    }

    currentFile = source;
    dirty = false;
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

    redoHistory.push_back(currentGraph);
    NodeGraph graph = std::move(undoHistory.back());
    undoHistory.pop_back();
    return restoreGraph(std::move(graph));
}

bool GraphDocument::redo() {
    if (redoHistory.empty()) {
        return false;
    }

    undoHistory.push_back(currentGraph);
    NodeGraph graph = std::move(redoHistory.back());
    redoHistory.pop_back();
    return restoreGraph(std::move(graph));
}

void GraphDocument::recordExternalChange(NodeGraph beforeGraph, GraphChangeSet change) {
    recordBeforeChange(std::move(beforeGraph));
    publishChange(std::move(change));
}

void GraphDocument::recordBeforeChange(NodeGraph graph) {
    undoHistory.push_back(std::move(graph));
    redoHistory.clear();
    if (undoHistory.size() > maximumHistoryDepth) {
        undoHistory.erase(undoHistory.begin());
    }
}

void GraphDocument::publishChange(GraphChangeSet change) {
    latestChange = std::move(change);
    ++documentRevision;
    dirty = true;
    if (listener) {
        listener(documentRevision, latestChange);
    }
}

bool GraphDocument::restoreGraph(NodeGraph graph) {
    currentGraph = std::move(graph);
    GraphChangeSet change;
    change.topologyChanged = true;
    change.layoutChanged = true;
    publishChange(std::move(change));
    return true;
}

}
