#pragma once

#include "Nodes/Trimesh/Panel/TrimeshPanelHostDelegate.h"
#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"

#include <atomic>
#include <memory>

#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelHostContext.h>

#include "UI/RenderInvalidationAccumulator.h"

class CommonGL;
class GLPanelRenderer;

namespace CycleV2 {

class TrimeshPanel2D;
class TrimeshPanel3D;
class TrimeshInteractor2D;
class TrimeshInteractor3D;

class TrimeshPanelHosts : private RenderInvalidationTarget, private CurvePanelHostDelegate {
public:
    TrimeshPanelHosts(
            TrimeshPanel2D& panel2D,
            TrimeshPanel3D& panel3D,
            TrimeshInteractor2D& interactor2D,
            TrimeshInteractor3D& interactor3D);
    ~TrimeshPanelHosts();

    Component* getPanel3DHostComponent();
    Component* getPanel3DHostComponentIfCreated() const;
    Component* getPanel2DHostComponent();
    Component* getPanel2DHostComponentIfCreated() const;

    void setDelegate(TrimeshPanelHostDelegate* delegate);
    void clearDelegate(TrimeshPanelHostDelegate* delegate);
    void configureFrequencyViewport(
            Range<float> limits,
            float defaultStart,
            bool enteringMagnitude);
    void initialiseSharedGlResources();
    void releaseSharedGlResources();
    void renderPanel3D(Rectangle<float> bounds, float scaleFactor);
    void renderPanel2D(Rectangle<float> bounds, float scaleFactor);
    bool renderPreview2D(Rectangle<float> bounds, float scaleFactor, uint64_t contentRevision);
    bool paintPreview2D(Graphics& graphics, Rectangle<float> bounds) const;
    uint64_t previewSnapshotRevision() const { return curveHost.previewSnapshotRevision(); }

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
    void initialiseCurvePanel(Component* component) override;
    void updateCurvePanelZoom(bool) override;
    void prepareCurvePanel() override {}
    void synchronizeCurvePanelSelection() override {}
    void repaintCurvePanel() override;
    void setCurvePanelCursor(const MouseCursor& cursor) override;
    void beginEdit() override {}
    void publishIntermediateRevision() override {}
    void commitEdit() override {}
    bool deleteCurvePanelSelection() override;
    void frequencyZoomChanged(bool from2D, bool sourceNeedsRefresh);
    bool applyFrequencyViewportTo2D();
    bool applyFrequencyViewportTo3D();
    void renderPanel(Panel& panel, Rectangle<float> bounds, float scaleFactor);
    void requestPanelInvalidation(Panel* panel, PanelDirtyState::Flag flag);
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    TrimeshPanel2D& panel2D;
    TrimeshPanel3D& panel3D;
    TrimeshInteractor2D& interactor2D;
    TrimeshInteractor3D& interactor3D;
    CurvePanelHost curveHost;
    std::unique_ptr<PanelHostComponent> panel3DHost;
    std::unique_ptr<GLPanelRenderer> panel3DRenderer;
    CommonGL* panel3DGfx {};
    TrimeshPanelHostDelegate* delegate {};
    RenderInvalidationAccumulator invalidation;
    Range<float> frequencyLimits { 0.f, 1.f };
    float frequencyStart {};
    float frequencyWidth { 1.f };
    bool frequencyZoomCustomized {};
    bool synchronizingFrequencyZoom {};
    bool panel3DHostInitialised {};
    bool panel2DHostInitialised {};
    std::atomic<bool> sharedGlResourcesInitialised {};
    std::atomic<bool> panel3DVisible {};
};

}
