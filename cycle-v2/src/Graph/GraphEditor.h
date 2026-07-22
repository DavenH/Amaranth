#pragma once

#include "GraphNodeFactory.h"
#include "NodeDefinition.h"
#include "GraphValidator.h"

namespace CycleV2 {

struct PortAddress {
    String nodeId;
    String portId;
    bool input {};
};

enum class GraphEditCode {
    Connected,
    MissingNode,
    MissingPort,
    MissingEdge,
    DirectionMismatch,
    ValidationRejected,
    UnknownParameter,
    InvalidParameterValue,
    StaleRevision,
    ConflictingRevision,
    InvalidTypedSnapshot,
    WrongNodeKind,
    InvalidControlValue
};

struct GraphChangeSet {
    std::vector<String> nodeIds;
    bool topologyChanged {};
    bool layoutChanged {};
    bool probesChanged {};
    ParameterImpact parameterImpacts { ParameterImpact::None };
    bool modelChanged {};
    bool editorStateChanged {};
};

struct GraphEditResult {
    GraphEditCode code { GraphEditCode::Connected };
    String nodeId;
    std::vector<GraphValidationIssue> validationIssues;
    GraphChangeSet changes;
    bool changed { true };

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

class GraphEditor {
public:
    GraphEditResult addNode(NodeGraph& graph, NodeKind kind, Point<float> position) const;
    GraphEditResult connect(NodeGraph& graph, const PortAddress& first, const PortAddress& second) const;
    GraphEditResult attachGuideCurveToTrimeshVertexParameter(
            NodeGraph& graph,
            const String& guideNodeId,
            const String& meshNodeId,
            int vertexIndex,
            const String& parameterField) const;
    GraphEditResult createAndAttachGuideCurveToTrimeshVertexParameter(
            NodeGraph& graph,
            const String& meshNodeId,
            int vertexIndex,
            const String& parameterField,
            Point<float> guidePosition) const;
    GraphEditResult toggleSignalProbe(NodeGraph& graph, size_t edgeIndex, float tapPosition) const;
    GraphEditResult removeSignalProbe(NodeGraph& graph, const String& probeId) const;
    GraphEditResult reattachSignalProbe(
            NodeGraph& graph,
            const String& probeId,
            size_t edgeIndex,
            float tapPosition) const;
    GraphEditResult spliceNodeIntoEdge(NodeGraph& graph, size_t edgeIndex, const String& nodeId) const;
    GraphEditResult removeEdgeAt(NodeGraph& graph, size_t index) const;
    GraphEditResult removeNode(NodeGraph& graph, const String& nodeId) const;
    GraphEditResult setNodeParameter(
            NodeGraph& graph,
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value) const;
    GraphEditResult setNodeParametersAtomic(
            NodeGraph& graph,
            const String& nodeId,
            const std::vector<NodeParameter>& parameters) const;
    GraphEditResult replaceNodeModel(
            NodeGraph& graph,
            const String& nodeId,
            uint64_t expectedRevision,
            NodeModelStatePtr model) const;
    GraphEditResult setNodeEditorState(
            NodeGraph& graph,
            const String& nodeId,
            var editorState) const;

private:
    const Node* findNode(const NodeGraph& graph, const String& nodeId) const;
    Node* findMutableNode(NodeGraph& graph, const String& nodeId) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    String guideVertexTargetPortId(int vertexIndex, const String& parameterField) const;
    String createUniqueNodeId(const NodeGraph& graph, NodeKind kind) const;
    String createUniqueProbeId(const NodeGraph& graph) const;
    String baseIdForKind(NodeKind kind) const;
};

}
