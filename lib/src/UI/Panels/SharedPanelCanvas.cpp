#include "SharedPanelCanvas.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>

#include "CommonGL.h"
#include "GLPanelRenderer.h"
#include "GLSurfaceCache.h"
#include "OpenGL.h"
#include "Panel.h"
#include "ScopedGL.h"

using namespace gl;

namespace {
bool shouldTraceOpenGLRenderers() {
    static bool shouldTrace = std::getenv("CYCLE_TRACE_OPENGL_RENDERERS") != nullptr;
    return shouldTrace;
}
}

struct SharedPanelCanvas::RenderState {
    class RenderHelper :
            public Panel::Renderer {
    public:
        explicit RenderHelper(SharedPanelCanvas& owner) :
                owner(owner)
        {}

        void activate() override {}
        void deactivate() override {}
        void clear() override { owner.clearCurrentPanelViewport(); }

    private:
        SharedPanelCanvas& owner;
    };

    RenderState(SharedPanelCanvas& owner, Panel* panel) :
            panel(panel)
        ,   renderHelper(owner)
    {}

    Panel* panel;
    CommonGL* commonGL = nullptr;
    std::unique_ptr<GLPanelRenderer> panelRenderer;
    GLSurfaceCache surfaceCache;
    RenderHelper renderHelper;
    bool cacheCreated = false;
    bool texturesInitialised = false;
};

SharedPanelCanvas::SharedPanelCanvas() :
        OpenGLBase(this, this)
{
    setInterceptsMouseClicks(false, false);
    setOpaque(false);
    startTimerHz(30);
}

SharedPanelCanvas::~SharedPanelCanvas() {
    detachOpenGL();
}

void SharedPanelCanvas::attachOpenGL() {
    attachOpenGL(*this, false);
}

void SharedPanelCanvas::attachOpenGL(juce::Component& target, bool enableComponentPainting) {
    if (context.isAttached() || openGLAttachRequested) {
        return;
    }

    openGLAttachRequested = true;
    context.setRenderer(this);
    context.setComponentPaintingEnabled(enableComponentPainting);
    context.attachTo(target);
    traceOpenGLAttach(&target);
}

void SharedPanelCanvas::detachOpenGL() {
    if (context.isAttached()) {
        juce::Component* attachedComponent = context.getTargetComponent();
        context.detach();
        traceOpenGLDetach(attachedComponent);
    } else {
        context.detach();
    }

    openGLAttachRequested = false;
    openGLReady = false;
}

void SharedPanelCanvas::invalidateAll(PanelDirtyState::Flag flag) {
    compositor.invalidateAll(flag);
    repaintDirtyRegions();
}

void SharedPanelCanvas::invalidatePanel(Panel* panel, PanelDirtyState::Flag flag) {
    compositor.invalidatePanel(panel, flag);
    repaintDirtyRegions();
}

void SharedPanelCanvas::registerOrUpdatePanel(Panel* panel, const juce::Rectangle<int>& bounds, bool visible) {
    compositor.registerOrUpdatePanel(panel, bounds, visible);
    repaintDirtyRegions();
}

void SharedPanelCanvas::removePanel(Panel* panel) {
    compositor.removePanel(panel);
    renderStates.erase(
        std::remove_if(renderStates.begin(), renderStates.end(), [panel](const std::unique_ptr<RenderState>& state) {
            return state == nullptr || state->panel == panel;
        }),
        renderStates.end()
    );
    repaint();
}

void SharedPanelCanvas::setDebugSnapshotOverlayEnabled(bool enabled) {
    if (debugSnapshotOverlayEnabled == enabled) {
        return;
    }

    debugSnapshotOverlayEnabled = enabled;
    repaint();
}

void SharedPanelCanvas::paint(juce::Graphics& g) {
    if (context.isAttached()) {
        return;
    }

    if (debugSnapshotOverlayEnabled) {
        for (const auto& entry : compositor.getVisibleEntries()) {
            if (entry.panel == nullptr || !entry.usesCachedSurface || !g.getClipBounds().intersects(entry.bounds)) {
                continue;
            }

            entry.panel->paintSharedCanvasDebugOverlay(g, entry.bounds);
        }

        compositor.clearDirtyFlags();
        return;
    }

    for (const auto& entry : compositor.getVisibleEntries()) {
        if (entry.panel == nullptr || !entry.usesCachedSurface || !g.getClipBounds().intersects(entry.bounds)) {
            continue;
        }

        if (entry.panel->usesSharedCanvasBackground()) {
            entry.panel->paintSharedCanvasBackground(g, entry.bounds);
        }

        entry.panel->paintSharedCanvasSurface(g, entry.bounds);

// #ifdef JUCE_DEBUG
//         g.saveState();
//         g.setColour(juce::Colour::fromRGBA((juce::uint8) 255, (juce::uint8) 128, (juce::uint8) 48, (juce::uint8) 220));
//         g.drawRect(entry.bounds, 2);
//         g.setFont(juce::Font(juce::FontOptions(12.f)));
//         auto labelBounds = entry.bounds.reduced(6, 4).removeFromTop(18);
//         g.drawText(entry.panel->getName(), labelBounds, juce::Justification::topLeft, false);
//         g.restoreState();
// #endif
    }

    compositor.clearDirtyFlags();
}

void SharedPanelCanvas::repaintDirtyRegions() {
    juce::RectangleList<int> dirtyBounds = compositor.collectDirtyBounds();

    if (dirtyBounds.isEmpty()) {
        return;
    }

    for (int i = 0; i < dirtyBounds.getNumRectangles(); ++i) {
        repaint(dirtyBounds.getRectangle(i));
    }
}

void SharedPanelCanvas::repaintCachedSurfaces() {
    for (const auto& entry : compositor.getVisibleEntries()) {
        if (entry.panel != nullptr && entry.usesCachedSurface) {
            repaint(entry.bounds);
        }
    }
}

SharedPanelCanvas::RenderState* SharedPanelCanvas::getOrCreateRenderState(Panel* panel) {
    if (panel == nullptr) {
        return nullptr;
    }

    for (auto& state : renderStates) {
        if (state != nullptr && state->panel == panel) {
            return state.get();
        }
    }

    auto state = std::make_unique<RenderState>(*this, panel);
    RenderState* rawState = state.get();
    renderStates.push_back(std::move(state));
    return rawState;
}

void SharedPanelCanvas::clearCurrentPanelViewport() {
    setSharedViewport(currentPanelBounds);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(1.f, 1.f, 1.f);
}

void SharedPanelCanvas::drawDebugOutlinesOpenGL() {
#ifdef JUCE_DEBUG
    resetSharedViewport();
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_SCISSOR_TEST);
    glLineWidth(2.f);
    glColor4f(1.f, 0.5f, 0.18f, 0.9f);

    for (const auto& entry : compositor.getVisibleEntries()) {
        if (entry.panel == nullptr || entry.bounds.isEmpty()) {
            continue;
        }

        const float x1 = (float) entry.bounds.getX();
        const float y1 = (float) entry.bounds.getY();
        const float x2 = (float) entry.bounds.getRight();
        const float y2 = (float) entry.bounds.getBottom();

        ScopedElement outline(GL_LINE_LOOP);
        glVertex2f(x1, y1);
        glVertex2f(x2, y1);
        glVertex2f(x2, y2);
        glVertex2f(x1, y2);
    }
#endif
}

void SharedPanelCanvas::initialiseRenderState(RenderState& state) {
    if (state.panel == nullptr || state.texturesInitialised) {
        return;
    }

    if (state.commonGL == nullptr) {
        state.commonGL = new CommonGL(state.panel, this);
        state.panel->setGraphicsHelper(state.commonGL);
    }

    state.panelRenderer = std::make_unique<GLPanelRenderer>(state.commonGL, &state.surfaceCache);

    state.commonGL->initializeTextures();
    state.commonGL->initLineParams();

    if (state.panel->usesCachedSurface()) {
        state.panel->bakeTexturesNextRepaint();
    }

    state.texturesInitialised = true;
}

void SharedPanelCanvas::renderSharedPanel(RenderState& state, const PanelCompositor::Entry& entry) {
    if (state.panel == nullptr || state.panelRenderer == nullptr || state.commonGL == nullptr) {
        return;
    }

    currentPanelBounds = entry.bounds;
    setSharedViewport(currentPanelBounds);

    Component* component = state.panel->getComponent();
    if (component != nullptr) {
        state.surfaceCache.setSize(jmax(1, component->getWidth()), jmax(1, component->getHeight()));
    } else {
        state.surfaceCache.setSize(jmax(1, entry.bounds.getWidth()), jmax(1, entry.bounds.getHeight()));
    }

    state.surfaceCache.setRenderScale(currentRenderScale);
    if (!state.cacheCreated) {
        state.surfaceCache.create();
        state.surfaceCache.allocate(true);
        state.cacheCreated = true;
    }

    const int framebufferX = roundToInt(entry.bounds.getX() * currentRenderScale);
    const int framebufferY = roundToInt((getHeight() - entry.bounds.getBottom()) * currentRenderScale);
    state.panelRenderer->setFramebufferOriginPixels({ framebufferX, framebufferY });

    if (entry.usesCachedSurface) {
        state.panel->bakeTexturesNextRepaint();
    }

    state.panel->setPanelRenderer(state.panelRenderer.get());
    state.panel->setRenderHelper(&state.renderHelper);
    state.panel->render();
}

void SharedPanelCanvas::resetSharedViewport() {
    const int width = jmax(1, roundToInt(getWidth() * currentRenderScale));
    const int height = jmax(1, roundToInt(getHeight() * currentRenderScale));

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, getWidth(), getHeight(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void SharedPanelCanvas::setSharedViewport(const juce::Rectangle<int>& bounds) {
    const int x = roundToInt(bounds.getX() * currentRenderScale);
    const int y = roundToInt((getHeight() - bounds.getBottom()) * currentRenderScale);
    const int width = jmax(1, roundToInt(bounds.getWidth() * currentRenderScale));
    const int height = jmax(1, roundToInt(bounds.getHeight() * currentRenderScale));

    glEnable(GL_SCISSOR_TEST);
    glViewport(x, y, width, height);
    glScissor(x, y, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, bounds.getWidth(), bounds.getHeight(), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void SharedPanelCanvas::newOpenGLContextCreated() {
    openGLReady = true;

    for (auto& state : renderStates) {
        if (state != nullptr) {
            state->cacheCreated = false;
            state->texturesInitialised = false;
        }
    }
}

void SharedPanelCanvas::openGLContextClosing() {
    for (auto& state : renderStates) {
        if (state != nullptr) {
            state->surfaceCache.clear();
            state->cacheCreated = false;
            state->texturesInitialised = false;
        }
    }

    openGLReady = false;
}

void SharedPanelCanvas::renderOpenGL() {
    if (!openGLReady || getWidth() <= 0 || getHeight() <= 0) {
        return;
    }

    if (shouldTraceOpenGLRenderers()) {
        static std::atomic<int> traceCount { 0 };
        int count = traceCount.fetch_add(1);

        if (count < 16) {
            DBG("CycleOpenGLTrace renderer=SharedPanelCanvas call=" + String(count)
                + " visiblePanels=" + String((int) compositor.getVisibleEntries().size()));
        }
    }

    currentRenderScale = jmax(1.0, context.getRenderingScale());
    resetSharedViewport();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (const auto& entry : compositor.getVisibleEntries()) {
        if (entry.panel == nullptr || entry.bounds.isEmpty()) {
            continue;
        }

        RenderState* state = getOrCreateRenderState(entry.panel);
        if (state == nullptr) {
            continue;
        }

        initialiseRenderState(*state);
        renderSharedPanel(*state, entry);
    }

    // drawDebugOutlinesOpenGL();
    compositor.clearDirtyFlags();
    resetSharedViewport();
    printErrors(nullptr);
}

void SharedPanelCanvas::timerCallback() {
    compositor.syncWithPanels();
    repaintDirtyRegions();
    repaintCachedSurfaces();

    if (context.isAttached()) {
        context.triggerRepaint();
    }
}
