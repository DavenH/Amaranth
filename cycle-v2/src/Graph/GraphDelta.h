#pragma once

#include <optional>

#include "Graph/GraphEditor.h"

namespace CycleV2 {

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

class GraphDelta {
public:
    void applyForward(NodeGraph& graph) const;
    void applyInverse(NodeGraph& graph) const;

    bool empty() const;
    const GraphChangeSet& change() const { return changes; }

private:
    friend class GraphDeltaBuilder;

    void apply(NodeGraph& graph, bool forward) const;

    std::vector<NodeParameterDelta> parameters;
    std::vector<NodeModelDelta> models;
    std::vector<NodeEditorStateDelta> editorStates;
    std::vector<NodeBoundsDelta> bounds;
    std::vector<GuideCurveDelta> guides;
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
};

}
