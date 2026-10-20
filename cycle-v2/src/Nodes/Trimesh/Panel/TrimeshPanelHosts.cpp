#include "Nodes/Trimesh/Panel/TrimeshPanelHosts.h"

#include "Nodes/Trimesh/Panel/TrimeshInteractor2D.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel2D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"

#include <UI/AmaranthLookAndFeel.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/PanelInputHostComponent.h>
#include <UI/Panels/ScopedGLScissor.h>
#include <UI/Panels/ZoomPanel.h>

#include <cmath>

#include "UI/NativeCursorRefresh.h"

namespace CycleV2 {

namespace {

AmaranthLookAndFeel& trimeshScrollbarLookAndFeel() {
    static AmaranthLookAndFeel lookAndFeel(nullptr, false);
    return lookAndFeel;
}

void styleZoomScrollbars(ZoomPanel& zoomPanel) {
    zoomPanel.getComponent(true)->setLookAndFeel(&trimeshScrollbarLookAndFeel());
    zoomPanel.getComponent(false)->setLookAndFeel(&trimeshScrollbarLookAndFeel());
}

bool sameZoomValue(float first, float second) {
    return std::abs(first - second) <= 1.0e-6f;
}

}

namespace TrimeshPanelInvalidation {

constexpr uint32_t Owner = 1u << 0;
constexpr uint32_t Panel3DBake = 1u << 1;

}

class TrimeshPanelHosts::PanelHostComponent final : public PanelInputHostComponent {
public:
    explicit PanelHostComponent(Panel& targetPanel) :
            PanelInputHostComponent(targetPanel) {}

    void paint(Graphics&) override {}

private:
    bool deleteKeyPressed() override {
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
    ,   curveHost       (panel2DToHost, *this)
    ,   invalidation    (*this) {
    panel2D.setZoomChangedCallback([this](bool sourceNeedsRefresh) {
        frequencyZoomChanged(true, sourceNeedsRefresh);
    });
    panel3D.setZoomChangedCallback([this](bool sourceNeedsRefresh) {
        frequencyZoomChanged(false, sourceNeedsRefresh);
    });
}

TrimeshPanelHosts::~TrimeshPanelHosts() {
    panel2D.setZoomChangedCallback(nullptr);
    panel3D.setZoomChangedCallback(nullptr);
    releaseSharedGlResources();
}

void TrimeshPanelHosts::configureFrequencyViewport(
        Range<float> limits,
        float defaultStart,
        bool enteringMagnitude) {
    const bool limitsChanged = limits != frequencyLimits;
    frequencyLimits = limits;
    if (limitsChanged || (enteringMagnitude && !frequencyZoomCustomized)) {
        frequencyStart = defaultStart;
        frequencyWidth = 1.f - defaultStart;
        frequencyZoomCustomized = false;
    }

    const ScopedValueSetter<bool> syncing(synchronizingFrequencyZoom, true);
    if (panel3DHostInitialised) {
        panel3D.configureViewport(limits);
        const bool viewChanged = applyFrequencyViewportTo3D();
        if (limitsChanged || viewChanged) {
            panel3D.zoomUpdated(interactor3D.getUpdateSource());
        }
    }
    if (panel2DHostInitialised) {
        panel2D.configureViewport(limits);
        const bool viewChanged = applyFrequencyViewportTo2D();
        if (limitsChanged || viewChanged) {
            panel2D.zoomUpdated(interactor2D.getUpdateSource());
        }
    }
}

void TrimeshPanelHosts::frequencyZoomChanged(
        bool from2D,
        bool sourceNeedsRefresh) {
    if (synchronizingFrequencyZoom) {
        return;
    }

    Panel& sourcePanel = from2D
            ? static_cast<Panel&>(panel2D)
            : static_cast<Panel&>(panel3D);
    const int sourceId = from2D
            ? interactor2D.getUpdateSource()
            : interactor3D.getUpdateSource();
    const ZoomRect& source = from2D
            ? panel2D.getZoomPanel()->rect
            : panel3D.getZoomPanel()->rect;
    const float nextStart = from2D ? source.x : 1.f - source.y - source.h;
    const float nextWidth = from2D ? source.w : source.h;
    if (sameZoomValue(nextStart, frequencyStart)
            && sameZoomValue(nextWidth, frequencyWidth)) {
        if (sourceNeedsRefresh) {
            const ScopedValueSetter<bool> syncing(synchronizingFrequencyZoom, true);
            sourcePanel.zoomUpdated(sourceId);
        }
        return;
    }

    frequencyStart = nextStart;
    frequencyWidth = nextWidth;
    frequencyZoomCustomized = true;

    const ScopedValueSetter<bool> syncing(synchronizingFrequencyZoom, true);
    const bool targetChanged = from2D
            ? applyFrequencyViewportTo3D()
            : applyFrequencyViewportTo2D();
    if (targetChanged) {
        Panel& targetPanel = from2D
                ? static_cast<Panel&>(panel3D)
                : static_cast<Panel&>(panel2D);
        const int targetId = from2D
                ? interactor3D.getUpdateSource()
                : interactor2D.getUpdateSource();
        targetPanel.zoomUpdated(targetId);
    }
    if (sourceNeedsRefresh) {
        sourcePanel.zoomUpdated(sourceId);
    }
}

bool TrimeshPanelHosts::applyFrequencyViewportTo2D() {
    if (!panel2DHostInitialised) {
        return false;
    }

    ZoomPanel* zoom = panel2D.getZoomPanel();
    ZoomRect& rect = zoom->rect;
    if (sameZoomValue(rect.x, frequencyStart)
            && sameZoomValue(rect.w, frequencyWidth)) {
        return false;
    }
    rect.x = frequencyStart;
    rect.w = frequencyWidth;
    zoom->panelZoomChanged(false);
    return true;
}

bool TrimeshPanelHosts::applyFrequencyViewportTo3D() {
    if (!panel3DHostInitialised) {
        return false;
    }

    ZoomPanel* zoom = panel3D.getZoomPanel();
    ZoomRect& rect = zoom->rect;
    const float verticalStart = 1.f - frequencyStart - frequencyWidth;
    if (sameZoomValue(rect.y, verticalStart)
            && sameZoomValue(rect.h, frequencyWidth)) {
        return false;
    }
    rect.y = verticalStart;
    rect.h = frequencyWidth;
    zoom->panelZoomChanged(false);
    return true;
}

Component* TrimeshPanelHosts::getPanel3DHostComponent() {
    initialisePanel3DHost();
    return panel3D.getZoomPanel();
}

Component* TrimeshPanelHosts::getPanel3DHostComponentIfCreated() const {
    return panel3DHostInitialised ? panel3D.getZoomPanel() : nullptr;
}

Component* TrimeshPanelHosts::getPanel2DHostComponent() {
    initialisePanel2DHost();
    return panel2D.getZoomPanel();
}

Component* TrimeshPanelHosts::getPanel2DHostComponentIfCreated() const {
    return panel2DHostInitialised ? panel2D.getZoomPanel() : nullptr;
}

void TrimeshPanelHosts::setDelegate(TrimeshPanelHostDelegate* nextDelegate) {
    delegate = nextDelegate;

    PanelHostCallbacks callbacks = createPanelHostCallbacks();
    panel3D.setHostCallbacks(callbacks);

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
            panel3D.getZoomPanel()->setMouseCursor(cursor);
            showNativeCursorForPanel(*panel3DHost, cursor);
        }

        if (delegate != nullptr && (panel == &panel2D || panel == &panel3D)) {
            delegate->setTrimeshPanelCursor(
                    panel == &panel2D
                            ? TrimeshPanelHostKind::Panel2D
                            : TrimeshPanelHostKind::Panel3D,
                    cursor);
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
    {
        const ScopedValueSetter<bool> syncing(synchronizingFrequencyZoom, true);
        panel3D.configureViewport(frequencyLimits);
        panel3DHostInitialised = true;
        applyFrequencyViewportTo3D();
    }
    styleZoomScrollbars(*panel3D.getZoomPanel());
    interactor3D.updateIntercepts();
}

void TrimeshPanelHosts::initialisePanel2DHost() {
    curveHost.component();
}

void TrimeshPanelHosts::initialiseCurvePanel(Component* component) {
    panel2D.initWithExternalComponent(component);
    const ScopedValueSetter<bool> syncing(synchronizingFrequencyZoom, true);
    panel2D.configureViewport(frequencyLimits);
    panel2DHostInitialised = true;
    applyFrequencyViewportTo2D();
    styleZoomScrollbars(*panel2D.getZoomPanel());
}

void TrimeshPanelHosts::updateCurvePanelZoom(bool) {
    applyFrequencyViewportTo2D();
}

void TrimeshPanelHosts::repaintCurvePanel() {
    if (delegate != nullptr) {
        delegate->requestTrimeshPanelRepaint();
    }
}

void TrimeshPanelHosts::setCurvePanelCursor(const MouseCursor& cursor) {
    panel2D.getZoomPanel()->setMouseCursor(cursor);
    if (delegate != nullptr) {
        delegate->setTrimeshPanelCursor(TrimeshPanelHostKind::Panel2D, cursor);
    }
}

bool TrimeshPanelHosts::deleteCurvePanelSelection() {
    interactor2D.deleteSelected();
    return true;
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

    panel3D.bakeTexturesNextRepaint();
    panel2D.bakeTexturesNextRepaint();
    sharedGlResourcesInitialised = true;
}

void TrimeshPanelHosts::releaseSharedGlResources() {
    curveHost.releaseSharedGlResources();
    if (panel3DRenderer != nullptr) {
        panel3DRenderer->clearResources();
    }
    panel3D.setPanelRenderer(nullptr);
    panel3DRenderer = nullptr;
    panel3D.setGraphicsHelper(nullptr);
    panel3DGfx = nullptr;
    sharedGlResourcesInitialised = false;
    panel3DVisible = false;
}

void TrimeshPanelHosts::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    panel3DVisible = !bounds.isEmpty();
    invalidation.notifyAvailabilityChanged();
    initialiseSharedGlResources();
    renderPanel(panel3D, bounds.withWidth((float) panel3DHost->getWidth())
            .withHeight((float) panel3DHost->getHeight()), scaleFactor);
}

void TrimeshPanelHosts::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
    curveHost.render(bounds, bounds, scaleFactor);
}

bool TrimeshPanelHosts::renderPreview2D(Rectangle<float> bounds, float scaleFactor, uint64_t contentRevision) {
    return curveHost.renderPreview(bounds, scaleFactor, true, contentRevision, 0, 0);
}

bool TrimeshPanelHosts::paintPreview2D(Graphics& graphics, Rectangle<float> bounds) const {
    return curveHost.paintPreviewSnapshot(graphics, bounds);
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
    ScopedGLScissor scissor(bounds, scaleFactor);
    panel.render(context);
}

void TrimeshPanelHosts::requestPanelInvalidation(
        Panel* sourcePanel,
        PanelDirtyState::Flag flag) {
    MessageManager* messageManager = MessageManager::getInstanceWithoutCreating();
    if (flag == PanelDirtyState::Flag::Overlay
            && messageManager != nullptr
            && messageManager->isThisTheMessageThread()) {
        if (delegate != nullptr) {
            delegate->requestTrimeshPanelRepaint();
        }
        return;
    }

    uint32_t categories = TrimeshPanelInvalidation::Owner;
    const bool requiresBake = flag == PanelDirtyState::Flag::StaticVisual
            || flag == PanelDirtyState::Flag::SurfaceCache
            || flag == PanelDirtyState::Flag::Resource
            || flag == PanelDirtyState::Flag::Full;
    if (requiresBake && sourcePanel == &panel3D) {
        categories |= TrimeshPanelInvalidation::Panel3DBake;
    }
    invalidation.request(categories);
}

uint32_t TrimeshPanelHosts::availableRenderInvalidations() const {
    uint32_t available = TrimeshPanelInvalidation::Owner;
    if (sharedGlResourcesInitialised && panel3DVisible) {
        available |= TrimeshPanelInvalidation::Panel3DBake;
    }
    return available;
}

void TrimeshPanelHosts::flushRenderInvalidations(uint32_t categories) {
    if ((categories & TrimeshPanelInvalidation::Panel3DBake) != 0) {
        panel3D.bakeTexturesNextRepaint();
    }
    if ((categories & TrimeshPanelInvalidation::Owner) != 0 && delegate != nullptr) {
        delegate->requestTrimeshPanelRepaint();
    }
}

}
