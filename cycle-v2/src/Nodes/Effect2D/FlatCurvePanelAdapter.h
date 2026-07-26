#pragma once

#include "CurveNodeModels.h"
#include "CurvePanelAdapterTypes.h"

#include <JuceHeader.h>

#include <vector>

class Vertex;

namespace CycleV2 {

class FlatCurvePanelAdapter final {
public:
    explicit FlatCurvePanelAdapter(NodeKind nodeKind);

    NodeKind kind() const { return nodeKind; }
    Mesh& mesh() { return model.getMesh(); }
    const Mesh& mesh() const { return model.getMesh(); }
    bool needsNodeSync(const Node& node) const;
    bool syncFromNode(const Node& node);
    Vertex* selectedMeshVertex() const { return model.selectedMeshVertex(); }
    void initialiseDefaultMesh();
    String serializedMeshState();
    NodeModelStatePtr modelPublication(Vertex* selectedVertex, uint64_t publicationRevision);
    std::vector<CurvePreviewVertex> previewVertices();
    bool registerMeshEdit();

    const String& lastNodeId() const { return syncedNodeId; }
    const String& lastMeshState() const { return syncedMeshState; }

private:
    void addVertex(float x, float y, float curve = 0.f);

    NodeKind nodeKind;
    FlatCurveModel model { "CycleV2FlatCurve" };
    String syncedNodeId;
    uint64_t syncedModelRevision {};
    String syncedMeshState;
};

}
