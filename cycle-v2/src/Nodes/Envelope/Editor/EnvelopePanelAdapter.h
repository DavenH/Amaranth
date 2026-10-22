#pragma once

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/CurvePanelAdapterTypes.h"

#include <JuceHeader.h>

#include <vector>
#include <optional>

class VertCube;

namespace CycleV2 {

class EnvelopePanelAdapter final {
public:
    EnvelopePanelAdapter();
    ~EnvelopePanelAdapter();

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

    void setMorph(const MorphPosition& position);
    bool setPreviewMorphPosition(std::optional<MorphPosition> position);
    MorphPosition effectiveMorphPosition() const { return { morphYellow, morphRed, morphBlue }; }
    int dimensionCount() const { return dimensions; }
    void setDimensionCount(int count) { dimensions = count; }
    void setLogarithmic(bool logarithmic);
    void setAxisLinks(bool redLinked, bool blueLinked);
    bool logarithmic() const { return model.logarithmic; }
    bool redLinked() const { return morphRedLinked; }
    bool blueLinked() const { return morphBlueLinked; }

    const String& lastNodeId() const { return syncedNodeId; }
private:
    void refreshEffectiveMorph();

    int dimensions { 2 };
    MorphPosition authoredMorph { 0.f, 0.5f, 0.5f };
    std::optional<MorphPosition> previewMorph;
    float morphYellow {};
    float morphRed { 0.5f };
    float morphBlue { 0.5f };
    bool morphRedLinked { true };
    bool morphBlueLinked { true };
    String syncedNodeId;
    EnvelopeNodeModel model;
    NodeModelStatePtr syncedModel;
};

}
