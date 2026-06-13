#pragma once

#include "../../Graph/NodeGraph.h"
#include "../Trimesh/TrimeshPanelEnvironment.h"
#include "../Trimesh/TrimeshNodeModel.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
#include <Curve/Rasterization/Rasterizer/FXRasterizer.h>
#include <Inter/Interactor2D.h>
#include <UI/Panels/Panel2D.h>

#include <functional>
#include <memory>

class CommonGL;
class GLPanelRenderer;

namespace CycleV2 {

class Effect2DPanelBridge {
public:
    struct PreviewVertex {
        float x {};
        float y {};
        float curve {};
    };

    explicit Effect2DPanelBridge(NodeKind nodeKind);
    ~Effect2DPanelBridge();

    void syncFromNode(const Node& node);
    Component* getPanelHostComponent();
    Component* getPanelHostComponentIfCreated();
    void setPanelHostCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback);
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
    std::vector<TrimeshVertexParameter> selectedVertexParameters() const;
    bool setSelectedVertexParameter(const String& parameterId, float normalizedValue);
    bool selectedEnvelopeMarkerState(bool loopMarker) const;
    void toggleSelectedEnvelopeMarker(bool loopMarker);

private:
    class EffectPanel;
    class PanelHostComponent;

    void initialisePanelHost();
    void initialiseMesh();
    void addVertex(float x, float y, float curve = 0.f);
    void setEnvelopeDefaultMorphVariant(int cubeIndex, float redHighAmp, float blueHighAmp);
    PanelHostCallbacks createPanelHostCallbacks();
    void applyPanelSettings();
    void captureRenderedPanelImage(
            Rectangle<float> bounds,
            float scaleFactor,
            Image& destination,
            bool& hasVisibleContent) const;

    NodeKind kind;
    TrimeshPanelEnvironment environment;
    EnvelopeMesh mesh;
    std::unique_ptr<EffectPanel> panel;
    std::unique_ptr<PanelHostComponent> panelHost;
    std::unique_ptr<GLPanelRenderer> panelRenderer;
    CommonGL* panelGfx {};
    std::function<void()> panelHostRepaintCallback;
    std::function<void(const MouseCursor&)> panelHostCursorCallback;
    juce::String lastSyncedNodeId;
    mutable juce::CriticalSection previewImageLock;
    juce::Image previewImage;
    mutable juce::CriticalSection expandedImageLock;
    juce::Image expandedImage;
    bool previewImageHasVisibleContent {};
    bool expandedImageHasVisibleContent {};
    bool panelHostInitialised {};
    bool sharedGlResourcesInitialised {};
    bool effectEnabled { true };
    float firstControlValue { 0.5f };
    float secondControlValue { 0.5f };
    float thirdControlValue { 0.5f };
    int menuControlId {};
    bool envelopeLogarithmicValue {};
    bool envelopeRedLinked { true };
    bool envelopeBlueLinked { true };
};

}
