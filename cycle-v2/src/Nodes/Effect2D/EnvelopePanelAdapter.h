#pragma once

#include "CurveNodeModels.h"
#include "CurvePanelAdapterTypes.h"

#include <JuceHeader.h>

#include <vector>

class VertCube;

namespace CycleV2 {

class EnvelopePanelAdapter final {
public:
    EnvelopePanelAdapter();

    EnvelopeMesh& mesh() { return model.getMesh(); }
    const EnvelopeMesh& mesh() const { return model.getMesh(); }
    bool needsNodeSync(const Node& node) const;
    bool syncFromNode(const Node& node);
    VertCube* selectedMeshCube() const { return model.selectedMeshCube(); }
    void initialiseDefaultMesh();
    String serializedMeshState();
    NodeModelStatePtr modelPublication(VertCube* selectedCube, uint64_t publicationRevision);
    std::vector<CurvePreviewVertex> previewVertices();
    bool registerMeshEdit();

    void setMorph(float red, float blue);
    void setLogarithmic(bool logarithmic);
    void setAxisLinks(bool redLinked, bool blueLinked);
    bool logarithmic() const { return model.logarithmic; }
    bool redLinked() const { return model.redLinked; }
    bool blueLinked() const { return model.blueLinked; }

    const String& lastNodeId() const { return syncedNodeId; }
    const String& lastMeshState() const { return syncedMeshState; }

private:
    EnvelopeNodeModel model;
    String syncedNodeId;
    uint64_t syncedModelRevision {};
    String syncedMeshState;
};

}
