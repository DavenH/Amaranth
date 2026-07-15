#include "CurvePanelInfrastructure.h"

#include <Inter/Interactor.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelHostContext.h>

using namespace gl;

namespace CycleV2 {

void CurvePanelSnapshotCache::publish(Image nextImage, bool hasVisibleContent) {
    const ScopedLock scopedLock(lock);
    image = std::move(nextImage);
    visibleContent = hasVisibleContent;
}

bool CurvePanelSnapshotCache::paint(
        Graphics& graphics,
        Rectangle<float> bounds,
        bool resample) const {
    const ScopedLock scopedLock(lock);
    if (!image.isValid() || !visibleContent) {
        return false;
    }

    Graphics::ScopedSaveState state(graphics);
    graphics.reduceClipRegion(bounds.toNearestInt());
    if (resample) {
        graphics.setImageResamplingQuality(Graphics::mediumResamplingQuality);
    }
    graphics.drawImage(image, bounds);
    return true;
}

namespace {

class ScopedCurvePanelScissor {
public:
    ScopedCurvePanelScissor(Rectangle<float> bounds, float scaleFactor) {
        glGetBooleanv(GL_SCISSOR_TEST, &wasEnabled);
        glGetIntegerv(GL_SCISSOR_BOX, previousBox);

        GLint viewport[4] {};
        glGetIntegerv(GL_VIEWPORT, viewport);
        const int x = jmax(0, roundToInt(bounds.getX() * scaleFactor));
        const int y = jmax(0, viewport[3] - roundToInt(bounds.getBottom() * scaleFactor));
        const int width = jmax(1, roundToInt(bounds.getWidth() * scaleFactor));
        const int height = jmax(1, roundToInt(bounds.getHeight() * scaleFactor));
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, width, height);
    }

    ~ScopedCurvePanelScissor() {
        if (wasEnabled != 0) {
            glEnable(GL_SCISSOR_TEST);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }

        glScissor(previousBox[0], previousBox[1], previousBox[2], previousBox[3]);
    }

private:
    GLboolean wasEnabled {};
    GLint previousBox[4] {};
};

}

class CurvePanelHost::HostComponent final : public Component {
public:
    HostComponent(
            Panel& targetPanel,
            CurvePanelSnapshotCache& snapshot,
            std::function<bool()> publishEditCallback,
            std::function<void()> synchronizeSelectionCallback) :
            panel(targetPanel)
        ,   snapshot(snapshot)
        ,   publishEdit(std::move(publishEditCallback))
        ,   synchronizeSelection(std::move(synchronizeSelectionCallback)) {
        setPaintingIsUnclipped(false);
        setInterceptsMouseClicks(true, true);
        setOpaque(false);
        setWantsKeyboardFocus(true);
    }

    void paint(Graphics& graphics) override {
        snapshot.paint(graphics, getLocalBounds().toFloat(), true);
    }

    void mouseEnter(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);
        mouseInside = true;
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseEnter(localEvent);
        }
    }

    void mouseMove(const MouseEvent& event) override {
        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseMove(localEvent);
        }
    }

    void mouseDown(const MouseEvent& event) override {
        if (!event.mods.isLeftButtonDown() && !event.mods.isRightButtonDown()) {
            activePointer = false;
            return;
        }
        activePointer = true;
        grabKeyboardFocus();
        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDown(localEvent);
        }
    }

    void mouseDoubleClick(const MouseEvent& event) override {
        if (!event.mods.isLeftButtonDown()) {
            return;
        }
        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDoubleClick(localEvent);
        }
        publishEdit();
    }

    void mouseDrag(const MouseEvent& event) override {
        if (!activePointer || !event.mods.isLeftButtonDown()) {
            return;
        }
        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDrag(localEvent);
        }
    }

    void mouseUp(const MouseEvent& event) override {
        if (!activePointer) {
            return;
        }
        activePointer = false;
        const MouseEvent localEvent = currentMouseEvent(event);
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseUp(localEvent);
        }
        if (!publishEdit()) {
            synchronizeSelection();
        }
    }

    void mouseExit(const MouseEvent& event) override {
        exitIfNeeded(currentMouseEvent(event));
    }

    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override {
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseWheelMove(currentMouseEvent(event), wheel);
        }
    }

    bool keyPressed(const KeyPress& key) override {
        if (key != KeyPress::deleteKey && key != KeyPress::backspaceKey) {
            return false;
        }
        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->eraseSelected();
            interactor->performUpdate(Update);
            publishEdit();
            return true;
        }
        return false;
    }

    void resized() override {
        panel.panelResized();
    }

private:
    MouseEvent currentMouseEvent(const MouseEvent& event) const {
        const Point<float> localPosition = event.eventComponent == this
                ? event.position
                : getLocalPoint(event.eventComponent, event.position).toFloat();
        const Point<float> localMouseDown = event.eventComponent == this
                ? event.mouseDownPosition
                : event.eventComponent != nullptr
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
                const_cast<HostComponent*>(this),
                event.originalComponent,
                event.eventTime,
                localMouseDown,
                event.mouseDownTime,
                event.getNumberOfClicks(),
                event.mouseWasDraggedSinceMouseDown());
    }

    void enterIfNeeded(const MouseEvent& event) {
        if (!mouseInside) {
            mouseInside = true;
            if (Interactor* interactor = panel.getInteractor().get()) {
                interactor->mouseEnter(event);
            }
        }
    }

    void exitIfNeeded(const MouseEvent& event) {
        if (mouseInside) {
            mouseInside = false;
            if (Interactor* interactor = panel.getInteractor().get()) {
                interactor->mouseExit(event);
            }
        }
    }

    Panel& panel;
    CurvePanelSnapshotCache& snapshot;
    std::function<bool()> publishEdit;
    std::function<void()> synchronizeSelection;
    bool mouseInside {};
    bool activePointer {};
};

CurvePanelHost::CurvePanelHost(
        Panel& panelToHost,
        std::function<void(Component*)> initialisePanelCallback,
        std::function<void(bool)> updateZoomCallback,
        std::function<void()> preparePanelCallback,
        std::function<bool()> publishEditCallback,
        std::function<void()> synchronizeSelectionCallback) :
        panel(panelToHost)
    ,   initialisePanel(std::move(initialisePanelCallback))
    ,   updateZoom(std::move(updateZoomCallback))
    ,   preparePanel(std::move(preparePanelCallback))
    ,   publishEdit(std::move(publishEditCallback))
    ,   synchronizeSelection(std::move(synchronizeSelectionCallback)) {
}

CurvePanelHost::~CurvePanelHost() {
    releaseSharedGlResources();
}

Component* CurvePanelHost::component() {
    initialiseComponent();
    return hostComponent.get();
}

Component* CurvePanelHost::componentIfCreated() {
    return componentInitialised ? hostComponent.get() : nullptr;
}

void CurvePanelHost::setCallbacks(
        std::function<void()> repaintCallbackToUse,
        std::function<void(const MouseCursor&)> cursorCallbackToUse) {
    repaintCallback = std::move(repaintCallbackToUse);
    cursorCallback = std::move(cursorCallbackToUse);
    panel.setHostCallbacks(callbacks());
}

void CurvePanelHost::render(Rectangle<float> bounds, Rectangle<float>, float scaleFactor) {
    if (bounds.isEmpty()) {
        return;
    }
    initialiseSharedGlResources();
    PanelHostContext context;
    context.bounds = bounds;
    context.clip = bounds;
    context.scaleFactor = scaleFactor;
    context.visible = true;
    context.callbacks = callbacks();
    panel.setHostContext(context);
    panel.panelResized();
    preparePanel();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    ScopedCurvePanelScissor scissor(bounds, scaleFactor);
    panel.render();

    Image nextImage;
    bool hasVisibleContent {};
    captureRenderedPanelImage(bounds, scaleFactor, nextImage, hasVisibleContent);
    expandedSnapshot.publish(std::move(nextImage), hasVisibleContent);

    auto safeHost = Component::SafePointer<Component>(hostComponent.get());
    auto callback = repaintCallback;
    MessageManager::callAsync([safeHost, callback] {
        if (safeHost != nullptr) {
            safeHost->repaint();
        }
        if (callback != nullptr) {
            callback();
        }
    });
}

void CurvePanelHost::renderPreview(Rectangle<float> bounds, float scaleFactor) {
    if (bounds.isEmpty()) {
        return;
    }
    initialiseSharedGlResources();
    const ZoomRect interactiveZoom = panel.getZoomPanel()->rect;
    PanelHostContext context;
    context.bounds = bounds;
    context.clip = bounds;
    context.scaleFactor = scaleFactor;
    context.visible = true;
    context.callbacks = callbacks();
    panel.setHostContext(context);
    panel.panelResized();
    updateZoom(true);
    preparePanel();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    ScopedCurvePanelScissor scissor(bounds, scaleFactor);
    panel.render();
    panel.getZoomPanel()->rect = interactiveZoom;

    Image nextImage;
    bool hasVisibleContent {};
    captureRenderedPanelImage(bounds, scaleFactor, nextImage, hasVisibleContent);
    previewSnapshot.publish(std::move(nextImage), hasVisibleContent);
}

bool CurvePanelHost::paintExpandedSnapshot(Graphics& graphics, Rectangle<float> bounds) const {
    return expandedSnapshot.paint(graphics, bounds, true);
}

bool CurvePanelHost::paintPreviewSnapshot(Graphics& graphics, Rectangle<float> bounds) const {
    return previewSnapshot.paint(graphics, bounds, false);
}

void CurvePanelHost::releaseSharedGlResources() {
    panel.setPanelRenderer(nullptr);
    panelRenderer = nullptr;
    panel.setGraphicsHelper(nullptr);
    panelGfx = nullptr;
    sharedGlResourcesInitialised = false;
}

void CurvePanelHost::initialiseComponent() {
    if (componentInitialised) {
        return;
    }
    hostComponent = std::make_unique<HostComponent>(
            panel, expandedSnapshot, publishEdit, synchronizeSelection);
    initialisePanel(hostComponent.get());
    hostComponent->removeMouseListener(panel.getInteractor().get());
    componentInitialised = true;
    preparePanel();
}

void CurvePanelHost::initialiseSharedGlResources() {
    initialiseComponent();
    if (sharedGlResourcesInitialised) {
        return;
    }
    panelGfx = new CommonGL(&panel);
    panelRenderer = std::make_unique<GLPanelRenderer>(panelGfx);
    panel.setGraphicsHelper(panelGfx);
    panel.setPanelRenderer(panelRenderer.get());
    panelGfx->initializeTextures();
    panel.bakeTexturesNextRepaint();
    sharedGlResourcesInitialised = true;
}

void CurvePanelHost::captureRenderedPanelImage(
        Rectangle<float> bounds,
        float scaleFactor,
        Image& destination,
        bool& hasVisibleContent) const {
    const int width = jmax(1, roundToInt(bounds.getWidth() * scaleFactor));
    const int height = jmax(1, roundToInt(bounds.getHeight() * scaleFactor));
    const int sourceX = roundToInt(bounds.getX() * scaleFactor);
    GLint viewport[4] {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int sourceY = jmax(0, viewport[3] - roundToInt(bounds.getBottom() * scaleFactor));
    HeapBlock<uint8> pixels(width * height * 4);
    glFlush();
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(sourceX, sourceY, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

    Image nextImage(Image::RGB, width, height, true);
    Image::BitmapData bitmap(nextImage, Image::BitmapData::writeOnly);
    bool nextHasVisibleContent = false;
    for (int y = 0; y < height; ++y) {
        const uint8* srcRow = pixels.get() + (height - 1 - y) * width * 4;
        for (int x = 0; x < width; ++x) {
            const uint8* src = srcRow + x * 4;
            bitmap.setPixelColour(x, y, Colour::fromRGB(src[0], src[1], src[2]));
            nextHasVisibleContent = nextHasVisibleContent || (src[0] + src[1] + src[2]) > 160;
        }
    }
    destination = nextImage;
    hasVisibleContent = nextHasVisibleContent;
}

PanelHostCallbacks CurvePanelHost::callbacks() const {
    PanelHostCallbacks result;
    result.setRepaintCallback([callback = repaintCallback](Panel*, PanelDirtyState::Flag) {
        if (callback != nullptr) {
            callback();
        }
    });
    result.setCursorCallback([this](Panel*, const MouseCursor& cursor) {
        if (cursorCallback != nullptr) {
            cursorCallback(cursor);
        }
        if (hostComponent != nullptr) {
            hostComponent->setMouseCursor(cursor);
        }
    });
    return result;
}

}
