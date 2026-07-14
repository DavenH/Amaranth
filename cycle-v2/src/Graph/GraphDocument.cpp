#include "GraphDocument.h"

namespace CycleV2 {

GraphDocument::GraphDocument(NodeGraph initialGraph) :
        currentGraph(std::move(initialGraph)) {}

GraphDocument GraphDocument::openOrDefault(
        const juce::File& source,
        NodeGraph fallback) {
    if (source.existsAsFile()) {
        NodeGraph loaded = GraphSerializer().fromXmlString(source.loadFileAsString());
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
    if (!destination.replaceWithText(toXml())) {
        return false;
    }

    currentFile = destination;
    dirty = false;
    return true;
}

bool GraphDocument::load(const juce::File& source) {
    if (!source.existsAsFile() || !loadXml(source.loadFileAsString())) {
        return false;
    }

    currentFile = source;
    dirty = false;
    return true;
}

bool GraphDocument::loadXml(const juce::String& xml, bool recordUndo) {
    NodeGraph candidate = GraphSerializer().fromXmlString(xml);
    if (candidate.getNodes().empty()) {
        return false;
    }

    if (recordUndo) {
        recordBeforeChange(toXml());
    }
    currentGraph = std::move(candidate);
    GraphChangeSet change;
    change.topologyChanged = true;
    change.layoutChanged = true;
    publishChange(std::move(change));
    return true;
}

juce::String GraphDocument::toXml() const {
    return GraphSerializer().toXmlString(currentGraph);
}

bool GraphDocument::undo() {
    if (undoHistory.empty()) {
        return false;
    }

    redoHistory.push_back(toXml());
    const juce::String xml = std::move(undoHistory.back());
    undoHistory.pop_back();
    return restoreXml(xml);
}

bool GraphDocument::redo() {
    if (redoHistory.empty()) {
        return false;
    }

    undoHistory.push_back(toXml());
    const juce::String xml = std::move(redoHistory.back());
    redoHistory.pop_back();
    return restoreXml(xml);
}

void GraphDocument::recordExternalChange(juce::String beforeXml, GraphChangeSet change) {
    recordBeforeChange(std::move(beforeXml));
    publishChange(std::move(change));
}

void GraphDocument::recordBeforeChange(juce::String xml) {
    if (xml.isEmpty()) {
        return;
    }

    undoHistory.push_back(std::move(xml));
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

bool GraphDocument::restoreXml(const juce::String& xml) {
    NodeGraph restored = GraphSerializer().fromXmlString(xml);
    if (restored.getNodes().empty()) {
        return false;
    }

    currentGraph = std::move(restored);
    GraphChangeSet change;
    change.topologyChanged = true;
    change.layoutChanged = true;
    publishChange(std::move(change));
    return true;
}

}
