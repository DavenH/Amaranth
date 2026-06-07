#include "Effect2DPanelBridge.h"

#include <Curve/Mesh/Vertex.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/PanelHostContext.h>

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;
constexpr float kWaveshaperPadding = 0.125f;
constexpr float kIrPadding = 0.0625f;

bool isEffect2DNode(NodeKind kind) {
    return kind == NodeKind::GuideCurve
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

}

class Effect2DPanelBridge::EffectPanel :
        public Panel2D
    ,   public Interactor2D {
public:
    EffectPanel(SingletonRepo* repo, const String& name, Mesh& meshToEdit) :
            Panel2D         (repo, name, true, true)
        ,   Interactor2D    (repo, name, Dimensions(Vertex::Phase, Vertex::Amp))
        ,   SingletonAccessor(repo, name)
        ,   rasterizer      (repo, name + "Rasterizer")
        ,   mesh            (meshToEdit) {
        vertPadding = 0;
        paddingLeft = 0;
        paddingRight = 0;
        backgroundTimeRelevant = false;
        speedApplicable = false;
        guideCurveApplicable = false;
        alwaysDrawDepthLines = true;
        drawLinesAfterFill = true;

        colorA = Color(0.92f, 0.93f, 0.96f, 0.92f);
        colorB = Color(0.92f, 0.93f, 0.96f, 0.92f);

        rasterizer.setDims(dims);
        rasterizer.setMesh(&mesh);
        Interactor2D::setRasterizer(&rasterizer);
        interactor = this;
        suspendUndo = true;

        vertexProps.sliderApplicable[Vertex::Time] = false;
        vertexProps.sliderApplicable[Vertex::Red] = false;
        vertexProps.sliderApplicable[Vertex::Blue] = false;
        vertexProps.ampVsPhaseApplicable = false;

        for (auto& flag : vertexProps.guideCurveApplicable) {
            flag = false;
        }

        vertexProps.dimensionNames.set(Vertex::Time, {});
        vertexProps.dimensionNames.set(Vertex::Red, {});
        vertexProps.dimensionNames.set(Vertex::Blue, {});
        vertexProps.dimensionNames.set(Vertex::Phase, "x");
        vertexProps.dimensionNames.set(Vertex::Amp, "y");
    }

    void setNodeKind(NodeKind nodeKind) {
        kind = nodeKind;
        curveIsBipolar = kind != NodeKind::Waveshaper;
        bgPaddingLeft = paddingForKind();
        bgPaddingRight = kind == NodeKind::ImpulseResponse ? 0.f : paddingForKind();
        bgPaddingTop = kind == NodeKind::Waveshaper ? paddingForKind() : 0.f;
        bgPaddingBttm = kind == NodeKind::Waveshaper ? paddingForKind() : 0.f;
    }

    void init() override {
        Panel2D::init();
        Interactor2D::init();
    }

    void initWithHost(Component* hostComponent) {
        Panel2D::initWithExternalComponent(hostComponent);
        Interactor2D::init();
        applyLegacyZoomBounds();
    }

    bool isEffectEnabled() const { return true; }
    bool isMeshEnabled() override { return isEffectEnabled(); }
    Mesh* getMesh() override { return &mesh; }

    bool doCreateVertex() override {
        return addNewCube(0.f, state.currentMouse.x, state.currentMouse.y, 0.f);
    }

    bool addNewCube(float startTime, float x, float y, float curve) override {
        ignoreUnused(startTime);

        auto* vertex = new Vertex(x, y);
        vertex->values[Vertex::Curve] = curve;

        {
            ScopedLock lock(vertexLock);
            mesh.addVertex(vertex);
            state.currentVertex = vertex;

            vector<Vertex*>& selected = getSelected();
            selected.clear();
            selected.push_back(vertex);
            resetFinalSelection();
            updateSelectionFrames();
        }

        refreshRasterizer();
        state.flags[PanelState::DidMeshChange] = true;
        Panel2D::repaint();
        return true;
    }

    void refreshRasterizer() {
        rasterizer.updateGeometry();
        rasterizer.updateWaveform();
    }

    void performUpdate(UpdateType updateType) override {
        if (updateType == Update) {
            refreshRasterizer();
        }

        Panel2D::repaint();
    }

    void preDraw() override {
        if (kind == NodeKind::GuideCurve) {
            drawGuideBackground();
        } else if (kind == NodeKind::ImpulseResponse) {
            drawIrBackground();
        }
    }

    void postCurveDraw() override {
        if (kind == NodeKind::Waveshaper) {
            drawWaveshaperBounds();
        } else if (kind == NodeKind::GuideCurve) {
            drawGuideBounds();
        } else if (kind == NodeKind::ImpulseResponse) {
            drawIrBounds();
        }
    }

    void drawInterceptLines() override {}

private:
    float paddingForKind() const {
        if (kind == NodeKind::GuideCurve) {
            return kGuidePadding;
        }

        if (kind == NodeKind::Waveshaper) {
            return kWaveshaperPadding;
        }

        return kIrPadding;
    }

    void applyLegacyZoomBounds() {
        if (zoomPanel == nullptr) {
            return;
        }

        const float padding = paddingForKind();
        zoomPanel->rect.x = 0.5f * padding;
        zoomPanel->rect.w = kind == NodeKind::ImpulseResponse ? 1.f - padding : 1.f - padding;
        zoomPanel->rect.y = kind == NodeKind::Waveshaper ? 0.5f * padding : 0.f;
        zoomPanel->rect.h = kind == NodeKind::Waveshaper ? 1.f - padding : 1.f;
    }

    void drawGuideBackground() {
        const float padding = kGuidePadding;
        const int left = 0;
        const int right = getWidth();
        const int innerLeft = sx(padding);
        const int innerRight = sx(1.f - padding);
        const int bottom = 0;
        const int top = getHeight();

        gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
        gfx->fillRect(left, top, innerLeft, bottom, false);
        gfx->fillRect(right, top, innerRight, bottom, false);

        gfx->disableSmoothing();
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.32f);
        gfx->drawLine(innerLeft, top, innerLeft, bottom, false);
        gfx->drawLine(innerRight, top, innerRight, bottom, false);
        gfx->enableSmoothing();
    }

    void drawIrBackground() {
        const int left = 0;
        const int bottom = 0;
        const int top = getHeight();
        const int innerLeft = sx(kIrPadding);

        gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
        gfx->fillRect(left, top, innerLeft, bottom, false);
        gfx->disableSmoothing();
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.28f);
        gfx->drawLine(innerLeft, top, innerLeft, bottom, false);
        gfx->enableSmoothing();

    }

    void drawWaveshaperBounds() {
        const int left = 0;
        const int right = getWidth();
        const int innerRight = sx(1.f - kWaveshaperPadding);
        const int innerLeft = sx(kWaveshaperPadding);
        const int bottom = 0;
        const int low = sy(1.f - kWaveshaperPadding);
        const int high = sy(kWaveshaperPadding);
        const int top = getHeight();

        gfx->setCurrentColour(0.1f, 0.1f, 0.1f, 0.5f);
        gfx->fillRect(innerLeft, top, innerRight, high, false);
        gfx->fillRect(innerLeft, bottom, innerRight, low, false);
        gfx->fillRect(left, top, innerLeft, bottom, false);
        gfx->fillRect(right, top, innerRight, bottom, false);

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.42f);
        gfx->drawRect(innerLeft, high, innerRight, low, false);
        gfx->enableSmoothing();
    }

    void drawGuideBounds() {
        const int innerLeft = sx(kGuidePadding);
        const int innerRight = sx(1.f - kGuidePadding);

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.42f);
        gfx->drawRect(innerLeft, getHeight(), innerRight, 0, false);
        gfx->enableSmoothing();
    }

    void drawIrBounds() {
        const int innerLeft = sx(kIrPadding);

        gfx->disableSmoothing();
        gfx->setCurrentLineWidth(1.f);
        gfx->setCurrentColour(0.82f, 0.84f, 0.88f, 0.42f);
        gfx->drawRect(innerLeft, getHeight(), getWidth(), 0, false);
        gfx->enableSmoothing();
    }

    FXRasterizer rasterizer;
    Mesh& mesh;
    NodeKind kind { NodeKind::Waveshaper };
};

class Effect2DPanelBridge::PanelHostComponent :
        public Component {
public:
    explicit PanelHostComponent(Panel& targetPanel) :
            panel(targetPanel) {
        setPaintingIsUnclipped(false);
        setInterceptsMouseClicks(true, true);
        setOpaque(false);
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
        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseMove(localEvent);
        }
    }

    void mouseDown(const MouseEvent& event) override {
        if (!event.mods.isLeftButtonDown()) {
            activeLeftButton = false;
            return;
        }

        activeLeftButton = true;
        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDown(localEvent);
        }
    }

    void mouseDrag(const MouseEvent& event) override {
        if (!activeLeftButton || !event.mods.isLeftButtonDown()) {
            return;
        }

        const MouseEvent localEvent = currentMouseEvent(event);
        enterIfNeeded(localEvent);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseDrag(localEvent);
        }
    }

    void mouseUp(const MouseEvent& event) override {
        if (!activeLeftButton) {
            return;
        }

        activeLeftButton = false;
        const MouseEvent localEvent = currentMouseEvent(event);

        if (Interactor* interactor = panel.getInteractor().get()) {
            interactor->mouseUp(localEvent);
        }
    }

    void mouseExit(const MouseEvent& event) override {
        exitIfNeeded(currentMouseEvent(event));
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

    Panel& panel;
    bool mouseInside {};
    bool activeLeftButton {};
};

Effect2DPanelBridge::Effect2DPanelBridge(NodeKind nodeKind) :
        kind    (nodeKind)
    ,   mesh    ("CycleV2Effect2D") {
    jassert(isEffect2DNode(kind));
    panel = std::make_unique<EffectPanel>(&environment.getRepo(), "CycleV2Effect2DPanel", mesh);
    panel->setNodeKind(kind);
    initialiseMesh();
}

Effect2DPanelBridge::~Effect2DPanelBridge() {
    releaseSharedGlResources();
}

void Effect2DPanelBridge::syncFromNode(const Node& node) {
    if (node.kind != kind) {
        return;
    }

    if (lastSyncedNodeId == node.id) {
        return;
    }

    lastSyncedNodeId = node.id;
    panel->refreshRasterizer();
}

Component* Effect2DPanelBridge::getPanelHostComponent() {
    initialisePanelHost();
    return panelHost.get();
}

Component* Effect2DPanelBridge::getPanelHostComponentIfCreated() {
    return panelHostInitialised ? panelHost.get() : nullptr;
}

void Effect2DPanelBridge::setPanelHostCallbacks(
        std::function<void()> repaintCallback,
        std::function<void(const MouseCursor&)> cursorCallback) {
    panelHostRepaintCallback = std::move(repaintCallback);
    panelHostCursorCallback = std::move(cursorCallback);
    panel->setHostCallbacks(createPanelHostCallbacks());
}

void Effect2DPanelBridge::renderPanel(Rectangle<float> bounds, float scaleFactor) {
    if (bounds.isEmpty()) {
        return;
    }

    initialiseSharedGlResources();

    PanelHostContext context;
    context.bounds = bounds;
    context.clip = bounds;
    context.scaleFactor = scaleFactor;
    context.visible = true;
    context.callbacks = createPanelHostCallbacks();
    panel->render(context);
}

void Effect2DPanelBridge::initialiseSharedGlResources() {
    initialisePanelHost();

    if (sharedGlResourcesInitialised) {
        return;
    }

    panelGfx = new CommonGL(panel.get());
    panelRenderer = std::make_unique<GLPanelRenderer>(panelGfx);
    panel->setGraphicsHelper(panelGfx);
    panel->setPanelRenderer(panelRenderer.get());
    panelGfx->initializeTextures();
    panel->bakeTexturesNextRepaint();
    sharedGlResourcesInitialised = true;
}

void Effect2DPanelBridge::releaseSharedGlResources() {
    panel->setPanelRenderer(nullptr);
    panelRenderer = nullptr;
    panel->setGraphicsHelper(nullptr);
    panelGfx = nullptr;
    sharedGlResourcesInitialised = false;
}

void Effect2DPanelBridge::initialisePanelHost() {
    if (panelHostInitialised) {
        return;
    }

    panelHost = std::make_unique<PanelHostComponent>(*panel);
    panel->initWithHost(panelHost.get());
    panelHost->removeMouseListener(panel.get());
    panel->stopTimer();
    panelHostInitialised = true;
}

void Effect2DPanelBridge::initialiseMesh() {
    if (mesh.getNumVerts() > 0) {
        return;
    }

    if (kind == NodeKind::GuideCurve) {
        addVertex(kGuidePadding, 0.5f, 1.f);
        addVertex(1.f - kGuidePadding, 0.5f, 1.f);
    } else if (kind == NodeKind::ImpulseResponse) {
        addVertex(kIrPadding * 0.5f, 0.5f);
        addVertex(kIrPadding - 0.001f, 0.5f);
        addVertex(kIrPadding + 0.001f, 0.5f);
        addVertex(kIrPadding + 0.003f, 0.5f);
        addVertex(kIrPadding + 0.005f, 1.0f);
        addVertex(kIrPadding + 0.010f, 0.1313f);
        addVertex(kIrPadding + 0.1f, 0.6f);
        addVertex(kIrPadding + 0.15f, 0.5f);
        addVertex(kIrPadding + 0.2f, 0.5f);
        addVertex(1.f, 0.5f);
    } else if (kind == NodeKind::Waveshaper) {
        addVertex(kWaveshaperPadding * 0.5f, kWaveshaperPadding * 0.5f, 1.f);
        addVertex(kWaveshaperPadding, kWaveshaperPadding, 1.f);
        addVertex(1.f - kWaveshaperPadding, 1.f - kWaveshaperPadding, 1.f);
        addVertex(1.f - kWaveshaperPadding * 0.5f, 1.f - kWaveshaperPadding * 0.5f, 1.f);
    }

    panel->refreshRasterizer();
}

void Effect2DPanelBridge::addVertex(float x, float y, float curve) {
    auto* vertex = new Vertex(x, y);
    vertex->values[Vertex::Curve] = curve;

    if (curve >= 1.f) {
        vertex->setMaxSharpness();
    }

    mesh.addVertex(vertex);
}

PanelHostCallbacks Effect2DPanelBridge::createPanelHostCallbacks() {
    PanelHostCallbacks callbacks;
    callbacks.setRepaintCallback([this](Panel*, PanelDirtyState::Flag) {
        if (panelHostRepaintCallback != nullptr) {
            panelHostRepaintCallback();
        }
    });
    callbacks.setCursorCallback([this](Panel*, const MouseCursor& cursor) {
        if (panelHostCursorCallback != nullptr) {
            panelHostCursorCallback(cursor);
        }

        if (panelHost != nullptr) {
            panelHost->setMouseCursor(cursor);
        }
    });

    return callbacks;
}

}
