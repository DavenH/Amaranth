#include "CurvePanelInfrastructure.h"

#include <Inter/Interactor.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelHostContext.h>

using namespace gl;

namespace CycleV2 {

namespace CurvePanelInvalidation {

constexpr uint32_t HostSnapshot = 1u << 0;
constexpr uint32_t Owner = 1u << 1;
constexpr uint32_t TextureBake = 1u << 2;

}

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
        const int y = jmax(
                0,
                viewport[1] + viewport[3] - roundToInt(bounds.getBottom() * scaleFactor));
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
            CurvePanelHostDelegate& delegateToUse) :
            panel(targetPanel)
        ,   snapshot(snapshot)
        ,   delegate(delegateToUse) {
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
            interactor->mouseMove(localEvent);
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
        delegate.beginEdit();
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
        delegate.publishIntermediateRevision();
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
        delegate.publishIntermediateRevision();
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
        delegate.publishIntermediateRevision();
        delegate.commitEdit();
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
            delegate.beginEdit();
            interactor->eraseSelected();
            interactor->performUpdate(Update);
            delegate.publishIntermediateRevision();
            delegate.commitEdit();
            return true;
        }
        return false;
    }

    void resized() override {
        panel.panelResized();
    }

private:
    MouseEvent currentMouseEvent(const MouseEvent& event) const {
        const Point<float> localPosition = getLocalPoint(
                nullptr, Desktop::getMousePosition()).toFloat();
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
    CurvePanelHostDelegate& delegate;
    bool mouseInside {};
    bool activePointer {};
};

CurvePanelHost::CurvePanelHost(
        Panel& panelToHost,
        CurvePanelHostDelegate& delegateToUse) :
        panel(panelToHost)
    ,   delegate(delegateToUse)
    ,   invalidation(*this) {
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

void CurvePanelHost::render(Rectangle<float> bounds, Rectangle<float>, float scaleFactor) {
    if (bounds.isEmpty()) {
        renderSurfaceVisible = false;
        return;
    }
    renderSurfaceVisible = true;
    invalidation.notifyAvailabilityChanged();
    initialiseSharedGlResources();
    PanelHostContext context;
    context.bounds = bounds;
    context.clip = bounds;
    context.scaleFactor = scaleFactor;
    context.visible = true;
    context.callbacks = callbacks();
    panel.setHostContext(context);
    panel.panelResized();
    delegate.prepareCurvePanel();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    ScopedCurvePanelScissor scissor(bounds, scaleFactor);
    panel.render();

    Image nextImage;
    bool hasVisibleContent {};
    captureRenderedPanelImage(bounds, scaleFactor, nextImage, hasVisibleContent);
    expandedSnapshot.publish(std::move(nextImage), hasVisibleContent);

    invalidation.request(
            CurvePanelInvalidation::HostSnapshot
            | CurvePanelInvalidation::Owner);
}

void CurvePanelHost::renderPreview(Rectangle<float> bounds, float scaleFactor) {
    if (bounds.isEmpty()) {
        renderSurfaceVisible = false;
        return;
    }
    renderSurfaceVisible = true;
    invalidation.notifyAvailabilityChanged();
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
    delegate.updateCurvePanelZoom(true);
    delegate.prepareCurvePanel();
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

bool CurvePanelHost::usesCursor(const MouseCursor& cursor) const {
    return hostComponent != nullptr && hostComponent->getMouseCursor() == cursor;
}

void CurvePanelHost::releaseSharedGlResources() {
    panel.setPanelRenderer(nullptr);
    panelRenderer = nullptr;
    panel.setGraphicsHelper(nullptr);
    panelGfx = nullptr;
    sharedGlResourcesInitialised = false;
    renderSurfaceVisible = false;
}

void CurvePanelHost::initialiseComponent() {
    if (componentInitialised) {
        return;
    }
    hostComponent = std::make_unique<HostComponent>(
            panel, expandedSnapshot, delegate);
    delegate.initialiseCurvePanel(hostComponent.get());
    hostComponent->removeMouseListener(panel.getInteractor().get());
    componentInitialised = true;
    delegate.prepareCurvePanel();
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
    const int sourceY = jmax(
            0,
            viewport[1] + viewport[3] - roundToInt(bounds.getBottom() * scaleFactor));
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
    result.setRepaintCallback([this](Panel*, PanelDirtyState::Flag flag) {
        const_cast<CurvePanelHost*>(this)->requestPanelInvalidation(flag);
    });
    result.setCursorCallback([this](Panel*, const MouseCursor& cursor) {
        delegate.setCurvePanelCursor(cursor);
        if (hostComponent != nullptr) {
            hostComponent->setMouseCursor(cursor);
        }
    });
    return result;
}

void CurvePanelHost::requestPanelInvalidation(PanelDirtyState::Flag flag) {
    uint32_t categories = CurvePanelInvalidation::Owner;
    if (flag == PanelDirtyState::Flag::StaticVisual
            || flag == PanelDirtyState::Flag::SurfaceCache
            || flag == PanelDirtyState::Flag::Resource
            || flag == PanelDirtyState::Flag::Full) {
        categories |= CurvePanelInvalidation::TextureBake;
    }
    invalidation.request(categories);
}

uint32_t CurvePanelHost::availableRenderInvalidations() const {
    uint32_t available = CurvePanelInvalidation::HostSnapshot
            | CurvePanelInvalidation::Owner;
    if (renderSurfaceVisible && sharedGlResourcesInitialised) {
        available |= CurvePanelInvalidation::TextureBake;
    }
    return available;
}

void CurvePanelHost::flushRenderInvalidations(uint32_t categories) {
    if ((categories & CurvePanelInvalidation::TextureBake) != 0) {
        panel.bakeTexturesNextRepaint();
    }
    if ((categories & CurvePanelInvalidation::HostSnapshot) != 0
            && hostComponent != nullptr) {
        hostComponent->repaint();
    }
    if ((categories & CurvePanelInvalidation::Owner) != 0) {
        delegate.repaintCurvePanel();
    }
}

}
