#include "Nodes/Trimesh/Panel/TrimeshPanelHosts.h"

#include "Nodes/Trimesh/Panel/TrimeshInteractor2D.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel2D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"

#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/PanelInputHostComponent.h>

namespace CycleV2 {

namespace TrimeshPanelInvalidation {

constexpr uint32_t Owner = 1u << 0;
constexpr uint32_t Panel2DBake = 1u << 1;
constexpr uint32_t Panel3DBake = 1u << 2;

}

class TrimeshPanelHosts::PanelHostComponent final : public PanelInputHostComponent {
public:
    explicit PanelHostComponent(Panel& targetPanel) :
            PanelInputHostComponent(targetPanel) {}

    void paint(Graphics&) override {}

private:
    bool deleteKeyPressed() override {
        if (auto* interactor = dynamic_cast<TrimeshInteractor2D*>(panelInteractor())) {
            interactor->deleteSelected();
            return true;
        }
        if (auto* interactor = dynamic_cast<TrimeshInteractor3D*>(panelInteractor())) {
            interactor->deleteSelected();
            return true;
        }
        return false;
    }
};

TrimeshPanelHosts::TrimeshPanelHosts(
        TrimeshPanel2D& panel2DToHost,
        TrimeshPanel3D& panel3DToHost,
        TrimeshInteractor2D& interactor2DToHost,
        TrimeshInteractor3D& interactor3DToHost) :
        panel2D         (panel2DToHost)
    ,   panel3D         (panel3DToHost)
    ,   interactor2D    (interactor2DToHost)
    ,   interactor3D    (interactor3DToHost)
    ,   invalidation    (*this) {
}

TrimeshPanelHosts::~TrimeshPanelHosts() {
    releaseSharedGlResources();
}

Component* TrimeshPanelHosts::getPanel3DHostComponent() {
    initialisePanel3DHost();
    return panel3DHost.get();
}

Component* TrimeshPanelHosts::getPanel3DHostComponentIfCreated() const {
    return panel3DHostInitialised ? panel3DHost.get() : nullptr;
}

Component* TrimeshPanelHosts::getPanel2DHostComponent() {
    initialisePanel2DHost();
    return panel2DHost.get();
}

Component* TrimeshPanelHosts::getPanel2DHostComponentIfCreated() const {
    return panel2DHostInitialised ? panel2DHost.get() : nullptr;
}

void TrimeshPanelHosts::setDelegate(TrimeshPanelHostDelegate* nextDelegate) {
    delegate = nextDelegate;

    PanelHostCallbacks callbacks = createPanelHostCallbacks();
    panel3D.setHostCallbacks(callbacks);
    panel2D.setHostCallbacks(callbacks);

}

void TrimeshPanelHosts::clearDelegate(TrimeshPanelHostDelegate* delegateToClear) {
    if (delegate != delegateToClear) {
        return;
    }

    setDelegate(nullptr);
}

PanelHostCallbacks TrimeshPanelHosts::createPanelHostCallbacks() {
    PanelHostCallbacks callbacks;
    callbacks.setRepaintCallback([this](Panel* panel, PanelDirtyState::Flag flag) {
        requestPanelInvalidation(panel, flag);
    });
    callbacks.setCursorCallback([this](Panel* panel, const MouseCursor& cursor) {
        if (panel == &panel3D && panel3DHost != nullptr) {
            panel3DHost->setMouseCursor(cursor);
        }

        if (panel == &panel2D && panel2DHost != nullptr) {
            panel2DHost->setMouseCursor(cursor);
        }
    });

    return callbacks;
}

void TrimeshPanelHosts::initialisePanel3DHost() {
    if (panel3DHostInitialised) {
        return;
    }

    panel3DHost = std::make_unique<PanelHostComponent>(panel3D);
    panel3D.setSharedCanvasMode(true);
    panel3D.setInteractorMouseListenerEnabled(false);
    panel3D.initWithExternalComponent(panel3DHost.get());
    interactor3D.updateIntercepts();
    panel3DHostInitialised = true;
}

void TrimeshPanelHosts::initialisePanel2DHost() {
    if (panel2DHostInitialised) {
        return;
    }

    panel2DHost = std::make_unique<PanelHostComponent>(panel2D);
    panel2D.setInteractorMouseListenerEnabled(false);
    panel2D.initWithExternalComponent(panel2DHost.get());
    panel2DHostInitialised = true;
}

void TrimeshPanelHosts::initialiseSharedGlResources() {
    initialisePanel3DHost();
    initialisePanel2DHost();

    if (sharedGlResourcesInitialised) {
        return;
    }

    panel3DGfx = new CommonGL(&panel3D);
    panel3DRenderer = std::make_unique<GLPanelRenderer>(panel3DGfx);
    panel3D.setGraphicsHelper(panel3DGfx);
    panel3D.setPanelRenderer(panel3DRenderer.get());
    panel3DGfx->initializeTextures();

    panel2DGfx = new CommonGL(&panel2D);
    panel2DRenderer = std::make_unique<GLPanelRenderer>(panel2DGfx);
    panel2D.setGraphicsHelper(panel2DGfx);
    panel2D.setPanelRenderer(panel2DRenderer.get());
    panel2DGfx->initializeTextures();

    panel3D.bakeTexturesNextRepaint();
    panel2D.bakeTexturesNextRepaint();
    sharedGlResourcesInitialised = true;
}

void TrimeshPanelHosts::releaseSharedGlResources() {
    panel2D.setPanelRenderer(nullptr);
    panel3D.setPanelRenderer(nullptr);
    panel2DRenderer = nullptr;
    panel3DRenderer = nullptr;
    panel2D.setGraphicsHelper(nullptr);
    panel3D.setGraphicsHelper(nullptr);
    panel2DGfx = nullptr;
    panel3DGfx = nullptr;
    sharedGlResourcesInitialised = false;
    panel2DVisible = false;
    panel3DVisible = false;
}

void TrimeshPanelHosts::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    panel3DVisible = !bounds.isEmpty();
    invalidation.notifyAvailabilityChanged();
    initialiseSharedGlResources();
    renderPanel(panel3D, bounds, scaleFactor);
}

void TrimeshPanelHosts::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
    panel2DVisible = !bounds.isEmpty();
    invalidation.notifyAvailabilityChanged();
    initialiseSharedGlResources();
    renderPanel(panel2D, bounds, scaleFactor);
}

void TrimeshPanelHosts::renderPanel(
        Panel& panel,
        Rectangle<float> bounds,
        float scaleFactor) {
    if (bounds.isEmpty()) {
        return;
    }

    PanelHostContext context;
    context.bounds = bounds;
    context.clip = bounds;
    context.scaleFactor = scaleFactor;
    context.visible = true;
    context.callbacks = createPanelHostCallbacks();
    panel.render(context);
}

void TrimeshPanelHosts::requestPanelInvalidation(
        Panel* sourcePanel,
        PanelDirtyState::Flag flag) {
    uint32_t categories = TrimeshPanelInvalidation::Owner;
    const bool requiresBake = flag == PanelDirtyState::Flag::StaticVisual
            || flag == PanelDirtyState::Flag::SurfaceCache
            || flag == PanelDirtyState::Flag::Resource
            || flag == PanelDirtyState::Flag::Full;
    if (requiresBake && sourcePanel == &panel2D) {
        categories |= TrimeshPanelInvalidation::Panel2DBake;
    }
    if (requiresBake && sourcePanel == &panel3D) {
        categories |= TrimeshPanelInvalidation::Panel3DBake;
    }
    invalidation.request(categories);
}

uint32_t TrimeshPanelHosts::availableRenderInvalidations() const {
    uint32_t available = TrimeshPanelInvalidation::Owner;
    if (sharedGlResourcesInitialised && panel2DVisible) {
        available |= TrimeshPanelInvalidation::Panel2DBake;
    }
    if (sharedGlResourcesInitialised && panel3DVisible) {
        available |= TrimeshPanelInvalidation::Panel3DBake;
    }
    return available;
}

void TrimeshPanelHosts::flushRenderInvalidations(uint32_t categories) {
    if ((categories & TrimeshPanelInvalidation::Panel2DBake) != 0) {
        panel2D.bakeTexturesNextRepaint();
    }
    if ((categories & TrimeshPanelInvalidation::Panel3DBake) != 0) {
        panel3D.bakeTexturesNextRepaint();
    }
    if ((categories & TrimeshPanelInvalidation::Owner) != 0 && delegate != nullptr) {
        delegate->requestTrimeshPanelRepaint();
    }
}

}
