#pragma once

#include "Graph/GraphEditor.h"
#include "Graph/GraphSerializer.h"

#include <functional>
#include <vector>

namespace CycleV2 {

class GraphCommandDispatcher;

class GraphDocument {
public:
    using Listener = std::function<void(uint64_t, const GraphChangeSet&)>;

    GraphDocument() = default;
    explicit GraphDocument(NodeGraph initialGraph);
    static GraphDocument openOrDefault(
            const juce::File& source,
            NodeGraph fallback);

    const NodeGraph& graph() const { return currentGraph; }
    uint64_t revision() const { return documentRevision; }
    bool isDirty() const { return dirty; }
    const juce::File& file() const { return currentFile; }
    const GraphChangeSet& lastChange() const { return latestChange; }

    bool save(const juce::File& destination);
    bool load(const juce::File& source);
    bool loadJson(const juce::String& json, bool recordUndo = true);
    juce::String toJson() const;
    bool undo();
    bool redo();
    void recordExternalChange(NodeGraph beforeGraph, GraphChangeSet change = {});
    bool canUndo() const { return !undoHistory.empty(); }
    bool canRedo() const { return !redoHistory.empty(); }
    void setListener(Listener listenerToUse) { listener = std::move(listenerToUse); }

private:
    friend class GraphCommandDispatcher;

    NodeGraph& graphForCommand() { return currentGraph; }
    void recordBeforeChange(NodeGraph graph);
    void publishChange(GraphChangeSet change);
    bool restoreGraph(NodeGraph graph);

    static constexpr size_t maximumHistoryDepth = 64;

    NodeGraph currentGraph;
    juce::File currentFile;
    std::vector<NodeGraph> undoHistory;
    std::vector<NodeGraph> redoHistory;
    GraphChangeSet latestChange;
    Listener listener;
    uint64_t documentRevision { 1 };
    bool dirty {};
};

}
