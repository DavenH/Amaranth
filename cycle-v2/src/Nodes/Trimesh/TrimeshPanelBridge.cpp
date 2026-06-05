#include "TrimeshPanelBridge.h"

#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <Curve/Mesh/Vertex.h>
#include <UI/MiscGraphics.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/PanelHostContext.h>

namespace CycleV2 {

namespace {

class PanelHostComponent :
        public Component {
public:
    explicit PanelHostComponent(Panel& targetPanel) :
            panel(targetPanel) {
        setPaintingIsUnclipped(true);
        setInterceptsMouseClicks(true, true);
        setOpaque(false);
    }

    void setHoverPeer(PanelHostComponent* peer) {
        hoverPeer = peer;
    }

    void setOutsideHoverCallback(std::function<void(Point<float>)> callback) {
        outsideHoverCallback = std::move(callback);
    }

    void paint(Graphics&) override {}

    void mouseEnter(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);
        mouseInside = true;

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseEnter(localEvent);
        }
    }

    void mouseMove(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);

        if (!getLocalBounds().contains(localEvent.getPosition())) {
            exitIfNeeded(localEvent);
            if (!forwardMouseMoveToPeer(event)) {
                panel.setPanelMouseCursor(MouseCursor::NormalCursor);
            }
            return;
        }

        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseMove(localEvent);
        }
    }

    void mouseDown(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDown(localEvent);
        }
    }

    void mouseDrag(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);

        if (!getLocalBounds().contains(localEvent.getPosition())) {
            forwardMouseMoveToPeer(event);
            return;
        }

        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDrag(localEvent);
        }
    }

    void mouseUp(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseUp(localEvent);
        }
    }

    void mouseExit(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);
        exitIfNeeded(localEvent);
    }

    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override {
        const MouseEvent localEvent = currentMouseEvent(event);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseWheelMove(localEvent, wheel);
        }
    }

    void resized() override {
        panel.panelResized();
    }

private:
    MouseEvent currentMouseEvent(const MouseEvent& event) const {
        return localMouseEvent(event, getLocalPoint(nullptr, Desktop::getMousePosition()).toFloat());
    }

    MouseEvent localMouseEvent(const MouseEvent& event, Point<float> localPosition) const {
        const Point<float> localMouseDown = event.eventComponent != nullptr
                ? getLocalPoint(event.eventComponent, event.mouseDownPosition).toFloat()
                : localPosition;

        return MouseEvent(
                event.source,
                localPosition,
                event.mods,
                event.pressure,
                event.orientation,
                event.rotation,
                event.tiltX,
                event.tiltY,
                const_cast<PanelHostComponent*>(this),
                event.originalComponent,
                event.eventTime,
                localMouseDown,
                event.mouseDownTime,
                event.getNumberOfClicks(),
                event.mouseWasDraggedSinceMouseDown());
    }

    void enterIfNeeded(const MouseEvent& event) {
        if (mouseInside) {
            return;
        }

        mouseInside = true;

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseEnter(event);
        }
    }

    void exitIfNeeded(const MouseEvent& event) {
        if (!mouseInside) {
            return;
        }

        mouseInside = false;

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseExit(event);
        }
    }

    bool forwardMouseMoveToPeer(const MouseEvent& event) {
        if (hoverPeer == nullptr) {
            return false;
        }

        if (hoverPeer->mouseMoveFromPeer(event)) {
            setMouseCursor(hoverPeer->getMouseCursor());
            return true;
        }

        if (outsideHoverCallback != nullptr) {
            outsideHoverCallback(Desktop::getMousePosition().toFloat());
        }

        return false;
    }

    bool mouseMoveFromPeer(const MouseEvent& event) {
        const MouseEvent localEvent = currentMouseEvent(event);

        if (!getLocalBounds().contains(localEvent.getPosition())) {
            exitIfNeeded(localEvent);
            return false;
        }

        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseMove(localEvent);
        }

        return true;
    }

    Panel& panel;
    PanelHostComponent* hoverPeer {};
    std::function<void(Point<float>)> outsideHoverCallback;
    bool mouseInside {};
};

}

TrimeshPanelBridge::TrimeshPanelBridge() :
        console         (&repo)
    ,   interactor2D    (&repo, "CycleV2TrimeshInteractor2D",
                         Dimensions(Vertex::Phase, Vertex::Amp, Vertex::Time, Vertex::Red, Vertex::Blue))
    ,   interactor3D    (&repo, "CycleV2TrimeshInteractor3D")
    ,   panel2D         (&repo)
    ,   panel3D         (&repo, dataSource) {
    repo.add(new AppConstants(&repo));
    repo.add(new MiscGraphics(&repo));
    repo.add(new Settings(&repo));
    repo.add(new MeshLibrary(&repo));

    auto& constants = repo.get<AppConstants>("AppConstants");
    constants.setConstant(Constants::FontFace, String("Verdana"));

    repo.get<MiscGraphics>("MiscGraphics").init();

    auto& settings = repo.get<Settings>("Settings");
    settings.initialiseSettings();
    settings.createPropertiesFile(
            File::getSpecialLocation(File::tempDirectory)
                    .getChildFile("cycle-v2-trimesh-bridge-settings.xml")
                    .getFullPathName());
    settings.getGlobalSetting(AppSettings::DrawScales) = false;
    repo.setConsole(&console);
    repo.setMorphPositioner(&morphPositioner);

    interactor2D.init();
    interactor3D.init();
    interactor2D.stopTimer();
    interactor3D.stopTimer();
    interactor2D.setRasterizer(&rasterizer);
    interactor3D.setRasterizer(&rasterizer);
    interactor2D.setMeshEditedCallback([this](bool sourceIs3D) { refreshAfterMeshEdit(sourceIs3D); });
    interactor3D.setMeshEditedCallback([this](bool sourceIs3D) { refreshAfterMeshEdit(sourceIs3D); });
    panel2D.setInteractor(&interactor2D);
    panel3D.setInteractor(&interactor3D);
}

TrimeshPanelBridge::~TrimeshPanelBridge() {
    interactor2D.stopTimer();
    interactor3D.stopTimer();
    panel2D.setInteractor(nullptr);
    panel3D.setInteractor(nullptr);
    interactor2D.setRasterizer(nullptr);
    interactor3D.setRasterizer(nullptr);
    releaseSharedGlResources();
}

void TrimeshPanelBridge::syncFromNode(
        const Node& node,
        int rows,
        int columns) {
    const uint64_t previousRevision = model.getRevision();
    const int previousPrimaryAxis = model.getPrimaryViewAxis();
    const MorphPosition previousMorph = model.getMorphPosition();

    model.syncFromNode(node);
    morphPositioner.setPosition(model.getMorphPosition(), model.getPrimaryViewAxis());
    syncPrimaryAxisContext();

    const bool modelChanged = previousRevision != model.getRevision();
    const bool primaryAxisChanged = previousPrimaryAxis != model.getPrimaryViewAxis();
    const MorphPosition& nextMorph = model.getMorphPosition();
    const bool yellowChanged = previousMorph.time.getTargetValue() != nextMorph.time.getTargetValue();
    const bool redChanged = previousMorph.red.getTargetValue() != nextMorph.red.getTargetValue();
    const bool blueChanged = previousMorph.blue.getTargetValue() != nextMorph.blue.getTargetValue();
    const bool morphChanged = yellowChanged || redChanged || blueChanged;

    if (!modelChanged
            && !morphChanged
            && !primaryAxisChanged
            && lastSyncedRevision == model.getRevision()
            && lastRows == rows
            && lastColumns == columns) {
        return;
    }

    TrimeshChange change;
    change.kind = primaryAxisChanged
            ? TrimeshChangeKind::PrimaryAxis
            : (morphChanged ? TrimeshChangeKind::Morph : TrimeshChangeKind::Layout);
    change.yellowChanged = yellowChanged;
    change.redChanged = redChanged;
    change.blueChanged = blueChanged;
    change.primaryViewAxis = model.getPrimaryViewAxis();

    const TrimeshInvalidationResult invalidated = invalidation.invalidate(change);
    dataSource.rebuild(model, rows, columns);
    updateRasterizer(
            invalidated.refresh2DPanel,
            invalidated.refresh3DGeometry || lastRows != rows || lastColumns != columns);
    lastSyncedRevision = model.getRevision();
    lastRows = rows;
    lastColumns = columns;
}

void TrimeshPanelBridge::refreshAfterMeshEdit(bool sourceIs3D) {
    const TrimeshInvalidationResult invalidated = invalidation.invalidate({
            TrimeshChangeKind::MeshEdit,
            false,
            false,
            false,
            sourceIs3D,
            model.getPrimaryViewAxis()
    });

    model.markMeshEdited();
    syncPrimaryAxisContext();
    dataSource.rebuild(model, lastRows, lastColumns);
    updateRasterizer(invalidated.refresh2DPanel, invalidated.refresh3DGeometry);
}

void TrimeshPanelBridge::syncPrimaryAxisContext() {
    interactor3D.setPrimaryViewAxis(model.getPrimaryViewAxis());
    panel3D.setPrimaryViewAxis(model.getPrimaryViewAxis());
}

void TrimeshPanelBridge::updateRasterizer(bool refresh2DPanel, bool refresh3DGeometry) {
    auto& request = rasterizer.getRequest();
    request.cyclic = true;
    request.xMinimum = -0.05f;
    request.xMaximum = 1.05f;
    request.morph = model.getMorphPosition();
    request.primaryViewDimension = model.getPrimaryViewAxis();
    request.dims = interactor2D.dims;
    request.scalingMode = Rasterization::PointScalingMode::Unipolar;
    request.calcDepthDimensions = true;
    request.lowResCurves = false;
    request.batchMode = true;

    rasterizer.setWrapsEnds(true);
    rasterizer.setMesh(&model.getMeshForPanel());
    interactor2D.setMesh(&model.getMeshForPanel());
    interactor3D.setMesh(&model.getMeshForPanel());
    rasterizer.updateWaveform();

    if (panel3DHostInitialised && refresh3DGeometry) {
        interactor3D.updateIntercepts();
        panel3D.bakeTexturesNextRepaint();
        panel3D.requestRepaint();
    }

    if (refresh2DPanel && panel2DHostInitialised) {
        interactor2D.performUpdate(Update);
        panel2D.requestRepaint();
    }
}

Component* TrimeshPanelBridge::getPanel3DHostComponent() {
    initialisePanel3DHost();
    return panel3DHost.get();
}

Component* TrimeshPanelBridge::getPanel3DHostComponentIfCreated() {
    return panel3DHostInitialised ? panel3DHost.get() : nullptr;
}

Component* TrimeshPanelBridge::getPanel2DHostComponent() {
    initialisePanel2DHost();
    return panel2DHost.get();
}

Component* TrimeshPanelBridge::getPanel2DHostComponentIfCreated() {
    return panel2DHostInitialised ? panel2DHost.get() : nullptr;
}

void TrimeshPanelBridge::setPanelHostCallbacks(
        std::function<void()> repaintCallback,
        std::function<void(const MouseCursor&)> cursorCallback,
        std::function<void(Point<float>)> hoverCallback) {
    panelHostRepaintCallback = std::move(repaintCallback);
    panelHostCursorCallback = std::move(cursorCallback);
    panelHostHoverCallback = std::move(hoverCallback);

    PanelHostCallbacks callbacks = createPanelHostCallbacks();
    panel3D.setHostCallbacks(callbacks);
    panel2D.setHostCallbacks(callbacks);
    updatePanelHostPeers();
}

PanelHostCallbacks TrimeshPanelBridge::createPanelHostCallbacks() {
    PanelHostCallbacks callbacks;
    callbacks.setRepaintCallback([this](Panel*, PanelDirtyState::Flag) {
        if (panelHostRepaintCallback != nullptr) {
            panelHostRepaintCallback();
        }
    });
    callbacks.setCursorCallback([this](Panel* panel, const MouseCursor& cursor) {
        if (panelHostCursorCallback != nullptr) {
            panelHostCursorCallback(cursor);
        }

        if (panel == &panel3D && panel3DHost != nullptr) {
            panel3DHost->setMouseCursor(cursor);
        }

        if (panel == &panel2D && panel2DHost != nullptr) {
            panel2DHost->setMouseCursor(cursor);
        }
    });

    return callbacks;
}

void TrimeshPanelBridge::initialisePanel3DHost() {
    if (panel3DHostInitialised) {
        return;
    }

    panel3DHost = std::make_unique<PanelHostComponent>(panel3D);
    panel3D.setSharedCanvasMode(true);
    panel3D.initWithExternalComponent(panel3DHost.get());
    panel3DHost->removeMouseListener(&interactor3D);
    interactor3D.stopTimer();
    interactor3D.updateIntercepts();
    panel3DHostInitialised = true;
    updatePanelHostPeers();
}

void TrimeshPanelBridge::initialisePanel2DHost() {
    if (panel2DHostInitialised) {
        return;
    }

    panel2DHost = std::make_unique<PanelHostComponent>(panel2D);
    panel2D.initWithExternalComponent(panel2DHost.get());
    panel2DHost->removeMouseListener(&interactor2D);
    interactor2D.stopTimer();
    panel2DHostInitialised = true;
    updatePanelHostPeers();
}

void TrimeshPanelBridge::updatePanelHostPeers() {
    if (panel3DHost == nullptr || panel2DHost == nullptr) {
        return;
    }

    auto* host3D = static_cast<PanelHostComponent*>(panel3DHost.get());
    auto* host2D = static_cast<PanelHostComponent*>(panel2DHost.get());
    host3D->setHoverPeer(host2D);
    host2D->setHoverPeer(host3D);
    host3D->setOutsideHoverCallback(panelHostHoverCallback);
    host2D->setOutsideHoverCallback(panelHostHoverCallback);
}

void TrimeshPanelBridge::initialiseSharedGlResources() {
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

void TrimeshPanelBridge::releaseSharedGlResources() {
    panel2D.setPanelRenderer(nullptr);
    panel3D.setPanelRenderer(nullptr);
    panel2DRenderer = nullptr;
    panel3DRenderer = nullptr;
    panel2D.setGraphicsHelper(nullptr);
    panel3D.setGraphicsHelper(nullptr);
    panel2DGfx = nullptr;
    panel3DGfx = nullptr;
    sharedGlResourcesInitialised = false;
}

void TrimeshPanelBridge::setDisplayDomain(PortDomain domain) {
    setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
}

void TrimeshPanelBridge::setRenderProfile(TrimeshRenderProfile profile) {
    panel3D.setRenderProfile(profile);
    panel2D.setRenderProfile(profile);
}

void TrimeshPanelBridge::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    initialiseSharedGlResources();
    renderPanel(panel3D, bounds, scaleFactor);
}

void TrimeshPanelBridge::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
    initialiseSharedGlResources();
    renderPanel(panel2D, bounds, scaleFactor);
}

void TrimeshPanelBridge::renderPanel(Panel& panel, Rectangle<float> bounds, float scaleFactor) {
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

void TrimeshPanelBridge::NodeMorphPositioner::setPosition(
        const MorphPosition& position,
        int primaryAxis) {
    morph = position;
    primaryDimension = primaryAxis;
}

float TrimeshPanelBridge::NodeMorphPositioner::getValue(int dim) {
    switch (dim) {
        case Vertex::Time: return morph.time.getCurrentValue();
        case Vertex::Red:  return morph.red.getCurrentValue();
        case Vertex::Blue: return morph.blue.getCurrentValue();
        default:           return 0.f;
    }
}

}
