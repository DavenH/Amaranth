#pragma once

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/CurvePanelAdapterTypes.h"

#include <JuceHeader.h>

#include <vector>

class Vertex;

namespace CycleV2 {

class FlatCurvePanelAdapter final {
public:
    explicit FlatCurvePanelAdapter(NodeKind nodeKind);
    explicit FlatCurvePanelAdapter(bool guideResource);

    NodeKind kind() const { return nodeKind; }
    Mesh& mesh() { return model.getMesh(); }
    const Mesh& mesh() const { return model.getMesh(); }
    bool needsNodeSync(const Node& node) const;
    bool syncFromNode(const Node& node);
    bool syncFromGuideResource(const GuideCurveResource& guide);
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
    bool guideResource {};
    FlatCurveModel model { "CycleV2FlatCurve" };
    String syncedNodeId;
    NodeModelStatePtr syncedModel;
    String syncedMeshState;
};

}
