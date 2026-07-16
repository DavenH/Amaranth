#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelHostContext.h>

#include "../../UI/RenderInvalidationAccumulator.h"

class CommonGL;
class GLPanelRenderer;

namespace CycleV2 {

class TrimeshPanel2D;
class TrimeshPanel3D;
class TrimeshInteractor2D;
class TrimeshInteractor3D;

class TrimeshPanelHosts : private RenderInvalidationTarget {
public:
    TrimeshPanelHosts(
            TrimeshPanel2D& panel2D,
            TrimeshPanel3D& panel3D,
            TrimeshInteractor2D& interactor2D,
            TrimeshInteractor3D& interactor3D);
    ~TrimeshPanelHosts();

    Component* getPanel3DHostComponent();
    Component* getPanel3DHostComponentIfCreated();
    Component* getPanel2DHostComponent();
    Component* getPanel2DHostComponentIfCreated();

    void setCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const MouseCursor&)> cursorCallback,
            std::function<void(Point<float>)> hoverCallback);
    void initialiseSharedGlResources();
    void releaseSharedGlResources();
    void renderPanel3D(Rectangle<float> bounds, float scaleFactor);
    void renderPanel2D(Rectangle<float> bounds, float scaleFactor);

    bool isPanel3DHostInitialised() const { return panel3DHostInitialised; }
    bool isPanel2DHostInitialised() const { return panel2DHostInitialised; }

    RenderInvalidationAccumulator::Diagnostics invalidationDiagnostics() const {
        return invalidation.diagnostics();
    }

private:
    class PanelHostComponent;

    PanelHostCallbacks createPanelHostCallbacks();
    void initialisePanel3DHost();
    void initialisePanel2DHost();
    void updatePanelHostPeers();
    void renderPanel(Panel& panel, Rectangle<float> bounds, float scaleFactor);
    void requestPanelInvalidation(Panel* panel, PanelDirtyState::Flag flag);
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    TrimeshPanel2D& panel2D;
    TrimeshPanel3D& panel3D;
    TrimeshInteractor2D& interactor2D;
    TrimeshInteractor3D& interactor3D;
    std::unique_ptr<PanelHostComponent> panel2DHost;
    std::unique_ptr<PanelHostComponent> panel3DHost;
    std::unique_ptr<GLPanelRenderer> panel2DRenderer;
    std::unique_ptr<GLPanelRenderer> panel3DRenderer;
    CommonGL* panel2DGfx {};
    CommonGL* panel3DGfx {};
    std::function<void()> panelHostRepaintCallback;
    std::function<void(const MouseCursor&)> panelHostCursorCallback;
    std::function<void(Point<float>)> panelHostHoverCallback;
    RenderInvalidationAccumulator invalidation;
    bool panel3DHostInitialised {};
    bool panel2DHostInitialised {};
    std::atomic<bool> sharedGlResourcesInitialised {};
    std::atomic<bool> panel3DVisible {};
    std::atomic<bool> panel2DVisible {};
};

}
