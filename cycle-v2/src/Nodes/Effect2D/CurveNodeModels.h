#pragma once

#include "../../Graph/NodeGraph.h"
#include "../../Graph/NodeDefinition.h"
#include "Effect2DMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/Mesh.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CycleV2 {

using CurveVertexId = uint64_t;
using EnvelopeCubeId = uint64_t;

struct CurvePoint {
    float x {};
    float y {};
};

enum class CurveEditCode {
    Applied,
    NoChange,
    MissingIdentity,
    InvalidEdit
};

struct CurveEditResult {
    CurveEditCode code { CurveEditCode::InvalidEdit };
    CurveVertexId vertexId {};

    bool succeeded() const { return code == CurveEditCode::Applied || code == CurveEditCode::NoChange; }
};

struct FlatCurveVertex {
    CurveVertexId id {};
    float x {};
    float y {};
    float curve {};

    bool operator==(const FlatCurveVertex& other) const {
        return id == other.id && x == other.x && y == other.y && curve == other.curve;
    }
    bool operator!=(const FlatCurveVertex& other) const { return !(*this == other); }
};

class FlatCurveModel {
public:
    static constexpr int currentVersion = 1;

    explicit FlatCurveModel(String name = "CycleV2FlatCurveModel");
    ~FlatCurveModel();

    bool replaceVertices(std::vector<FlatCurveVertex> nextVertices);
    CurveEditResult moveVertex(CurveVertexId vertexId, CurvePoint position);
    CurveEditResult setCurve(CurveVertexId vertexId, float curve);
    CurveEditResult insertVertex(CurvePoint position, float curve = 0.f);
    CurveEditResult removeVertex(CurveVertexId vertexId);
    bool selectVertex(std::optional<CurveVertexId> vertexId);
    bool synchronizeFromMesh(Vertex* selectedVertex);
    bool loadSnapshot(const String& snapshot);
    String snapshot() const;
    bool readJSON(const var& value);
    var writeJSON() const;
    bool copyFrom(const FlatCurveModel& other);
    bool equals(const FlatCurveModel& other) const;

    const Mesh& getMesh() const { return mesh; }
    Mesh& getMesh() { return mesh; }
    const std::vector<FlatCurveVertex>& getVertices() const { return vertices; }
    std::optional<CurveVertexId> selectedVertexId() const { return selection; }
    Vertex* vertexForIdentity(CurveVertexId vertexId) const;
    Vertex* selectedMeshVertex() const;
    uint64_t revision() const { return modelRevision; }
    void setPublicationRevision(uint64_t revisionToUse) { modelRevision = juce::jmax<uint64_t>(1, revisionToUse); }

private:
    static bool validate(const std::vector<FlatCurveVertex>& vertices);
    static bool validateVertex(const FlatCurveVertex& vertex);
    void rebuildLookups();
    CurveVertexId nextIdentity() const;
    void rebuildMesh();

    Mesh mesh;
    std::vector<FlatCurveVertex> vertices;
    std::optional<CurveVertexId> selection;
    std::unordered_map<Vertex*, CurveVertexId> identitiesByVertex;
    std::unordered_map<CurveVertexId, Vertex*> verticesByIdentity;
    std::unordered_map<CurveVertexId, size_t> indicesByIdentity;
    CurveVertexId nextVertexIdentity { 1 };
    uint64_t modelRevision { 1 };
};

class EnvelopeNodeModel {
public:
    static constexpr int currentVersion = 2;

    EnvelopeNodeModel();
    ~EnvelopeNodeModel();

    bool loadSnapshot(const String& snapshot);
    bool syncFromNode(const Node& node);
    bool synchronizeFromMesh(VertCube* selectedCube);
    String snapshot() const;
    bool readJSON(const var& value);
    var writeJSON() const;
    bool copyFrom(const EnvelopeNodeModel& other);
    bool equals(const EnvelopeNodeModel& other) const;
    bool selectCube(std::optional<EnvelopeCubeId> cubeId);

    EnvelopeMesh& getMesh() { return mesh; }
    const EnvelopeMesh& getMesh() const { return mesh; }
    uint64_t revision() const { return modelRevision; }
    std::optional<EnvelopeCubeId> selectedCubeId() const { return selection; }
    VertCube* cubeForIdentity(EnvelopeCubeId cubeId) const;
    VertCube* selectedMeshCube() const;
    const std::vector<EnvelopeCubeId>& getCubeIds() const { return cubeIds; }
    void setPublicationRevision(uint64_t revisionToUse) { modelRevision = juce::jmax<uint64_t>(1, revisionToUse); }

    bool logarithmic {};
    bool dynamicWhileLive {};
    float red { 0.5f };
    float blue { 0.5f };
    bool redLinked { true };
    bool blueLinked { true };

private:
    EnvelopeCubeId nextIdentity() const;
    void rebuildIdentityMap();

    EnvelopeMesh mesh;
    std::vector<EnvelopeCubeId> cubeIds;
    std::optional<EnvelopeCubeId> selection;
    std::unordered_map<VertCube*, EnvelopeCubeId> identitiesByCube;
    std::unordered_map<EnvelopeCubeId, VertCube*> cubesByIdentity;
    EnvelopeCubeId nextCubeIdentity { 1 };
    EnvelopeMesh committedMesh { "CycleV2EnvelopeCommittedModel" };
    uint64_t modelRevision { 1 };
};

struct WaveshaperNodeModel {
    FlatCurveModel curve { "CycleV2WaveshaperCurve" };
    bool enabled { true };
    float preGain { 0.5f };
    float postGain { 0.5f };
    int oversampling { 1 };

    void syncFromNode(const Node& node);
};

struct ImpulseResponseNodeModel {
    FlatCurveModel curve { "CycleV2ImpulseResponseCurve" };
    bool enabled { true };
    float size { 0.5f };
    float postGain { 0.5f };
    float highPass { 0.5f };

    void syncFromNode(const Node& node);
};

struct GuideCurveNodeModel {
    FlatCurveModel curve { "CycleV2GuideCurve" };
    bool enabled { true };
    float noise { 0.5f };
    float dcOffset { 0.5f };
    float phase { 0.5f };

    void syncFromNode(const Node& node);
};

class CurveNodeModelState : public NodeModelState {
public:
    static std::shared_ptr<const CurveNodeModelState> copyOf(
            const FlatCurveModel& model,
            uint64_t revision,
            var editorState = {});
    static std::shared_ptr<const CurveNodeModelState> copyOf(
            const EnvelopeNodeModel& model,
            uint64_t revision,
            var editorState = {});

    String schemaId() const override;
    int schemaVersion() const override;
    uint64_t revision() const override;
    var writeJSON() const override;
    bool equals(const NodeModelState& other) const override;

    const FlatCurveModel* flatCurve() const { return flatCurveState.get(); }
    const EnvelopeNodeModel* envelope() const { return envelopeState.get(); }
    const var& editorJSON() const { return editorState; }

private:
    friend class CurveNodeDomainCodec;

    CurveNodeModelState(
            String schema,
            int version,
            uint64_t revision,
            std::shared_ptr<const FlatCurveModel> modelState,
            var editorState = {});
    CurveNodeModelState(
            String schema,
            int version,
            uint64_t revision,
            std::shared_ptr<const EnvelopeNodeModel> modelState,
            var editorState = {});

    String schema;
    int version {};
    uint64_t modelRevision {};
    std::shared_ptr<const FlatCurveModel> flatCurveState;
    std::shared_ptr<const EnvelopeNodeModel> envelopeState;
    var editorState;
};

class CurveNodeDomainCodec : public NodeModelCodec {
public:
    explicit CurveNodeDomainCodec(NodeKind kindToUse);

    String schemaId() const override;
    int currentVersion() const override;
    NodeModelStatePtr createDefault() const override;
    NodeModelStatePtr readJSON(const var& value, String& error) const override;

private:
    NodeKind kind;
};

}
