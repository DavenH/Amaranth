#include "TrimeshPanelHosts.h"

#include "TrimeshInteractor2D.h"
#include "TrimeshInteractor3D.h"
#include "TrimeshPanel2D.h"
#include "TrimeshPanel3D.h"

#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>

namespace CycleV2 {

class TrimeshPanelHosts::PanelHostComponent :
        public Component {
public:
    explicit PanelHostComponent(Panel& targetPanel) :
            panel(targetPanel) {
        setPaintingIsUnclipped(false);
        setInterceptsMouseClicks(true, true);
        setOpaque(false);
        setWantsKeyboardFocus(true);
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
        grabKeyboardFocus();
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

    bool keyPressed(const KeyPress& key) override {
        if (key != KeyPress::deleteKey && key != KeyPress::backspaceKey) {
            return false;
        }
        if (auto* interactor = dynamic_cast<TrimeshInteractor2D*>(panel.getInteractor().get())) {
            interactor->deleteSelected();
            return true;
        }
        if (auto* interactor = dynamic_cast<TrimeshInteractor3D*>(panel.getInteractor().get())) {
            interactor->deleteSelected();
            return true;
        }
        return false;
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

TrimeshPanelHosts::TrimeshPanelHosts(
        TrimeshPanel2D& panel2DToHost,
        TrimeshPanel3D& panel3DToHost,
        TrimeshInteractor2D& interactor2DToHost,
        TrimeshInteractor3D& interactor3DToHost) :
        panel2D         (panel2DToHost)
    ,   panel3D         (panel3DToHost)
    ,   interactor2D    (interactor2DToHost)
    ,   interactor3D    (interactor3DToHost) {
}

TrimeshPanelHosts::~TrimeshPanelHosts() {
    releaseSharedGlResources();
}

Component* TrimeshPanelHosts::getPanel3DHostComponent() {
    initialisePanel3DHost();
    return panel3DHost.get();
}

Component* TrimeshPanelHosts::getPanel3DHostComponentIfCreated() {
    return panel3DHostInitialised ? panel3DHost.get() : nullptr;
}

Component* TrimeshPanelHosts::getPanel2DHostComponent() {
    initialisePanel2DHost();
    return panel2DHost.get();
}

Component* TrimeshPanelHosts::getPanel2DHostComponentIfCreated() {
    return panel2DHostInitialised ? panel2DHost.get() : nullptr;
}

void TrimeshPanelHosts::setCallbacks(
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

PanelHostCallbacks TrimeshPanelHosts::createPanelHostCallbacks() {
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

void TrimeshPanelHosts::initialisePanel3DHost() {
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

void TrimeshPanelHosts::initialisePanel2DHost() {
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

void TrimeshPanelHosts::updatePanelHostPeers() {
    if (panel3DHost == nullptr || panel2DHost == nullptr) {
        return;
    }

    panel3DHost->setHoverPeer(panel2DHost.get());
    panel2DHost->setHoverPeer(panel3DHost.get());
    panel3DHost->setOutsideHoverCallback(panelHostHoverCallback);
    panel2DHost->setOutsideHoverCallback(panelHostHoverCallback);
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
}

void TrimeshPanelHosts::renderPanel3D(Rectangle<float> bounds, float scaleFactor) {
    initialiseSharedGlResources();
    renderPanel(panel3D, bounds, scaleFactor);
}

void TrimeshPanelHosts::renderPanel2D(Rectangle<float> bounds, float scaleFactor) {
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

}
