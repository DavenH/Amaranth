#include "Nodes/Envelope/Rendering/EnvelopeMorphSurfacePreview.h"

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvelopeMorphSurfaceRasterizer.h>
#include <UI/Panels/ZoomPanel.h>

#include <algorithm>

#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Curve/Panel/CurvePanelInfrastructure.h"
#include "Nodes/Guide/GuideCurveSnapshotProvider.h"
#include "Nodes/Trimesh/Panel/TrimeshInteractor3D.h"
#include "Nodes/Envelope/Rendering/EnvelopeSurfacePanel.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelEnvironment.h"
#include "Nodes/Trimesh/Rendering/TrimeshGuideRailRenderService.h"

namespace CycleV2 {

struct EnvelopeMorphSurfacePreview::Impl final :
        Component
    ,   private CurvePanelHostDelegate
    ,   private Panel3D::DataRetriever {
    Impl(TrimeshPanelEnvironment& panelEnvironment, std::function<void()> repaint) :
            environment (panelEnvironment)
        ,   repaint (std::move(repaint))
        ,   interactor (&environment.getRepo(), "EnvelopeSurfaceZoom")
        ,   panel (&environment.getRepo(), *this)
        ,   host (panel, *this, CurvePanelHost::ExpandedRendering::DirectOpenGL) {
        interactor.init();
        interactor.stopTimer();
        interactor.vertexLimits[Vertex::Phase] = { 0.f, 10.f };
        interactor.setRasterizer(&rasterizer);
        panel.setInteractor(&interactor);
        panel.setInteractorMouseListenerEnabled(false);
        panel.setPrimaryAxisVertical(true);
        panel.setUseVertices(false);
        panel.setRenderProfile(TrimeshRenderProfile::fromDomain(PortDomain::EnvelopeSignal));
        addAndMakeVisible(host.zoomComponent());
    }

    ~Impl() override {
        const ScopedLock renderLock(panel.getRenderLock());
        panel.setInteractor(nullptr);
        interactor.setMesh(nullptr);
        interactor.setRasterizer(nullptr);
        if (preparedMesh != nullptr) {
            preparedMesh->destroy();
        }
    }

    Component* component() { return this; }
    void setBipolar(bool value) { bipolar = value; }
    void setPlaybackCursors(const PlaybackCursorFrame& frame) { panel.setPlaybackCursors(frame); }

    void resized() override {
        host.zoomComponent()->setBounds(getLocalBounds().withTrimmedRight(4));
    }

    Rectangle<float> axisBarBounds() const {
        return { (float) getWidth() - 3.f, 1.f, 3.f,
                jmax(0.f, (float) panel.getLocalBounds().getHeight() - 2.f) };
    }

    void paint(Graphics& graphics) override {
        graphics.setColour(panel.primaryAxisColour());
        graphics.fillRect(axisBarBounds());
    }

    void prepare(Mesh& mesh, GuideCurveSnapshotProvider* guides, uint64_t revision,
            const MorphPosition& morph, int axis, int dimensions, bool gestureActive,
            Range<float> timeRange) {
        panel.setSlicePosition(morph[axis]);
        if (gestureActive || dimensions == 0) {
            return;
        }
        bool changed = cachedRevision != revision || cachedAxis != axis
                || cachedDimensions != dimensions || cachedTimeRange != timeRange
                || cachedBipolar != bipolar || values.empty();
        for (const int coordinate : MorphDimensions::axes) {
            changed |= coordinate != axis && cachedMorph[coordinate] != morph[coordinate];
        }
        if (!changed) {
            return;
        }
        const ScopedLock renderLock(panel.getRenderLock());
        const ScopedLock lock(gridLock);
        InteractionComplexityDiagnostics::recordMeshCopy(mesh.getNumVerts(), mesh.getNumCubes());
        copyPreparedMesh(mesh);
        prepareSurface(*preparedMesh, guides, morph, axis, dimensions, timeRange);
        prepareVertexRails(*preparedMesh, guides, morph, axis);
        configureTimeRange(timeRange);
        cachedRevision = revision;
        cachedMorph = morph;
        cachedAxis = axis;
        cachedDimensions = dimensions;
        cachedTimeRange = timeRange;
        cachedBipolar = bipolar;
        ++rebuildCount;
        panel.bakeTexturesNextRepaint();
    }

    void prepareSurface(Mesh& mesh, GuideCurveSnapshotProvider* guides,
            const MorphPosition& morph, int axis, int dimensions, Range<float> timeRange) {
        constexpr int rows = 128;
        constexpr int columnCount = 48;
        crossSections.resize(rows * columnCount);
        std::vector<Column> morphColumns;
        morphColumns.reserve(columnCount);
        for (int column = 0; column < columnCount; ++column) {
            morphColumns.emplace_back(crossSections.data() + column * rows, rows,
                    (float) column / (columnCount - 1), 60);
        }
        rasterizer.setDimensionality(dimensions);
        rasterizer.setGuideCurveProvider(guides);
        const uint32_t seed = GuideCurveSnapshotProvider::visualizationSeed(PortDomain::EnvelopeSignal);
        rasterizer.updateOffsetSeeds(guides != nullptr ? guides->size() : 0,
                GuideCurveProvider::tableSize, Rasterization::GuideCurveSeed::visualization(seed));
        rasterizer.setNoiseSeed((int) (seed % GuideCurveProvider::tableSize));
        rasterizer.renderSurface(&mesh, morphColumns, rows, axis, morph, timeRange, &sectionMarkers);
        // E3 emits authored amplitude minus 0.5; scalar materials consume [0,1].
        if (mesh.getNumCubes() > 0) {
            Buffer<float>(crossSections.data(), (int) crossSections.size()).add(0.5f).clip(0.f, 1.f);
        }
        transposeCrossSections(rows, columnCount, timeRange);
        panel.setSurfaceImage(EnvelopeSurfacePanel::createColourMap(
                values, rows, columnCount, timeRange, sectionMarkers, bipolar), timeRange);
    }

    void prepareVertexRails(Mesh& mesh, GuideCurveSnapshotProvider* guides,
            const MorphPosition& morph, int axis) {
        environment.setMorphPosition(morph, axis);
        interactor.setMesh(&mesh);
        interactor.setPrimaryViewAxis(axis);
        interactor.updateIntercepts();
        panel.setPrimaryViewAxis(axis);
        panel.setLineGuideTables(guides != nullptr
                ? TrimeshGuideRailRenderService::prepareTables(*guides,
                        GuideCurveSnapshotProvider::visualizationSeed(PortDomain::EnvelopeSignal))
                : std::vector<std::vector<float>>());
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
        state->setProperty("centreValue", values.empty() ? 0.f : values[values.size() / 2 + 24]);
        state->setProperty("columnCount", (int) columns.size());
        state->setProperty("rowCount", columns.empty() ? 0 : columns.front().size());
        state->setProperty("timeAxis", "x");
        state->setProperty("palette", bipolar ? "bipolar" : "blues");
        state->setProperty("textureUploadCount", (int64) panel.textureUploadCount());
        Array<var> sections;
        for (const auto& marker : sectionMarkers) {
            auto* section = new DynamicObject();
            section->setProperty("loopStart", marker.loopStart);
            section->setProperty("releaseStart", marker.releaseStart);
            section->setProperty("signalStart", marker.signalStart);
            section->setProperty("signalEnd", marker.signalEnd);
            sections.add(section);
        }
        state->setProperty("sections", sections);
        state->setProperty("timeMinimum", cachedTimeRange.getStart());
        state->setProperty("timeMaximum", cachedTimeRange.getEnd());
        state->setProperty("sampleTimeStep", columns.size() > 1 ? columns[1].x - columns[0].x : 0.f);
        Array<var> timeSamples;
        for (const auto& column : columns) {
            auto* sample = new DynamicObject();
            sample->setProperty("time", column.x);
            sample->setProperty("value", column[column.size() / 2]);
            timeSamples.add(sample);
        }
        state->setProperty("timeSamples", timeSamples);
        panel.appendPrimaryAxisAutomation(*state, axisBarBounds());
        Array<var> rails;
        const auto& pairs = interactor.getInterceptPairs();
        for (size_t index = 0; index + 1 < pairs.size(); index += 2) {
            const Vertex2 first = panel.displayIntercept(Vertex2(pairs[index]));
            const Vertex2 second = panel.displayIntercept(Vertex2(pairs[index + 1]));
            auto* rail = new DynamicObject();
            rail->setProperty("startX", first.x);
            rail->setProperty("startY", first.y);
            rail->setProperty("endX", second.x);
            rail->setProperty("endY", second.y);
            rails.add(rail);
        }
        state->setProperty("vertexRails", rails);
        state->setProperty("zoomControls", host.zoomControlsState());
        host.appendRenderDiagnostics(*state);
        return state;
    }

    void render(Rectangle<float> bounds, float scaleFactor) {
        const ScopedLock renderLock(panel.getRenderLock());
        const ScopedLock lock(gridLock);
        const auto frame = host.zoomComponent()->getBounds().toFloat()
                .translated(bounds.getX(), bounds.getY());
        host.render(host.contentBounds(frame), frame, scaleFactor, 0, rebuildCount);
    }
    void releaseOpenGLResources() { host.releaseSharedGlResources(); }

private:
    void copyPreparedMesh(const Mesh& mesh) {
        if (preparedMesh != nullptr) {
            preparedMesh->destroy();
        }
        if (dynamic_cast<const EnvelopeMesh*>(&mesh) != nullptr) {
            preparedMesh = std::make_unique<EnvelopeMesh>(mesh.getName());
        } else {
            preparedMesh = std::make_unique<Mesh>();
        }
        preparedMesh->deepCopy(&mesh);
    }

    void transposeCrossSections(int rows, int columnCount, Range<float> timeRange) {
        values.resize(crossSections.size());
        columns.clear();
        columns.reserve(rows);
        for (int time = 0; time < rows; ++time) {
            for (int morph = 0; morph < columnCount; ++morph) {
                values[(size_t) time * columnCount + morph] = crossSections[(size_t) morph * rows + time];
            }
            columns.emplace_back(values.data() + time * columnCount, columnCount,
                    timeRange.getStart() + (float) time * timeRange.getLength() / (rows - 1), 60);
        }
    }

    void configureTimeRange(Range<float> range) {
        if (auto* zoom = panel.getZoomPanel()) {
            auto& rect = zoom->rect;
            if (rect.xMinimum != range.getStart() || rect.xMaximum != range.getEnd()) {
                const bool fullRange = rect.x == rect.xMinimum && rect.w == rect.xMaximum - rect.xMinimum;
                rect.xMinimum = range.getStart();
                rect.xMaximum = range.getEnd();
                if (fullRange) {
                    rect.x = range.getStart();
                    rect.w = range.getLength();
                }
                zoom->panelZoomChanged(false);
            }
        }
    }

    void initialiseCurvePanel(Component* component) override {
        panel.initWithExternalComponent(component);
        panel.configureViewport({ 0.f, 1.f });
        configureTimeRange(cachedTimeRange);
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

    TrimeshPanelEnvironment& environment;
    std::function<void()> repaint;
    std::unique_ptr<Mesh> preparedMesh;
    Rasterization::EnvelopeMorphSurfaceRasterizer rasterizer;
    TrimeshInteractor3D interactor;
    std::vector<float> values;
    std::vector<float> crossSections;
    std::vector<Rasterization::EnvelopeSurfaceMarkers> sectionMarkers;
    std::vector<Column> columns;
    mutable CriticalSection gridLock;
    bool bipolar {};
    bool cachedBipolar {};
    uint64_t rebuildCount {};
    uint64_t cachedRevision {};
    MorphPosition cachedMorph;
    int cachedAxis { -1 };
    int cachedDimensions { -1 };
    Range<float> cachedTimeRange { 0.f, 1.f };
    EnvelopeSurfacePanel panel;
    CurvePanelHost host;
};

EnvelopeMorphSurfacePreview::EnvelopeMorphSurfacePreview(
        TrimeshPanelEnvironment& environment, std::function<void()> repaint) :
        impl(std::make_unique<Impl>(environment, std::move(repaint))) {}

EnvelopeMorphSurfacePreview::~EnvelopeMorphSurfacePreview() = default;

Component* EnvelopeMorphSurfacePreview::component() { return impl->component(); }

void EnvelopeMorphSurfacePreview::prepare(Mesh& mesh, GuideCurveSnapshotProvider* guides, uint64_t revision,
        const MorphPosition& morph, int axis, int dimensions, bool gestureActive, Range<float> timeRange) {
    impl->prepare(mesh, guides, revision, morph, axis, dimensions, gestureActive, timeRange);
}

void EnvelopeMorphSurfacePreview::render(Rectangle<float> bounds, float scaleFactor) {
    impl->render(bounds, scaleFactor);
}

void EnvelopeMorphSurfacePreview::setBipolar(bool bipolar) { impl->setBipolar(bipolar); }

void EnvelopeMorphSurfacePreview::setPlaybackCursors(const PlaybackCursorFrame& frame) {
    impl->setPlaybackCursors(frame);
}

void EnvelopeMorphSurfacePreview::releaseOpenGLResources() {
    impl->releaseOpenGLResources();
}

var EnvelopeMorphSurfacePreview::automationState() const { return impl->automationState(); }

}
