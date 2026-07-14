#pragma once

#include "../../Graph/NodeGraph.h"
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

    const Mesh& getMesh() const { return mesh; }
    Mesh& getMesh() { return mesh; }
    const std::vector<FlatCurveVertex>& getVertices() const { return vertices; }
    std::optional<CurveVertexId> selectedVertexId() const { return selection; }
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
    bool selectCube(std::optional<EnvelopeCubeId> cubeId);

    EnvelopeMesh& getMesh() { return mesh; }
    const EnvelopeMesh& getMesh() const { return mesh; }
    uint64_t revision() const { return modelRevision; }
    std::optional<EnvelopeCubeId> selectedCubeId() const { return selection; }
    const std::vector<EnvelopeCubeId>& getCubeIds() const { return cubeIds; }
    void setPublicationRevision(uint64_t revisionToUse) { modelRevision = juce::jmax<uint64_t>(1, revisionToUse); }

    bool logarithmic {};
    float red { 0.5f };
    float blue { 0.5f };
    bool redLinked { true };
    bool blueLinked { true };

private:
    bool applyEnvelopePayload(const String& payload);
    EnvelopeCubeId nextIdentity() const;
    void rebuildIdentityMap();

    EnvelopeMesh mesh;
    std::vector<EnvelopeCubeId> cubeIds;
    std::optional<EnvelopeCubeId> selection;
    std::unordered_map<VertCube*, EnvelopeCubeId> identitiesByCube;
    std::unordered_map<EnvelopeCubeId, VertCube*> cubesByIdentity;
    EnvelopeCubeId nextCubeIdentity { 1 };
    String committedPayload;
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

class CurveNodeModelCodec {
public:
    static String snapshotParameterId();
    static String revisionParameterId();
    static String defaultSnapshot(NodeKind kind);
    static String snapshotFromParameters(const std::vector<NodeParameter>& parameters);
    static uint64_t revisionFromParameters(const std::vector<NodeParameter>& parameters);
    static std::vector<Effect2DVertexState> flatVerticesFromParameters(
            const std::vector<NodeParameter>& parameters,
            NodeKind kind);
    static String envelopePayloadFromParameters(const std::vector<NodeParameter>& parameters);
};

}
