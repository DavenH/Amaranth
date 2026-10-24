#include "UI/ComparisonSurfaceView.h"
#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"

#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"

#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Array/Buffer.h>
#include <UI/Panels/CommonGL.h>
#include <UI/Panels/GLPanelRenderer.h>
#include <UI/Panels/GLSurfaceCache.h>
#include <UI/Panels/PanelHostContext.h>
#include <UI/Panels/PanelInputHostComponent.h>
#include <UI/Panels/ZoomPanel.h>

#include <atomic>

using namespace gl;

namespace CycleV2 {

namespace {

class ComparisonGridSource final : public Panel3D::DataRetriever {
public:
    void setGrid(const std::vector<float>& values, size_t columns, size_t rows,
            PortDomain domain, int note, float sourceGain) {
        const juce::ScopedLock lock(gridLock);
        storage = values;
        panelColumns.clear();
        if (columns < 2 || rows < 2 || storage.size() < columns * rows) {
            storage.clear();
            displayStorage.clear();
            ++revision;
            return;
        }
        displayStorage = TrimeshRenderProfile::fromDomain(domain)
                .mapGridToDisplay(storage, columns, rows, note, sourceGain);
        panelColumns.reserve(columns);
        for (size_t index = 0; index < columns; ++index) {
            panelColumns.emplace_back(storage.data() + index * rows,
                    (int) rows, (float) index / (float) (columns - 1), (char) note);
        }
        ++revision;
    }

    Buffer<float> getColumnArray() override {
        return storage.empty()
                ? Buffer<float>() : Buffer<float>(storage.data(), (int) storage.size());
    }

    Buffer<float> getScalarSurfaceArray() override {
        return displayStorage.empty()
                ? Buffer<float>()
                : Buffer<float>(displayStorage.data(), (int) displayStorage.size());
    }
    const std::vector<Column>& getColumns() override { return panelColumns; }
    const std::vector<Column>& columns() const { return panelColumns; }
    juce::CriticalSection& getGridLock() override { return gridLock; }
    uint64_t getScalarSurfaceRevision() const override { return revision; }
    bool hasStableScalarSurfaceRevision() const override { return true; }

private:
    std::vector<float> storage;
    std::vector<float> displayStorage;
    std::vector<Column> panelColumns;
    juce::CriticalSection gridLock;
    uint64_t revision {};
};

class ComparisonInputHost final : public PanelInputHostComponent {
public:
    ComparisonInputHost(Panel& panel, ComparisonSurfaceView& viewToUse) :
            PanelInputHostComponent(panel)
        ,   view(viewToUse) {}

private:
    bool acceptsPointerDown(const juce::MouseEvent&) const override { return false; }
    bool acceptsDoubleClick(const juce::MouseEvent&) const override { return false; }

    void mouseEnter(const juce::MouseEvent&) override {}
    void mouseMove(const juce::MouseEvent&) override {}
    void mouseExit(const juce::MouseEvent&) override {}

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override {
        if (view.onWheel) {
            view.onWheel(event, wheel);
        } else {
            PanelInputHostComponent::mouseWheelMove(event, wheel);
        }
    }

    void mouseUp(const juce::MouseEvent& event) override {
        if (!event.mods.isPopupMenu() && view.onClicked) {
            view.onClicked();
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override {
        if (!event.mods.isPopupMenu() && view.onDoubleClicked) {
            view.onDoubleClicked();
        }
    }

    ComparisonSurfaceView& view;
};

}

class ComparisonSurfaceView::Impl final : private juce::ScrollBar::Listener {
public:
    explicit Impl(ComparisonSurfaceView& ownerToUse) :
            owner(ownerToUse)
        ,   interactor(&environment.getRepo(), "ComparisonSurfaceInteractor")
        ,   panel(&environment.getRepo(), source) {
        interactor.init();
        interactor.stopTimer();
        interactor.setRasterizer(&rasterizer);
        panel.setInteractor(&interactor);
        panel.setInteractorMouseListenerEnabled(false);
        inputHost = std::make_unique<ComparisonInputHost>(panel, owner);
        panel.initWithExternalComponent(inputHost.get());
        panel.configureViewport({ 0.f, 1.f });
        for (bool horizontal : { true, false }) {
            static_cast<juce::ScrollBar*>(panel.getZoomPanel()->getComponent(horizontal))->addListener(this);
        }
        panel.getZoomPanel()->setScrollbarsVisible(false);
        owner.addAndMakeVisible(panel.getZoomPanel());
    }

    ~Impl() override {
        for (bool horizontal : { true, false }) {
            static_cast<juce::ScrollBar*>(panel.getZoomPanel()->getComponent(horizontal))->removeListener(this);
        }
        panel.setInteractor(nullptr);
        interactor.setRasterizer(nullptr);
    }

    void setGrid(const std::vector<float>& values, size_t columns, size_t rows,
            PortDomain domain, int note, float displayGain) {
        source.setGrid(values, columns, rows, domain, note, displayGain);
        panel.setRenderProfile(TrimeshRenderProfile::fromDomain(domain));
        panel.setPreviewMidiNote(note);
        panel.bakeTexturesNextRepaint();
        snapshotPending = true;
        owner.repaint();
    }

    void setCamera(const ZoomRect& camera) {
        if (panel.getZoomPanel()->rect == camera) {
            return;
        }
        relayingCamera = true;
        panel.getZoomPanel()->rect = camera;
        panel.getZoomPanel()->panelZoomChanged(false);
        panel.bakeTexturesNextRepaint();
        snapshotPending = true;
        relayingCamera = false;
    }

    ZoomRect camera() { return panel.getZoomPanel()->rect; }

    void setZoomBarsVisible(bool visible) {
        stylePanelZoomScrollbars(*panel.getZoomPanel());
        panel.getZoomPanel()->setScrollbarsVisible(visible);
    }

    juce::Rectangle<int> plotBounds() const {
        return owner.getLocalArea(inputHost.get(), inputHost->getLocalBounds());
    }

    juce::Rectangle<int> zoomBounds(bool horizontal) {
        auto* bar = panel.getZoomPanel()->getComponent(horizontal);
        return owner.getLocalArea(bar, bar->getLocalBounds());
    }

    bool renderOpenGL(float scaleFactor) {
        const int width = inputHost->getWidth();
        const int height = inputHost->getHeight();
        if (width < 2 || height < 2) {
            return false;
        }
        if (renderer == nullptr) {
            graphics = new CommonGL(&panel);
            renderer = std::make_unique<GLPanelRenderer>(graphics, &surfaceCache);
            panel.setGraphicsHelper(graphics);
            panel.setPanelRenderer(renderer.get());
            graphics->initializeTextures();
            surfaceCache.create();
            surfaceCache.allocate(false);
            ++contextCreateCount;
            resourceReady = true;
            snapshotPending = true;
        }

        surfaceCache.setSize(width, height);
        surfaceCache.setRenderScale(scaleFactor);
        glViewport(0, 0, juce::roundToInt((float) width * scaleFactor),
                juce::roundToInt((float) height * scaleFactor));
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, width, height, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        graphics->initLineParams();

        PanelHostContext context;
        context.bounds = { 0.f, 0.f, (float) width, (float) height };
        context.clip = context.bounds;
        context.scaleFactor = scaleFactor;
        panel.render(context);
        ++renderCount;
        return snapshotPending.exchange(false);
    }

    void releaseOpenGLResources() {
        if (renderer != nullptr) {
            renderer->clearResources();
        }
        panel.setPanelRenderer(nullptr);
        renderer = nullptr;
        panel.setGraphicsHelper(nullptr);
        graphics = nullptr;
        surfaceCache.clear();
        resourceReady = false;
    }

    void paint(juce::Graphics& graphicsToUse) const {
        surfaceCache.paintSnapshot(graphicsToUse, owner.getLocalBounds()
                .withWidth(inputHost->getWidth()).withHeight(inputHost->getHeight()));
    }

    juce::var automationDiagnostics() {
        auto result = std::make_unique<juce::DynamicObject>();
        result->setProperty("attached", resourceReady.load());
        result->setProperty("renderCount", renderCount.load());
        result->setProperty("contextCreateCount", contextCreateCount.load());
        result->setProperty("width", inputHost->getWidth());
        result->setProperty("height", inputHost->getHeight());
        result->setProperty("gridColumns", (int) source.columns().size());
        result->setProperty("gridRows", source.columns().empty() ? 0 : source.columns().front().size());
        auto* view = new juce::DynamicObject();
        const auto& rect = panel.getZoomPanel()->rect;
        view->setProperty("x", rect.x);
        view->setProperty("y", rect.y);
        view->setProperty("w", rect.w);
        view->setProperty("h", rect.h);
        result->setProperty("camera", juce::var(view));
        result->setProperty("errorCount", 0);
        return { result.release() };
    }

    void resized() {
        panel.getZoomPanel()->setBounds(0, 0, owner.getWidth(), owner.getHeight());
        snapshotPending = true;
    }

private:
    void scrollBarMoved(juce::ScrollBar*, double) override {
        if (relayingCamera) {
            return;
        }
        juce::Component::SafePointer<ComparisonSurfaceView> safeOwner(&owner);
        juce::MessageManager::callAsync([safeOwner] {
            if (safeOwner != nullptr) {
                auto& self = *safeOwner->impl;
                self.snapshotPending = true;
                self.panel.bakeTexturesNextRepaint();
                if (safeOwner->onCameraChanged) {
                    safeOwner->onCameraChanged(self.camera());
                }
                safeOwner->repaint();
            }
        });
    }

    ComparisonSurfaceView& owner;
    TrimeshPanelEnvironment environment;
    ComparisonGridSource source;
    Rasterization::TrilinearMeshRasterizer rasterizer;
    TrimeshInteractor3D interactor;
    TrimeshPanel3D panel;
    std::unique_ptr<PanelInputHostComponent> inputHost;
    CommonGL* graphics {};
    std::unique_ptr<GLPanelRenderer> renderer;
    GLSurfaceCache surfaceCache;
    std::atomic<int> renderCount {};
    std::atomic<int> contextCreateCount {};
    std::atomic<bool> resourceReady {};
    std::atomic<bool> snapshotPending { true };
    bool relayingCamera {};
};

ComparisonSurfaceView::ComparisonSurfaceView() :
        impl(std::make_unique<Impl>(*this)) {}

ComparisonSurfaceView::~ComparisonSurfaceView() = default;

void ComparisonSurfaceView::setGrid(
        const std::vector<float>& values,
        size_t columns,
        size_t rows,
        PortDomain domain,
        int note,
        float displayGain) {
    impl->setGrid(values, columns, rows, domain, note, displayGain);
}

void ComparisonSurfaceView::setCamera(const ZoomRect& camera) {
    impl->setCamera(camera);
}

ZoomRect ComparisonSurfaceView::camera() {
    return impl->camera();
}

void ComparisonSurfaceView::setZoomBarsVisible(bool visible) { impl->setZoomBarsVisible(visible); }
juce::Rectangle<int> ComparisonSurfaceView::plotBounds() const { return impl->plotBounds(); }
juce::Rectangle<int> ComparisonSurfaceView::zoomBounds(bool horizontal) const { return impl->zoomBounds(horizontal); }

bool ComparisonSurfaceView::renderOpenGL(float scaleFactor) {
    GLint viewport[4] {};
    GLint matrixMode {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_MATRIX_MODE, &matrixMode);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    const bool snapshotUpdated = impl->renderOpenGL(scaleFactor);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode((GLenum) matrixMode);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    return snapshotUpdated;
}

void ComparisonSurfaceView::releaseOpenGLResources() {
    impl->releaseOpenGLResources();
}

juce::var ComparisonSurfaceView::automationDiagnostics() const {
    return impl->automationDiagnostics();
}

void ComparisonSurfaceView::paint(juce::Graphics& graphics) {
    impl->paint(graphics);
}

void ComparisonSurfaceView::resized() {
    impl->resized();
}

}
