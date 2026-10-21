#include "Nodes/Envelope/Rendering/EnvelopeMorphSurfacePreview.h"

#include <Curve/Rasterization/Rasterizer/EnvelopeMorphSurfaceRasterizer.h>
#include <UI/Panels/ZoomPanel.h>

#include <algorithm>

#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"

namespace CycleV2 {

struct EnvelopeMorphSurfacePreview::Impl final :
        private CurvePanelHostDelegate
    ,   private Panel3D::DataRetriever {
    Impl(TrimeshPanelEnvironment& environment, std::function<void()> repaint) :
            repaint (std::move(repaint))
        ,   interactor (&environment.getRepo(), "EnvelopeSurfaceZoom")
        ,   panel (&environment.getRepo(), *this)
        ,   host (panel, *this) {
        interactor.init();
        interactor.stopTimer();
        interactor.setRasterizer(&rasterizer);
        panel.setInteractor(&interactor);
        panel.setInteractorMouseListenerEnabled(false);
        panel.setSharedCanvasMode(true);
        panel.setUseVertices(false);
        panel.setRenderProfile(TrimeshRenderProfile::fromDomain(PortDomain::EnvelopeSignal));
    }

    ~Impl() override {
        panel.setInteractor(nullptr);
        interactor.setRasterizer(nullptr);
    }

    Component* component() { return host.zoomComponent(); }

    void prepare(Mesh& mesh, GuideCurveProvider* guides, uint64_t revision,
            const MorphPosition& morph, int axis, int dimensions, bool gestureActive) {
        if (gestureActive || dimensions == 0) {
            return;
        }
        bool changed = cachedRevision != revision || cachedAxis != axis
                || cachedDimensions != dimensions || values.empty();
        for (const int coordinate : MorphDimensions::axes) {
            changed |= coordinate != axis && cachedMorph[coordinate] != morph[coordinate];
        }
        if (!changed) {
            return;
        }
        const ScopedLock lock(gridLock);
        constexpr int rows = 128;
        constexpr int columnCount = 48;
        values.resize(rows * columnCount);
        columns.clear();
        columns.reserve(columnCount);
        for (int column = 0; column < columnCount; ++column) {
            columns.emplace_back(values.data() + column * rows, rows,
                    (float) column / (columnCount - 1), 60);
        }
        rasterizer.setDimensionality(dimensions);
        rasterizer.setGuideCurveProvider(guides);
        rasterizer.renderSurface(&mesh, columns, rows, axis, morph);
        // E3 emits authored amplitude minus 0.5; scalar materials consume [0,1].
        if (mesh.getNumCubes() > 0) {
            Buffer<float>(values.data(), (int) values.size()).add(0.5f).clip(0.f, 1.f);
        }
        cachedRevision = revision;
        cachedMorph = morph;
        cachedAxis = axis;
        cachedDimensions = dimensions;
        ++rebuildCount;
        panel.bakeTexturesNextRepaint();
    }

    var automationState() const {
        const ScopedLock lock(gridLock);
        auto* state = new DynamicObject();
        state->setProperty("rebuildCount", (int64) rebuildCount);
        float minimum {};
        float maximum {};
        if (!values.empty()) {
            const auto extremes = std::minmax_element(values.begin(), values.end());
            minimum = *extremes.first;
            maximum = *extremes.second;
        }
        state->setProperty("minimum", minimum);
        state->setProperty("maximum", maximum);
        state->setProperty("centreValue", values.empty() ? 0.f : values[values.size() / 2 + 64]);
        state->setProperty("columnCount", (int) columns.size());
        state->setProperty("zoomControls", host.zoomControlsState());
        return state;
    }

    void render(Rectangle<float> bounds, float scaleFactor) {
        host.render(host.contentBounds(bounds), bounds, scaleFactor);
    }
    void releaseOpenGLResources() { host.releaseSharedGlResources(); }

private:
    void initialiseCurvePanel(Component* component) override {
        panel.initWithExternalComponent(component);
        panel.configureViewport({ 0.f, 1.f });
    }
    void updateCurvePanelZoom(bool) override {}
    void prepareCurvePanel() override {}
    void synchronizeCurvePanelSelection() override {}
    void repaintCurvePanel() override { repaint(); }
    void setCurvePanelCursor(const MouseCursor&) override {}
    bool allowsCurvePanelEditing() const override { return false; }
    void beginEdit() override {}
    void publishIntermediateRevision() override {}
    void commitEdit() override {}

    Buffer<float> getColumnArray() override { return { values.data(), (int) values.size() }; }
    Buffer<float> getScalarSurfaceArray() override { return getColumnArray(); }
    const std::vector<Column>& getColumns() override { return columns; }
    CriticalSection& getGridLock() override { return gridLock; }
    uint64_t getScalarSurfaceRevision() const override { return rebuildCount; }
    bool hasStableScalarSurfaceRevision() const override { return true; }

    std::function<void()> repaint;
    Rasterization::EnvelopeMorphSurfaceRasterizer rasterizer;
    TrimeshInteractor3D interactor;
    std::vector<float> values;
    std::vector<Column> columns;
    mutable CriticalSection gridLock;
    uint64_t rebuildCount {};
    uint64_t cachedRevision {};
    MorphPosition cachedMorph;
    int cachedAxis { -1 };
    int cachedDimensions { -1 };
    TrimeshPanel3D panel;
    CurvePanelHost host;
};

EnvelopeMorphSurfacePreview::EnvelopeMorphSurfacePreview(
        TrimeshPanelEnvironment& environment, std::function<void()> repaint) :
        impl(std::make_unique<Impl>(environment, std::move(repaint))) {}

EnvelopeMorphSurfacePreview::~EnvelopeMorphSurfacePreview() = default;

Component* EnvelopeMorphSurfacePreview::component() { return impl->component(); }

void EnvelopeMorphSurfacePreview::prepare(Mesh& mesh, GuideCurveProvider* guides, uint64_t revision,
        const MorphPosition& morph, int axis, int dimensions, bool gestureActive) {
    impl->prepare(mesh, guides, revision, morph, axis, dimensions, gestureActive);
}

void EnvelopeMorphSurfacePreview::render(Rectangle<float> bounds, float scaleFactor) {
    impl->render(bounds, scaleFactor);
}

void EnvelopeMorphSurfacePreview::releaseOpenGLResources() {
    impl->releaseOpenGLResources();
}

var EnvelopeMorphSurfacePreview::automationState() const { return impl->automationState(); }

}
