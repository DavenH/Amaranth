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

enum class ExplanationScope {
    Preset,
    Node,
    Group
};

struct ExplanationTarget {
    ExplanationScope scope { ExplanationScope::Preset };
    String id;

    bool operator==(const ExplanationTarget& other) const {
        return scope == other.scope && id == other.id;
    }
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

struct IndexedEdgeState {
    size_t index {};
    Edge edge;
};

struct GraphChangeSet {
    std::vector<String> nodeIds;
    // Recorded in removal order; inverse application restores them in reverse.
    std::vector<IndexedEdgeState> removedEdges;
    std::vector<IndexedGuideAssignmentState> removedGuideAssignments;
    bool topologyChanged {};
    bool layoutChanged {};
    bool probesChanged {};
    bool guidesChanged {};
    bool guidePresentationChanged {};
    ParameterImpact parameterImpacts { ParameterImpact::None };
    bool modelChanged {};
    bool editorStateChanged {};
    bool resourcesChanged {};
    bool explanationsChanged {};
};

struct NodeAudioResourceEdit {
    String nodeId;
    AudioSampleResource resource;
    String mode;
    std::vector<NodeParameter> parameters;
    NodeModelStatePtr model;
    uint64_t expectedModelRevision {};
};

constexpr size_t maximumNodeAuthoringEdits = 64;
constexpr size_t maximumNodeAuthoringParameters = 1024;

struct NodeAuthoringEdit {
    String nodeId;
    std::vector<NodeParameter> parameters;
    std::optional<uint64_t> expectedModelRevision;
    var model;
};

struct NodeAuthoringEdits {
    uint64_t expectedDocumentRevision {};
    std::vector<NodeAuthoringEdit> edits;
};

constexpr size_t maximumGuideCurvePointEdits = 64;

enum class GuideCurvePointEditKind {
    Set,
    Insert,
    Remove
};

struct GuideCurvePointEdit {
    GuideCurvePointEditKind kind { GuideCurvePointEditKind::Set };
    uint64_t vertexId {};
    std::optional<float> x;
    std::optional<float> y;
    std::optional<float> curve;
};

struct GuideCurveEdits {
    String guideId;
    uint64_t expectedDocumentRevision {};
    uint64_t expectedModelRevision {};
    std::vector<GuideCurvePointEdit> edits;
};

struct GraphEditResult {
    GraphEditCode code { GraphEditCode::Connected };
    String nodeId;
    std::vector<GraphValidationIssue> validationIssues;
    GraphChangeSet changes;
    bool changed { true };
    String message;
    int editIndex { -1 };

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

}
