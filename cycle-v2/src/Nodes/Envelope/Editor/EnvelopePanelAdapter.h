#pragma once

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/CurvePanelAdapterTypes.h"
#include "UI/Editors/EditorMorphPosition.h"

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
    void syncAuthoredMorph(const Node& node);
    void resetDocumentPresentation();
    VertCube* selectedMeshCube() const { return model.selectedMeshCube(); }
    void initialiseDefaultMesh();
    String serializedMeshState();
    NodeModelStatePtr modelPublication(VertCube* selectedCube, uint64_t publicationRevision);
    std::vector<CurvePreviewVertex> previewVertices();
    bool registerMeshEdit();

    void setMorph(const MorphPosition& position);
    bool setEditorMorphPosition(int axis, float value);
    void setMorphEditGestureActive(bool active);
    bool restoreEditorMorphGesture();
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
    void refreshEffectiveMorph(bool preserveValues);

    int dimensions { 2 };
    MorphPosition authoredMorph { 0.f, 0.5f, 0.5f };
    EditorMorphPosition editorMorph;
    std::optional<EditorMorphPosition::Checkpoint> morphGestureStart;
    bool morphEditGestureActive {};
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
