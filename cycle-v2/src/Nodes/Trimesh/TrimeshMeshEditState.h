#pragma once

#include "../../Graph/NodeGraph.h"

#include <vector>

class Mesh;
class NodeParameter;

namespace CycleV2 {

struct TrimeshVertexEdit {
    int vertexIndex { -1 };
    int valueIndex { -1 };
    float value {};
    String sourceId;

    bool operator==(const TrimeshVertexEdit& other) const;
    bool operator!=(const TrimeshVertexEdit& other) const { return !(*this == other); }
};

class TrimeshMeshEditState {
public:
    static TrimeshMeshEditState fromNode(const Node& node);
    static TrimeshMeshEditState fromParameters(const std::vector<NodeParameter>& parameters);
    static TrimeshMeshEditState fromMesh(Mesh& mesh);
    static String canonicalVertexParameterId(int vertexIndex, const String& field);
    static String fieldForVertexValueIndex(int valueIndex);

    bool applyTo(Mesh& mesh) const;
    bool empty() const { return vertexEdits.empty(); }
    const std::vector<TrimeshVertexEdit>& getVertexEdits() const { return vertexEdits; }

    bool operator==(const TrimeshMeshEditState& other) const;
    bool operator!=(const TrimeshMeshEditState& other) const { return !(*this == other); }

private:
    static bool parseVertexEditParameter(
            const NodeParameter& parameter,
            TrimeshVertexEdit& edit);
    static bool fieldToVertexValueIndex(const String& field, int& valueIndex);

    std::vector<TrimeshVertexEdit> vertexEdits;
};

}
