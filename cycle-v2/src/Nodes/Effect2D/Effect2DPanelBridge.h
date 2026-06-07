#pragma once

#include "../../Graph/NodeGraph.h"
#include "../Trimesh/TrimeshPanelEnvironment.h"

#include <Curve/Mesh/Mesh.h>
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
    explicit Effect2DPanelBridge(NodeKind nodeKind);
    ~Effect2DPanelBridge();

    void syncFromNode(const Node& node);
    Component* getPanelHostComponent();
    Component* getPanelHostComponentIfCreated();
    void setPanelHostCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback);
    void renderPanel(Rectangle<float> bounds, float scaleFactor);
    void initialiseSharedGlResources();
    void releaseSharedGlResources();

private:
    class EffectPanel;
    class PanelHostComponent;

    void initialisePanelHost();
    void initialiseMesh();
    void addVertex(float x, float y, float curve = 0.f);
    PanelHostCallbacks createPanelHostCallbacks();

    NodeKind kind;
    TrimeshPanelEnvironment environment;
    Mesh mesh;
    std::unique_ptr<EffectPanel> panel;
    std::unique_ptr<PanelHostComponent> panelHost;
    std::unique_ptr<GLPanelRenderer> panelRenderer;
    CommonGL* panelGfx {};
    std::function<void()> panelHostRepaintCallback;
    std::function<void(const MouseCursor&)> panelHostCursorCallback;
    juce::String lastSyncedNodeId;
    bool panelHostInitialised {};
    bool sharedGlResourcesInitialised {};
};

}
