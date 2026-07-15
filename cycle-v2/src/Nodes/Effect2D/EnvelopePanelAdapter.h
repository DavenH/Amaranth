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
    bool syncFromNode(const Node& node);
    void initialiseDefaultMesh();
    String serializedMeshState();
    String serializedModelSnapshot(VertCube* selectedCube, uint64_t publicationRevision);
    std::vector<CurvePreviewVertex> previewVertices();
    bool registerMeshEdit();

    void setMorph(float red, float blue);
    void setLogarithmic(bool logarithmic);
    void setAxisLinks(bool redLinked, bool blueLinked);
    bool logarithmic() const { return model.logarithmic; }
    bool redLinked() const { return model.redLinked; }
    bool blueLinked() const { return model.blueLinked; }

    const String& lastNodeId() const { return syncedNodeId; }
    const String& lastModelSnapshot() const { return syncedModelSnapshot; }
    const String& lastMeshState() const { return syncedMeshState; }

private:
    EnvelopeNodeModel model;
    String syncedNodeId;
    String syncedModelSnapshot;
    String syncedMeshState;
};

}

