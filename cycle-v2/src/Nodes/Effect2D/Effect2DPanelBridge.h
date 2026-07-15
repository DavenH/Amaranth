#pragma once

#include "../../Graph/NodeGraph.h"
#include "CurvePanelInfrastructure.h"
#include "EnvelopePanelAdapter.h"
#include "FlatCurvePanelAdapter.h"
#include "../Trimesh/TrimeshPanelEnvironment.h"
#include "../Trimesh/TrimeshNodeModel.h"

#include <UI/Panels/PanelHostContext.h>

#include <functional>
#include <memory>
#include <variant>

class CommonGL;
class GLPanelRenderer;

namespace CycleV2 {

class Effect2DPanelBridge {
public:
    using PreviewVertex = CurvePreviewVertex;

    explicit Effect2DPanelBridge(NodeKind nodeKind);
    ~Effect2DPanelBridge();

    void syncFromNode(const Node& node);
    Component* getPanelHostComponent();
    Component* getPanelHostComponentIfCreated();
    void setPanelHostCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback);
    void setMeshEditedCallback(std::function<void()> callback);
    void setControlValues(bool enabled, float firstValue, float secondValue, float thirdValue, int menuId);
    void setEnvelopeLogarithmic(bool shouldUseLogarithmicScale);
    void setEnvelopeAxisLinks(bool redLinked, bool blueLinked);
    void renderPanel(Rectangle<float> bounds, Rectangle<float> clipBounds, float scaleFactor);
    void renderPreviewSnapshot(Rectangle<float> bounds, float scaleFactor);
    bool paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const;
    bool paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const;
    void initialiseSharedGlResources();
    void releaseSharedGlResources();
    int vertexCountForAutomation() const;
    var automationState() const;
    std::vector<PreviewVertex> previewVertices();
    String serializedMeshState();
    String serializedModelSnapshot();
    uint64_t modelRevision() const { return publicationRevision; }
    std::vector<TrimeshVertexParameter> selectedVertexParameters() const;
    bool setSelectedVertexParameter(const String& parameterId, float normalizedValue);
    bool selectedEnvelopeMarkerState(bool loopMarker) const;
    void toggleSelectedEnvelopeMarker(bool loopMarker);

private:
    class EffectPanel;

    void initialiseMesh();
    bool notifyMeshEdited();
    void synchronizeModelSelection();
    void applyPanelSettings();
    Mesh& adapterMesh();
    const Mesh& adapterMesh() const;
    FlatCurvePanelAdapter* flatAdapter();
    const FlatCurvePanelAdapter* flatAdapter() const;
    EnvelopePanelAdapter* envelopeAdapter();
    const EnvelopePanelAdapter* envelopeAdapter() const;

    struct ControlState {
        bool enabled { true };
        float first { 0.5f };
        float second { 0.5f };
        float third { 0.5f };
        int menuId {};
    };

    NodeKind kind;
    CurvePanelEnvironment environment;
    std::variant<FlatCurvePanelAdapter, EnvelopePanelAdapter> adapter;
    std::unique_ptr<EffectPanel> panel;
    std::unique_ptr<CurvePanelHost> host;
    std::function<void()> meshEditedCallback;
    ControlState controls;
    uint64_t publicationRevision { 1 };
    juce::String lastSyncedNodeId;
    juce::String lastSyncedModelSnapshot;
    juce::String lastSyncedMeshState;
};

}
