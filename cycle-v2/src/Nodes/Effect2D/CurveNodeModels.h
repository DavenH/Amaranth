#pragma once

#include "../../Graph/NodeGraph.h"
#include "Effect2DMeshState.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/Mesh.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace CycleV2 {

struct FlatCurveVertex {
    uint64_t id {};
    float x {};
    float y {};
    float curve {};
};

class FlatCurveModel {
public:
    static constexpr int currentVersion = 1;

    explicit FlatCurveModel(String name = "CycleV2FlatCurveModel");
    ~FlatCurveModel();

    bool replaceVertices(std::vector<FlatCurveVertex> nextVertices);
    bool selectVertex(uint64_t vertexId);
    bool loadSnapshot(const String& snapshot);
    bool adoptEditedVertices(const std::vector<Effect2DVertexState>& editedVertices);
    String snapshot() const;

    const Mesh& getMesh() const { return mesh; }
    Mesh& getMesh() { return mesh; }
    const std::vector<FlatCurveVertex>& getVertices() const { return vertices; }
    std::optional<uint64_t> selectedVertexId() const { return selection; }
    uint64_t revision() const { return modelRevision; }
    void setPublicationRevision(uint64_t revisionToUse) { modelRevision = juce::jmax<uint64_t>(1, revisionToUse); }

private:
    static bool validate(const std::vector<FlatCurveVertex>& vertices);
    void rebuildMesh();

    Mesh mesh;
    std::vector<FlatCurveVertex> vertices;
    std::optional<uint64_t> selection;
    uint64_t modelRevision { 1 };
};

class EnvelopeNodeModel {
public:
    static constexpr int currentVersion = 1;

    EnvelopeNodeModel();
    ~EnvelopeNodeModel();

    bool loadSnapshot(const String& snapshot);
    bool syncFromNode(const Node& node);
    String snapshot() const;
    bool selectCube(int cubeIndex);

    EnvelopeMesh& getMesh() { return mesh; }
    const EnvelopeMesh& getMesh() const { return mesh; }
    uint64_t revision() const { return modelRevision; }
    int selectedCubeIndex() const { return selection; }
    void setPublicationRevision(uint64_t revisionToUse) { modelRevision = juce::jmax<uint64_t>(1, revisionToUse); }

    bool logarithmic {};
    float red { 0.5f };
    float blue { 0.5f };
    bool redLinked { true };
    bool blueLinked { true };

private:
    bool applyEnvelopePayload(const String& payload);

    EnvelopeMesh mesh;
    int selection { -1 };
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
