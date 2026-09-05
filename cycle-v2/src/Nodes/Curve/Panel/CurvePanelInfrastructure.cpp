#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"

#include <Inter/Interactor.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/Panel.h>
#include <UI/Panels/PanelHostContext.h>
#include <UI/Panels/PanelInputHostComponent.h>
#include <UI/Panels/ScopedGLScissor.h>

using namespace gl;

namespace CycleV2 {

bool CurvePanelPreviewRenderCache::Key::operator==(const Key& other) const {
    return width == other.width
            && height == other.height
            && scaleFactor == other.scaleFactor
            && modelRevision == other.modelRevision
            && contentRevision == other.contentRevision
            && presentationRevision == other.presentationRevision
            && invalidationGeneration == other.invalidationGeneration;
}

bool CurvePanelPreviewRenderCache::canReuse(const Key& key) {
    if (valid && renderedKey == key) {
        ++hits;
        return true;
    }
    ++misses;
    return false;
}

void CurvePanelPreviewRenderCache::didRender(Key key) {
    renderedKey = key;
    valid = true;
}

void CurvePanelPreviewRenderCache::invalidate() {
    valid = false;
}

CurvePanelPreviewRenderCache::Diagnostics CurvePanelPreviewRenderCache::diagnostics() const {
    return { hits.load(), misses.load() };
}

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

class CurvePanelHost::HostComponent final : public PanelInputHostComponent {
public:
    HostComponent(
            Panel& targetPanel,
            CurvePanelSnapshotCache& snapshot,
            CurvePanelHostDelegate& delegateToUse) :
            PanelInputHostComponent(targetPanel)
        ,   snapshot(snapshot)
        ,   delegate(delegateToUse) {}

    void paint(Graphics& graphics) override {
        snapshot.paint(graphics, getLocalBounds().toFloat(), true);
    }

private:
    void pointerGestureBegan() override {
        delegate.beginEdit();
    }

    bool acceptsDoubleClick(const MouseEvent& event) const override {
        return event.mods.isLeftButtonDown();
    }

    void pointerGestureUpdated() override {
        delegate.publishIntermediateRevision();
    }

    void pointerGestureEnded() override {
        delegate.publishIntermediateRevision();
        delegate.commitEdit();
    }

    bool deleteKeyPressed() override {
        if (Interactor* interactor = panelInteractor()) {
            delegate.beginEdit();
            interactor->eraseSelected();
            interactor->performUpdate(Update);
            delegate.publishIntermediateRevision();
            delegate.commitEdit();
            return true;
        }
        return false;
    }

    CurvePanelSnapshotCache& snapshot;
    CurvePanelHostDelegate& delegate;
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
    ScopedGLScissor scissor(bounds, scaleFactor);
    panel.render();

    Image nextImage;
    bool hasVisibleContent {};
    captureRenderedPanelImage(bounds, scaleFactor, nextImage, hasVisibleContent);
    expandedSnapshot.publish(std::move(nextImage), hasVisibleContent);

    invalidation.request(
            CurvePanelInvalidation::HostSnapshot
            | CurvePanelInvalidation::Owner);
}

void CurvePanelHost::renderPreview(
        Rectangle<float> bounds,
        float scaleFactor,
        bool preserveInteractiveZoom,
        uint64_t modelRevision,
        uint64_t contentRevision,
        uint64_t presentationRevision) {
    if (bounds.isEmpty()) {
        renderSurfaceVisible = false;
        return;
    }
    auto renderKey = previewRenderKey(
            bounds,
            scaleFactor,
            modelRevision,
            contentRevision,
            presentationRevision);
    if (previewRenderCache.canReuse(renderKey)) {
        return;
    }

    renderPreviewUncached(bounds, scaleFactor, preserveInteractiveZoom);
    renderKey.invalidationGeneration = previewInvalidationGeneration.load();
    previewRenderCache.didRender(renderKey);
}

void CurvePanelHost::renderPreviewUncached(
        Rectangle<float> bounds,
        float scaleFactor,
        bool preserveInteractiveZoom) {
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
    if (preserveInteractiveZoom) {
        panel.getZoomPanel()->rect = interactiveZoom;
    } else {
        delegate.updateCurvePanelZoom(true);
    }
    delegate.prepareCurvePanel();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    ScopedGLScissor scissor(bounds, scaleFactor);
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
    previewRenderCache.invalidate();
}

void CurvePanelHost::initialiseComponent() {
    if (componentInitialised) {
        return;
    }
    hostComponent = std::make_unique<HostComponent>(
            panel, expandedSnapshot, delegate);
    panel.setInteractorMouseListenerEnabled(false);
    delegate.initialiseCurvePanel(hostComponent.get());
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

CurvePanelPreviewRenderCache::Key CurvePanelHost::previewRenderKey(
        Rectangle<float> bounds,
        float scaleFactor,
        uint64_t modelRevision,
        uint64_t contentRevision,
        uint64_t presentationRevision) const {
    return {
            bounds.getWidth(),
            bounds.getHeight(),
            scaleFactor,
            modelRevision,
            contentRevision,
            presentationRevision,
            previewInvalidationGeneration.load()
    };
}

PanelHostCallbacks CurvePanelHost::callbacks() const {
    PanelHostCallbacks result;
    result.setRepaintCallback([this](Panel*, PanelDirtyState::Flag flag) {
        const_cast<CurvePanelHost*>(this)->requestPanelInvalidation(flag);
    });
    result.setCursorCallback([this](Panel*, const MouseCursor& cursor) {
        if (hostComponent != nullptr) {
            hostComponent->setMouseCursor(cursor);
        }
    });
    return result;
}

void CurvePanelHost::requestPanelInvalidation(PanelDirtyState::Flag flag) {
    ++previewInvalidationGeneration;
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
