#pragma once

#include "GraphDocument.h"

#include <functional>
#include <optional>

namespace CycleV2 {

struct CurveNodeStatePublication {
    juce::String nodeId;
    // Captured once from GraphDocument::graph() when the gesture begins.
    uint64_t durableBaseRevision {};
    NodeModelStatePtr model;
    std::vector<NodeParameter> controls;
};

class GraphCommandDispatcher {
public:
    explicit GraphCommandDispatcher(GraphDocument& documentToUse) : document(documentToUse) {}

    GraphEditResult addNode(NodeKind kind, juce::Point<float> position);
    GraphEditResult removeNode(const juce::String& nodeId);
    GraphEditResult removeEdgeAt(size_t edgeIndex);
    GraphEditResult connect(const PortAddress& first, const PortAddress& second);
    GraphEditResult toggleSignalProbe(size_t edgeIndex, float tapPosition = 0.5f);
    GraphEditResult removeSignalProbe(const juce::String& probeId);
    GraphEditResult reattachSignalProbe(
            const juce::String& probeId,
            size_t edgeIndex,
            float tapPosition);
    GraphEditResult spliceNodeIntoEdge(size_t edgeIndex, const juce::String& nodeId);
    GraphEditResult createGuideCurve();
    GraphEditResult duplicateGuideCurve(const juce::String& guideId);
    GraphEditResult assignGuideCurve(
            const juce::String& guideId,
            const juce::String& meshNodeId,
            int vertexIndex,
            const juce::String& parameterField);
    GraphEditResult detachGuideCurve(
            const juce::String& meshNodeId,
            int vertexIndex,
            const juce::String& parameterField);
    GraphEditResult createAndAssignGuideCurve(
            const juce::String& meshNodeId,
            int vertexIndex,
            const juce::String& parameterField);
    GraphEditResult removeGuideCurve(const juce::String& guideId);
    GraphEditResult renameGuideCurve(
            const juce::String& guideId,
            const juce::String& name);
    GraphEditResult replaceGuideCurve(
            const juce::String& guideId,
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls);
    GraphEditResult setNodeParameter(
            const juce::String& nodeId,
            const juce::String& parameterId,
            const juce::String& label,
            const juce::String& value);
    GraphEditResult publishCurveState(const CurveNodeStatePublication& publication);
    GraphEditResult replaceNodeModel(
            const juce::String& nodeId,
            uint64_t expectedRevision,
            NodeModelStatePtr model);
    GraphEditResult setNodeEditorState(const juce::String& nodeId, juce::var editorState);
    GraphEditResult moveNode(const juce::String& nodeId, juce::Point<float> position);
    GraphEditResult resizeNode(const juce::String& nodeId, juce::Rectangle<float> bounds);
    GraphEditResult editNodePresentation(
            const juce::String& nodeId,
            const std::function<void(Node&)>& edit);
    GraphEditResult translateNodes(
            const std::vector<juce::String>& nodeIds,
            juce::Point<float> offset);
    GraphEditResult setPerformanceKeyboardBounds(juce::Rectangle<float> bounds);

    void beginCompoundEdit();
    void commitCompoundEdit();
    void cancelCompoundEdit();
    void beginTransientEdit();
    void commitTransientEdit();
    void cancelTransientEdit();

    const NodeGraph& editingGraph() const;
    const GraphChangeSet& transientChanges() const;
    bool hasTransientEdit() const { return transientEdit.has_value(); }

private:
    struct TransientEdit {
        NodeGraph graph;
        GraphChangeSet changes;
        int depth { 1 };
        bool changed {};
    };

    GraphEditResult apply(const std::function<GraphEditResult(NodeGraph&)>& command);
    GraphEditResult setNodeBounds(const juce::String& nodeId, juce::Rectangle<float> bounds);
    void accumulateCompoundChange(const GraphChangeSet& change);
    static void accumulateChange(GraphChangeSet& destination, const GraphChangeSet& change);

    GraphDocument& document;
    juce::String compoundBefore;
    GraphChangeSet compoundChanges;
    bool compoundActive {};
    bool compoundChanged {};
    int compoundDepth {};
    std::optional<TransientEdit> transientEdit;
};

}
