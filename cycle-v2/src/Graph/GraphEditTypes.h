#pragma once

#include <vector>

#include "Graph/GraphValidationTypes.h"
#include "Graph/NodeDefinition.h"

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
    std::vector<Edge> removedEdges;
    std::vector<GuideCurveAssignment> removedGuideAssignments;
    bool topologyChanged {};
    bool layoutChanged {};
    bool probesChanged {};
    bool guidesChanged {};
    bool guidePresentationChanged {};
    ParameterImpact parameterImpacts { ParameterImpact::None };
    bool modelChanged {};
    bool editorStateChanged {};
    bool resourcesChanged {};
};

struct NodeAudioResourceEdit {
    String nodeId;
    AudioSampleResource resource;
    String mode;
    std::vector<NodeParameter> parameters;
    NodeModelStatePtr model;
    uint64_t expectedModelRevision {};
};

struct GraphEditResult {
    GraphEditCode code { GraphEditCode::Connected };
    String nodeId;
    std::vector<GraphValidationIssue> validationIssues;
    GraphChangeSet changes;
    bool changed { true };

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

}
