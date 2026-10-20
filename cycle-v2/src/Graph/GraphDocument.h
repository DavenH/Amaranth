#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include "Graph/GraphDelta.h"
#include "Graph/GraphEditTypes.h"
#include "Graph/GraphSerializer.h"

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
    bool isDirty() const {
        return currentStateId != savedStateId || presentationRevision != savedPresentationRevision;
    }
    const juce::File& file() const { return currentFile; }
    const GraphChangeSet& lastChange() const { return latestChange; }
    const PresetPresentation& presentation() const { return presetPresentation; }
    void setPresentation(PresetPresentation presentation);

    bool save(const juce::File& destination);
    bool load(const juce::File& source);
    bool loadJson(const juce::String& json, bool recordUndo = true);
    juce::String toJson() const;
    bool undo();
    bool redo();
    bool nextUndoIsExplanation() const {
        return !undoHistory.empty() && std::holds_alternative<ExplanationEdit>(undoHistory.back().edit);
    }
    bool nextRedoIsExplanation() const {
        return !redoHistory.empty() && std::holds_alternative<ExplanationEdit>(redoHistory.back().edit);
    }
    void recordExternalChange(NodeGraph beforeGraph, GraphChangeSet change = {});
    bool canUndo() const { return !undoHistory.empty(); }
    bool canRedo() const { return !redoHistory.empty(); }
    void setListener(Listener listenerToUse) { listener = std::move(listenerToUse); }

private:
    friend class GraphCommandDispatcher;

    void setOutputSpyPosition(std::optional<juce::Point<float>> position);

    NodeGraph& graphForCommand() { return currentGraph; }
    void recordBeforeChange(NodeGraph graph, bool includePresentation = false);
    void recordDelta(GraphDelta delta);
    void publishChange(GraphChangeSet change);
    void publishChangeAtState(GraphChangeSet change, uint64_t stateId);
    bool restoreGraph(NodeGraph graph, uint64_t stateId);

    static constexpr size_t maximumHistoryDepth = 64;

    NodeGraph currentGraph;
    PresetPresentation presetPresentation;
    juce::File currentFile;
    struct ExplanationEdit {
        juce::String nodeId;
        juce::String before;
        juce::String after;
    };
    void recordExplanation(ExplanationEdit edit);
    void applyExplanation(const ExplanationEdit& edit, bool forward);

    struct GraphSnapshot {
        NodeGraph graph;
        std::optional<PresetPresentation> presentation;
    };
    struct HistoryEntry {
        std::variant<GraphSnapshot, GraphDelta, ExplanationEdit> edit;
        uint64_t beforeStateId {};
        uint64_t afterStateId {};
    };

    std::vector<HistoryEntry> undoHistory;
    std::vector<HistoryEntry> redoHistory;
    GraphChangeSet latestChange;
    Listener listener;
    uint64_t documentRevision { 1 };
    uint64_t currentStateId { 1 };
    uint64_t savedStateId { 1 };
    uint64_t nextStateId { 2 };
    uint64_t presentationRevision {};
    uint64_t savedPresentationRevision {};
    std::optional<uint64_t> pendingStateId;
};

}
