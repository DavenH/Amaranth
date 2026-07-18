#pragma once

#include "GraphDocument.h"

#include <functional>

namespace CycleV2 {

struct CurveNodeStatePublication {
    juce::String nodeId;
    uint64_t expectedRevision {};
    uint64_t revision {};
    juce::String modelSnapshot;
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
    GraphEditResult attachGuideCurve(
            const juce::String& guideNodeId,
            const juce::String& meshNodeId,
            int vertexIndex,
            const juce::String& parameterField);
    GraphEditResult createAndAttachGuideCurve(
            const juce::String& meshNodeId,
            int vertexIndex,
            const juce::String& parameterField,
            juce::Point<float> guidePosition);
    GraphEditResult setNodeParameter(
            const juce::String& nodeId,
            const juce::String& parameterId,
            const juce::String& label,
            const juce::String& value);
    GraphEditResult publishCurveState(const CurveNodeStatePublication& publication);
    GraphEditResult moveNode(const juce::String& nodeId, juce::Point<float> position);
    GraphEditResult resizeNode(const juce::String& nodeId, juce::Rectangle<float> bounds);
    GraphEditResult editNodePresentation(
            const juce::String& nodeId,
            const std::function<void(Node&)>& edit);
    GraphEditResult translateNodes(
            const std::vector<juce::String>& nodeIds,
            juce::Point<float> offset);

    void beginCompoundEdit();
    void commitCompoundEdit();
    void cancelCompoundEdit();

private:
    GraphEditResult apply(const std::function<GraphEditResult(NodeGraph&)>& command);
    GraphEditResult setNodeBounds(const juce::String& nodeId, juce::Rectangle<float> bounds);

    GraphDocument& document;
    juce::String compoundBefore;
    bool compoundActive {};
    bool compoundChanged {};
    int compoundDepth {};
};

}
