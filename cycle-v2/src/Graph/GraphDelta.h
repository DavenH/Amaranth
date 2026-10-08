#pragma once

#include <cstddef>
#include <optional>

#include "Graph/GraphEditTypes.h"

namespace CycleV2 {

struct CableDeletionPlan;

struct NodeParameterDelta {
    juce::String nodeId;
    juce::String parameterId;
    std::optional<NodeParameter> before;
    std::optional<NodeParameter> after;
};

struct NodeModelDelta {
    juce::String nodeId;
    NodeModelStatePtr before;
    NodeModelStatePtr after;
};

struct NodeEditorStateDelta {
    juce::String nodeId;
    juce::var before;
    juce::var after;
};

struct NodeBoundsDelta {
    juce::String nodeId;
    juce::Rectangle<float> before;
    juce::Rectangle<float> after;
};

struct GuideCurveDelta {
    juce::String guideId;
    GuideCurveResource before;
    GuideCurveResource after;
};

struct IndexedEdgeState {
    size_t index {};
    Edge edge;
};

struct IndexedNodeState {
    size_t index {};
    Node node;
};

struct IndexedProbeState {
    size_t index {};
    SignalProbe probe;
};

struct CableDeletionDelta {
    std::optional<IndexedNodeState> pan;
    std::vector<IndexedEdgeState> edges;
    std::vector<IndexedProbeState> probes;
};

struct EdgeInputDelta {
    juce::String nodeId;
    juce::String portId;
    std::vector<IndexedEdgeState> before;
    std::vector<IndexedEdgeState> after;
};

class GraphDelta {
public:
    void applyForward(NodeGraph& graph) const;
    void applyInverse(NodeGraph& graph) const;

    bool empty() const;
    const GraphChangeSet& change() const { return changes; }

private:
    friend class GraphDeltaBuilder;

    void apply(NodeGraph& graph, bool forward) const;
    void applyEdgeInputs(NodeGraph& graph, bool forward) const;
    static void applyCableDeletion(
            NodeGraph& graph,
            const CableDeletionDelta& deletion,
            bool forward);

    std::vector<NodeParameterDelta> parameters;
    std::vector<NodeModelDelta> models;
    std::vector<NodeEditorStateDelta> editorStates;
    std::vector<NodeBoundsDelta> bounds;
    std::vector<GuideCurveDelta> guides;
    std::vector<EdgeInputDelta> edgeInputs;
    std::vector<CableDeletionDelta> cableDeletions;
    GraphChangeSet changes;
};

class GraphDeltaBuilder {
public:
    void captureNodeParameter(
            const NodeGraph& graph,
            const juce::String& nodeId,
            const juce::String& parameterId);
    void captureNodeModel(const NodeGraph& graph, const juce::String& nodeId);
    void captureNodeEditorState(const NodeGraph& graph, const juce::String& nodeId);
    void captureNodeBounds(const NodeGraph& graph, const juce::String& nodeId);
    void captureGuideCurve(const NodeGraph& graph, const juce::String& guideId);
    void captureEdgesToInput(
            const NodeGraph& graph,
            const juce::String& nodeId,
            const juce::String& portId);
    void captureCableDeletion(const NodeGraph& graph, const CableDeletionPlan& plan);

    GraphDelta finish(const NodeGraph& graph, GraphChangeSet changes) const;
    void restore(NodeGraph& graph) const;
    bool empty() const;
    NodeModelStatePtr originalNodeModel(const juce::String& nodeId) const;
    const GuideCurveResource* originalGuideCurve(const juce::String& guideId) const;

private:
    std::vector<NodeParameterDelta> parameters;
    std::vector<NodeModelDelta> models;
    std::vector<NodeEditorStateDelta> editorStates;
    std::vector<NodeBoundsDelta> bounds;
    std::vector<GuideCurveDelta> guides;
    std::vector<EdgeInputDelta> edgeInputs;
    std::vector<CableDeletionDelta> cableDeletions;
};

}
