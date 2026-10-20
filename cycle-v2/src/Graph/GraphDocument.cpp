#include "Graph/GraphDocument.h"

namespace CycleV2 {

GraphDocument::GraphDocument(NodeGraph initialGraph) :
        currentGraph(std::move(initialGraph)) {}

GraphDocument GraphDocument::openOrDefault(
        const juce::File& source,
        NodeGraph fallback) {
    if (source.existsAsFile()) {
        GraphLoadResult loaded = GraphSerializer().loadJsonString(source.loadFileAsString());
        if (loaded.succeeded() && !loaded.graph.getNodes().empty()) {
            GraphDocument document(std::move(loaded.graph));
            document.presetPresentation = std::move(loaded.presentation);
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
    savedPresentationRevision = presentationRevision;
    return true;
}

bool GraphDocument::load(const juce::File& source) {
    if (!source.existsAsFile() || !loadJson(source.loadFileAsString())) {
        return false;
    }

    currentFile = source;
    savedStateId = currentStateId;
    savedPresentationRevision = presentationRevision;
    return true;
}

bool GraphDocument::loadJson(const juce::String& json, bool recordUndo) {
    GraphLoadResult loaded = GraphSerializer().loadJsonString(json);
    if (!loaded.succeeded() || loaded.graph.getNodes().empty()) {
        return false;
    }

    if (recordUndo) {
        recordBeforeChange(currentGraph, true);
    }
    currentGraph = std::move(loaded.graph);
    presetPresentation = std::move(loaded.presentation);
    ++presentationRevision;
    GraphChangeSet change;
    change.topologyChanged = true;
    change.layoutChanged = true;
    publishChange(std::move(change));
    return true;
}

juce::String GraphDocument::toJson() const {
    return GraphSerializer().toJsonString(currentGraph, presetPresentation);
}

void GraphDocument::setPresentation(PresetPresentation presentation) {
    presetPresentation = std::move(presentation);
    ++presentationRevision;
}

void GraphDocument::setOutputSpyPosition(std::optional<juce::Point<float>> position) {
    if (presetPresentation.outputSpyPosition == position) {
        return;
    }
    presetPresentation.outputSpyPosition = position;
    ++presentationRevision;
}

bool GraphDocument::undo() {
    if (undoHistory.empty()) {
        return false;
    }

    HistoryEntry entry = std::move(undoHistory.back());
    undoHistory.pop_back();
    if (auto* snapshot = std::get_if<GraphSnapshot>(&entry.edit)) {
        const uint64_t targetStateId = entry.beforeStateId;
        GraphSnapshot inverse { currentGraph, {} };
        if (snapshot->presentation.has_value()) {
            inverse.presentation = presetPresentation;
            presetPresentation = std::move(*snapshot->presentation);
        }
        redoHistory.push_back({ std::move(inverse), entry.beforeStateId, entry.afterStateId });
        return restoreGraph(std::move(snapshot->graph), targetStateId);
    }

    if (const auto* explanation = std::get_if<ExplanationEdit>(&entry.edit)) {
        applyExplanation(*explanation, false);
        const uint64_t stateId = entry.beforeStateId;
        redoHistory.push_back(std::move(entry));
        GraphChangeSet change;
        change.explanationsChanged = true;
        publishChangeAtState(change, stateId);
        return true;
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
    if (auto* snapshot = std::get_if<GraphSnapshot>(&entry.edit)) {
        const uint64_t targetStateId = entry.afterStateId;
        GraphSnapshot inverse { currentGraph, {} };
        if (snapshot->presentation.has_value()) {
            inverse.presentation = presetPresentation;
            presetPresentation = std::move(*snapshot->presentation);
        }
        undoHistory.push_back({ std::move(inverse), entry.beforeStateId, entry.afterStateId });
        return restoreGraph(std::move(snapshot->graph), targetStateId);
    }

    if (const auto* explanation = std::get_if<ExplanationEdit>(&entry.edit)) {
        applyExplanation(*explanation, true);
        const uint64_t stateId = entry.afterStateId;
        undoHistory.push_back(std::move(entry));
        GraphChangeSet change;
        change.explanationsChanged = true;
        publishChangeAtState(change, stateId);
        return true;
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

void GraphDocument::recordBeforeChange(NodeGraph graph, bool includePresentation) {
    const uint64_t newStateId = nextStateId++;
    GraphSnapshot snapshot { std::move(graph), {} };
    if (includePresentation) {
        snapshot.presentation = presetPresentation;
    }
    undoHistory.push_back({ std::move(snapshot), currentStateId, newStateId });
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
