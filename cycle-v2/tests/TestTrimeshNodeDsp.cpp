#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphEditor.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeFactory.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideCurveSnapshotProvider.h"
#include "Nodes/Control/ModulationSource.h"
#include "Nodes/Trimesh/Dsp/TrimeshBlockwiseDsp.h"
#include "Nodes/Trimesh/Editor/TrimeshControlsComponent.h"
#include "Nodes/Trimesh/Dsp/TrimeshGridwiseDsp.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentMenu.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelBridge.h"
#include "Nodes/Trimesh/Panel/TrimeshPanel3D.h"
#include "Nodes/Trimesh/Panel/TrimeshPanelDataSource.h"
#include "Nodes/Trimesh/Rendering/TrimeshRenderProfile.h"
#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"
#include "Nodes/Trimesh/Rendering/SpectralRangeControlRenderer.h"
#include "Nodes/Trimesh/Rendering/TrimeshSurfaceRenderer.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"

#include <App/SingletonRepo.h>
#include <Audio/CycleDsp/SpectralLayerCore.h>
#include <Curve/Mesh/Intercept.h>
#include <Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.h>
#include <Util/LogRegionMapping.h>
#include <Util/LogRegions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace CycleV2;

namespace {

var selectedVertexEditorState(int vertexId) {
    auto state = std::make_unique<DynamicObject>();
    state->setProperty("selectedVertexId", vertexId);
    return var(state.release());
}

class RecordingTrimeshControlsDelegate final : public TrimeshControlsDelegate {
public:
    void setTrimeshPrimaryAxis(const String& axis) override { primaryAxis = axis; }
    void toggleTrimeshLinkAxis(const String& axis) override { linkedAxis = axis; }

    void beginTrimeshMorphControlEdit(const String& id, float value) override {
        activeParameter = id;
        beginValue = value;
        ++morphBeginCount;
    }

    void updateTrimeshMorphControlEdit(float value) override { updateValue = value; }
    void endTrimeshMorphControlEdit() override { ++morphEndCount; }

    void beginTrimeshRangeControlEdit(float value) override {
        beginValue = value;
        ++rangeBeginCount;
    }

    void updateTrimeshRangeControlEdit(float value) override { updateValue = value; }
    void endTrimeshRangeControlEdit() override { ++rangeEndCount; }

    void beginTrimeshVertexControlEdit(const String& id, float value) override {
        activeParameter = id;
        beginValue = value;
        ++vertexBeginCount;
    }

    void updateTrimeshVertexControlEdit(float value) override { updateValue = value; }
    void endTrimeshVertexControlEdit() override { ++vertexEndCount; }

    void showTrimeshVertexGuideMenu(const String& id, Rectangle<int>) override {
        guideParameter = id;
    }

    void selectTrimeshVertex(int index) override { selectedVertex = index; }

    int morphBeginCount {};
    int morphEndCount {};
    int rangeBeginCount {};
    int rangeEndCount {};
    int vertexBeginCount {};
    int vertexEndCount {};
    int selectedVertex { -1 };
    float beginValue {};
    float updateValue {};
    String activeParameter;
    String primaryAxis;
    String linkedAxis;
    String guideParameter;
};

class RecordingTrimeshPanelHostDelegate final : public TrimeshPanelHostDelegate {
public:
    void requestTrimeshPanelRepaint() override { ++repaintCount; }
    void setTrimeshPanelCursor(const MouseCursor& cursor) override { lastCursor = cursor; }

    int repaintCount {};
    MouseCursor lastCursor { MouseCursor::NormalCursor };
};

MouseEvent panelMouseEvent(
        Component& component,
        Point<float> position,
        ModifierKeys modifiers,
        Point<float> mouseDownPosition,
        bool dragged) {
    const Time now = Time::getCurrentTime();
    return {
            Desktop::getInstance().getMainMouseSource(),
            position,
            modifiers,
            1.f,
            0.f,
            0.f,
            0.f,
            0.f,
            &component,
            &component,
            now,
            mouseDownPosition,
            now,
            1,
            dragged
    };
}

}

TEST_CASE("Trimesh topology snapshots preserve the authoritative Mesh contract",
        "[cycle-v2][nodes][trimesh][topology]") {
    auto source = TrimeshMeshFactory::createDefaultMesh("AuthoredTrimesh");
    REQUIRE(source != nullptr);
    source->getVerts()[3]->values[Vertex::Amp] = 0.137f;
    source->getCubes()[1]->guideCurveChans[Vertex::Amp] = 7;
    source->getCubes()[1]->guideCurveGains[Vertex::Amp] = 0.73f;
    const var snapshot = source->writeJSON();

    Mesh restored("RestoredTrimesh");
    REQUIRE(restored.readJSON(snapshot));
    REQUIRE(restored.getNumVerts() == source->getNumVerts());
    REQUIRE(restored.getNumCubes() == source->getNumCubes());
    REQUIRE(restored.getVerts()[3]->values[Vertex::Amp] == Catch::Approx(0.137f));
    REQUIRE(restored.getCubes()[1]->guideCurveChans[Vertex::Amp] == 7);
    REQUIRE(restored.getCubes()[1]->guideCurveGains[Vertex::Amp] == Catch::Approx(0.73f));
    REQUIRE(JSON::toString(restored.writeJSON(), false) == JSON::toString(snapshot, false));

    restored.destroy();
    source->destroy();
}

TEST_CASE("Guide snapshots preserve Cycle 1 padded bipolar tables",
        "[cycle-v2][nodes][guide][parity]") {
    GuideCurveResource guide;
    guide.id = "guide";
    std::vector<FlatCurveVertex> vertices {
            { 1, 0.05f, 0.25f, 1.f },
            { 2, 0.95f, 0.75f, 1.f }
    };
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices(std::move(vertices)));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    guide.noise = 0.f;
    guide.dcOffset = 0.f;
    guide.phase = 0.f;

    GuideCurveSnapshotProvider provider;
    REQUIRE(provider.addGuide(guide));
    REQUIRE(provider.size() == 1);
    REQUIRE(provider.getTableDensity(0) == 2);
    REQUIRE(provider.getTable(0).size() == GuideCurveProvider::tableSize);

    GuideCurveProvider::NoiseContext context;
    REQUIRE(provider.getTableValue(0, 0.f, context) == Catch::Approx(-0.25f).margin(1e-4f));
    REQUIRE(provider.getTableValue(0, 0.5f, context) == Catch::Approx(0.f).margin(1e-4f));
    REQUIRE(provider.getTableValue(0, 1.f, context) == Catch::Approx(0.25f).margin(1e-4f));
}

TEST_CASE("Guide snapshot noise and offsets are deterministic",
        "[cycle-v2][nodes][guide][parity]") {
    GuideCurveResource guide;
    guide.id = "guide";
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({
            { 1, 0.05f, 0.2f, 1.f },
            { 2, 0.95f, 0.8f, 1.f }
    }));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    guide.noise = 0.4f;
    guide.dcOffset = 0.3f;
    guide.phase = 0.2f;

    GuideCurveSnapshotProvider first;
    GuideCurveSnapshotProvider repeated;
    REQUIRE(first.addGuide(guide));
    REQUIRE(repeated.addGuide(guide));

    GuideCurveProvider::NoiseContext context;
    context.noiseSeed = 127;
    context.vertOffset = 521;
    context.phaseOffset = 913;
    REQUIRE(first.getTableValue(0, 0.37f, context)
            == Catch::Approx(repeated.getTableValue(0, 0.37f, context)));

    std::array<float, 64> firstSamples {};
    std::array<float, 64> repeatedSamples {};
    first.sampleDownAddNoise(0, { firstSamples.data(), (int) firstSamples.size() }, context);
    repeated.sampleDownAddNoise(
            0,
            { repeatedSamples.data(), (int) repeatedSamples.size() },
            context);
    REQUIRE(firstSamples == repeatedSamples);
    REQUIRE(GuideCurveSnapshotProvider::visualizationSeed(PortDomain::TimeSignal)
            == 0x54494d45u);
    REQUIRE(GuideCurveSnapshotProvider::visualizationSeed(PortDomain::SpectralPhaseSignal)
            == 0x50484153u);
    REQUIRE(GuideCurveSnapshotProvider::visualizationSeed(
            PortDomain::SpectralMagnitudeSignal) == 0x53504543u);
}

TEST_CASE("Prepared Trimesh guides affect blockwise and gridwise rendering",
        "[cycle-v2][nodes][trimesh][guide][parity]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("GuidedTrimesh");
    REQUIRE(mesh != nullptr);
    mesh->getCubes().front()->guideCurveGainAt(Vertex::Amp) = 1.f;

    NodeGraph graph;
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({
            { 1, 0.05f, 1.f, 1.f },
            { 2, 0.95f, 1.f, 1.f }
    }));
    GuideCurveResource guide;
    guide.id = "guide";
    guide.shortLabel = "G1";
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    guide.noise = 0.4f;
    guide.dcOffset = 0.3f;
    guide.phase = 0.2f;
    Node trimesh = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    trimesh.model = TrimeshNodeModelState::copyOf(*mesh, 2);
    graph.addNode(std::move(trimesh));
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    REQUIRE(graph.assignGuideCurve({ "guide", "mesh", { 0, GuideCurveField::Amplitude } }));
    REQUIRE(graph.assignGuideCurve({ "guide", "mesh", { 0, GuideCurveField::Time } }));

    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    const auto step = std::find_if(
            compiled.plan.steps.begin(),
            compiled.plan.steps.end(),
            [](const GraphExecutionStep& candidate) {
                return candidate.nodeId == "mesh";
            });
    REQUIRE(step != compiled.plan.steps.end());
    const auto configuration = std::dynamic_pointer_cast<const TrimeshConfiguration>(
            step->configuration.value);
    REQUIRE(configuration != nullptr);
    REQUIRE(configuration->guideAssignmentCount == 2);
    REQUIRE(configuration->guideCurveProvider != nullptr);
    Mesh& preparedMesh = *const_cast<Mesh*>(configuration->mesh.get());
    REQUIRE(preparedMesh.getCubes().front()->guideCurveAt(Vertex::Amp) == 0);
    REQUIRE(preparedMesh.getCubes().front()->guideCurveAt(Vertex::Time) == 0);

    Rasterization::TrilinearMeshRasterizer componentRasterizer;
    componentRasterizer.setMesh(&preparedMesh);
    componentRasterizer.setGuideCurveProvider(configuration->guideCurveProvider.get());
    Rasterization::RasterizationRequest componentRequest;
    componentRequest.cyclic = true;
    componentRequest.morph = configuration->morph;
    componentRequest.primaryViewDimension = configuration->primaryViewAxis;
    componentRequest.scalingMode = Rasterization::PointScalingMode::Bipolar;
    componentRequest.decoupleComponentDeforms = true;
    const auto& componentResult = componentRasterizer.renderWaveform({
            preparedMesh,
            componentRequest,
            0.f
    });
    REQUIRE_FALSE(componentResult.guideCurveRegions.empty());

    constexpr int sampleCount = 128;
    std::vector<float> plainSamples(sampleCount);
    std::vector<float> guidedSamples(sampleCount);
    TrimeshBlockwiseDsp plain;
    plain.setMesh(mesh.get());
    plain.setMorphPosition(configuration->morph);
    plain.setPrimaryViewAxis(configuration->primaryViewAxis);
    plain.setCyclic(true);
    plain.renderCycleInto(Buffer<float>(plainSamples.data(), sampleCount), PortDomain::TimeSignal);

    TrimeshBlockwiseDsp guided;
    guided.setMesh(&preparedMesh);
    guided.setGuideCurveProvider(configuration->guideCurveProvider.get());
    guided.setMorphPosition(configuration->morph);
    guided.setPrimaryViewAxis(configuration->primaryViewAxis);
    guided.setCyclic(true);
    guided.renderCycleInto(Buffer<float>(guidedSamples.data(), sampleCount), PortDomain::TimeSignal);

    double blockDifference {};
    for (int i = 0; i < sampleCount; ++i) {
        blockDifference += std::abs(guidedSamples[(size_t) i] - plainSamples[(size_t) i]);
    }
    REQUIRE(blockDifference > 0.1);

    std::vector<float> firstLifecycle(sampleCount);
    std::vector<float> repeatedLifecycle(sampleCount);
    std::vector<float> nextLifecycle(sampleCount);
    guided.setVoiceLifecycleSeed(0x12345678u);
    guided.renderCycleInto(
            Buffer<float>(firstLifecycle.data(), sampleCount),
            PortDomain::TimeSignal);
    guided.renderCycleInto(
            Buffer<float>(repeatedLifecycle.data(), sampleCount),
            PortDomain::TimeSignal);
    guided.setVoiceLifecycleSeed(0x87654321u);
    guided.renderCycleInto(
            Buffer<float>(nextLifecycle.data(), sampleCount),
            PortDomain::TimeSignal);
    REQUIRE(firstLifecycle == repeatedLifecycle);
    REQUIRE(firstLifecycle != nextLifecycle);

    constexpr int columnCount = 8;
    std::vector<float> plainGrid(columnCount * sampleCount);
    std::vector<float> guidedGrid(columnCount * sampleCount);
    TrimeshGridwiseDsp plainGridDsp;
    TrimeshGridwiseDsp guidedGridDsp;
    guidedGridDsp.setGuideCurveProvider(configuration->guideCurveProvider.get());
    REQUIRE(plainGridDsp.renderColumnsInto(
            *mesh,
            configuration->morph,
            configuration->primaryViewAxis,
            columnCount,
            Buffer<float>(plainGrid.data(), (int) plainGrid.size()),
            PortDomain::TimeSignal));
    REQUIRE(guidedGridDsp.renderColumnsInto(
            preparedMesh,
            configuration->morph,
            configuration->primaryViewAxis,
            columnCount,
            Buffer<float>(guidedGrid.data(), (int) guidedGrid.size()),
            PortDomain::TimeSignal));

    double gridDifference {};
    for (size_t i = 0; i < guidedGrid.size(); ++i) {
        gridDifference += std::abs(guidedGrid[i] - plainGrid[i]);
    }
    REQUIRE(gridDifference > 0.5);

    mesh->destroy();
}

TEST_CASE("Invalid Trimesh topology snapshots do not partially mutate the mesh",
        "[cycle-v2][nodes][trimesh][topology]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("StableTrimesh");
    REQUIRE(mesh != nullptr);
    const var before = mesh->writeJSON();
    const var invalid = JSON::parse(R"({"name":"invalid","version":2,"vertices":[],"cubes":[{"vertexIds":[99]}]})");

    auto wrapper = std::make_unique<DynamicObject>();
    wrapper->setProperty("schema", "trimesh");
    wrapper->setProperty("version", 2);
    wrapper->setProperty("revision", 2);
    wrapper->setProperty("mesh", invalid);
    String error;
    REQUIRE(TrimeshNodeModelCodec().readJSON(var(wrapper.release()), error) == nullptr);
    REQUIRE(error.isNotEmpty());
    REQUIRE(JSON::toString(mesh->writeJSON(), false) == JSON::toString(before, false));
    mesh->destroy();
}

TEST_CASE("Trimesh surface profiles colour time and spectral domains distinctly", "[cycle-v2][nodes][trimesh]") {
    const TrimeshRenderProfile timeProfile =
            TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal);
    const TrimeshRenderProfile magProfile =
            TrimeshRenderProfile::fromDomain(PortDomain::SpectralMagnitudeSignal);
    const TrimeshRenderProfile phaseProfile =
            TrimeshRenderProfile::fromDomain(PortDomain::SpectralPhaseSignal);
    const auto& timeSurfaceStyle = timeProfile.getSurfaceStyle();
    const auto& magSurfaceStyle = magProfile.getSurfaceStyle();
    const auto& phaseSurfaceStyle = phaseProfile.getSurfaceStyle();
    const auto& timeCurveStyle = timeProfile.getCurveStyle();
    const auto& magCurveStyle = magProfile.getCurveStyle();
    const auto& phaseCurveStyle = phaseProfile.getCurveStyle();
    const auto& timeSliceStyle = timeProfile.getSliceStyle();
    const auto& magSliceStyle = magProfile.getSliceStyle();
    const auto& phaseSliceStyle = phaseProfile.getSliceStyle();
    const Colour timeMid = timeSurfaceStyle.colourForValue(0.55f);
    const Colour magMid = magSurfaceStyle.colourForValue(0.55f);
    const Colour phaseMid = phaseSurfaceStyle.colourForValue(0.55f);
    const Colour magLow = magSurfaceStyle.colourForValue(0.01f);
    const Colour magHigh = magSurfaceStyle.colourForValue(0.90f);

    REQUIRE(timeMid != magMid);
    REQUIRE(magMid != phaseMid);
    REQUIRE(timeMid.getFloatAlpha() == Catch::Approx(0.82f).margin(0.01f));
    REQUIRE(magLow.getFloatAlpha() < 0.01f);
    REQUIRE(magHigh.getFloatAlpha() == Catch::Approx(1.f));
    REQUIRE_FALSE(timeSurfaceStyle.textureUsesAlpha);
    REQUIRE(magSurfaceStyle.textureUsesAlpha);
    REQUIRE(phaseSurfaceStyle.textureUsesAlpha);
    REQUIRE(timeSliceStyle.background == TrimeshSliceBackground::Waveform);
    REQUIRE(magSliceStyle.background == TrimeshSliceBackground::SpectrumMagnitude);
    REQUIRE(phaseSliceStyle.background == TrimeshSliceBackground::SpectrumPhase);
    REQUIRE_FALSE(timeSliceStyle.isSpectral());
    REQUIRE(magSliceStyle.isSpectral());
    REQUIRE(phaseSliceStyle.isSpectral());
    REQUIRE(timeSliceStyle.fillColour != magSliceStyle.fillColour);
    REQUIRE(timeSliceStyle.minorGridColour != magSliceStyle.minorGridColour);
    REQUIRE(timeSliceStyle.majorGridColour != magSliceStyle.majorGridColour);
    REQUIRE(magSliceStyle.fillColour == phaseSliceStyle.fillColour);
    REQUIRE(timeSurfaceStyle.minorGridColour != magSurfaceStyle.minorGridColour);
    REQUIRE(timeSurfaceStyle.majorGridColour != magSurfaceStyle.majorGridColour);
    REQUIRE(magSurfaceStyle.minorGridColour == phaseSurfaceStyle.minorGridColour);
    REQUIRE(timeCurveStyle.bipolar);
    REQUIRE_FALSE(magCurveStyle.bipolar);
    REQUIRE(phaseCurveStyle.bipolar);
    REQUIRE(timeCurveStyle.cyclic);
    REQUIRE_FALSE(magCurveStyle.cyclic);
    REQUIRE_FALSE(phaseCurveStyle.cyclic);
    REQUIRE(timeCurveStyle.xMinimum == Catch::Approx(-0.05f));
    REQUIRE(timeCurveStyle.xMaximum == Catch::Approx(1.05f));
    REQUIRE(magCurveStyle.xMinimum == Catch::Approx(0.f));
    REQUIRE(magCurveStyle.xMaximum == Catch::Approx(1.f));
    REQUIRE(timeCurveStyle.positiveColour == timeCurveStyle.negativeColour);
    REQUIRE(magCurveStyle.positiveColour == magCurveStyle.negativeColour);
    REQUIRE_FALSE(phaseCurveStyle.positiveColour == phaseCurveStyle.negativeColour);
    REQUIRE(phaseCurveStyle.positiveColour.toColour() == colourForDomain(PortDomain::SpectralPhaseSignal).withAlpha(0.84f));
    REQUIRE_FALSE(phaseCurveStyle.negativeColour == magCurveStyle.negativeColour);
    REQUIRE_FALSE(phaseCurveStyle.negativeColour == magCurveStyle.positiveColour);
}

TEST_CASE("Expanded Trimesh surfaces use their complete layout rows", "[cycle-v2][nodes][trimesh][ui]") {
    const Rectangle<float> content { 10.f, 42.f, 880.f, 570.f };
    const Rectangle<float> grid = TrimeshWidget::expandedGridPanelContentBounds(content);
    const Rectangle<float> wave = TrimeshWidget::expandedWavePanelContentBounds(content);

    REQUIRE(grid.getY() == Catch::Approx(content.getY()));
    REQUIRE(grid.getHeight() == Catch::Approx(content.getHeight() * 0.54f));
    REQUIRE(grid.getWidth() == Catch::Approx(content.getWidth() * 0.50f));
    REQUIRE(wave.getY() - grid.getBottom() == Catch::Approx(8.f));
    REQUIRE(wave.getBottom() == Catch::Approx(content.getBottom()));
}

TEST_CASE("Trimesh surface renderer creates vertically oriented heatmap images", "[cycle-v2][nodes][trimesh]") {
    TrimeshRenderData renderData;
    renderData.rows = 2;
    renderData.columns = 2;
    renderData.surface = {
            0.10f,
            0.30f,
            0.60f,
            0.90f
    };

    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal);
    const Image image = TrimeshSurfaceRenderer::createHeatmapImage(renderData, profile);
    const auto requirePixelNear = [&image, &profile](int x, int y, float value) {
        const Colour actual = image.getPixelAt(x, y);
        const Colour expected = TrimeshSurfaceRenderer::colourForProfile(value, profile);

        REQUIRE(actual.getFloatRed() == Catch::Approx(expected.getFloatRed()).margin(0.01f));
        REQUIRE(actual.getFloatGreen() == Catch::Approx(expected.getFloatGreen()).margin(0.01f));
        REQUIRE(actual.getFloatBlue() == Catch::Approx(expected.getFloatBlue()).margin(0.01f));
        REQUIRE(actual.getFloatAlpha() == Catch::Approx(expected.getFloatAlpha()).margin(0.01f));
    };

    REQUIRE(image.isValid());
    REQUIRE(image.getWidth() == 2);
    REQUIRE(image.getHeight() == 2);
    requirePixelNear(0, 1, 0.10f);
    requirePixelNear(0, 0, 0.30f);
    requirePixelNear(1, 1, 0.60f);
    requirePixelNear(1, 0, 0.90f);
}

TEST_CASE("Trimesh side panel renderer keeps vertex rails inside parameter rows", "[cycle-v2][nodes][trimesh]") {
    const Rectangle<float> parameterArea { 20.f, 40.f, 180.f, 140.f };

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        const Rectangle<float> rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(row);

        REQUIRE(parameterArea.contains(row));
        REQUIRE(row.contains(rail));
        REQUIRE(rail.getHeight() == Catch::Approx(8.f));
        REQUIRE(rail.getWidth() > 0.f);
    }
}

TEST_CASE("Trimesh vertex deformer controls apply only to the right parameter column",
        "[cycle-v2][nodes][trimesh][geometry][guide]") {
    const Rectangle<float> parameterArea { 20.f, 40.f, 620.f, 150.f };

    for (int i = 0; i < 6; ++i) {
        const Rectangle<float> row =
                TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        const bool showsGuideControls =
                TrimeshSidePanelRenderer::showsGuideControlsForParameter(i);
        const Rectangle<float> rail = TrimeshSidePanelRenderer::vertexParameterRailBounds(
                row,
                showsGuideControls
                        ? TrimeshSidePanelRenderer::GuideControls::Visible
                        : TrimeshSidePanelRenderer::GuideControls::Hidden);

        REQUIRE(showsGuideControls == (i >= 3));
        if (i < 3) {
            const Rectangle<float> pairedRow =
                    TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i + 3);
            const Rectangle<float> pairedRail =
                    TrimeshSidePanelRenderer::vertexParameterRailBounds(pairedRow);
            REQUIRE(rail.getWidth() > pairedRail.getWidth() + 50.f);
        }
    }
}

TEST_CASE("Envelope vertex rails reclaim unsupported Guide control space",
        "[cycle-v2][nodes][envelope][geometry][guide]") {
    const Rectangle<float> row { 0.f, 0.f, 260.f, 30.f };
    const Rectangle<float> trimeshRail =
            TrimeshSidePanelRenderer::vertexParameterRailBounds(
                    row,
                    TrimeshSidePanelRenderer::GuideControls::Visible);
    const Rectangle<float> envelopeRail =
            TrimeshSidePanelRenderer::vertexParameterRailBounds(
                    row,
                    TrimeshSidePanelRenderer::GuideControls::Hidden);
    const Rectangle<float> guide =
            TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);

    REQUIRE(envelopeRail.getWidth() > trimeshRail.getWidth() + 40.f);
    REQUIRE(envelopeRail.getRight() > guide.getX());
    REQUIRE(trimeshRail.getRight() < guide.getX());
}

TEST_CASE("Trimesh side panel renderer keeps all control surfaces in panel bounds", "[cycle-v2][nodes][trimesh]") {
    const Rectangle<float> sideArea { 100.f, 50.f, 360.f, 420.f };

    const Rectangle<float> cube = TrimeshSidePanelRenderer::morphCubeBounds(sideArea, true);
    const Rectangle<float> parameterArea =
            TrimeshSidePanelRenderer::vertexParameterPanelBounds(sideArea, true);
    REQUIRE(sideArea.contains(cube));
    REQUIRE(sideArea.contains(parameterArea));
    REQUIRE_FALSE(cube.intersects(parameterArea));
    REQUIRE(cube.getWidth() >= 80.f);

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> morphRail =
                TrimeshSidePanelRenderer::morphRailBounds(sideArea, i, true);
        const Rectangle<float> primary =
                TrimeshSidePanelRenderer::primaryAxisBounds(sideArea, i, true);
        const Rectangle<float> link =
                TrimeshSidePanelRenderer::linkToggleBounds(sideArea, i, true);

        REQUIRE(sideArea.contains(morphRail));
        REQUIRE(morphRail.getWidth() >= 96.f);
        REQUIRE(sideArea.contains(primary));
        REQUIRE(sideArea.contains(link));
        REQUIRE(primary.getWidth() == Catch::Approx(link.getWidth()));
        REQUIRE(primary.getHeight() == Catch::Approx(link.getHeight()));
    }

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        REQUIRE(parameterArea.contains(row));
        REQUIRE(TrimeshSidePanelRenderer::vertexParameterRailBounds(row).getWidth() >= 72.f);
    }

    const Rectangle<float> rangeRow =
            TrimeshSidePanelRenderer::spectralRangeRowBounds(sideArea);
    const Rectangle<float> rangeRail =
            TrimeshSidePanelRenderer::spectralRangeRailBounds(sideArea);
    REQUIRE(sideArea.contains(rangeRow));
    REQUIRE(rangeRow.contains(rangeRail));
    REQUIRE(rangeRail.getWidth() >= 96.f);
    REQUIRE(rangeRow.getY()
            > TrimeshSidePanelRenderer::morphRailBounds(sideArea, 2, true).getBottom());
}

TEST_CASE("Production Trimesh controls place two-column vertex rows above morph and cube controls",
        "[cycle-v2][nodes][trimesh][geometry]") {
    const Rectangle<float> sideArea { 0.f, 0.f, 550.f, 370.f };
    const Rectangle<float> cube = TrimeshSidePanelRenderer::morphCubeBounds(sideArea, true);
    const Rectangle<float> vertex =
            TrimeshSidePanelRenderer::vertexParameterPanelBounds(sideArea, true);
    const Rectangle<float> morph =
            TrimeshSidePanelRenderer::morphRailBounds(sideArea, 0, true);
    const Rectangle<float> range =
            TrimeshSidePanelRenderer::spectralRangeRailBounds(sideArea);

    const Rectangle<float> timeRow =
            TrimeshSidePanelRenderer::vertexParameterRowBounds(vertex, 0);
    const Rectangle<float> blueRow =
            TrimeshSidePanelRenderer::vertexParameterRowBounds(vertex, 2);
    const Rectangle<float> phaseRow =
            TrimeshSidePanelRenderer::vertexParameterRowBounds(vertex, 3);
    const Rectangle<float> curveRow =
            TrimeshSidePanelRenderer::vertexParameterRowBounds(vertex, 5);

    REQUIRE(vertex.getWidth() > sideArea.getWidth() * 0.9f);
    REQUIRE(timeRow.getX() == Catch::Approx(blueRow.getX()));
    REQUIRE(phaseRow.getX() == Catch::Approx(curveRow.getX()));
    REQUIRE(timeRow.getY() == Catch::Approx(phaseRow.getY()));
    REQUIRE(blueRow.getY() == Catch::Approx(curveRow.getY()));
    REQUIRE(timeRow.getRight() < phaseRow.getX());
    REQUIRE(vertex.getBottom() < morph.getY());
    REQUIRE(vertex.getBottom() < cube.getY());
    REQUIRE(morph.getRight() < cube.getX());
    REQUIRE(range.getRight() < cube.getX());
}

TEST_CASE("Spectral range display scales round-trip through DSP mappings",
        "[cycle-v2][nodes][trimesh][range]") {
    using CycleDsp::SpectralLayerCore;

    for (const float scale : { 0.1f, 1.f, 10.f, 100.f }) {
        const float range = SpectralLayerCore::rangeForMagnitudeScale(scale);
        REQUIRE(SpectralLayerCore::magnitudeRangeScale(range)
                == Catch::Approx(scale).epsilon(0.0001));
    }

    for (const float scale : { 1.f, 10.f, 100.f }) {
        const float range = SpectralLayerCore::rangeForPhaseOffsetScale(scale);
        REQUIRE(SpectralLayerCore::phaseOffsetScale(range)
                == Catch::Approx(scale).epsilon(0.0001));
    }
}

TEST_CASE("Wide Trimesh controls pair morph travel with a lower-right cube",
        "[cycle-v2][nodes][trimesh][geometry]") {
    const Rectangle<float> sideArea { 0.f, 0.f, 1080.f, 260.f };
    const Rectangle<float> cube = TrimeshSidePanelRenderer::morphCubeBounds(sideArea);
    const Rectangle<float> vertex =
            TrimeshSidePanelRenderer::vertexParameterPanelBounds(sideArea);
    const Rectangle<float> rail = TrimeshSidePanelRenderer::morphRailBounds(sideArea, 0);
    const Rectangle<float> axis = TrimeshSidePanelRenderer::primaryAxisBounds(sideArea, 0);

    REQUIRE(vertex.getRight() == Catch::Approx(sideArea.getRight() - 9.f));
    REQUIRE(vertex.getBottom() < cube.getY());
    REQUIRE(rail.getRight() < cube.getX());
    REQUIRE(rail.getWidth() > 400.f);
    REQUIRE(rail.getRight() < axis.getX());
    REQUIRE(axis.getX() - rail.getRight() <= 12.f);
}

TEST_CASE("Trimesh Guide dropdowns sit between slider bodies and gain knobs",
        "[cycle-v2][nodes][trimesh][geometry][guide]") {
    const Rectangle<float> row { 20.f, 40.f, 280.f, 29.f };
    const Rectangle<float> rail =
            TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
    const Rectangle<float> guide =
            TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);

    REQUIRE(guide.getHeight() == Catch::Approx(row.getHeight()));
    REQUIRE(guide.getRight() < row.getRight());
    REQUIRE(rail.getRight() < guide.getX());
    REQUIRE(guide.getX() - rail.getRight() >= 6.f);
}

TEST_CASE("Trimesh guide gain knobs are distinct controls after Guide targets",
        "[cycle-v2][nodes][trimesh][geometry][guide][gain]") {
    const Rectangle<float> row { 20.f, 40.f, 280.f, 29.f };
    const Rectangle<float> rail =
            TrimeshSidePanelRenderer::vertexParameterRailBounds(row);
    const Rectangle<float> gain =
            TrimeshSidePanelRenderer::vertexParameterGuideGainBounds(row);
    const Rectangle<float> guide =
            TrimeshSidePanelRenderer::vertexParameterGuideBounds(row);

    REQUIRE(row.contains(gain));
    REQUIRE(gain.getWidth() == Catch::Approx(gain.getHeight()));
    REQUIRE(gain.getHeight() >= 20.f);
    REQUIRE(rail.getRight() < guide.getX());
    REQUIRE(guide.getRight() < gain.getX());
    REQUIRE_FALSE(rail.intersects(gain));
    REQUIRE_FALSE(gain.intersects(guide));
}

TEST_CASE("Spectral range label is vertically centred on its rail",
        "[cycle-v2][nodes][trimesh][geometry][range]") {
    const Rectangle<float> row { 20.f, 40.f, 280.f, 45.f };
    const Rectangle<float> rail { 92.f, 49.f, 196.f, 7.f };
    const Rectangle<float> label = SpectralRangeControlRenderer::labelBounds(row, rail);

    REQUIRE(label.getCentreY() == Catch::Approx(rail.getCentreY()));
}

TEST_CASE("Shared morph controls expose precise markers and aligned group headers",
        "[cycle-v2][nodes][trimesh][envelope][geometry]") {
    const Rectangle<float> rail { 20.f, 40.f, 120.f, 7.f };
    const Rectangle<float> marker =
            TrimeshSidePanelRenderer::morphMarkerBounds(rail, 0.25f);

    REQUIRE(marker.getCentreX() == Catch::Approx(50.f));
    REQUIRE(marker.getWidth() <= 2.f);
    REQUIRE(marker.getHeight() >= 14.f);
    REQUIRE(marker.getHeight() > marker.getWidth() * 7.f);

    const Rectangle<float> firstRow { 100.f, 80.f, 180.f, 31.f };
    const Rectangle<float> axisButton { 218.f, 85.5f, 20.f, 20.f };
    const Rectangle<float> linkButton { 244.f, 85.5f, 20.f, 20.f };
    const Rectangle<float> axisHeader =
            TrimeshSidePanelRenderer::morphColumnHeaderBounds(axisButton, firstRow);
    const Rectangle<float> linkHeader =
            TrimeshSidePanelRenderer::morphColumnHeaderBounds(linkButton, firstRow);

    REQUIRE(axisHeader.getCentreX() == Catch::Approx(axisButton.getCentreX()));
    REQUIRE(linkHeader.getCentreX() == Catch::Approx(linkButton.getCentreX()));
    REQUIRE(axisHeader.getBottom() <= firstRow.getY());
    REQUIRE(linkHeader.getBottom() <= firstRow.getY());
    REQUIRE_FALSE(axisHeader.intersects(linkHeader));
}

TEST_CASE("Trimesh blockwise DSP renders a source cycle from a trilinear mesh", "[cycle-v2][nodes][trimesh]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshBlockwiseDsp dsp;

    SignalPayload output;
    dsp.setMesh(mesh.get());
    dsp.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    dsp.renderCycle(8, PortDomain::TimeSignal, ChannelLayout::LinkedStereo, output);

    REQUIRE(output.domain == PortDomain::TimeSignal);
    REQUIRE(output.channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(output.block.samples.size() == 8);
    REQUIRE(output.block.samples[0] == Catch::Approx(0.f).margin(0.35f));
    REQUIRE(*std::min_element(output.block.samples.begin(), output.block.samples.end())
            < *std::max_element(output.block.samples.begin(), output.block.samples.end()));

    mesh->destroy();
}

TEST_CASE("Trimesh blockwise DSP initializes resolved linked-stereo channels",
        "[cycle-v2][nodes][trimesh][stereo]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshBlockwiseDsp dsp;
    SignalPayload output;
    output.secondaryBlock.samples.assign(32, std::numeric_limits<float>::quiet_NaN());

    dsp.setMesh(mesh.get());
    dsp.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    dsp.renderCycle(32, PortDomain::SpectralPhaseSignal, ChannelLayout::StereoPair, output);

    REQUIRE(output.isStereo());
    REQUIRE(output.secondaryBlock.samples == output.block.samples);
    REQUIRE(std::all_of(
            output.secondaryBlock.samples.begin(),
            output.secondaryBlock.samples.end(),
            [](float sample) { return std::isfinite(sample); }));

    mesh->destroy();
}

TEST_CASE(
        "Trimesh DSP preserves spectral domain scaling and phase curve interpolation",
        "[cycle-v2][nodes][trimesh][dsp][domains]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshBlockwiseDsp dsp;
    dsp.setMesh(mesh.get());
    dsp.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    dsp.setCyclic(false);
    dsp.setFrequencyMidiNote(48);

    SignalPayload magnitude;
    SignalPayload phase;
    SignalPayload time;
    SignalPayload c5Magnitude;
    dsp.renderCycle(
            32,
            PortDomain::SpectralMagnitudeSignal,
            ChannelLayout::Mono,
            magnitude);
    dsp.renderCycle(
            32,
            PortDomain::SpectralPhaseSignal,
            ChannelLayout::Mono,
            phase);
    dsp.renderCycle(32, PortDomain::TimeSignal, ChannelLayout::Mono, time);
    dsp.setFrequencyMidiNote(72);
    dsp.renderCycle(
            32,
            PortDomain::SpectralMagnitudeSignal,
            ChannelLayout::Mono,
            c5Magnitude);

    REQUIRE(magnitude.block.samples.size() == phase.block.samples.size());
    REQUIRE(magnitude.block.samples.size() == time.block.samples.size());
    REQUIRE(magnitude.block.samples.size() == c5Magnitude.block.samples.size());
    float pitchDifference {};
    float uniformSamplingDifference {};
    float phaseInterpolationDifference {};
    for (size_t index = 0; index < magnitude.block.samples.size(); ++index) {
        pitchDifference += std::abs(
                magnitude.block.samples[index]
                - c5Magnitude.block.samples[index]);
        uniformSamplingDifference += std::abs(
                magnitude.block.samples[index]
                - (time.block.samples[index] * 0.5f + 0.5f));
        phaseInterpolationDifference += std::abs(
                magnitude.block.samples[index]
                - (phase.block.samples[index] * 0.5f + 0.5f));
    }
    REQUIRE(*std::min_element(magnitude.block.samples.begin(), magnitude.block.samples.end()) >= 0.f);
    REQUIRE(*std::max_element(magnitude.block.samples.begin(), magnitude.block.samples.end()) <= 1.f);
    REQUIRE(*std::min_element(phase.block.samples.begin(), phase.block.samples.end()) >= -1.f);
    REQUIRE(*std::max_element(phase.block.samples.begin(), phase.block.samples.end()) <= 1.f);
    REQUIRE(pitchDifference > 0.01f);
    REQUIRE(uniformSamplingDifference > 0.01f);
    REQUIRE(phaseInterpolationDifference > 0.0001f);

    mesh->destroy();
}

TEST_CASE("Prepared spectral sampling clears bins beyond the legacy harmonic region",
        "[cycle-v2][nodes][trimesh][dsp][spectral]") {
    constexpr int midiNote = 72;
    constexpr int outputSize = 256;
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshBlockwiseDsp dsp;
    std::vector<float> output(outputSize);

    dsp.prepare(
            mesh.get(),
            MorphPosition(0.5f, 0.5f, 0.5f),
            Vertex::Time,
            false,
            PortDomain::SpectralMagnitudeSignal);
    dsp.setFrequencyMidiNote(midiNote);
    dsp.renderPreparedHarmonicsInto({ output.data(), (int) output.size() });

    const int activeHarmonicCount = LogRegionMapping(midiNote).regionSize();
    REQUIRE(activeHarmonicCount < outputSize);
    REQUIRE(std::any_of(
            output.begin(),
            output.begin() + activeHarmonicCount,
            [](float value) { return value != 0.f; }));
    REQUIRE(std::all_of(
            output.begin() + activeHarmonicCount,
            output.end(),
            [](float value) { return value == 0.f; }));

    mesh->destroy();
}

TEST_CASE("Prepared spectral raster reuses Cycle 1 logarithmic regions",
        "[cycle-v2][nodes][trimesh][dsp][spectral][parity]") {
    constexpr int midiNote = 60;
    constexpr int outputSize = 256;
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    const MorphPosition morph(0.37f, 0.42f, 0.63f);

    TrimeshBlockwiseDsp prepared;
    prepared.prepare(
            mesh.get(),
            morph,
            Vertex::Time,
            false,
            PortDomain::SpectralMagnitudeSignal);
    prepared.prepareSampling(outputSize);
    prepared.setFrequencyMidiNote(midiNote);
    std::array<float, outputSize> actual {};
    prepared.renderPreparedHarmonicsInto({ actual.data(), outputSize });

    Rasterization::TrilinearMeshRasterizer legacy;
    legacy.setWrapsEnds(false);
    legacy.setCalcDepthDimensions(false);
    legacy.setXLimits(-0.05f, 1.05f);
    legacy.setScalingMode(Rasterization::PointScalingMode::Unipolar);
    legacy.setMorphPosition(morph);
    legacy.renderWaveformOnly(mesh.get());
    const int harmonicCount = LogRegionMapping(midiNote).regionSize();
    const Buffer<float> positions = LogRegions::getDefaultRegion(midiNote);
    REQUIRE(positions.size() == harmonicCount);
    std::array<float, outputSize> expected {};
    legacy.sampler().sampleAtIntervals(
            positions,
            { expected.data(), harmonicCount });

    REQUIRE(actual == expected);
    mesh->destroy();
}

TEST_CASE("Trimesh node model renders compact grid data from node parameters", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {
                    { "yellow", "Yellow", "0.25" },
                    { "red", "Red", "0.50" },
                    { "blue", "Blue", "0.75" },
                    { "primaryAxis", "Primary Axis", "red" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const auto renderData = model.renderGrid(12, 6);
    const auto spectralRenderData = model.renderGrid(
            12,
            6,
            PortDomain::SpectralMagnitudeSignal);

    REQUIRE(model.getPrimaryViewAxis() == Vertex::Red);
    REQUIRE(model.getMorphPosition().time.getCurrentValue() == Catch::Approx(0.25f));
    REQUIRE(model.getMorphPosition().red.getCurrentValue() == Catch::Approx(0.50f));
    REQUIRE(model.getMorphPosition().blue.getCurrentValue() == Catch::Approx(0.75f));
    REQUIRE(renderData.rows == 12);
    REQUIRE(renderData.columns == 6);
    REQUIRE(renderData.slice.size() == 12);
    REQUIRE(renderData.surface.size() == 72);
    REQUIRE(renderData.canDrawSurface());
    REQUIRE(renderData.domain == PortDomain::TimeSignal);
    REQUIRE(renderData.cyclic);
    REQUIRE(spectralRenderData.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE_FALSE(spectralRenderData.cyclic);

    const auto vertexParameters = model.getSelectedVertexParameters();
    const auto vertexMarkers = model.getVertexMarkers();
    REQUIRE(vertexParameters.size() == 6);
    REQUIRE(vertexMarkers.size() >= vertexParameters.size());
    REQUIRE(vertexParameters[0].id == "vertex.time");
    REQUIRE(vertexParameters[1].id == "vertex.red");
    REQUIRE(vertexParameters[2].id == "vertex.blue");
    REQUIRE(vertexParameters[3].id == "vertex.phase");
    REQUIRE(vertexParameters[4].id == "vertex.amp");
    REQUIRE(vertexParameters[5].id == "vertex.curve");

    for (const auto& parameter : vertexParameters) {
        REQUIRE(parameter.value >= parameter.minimum);
        REQUIRE(parameter.value <= parameter.maximum);
    }
}

TEST_CASE("Trimesh node model exposes selected cube vertices for the side panel preview", "[cycle-v2][nodes][trimesh]") {
    Node node = NodeGraph::createDemoGraph().getNodes().front();
    TrimeshNodeModel model;

    model.syncFromNode(node);

    const auto previewVertices = model.getSelectedCubePreviewVertices();

    REQUIRE(previewVertices.size() == 8);

    bool hasSelectedVertex {};
    bool hasLowTime {};
    bool hasHighTime {};
    bool hasLowRed {};
    bool hasHighRed {};
    bool hasLowBlue {};
    bool hasHighBlue {};

    for (const auto& vertex : previewVertices) {
        hasSelectedVertex = hasSelectedVertex || vertex.selected;
        hasLowTime = hasLowTime || vertex.time == Catch::Approx(0.f);
        hasHighTime = hasHighTime || vertex.time == Catch::Approx(1.f);
        hasLowRed = hasLowRed || vertex.red == Catch::Approx(0.f);
        hasHighRed = hasHighRed || vertex.red == Catch::Approx(1.f);
        hasLowBlue = hasLowBlue || vertex.blue == Catch::Approx(0.f);
        hasHighBlue = hasHighBlue || vertex.blue == Catch::Approx(1.f);
    }

    REQUIRE(hasSelectedVertex);
    REQUIRE(hasLowTime);
    REQUIRE(hasHighTime);
    REQUIRE(hasLowRed);
    REQUIRE(hasHighRed);
    REQUIRE(hasLowBlue);
    REQUIRE(hasHighBlue);
}

TEST_CASE("Trimesh node model exposes explicit derived revisions", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const TrimeshDerivedRevisions initial = model.getDerivedRevisions();

    node.editorState = selectedVertexEditorState(2);
    model.syncFromNode(node);
    const TrimeshDerivedRevisions selected = model.getDerivedRevisions();

    REQUIRE(selected.aggregate > initial.aggregate);
    REQUIRE(selected.selectedControl > initial.selectedControl);
    REQUIRE(selected.meshContent == initial.meshContent);
    REQUIRE(selected.sliceRasterization == initial.sliceRasterization);
    REQUIRE(selected.interceptsRails == initial.interceptsRails);
    REQUIRE(selected.columns3D == initial.columns3D);
    REQUIRE(selected.compactPreview == initial.compactPreview);
    REQUIRE(selected.dspPrep == initial.dspPrep);

    node.parameters.push_back({ "yellow", "Yellow", "0.72" });
    model.syncFromNode(node);
    const TrimeshDerivedRevisions morphed = model.getDerivedRevisions();

    REQUIRE(morphed.aggregate > selected.aggregate);
    REQUIRE(morphed.meshContent == selected.meshContent);
    REQUIRE(morphed.selectedControl == selected.selectedControl);
    REQUIRE(morphed.sliceRasterization > selected.sliceRasterization);
    REQUIRE(morphed.interceptsRails > selected.interceptsRails);
    REQUIRE(morphed.columns3D > selected.columns3D);
    REQUIRE(morphed.compactPreview > selected.compactPreview);
    REQUIRE(morphed.dspPrep > selected.dspPrep);

    auto editedMesh = TrimeshMeshFactory::createDefaultMesh("RevisionMesh");
    editedMesh->getVerts()[2]->values[Vertex::Amp] = 0.17f;
    node.model = TrimeshNodeModelState::copyOf(*editedMesh, 2);
    editedMesh->destroy();
    model.syncFromNode(node);
    const TrimeshDerivedRevisions edited = model.getDerivedRevisions();

    REQUIRE(edited.aggregate > morphed.aggregate);
    REQUIRE(edited.meshContent > morphed.meshContent);
    REQUIRE(edited.selectedControl > morphed.selectedControl);
    REQUIRE(edited.sliceRasterization > morphed.sliceRasterization);
    REQUIRE(edited.interceptsRails > morphed.interceptsRails);
    REQUIRE(edited.columns3D > morphed.columns3D);
    REQUIRE(edited.compactPreview > morphed.compactPreview);
    REQUIRE(edited.dspPrep > morphed.dspPrep);
}

TEST_CASE("Trimesh node model applies one complete topology snapshot", "[cycle-v2][nodes][trimesh]") {
    auto authored = TrimeshMeshFactory::createDefaultMesh("AuthoredNodeMesh");
    authored->getVerts()[0]->values[Vertex::Time] = 0.03f;
    authored->getVerts()[0]->values[Vertex::Red] = 0.04f;
    authored->getVerts()[0]->values[Vertex::Blue] = 0.05f;
    authored->getVerts()[0]->values[Vertex::Amp] = 0.11f;
    authored->getVerts()[0]->values[Vertex::Phase] = 0.22f;
    authored->getVerts()[2]->values[Vertex::Amp] = 0.77f;
    authored->getVerts()[2]->values[Vertex::Curve] = 0.88f;
    const auto topology = TrimeshNodeModelState::copyOf(*authored, 2);
    authored->destroy();
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {},
            topology,
            selectedVertexEditorState(0)
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const auto firstVertexParameters = model.getSelectedVertexParameters();

    REQUIRE(firstVertexParameters.size() == 6);
    REQUIRE(firstVertexParameters[0].value == Catch::Approx(0.03f));
    REQUIRE(firstVertexParameters[1].value == Catch::Approx(0.04f));
    REQUIRE(firstVertexParameters[2].value == Catch::Approx(0.05f));
    REQUIRE(firstVertexParameters[3].value == Catch::Approx(0.22f));
    REQUIRE(firstVertexParameters[4].value == Catch::Approx(0.11f));

    const auto explicitSecondVertexParameters = model.getVertexParametersForIndex(2);
    REQUIRE(explicitSecondVertexParameters.size() == 6);
    REQUIRE(explicitSecondVertexParameters[4].value == Catch::Approx(0.77f));
    REQUIRE(explicitSecondVertexParameters[5].value == Catch::Approx(0.88f));
    REQUIRE(model.getVertexParametersForIndex(-1).empty());
    REQUIRE(model.getVertexParametersForIndex(999).empty());

    node.editorState = selectedVertexEditorState(2);
    model.syncFromNode(node);
    const auto secondVertexParameters = model.getSelectedVertexParameters();

    REQUIRE(secondVertexParameters.size() == 6);
    REQUIRE(secondVertexParameters[4].value == Catch::Approx(0.77f));
    REQUIRE(secondVertexParameters[5].value == Catch::Approx(0.88f));
}

TEST_CASE("Trimesh node model replaces equal-revision snapshots from another document",
        "[cycle-v2][nodes][trimesh][presets]") {
    auto populated = TrimeshMeshFactory::createDefaultMesh("PopulatedPresetMesh");
    Mesh empty("EmptyPresetMesh");
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "phaseLayer1", {});
    node.model = TrimeshNodeModelState::copyOf(*populated, 1);
    TrimeshNodeModel model;

    model.syncFromNode(node);
    REQUIRE(model.getMeshForPanel().getNumVerts() == populated->getNumVerts());

    node.model = TrimeshNodeModelState::copyOf(empty, 1);
    model.syncFromNode(node);
    REQUIRE(model.getMeshForPanel().getNumVerts() == 0);

    empty.destroy();
    populated->destroy();
}

TEST_CASE("Trimesh node model preserves live mesh pointers for equivalent publications",
        "[cycle-v2][nodes][trimesh][interaction]") {
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshNodeModel model;

    model.syncFromNode(node);
    Vertex* liveVertex = model.getMeshForPanel().getVerts().front();
    liveVertex->values[Vertex::Amp] = 0.15441218f;
    model.markMeshEdited();
    node.model = TrimeshNodeModelState::copyOf(model.getMeshForPanel(), 2);

    REQUIRE_FALSE(model.syncFromNode(node));
    REQUIRE(model.getMeshForPanel().getVerts().front() == liveVertex);

    auto changedMesh = TrimeshMeshFactory::createDefaultMesh("Cycle2TrimeshNode");
    changedMesh->getVerts().front()->values[Vertex::Amp] = 0.17f;
    node.model = TrimeshNodeModelState::copyOf(*changedMesh, 3);
    changedMesh->destroy();

    REQUIRE(model.syncFromNode(node));
    REQUIRE(model.getMeshForPanel().getVerts().front()->values[Vertex::Amp]
            == Catch::Approx(0.17f));
}

TEST_CASE("Trimesh guide attachment menu lists document Guide resources", "[cycle-v2][nodes][trimesh]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());
    REQUIRE(GraphEditor().createGuideCurve(graph).succeeded());
    REQUIRE(GraphEditor().assignGuideCurveToTrimeshVertexParameter(
            graph,
            "guide2",
            "waveMesh",
            2,
            "amp").succeeded());

    const auto items = TrimeshGuideAttachmentMenu::itemsFor(
            graph,
            "waveMesh",
            2,
            "amp");

    REQUIRE(items.size() == 4);
    REQUIRE(items[0].label == "detach");
    REQUIRE(items[0].detach);
    REQUIRE(items[1].label == "new...");
    REQUIRE(items[1].createNew);
    REQUIRE(items[2].label == "G1");
    REQUIRE(items[2].guideId == "guide1");
    REQUIRE_FALSE(items[2].attached);
    REQUIRE(items[3].label == "G2");
    REQUIRE(items[3].guideId == "guide2");
    REQUIRE(items[3].attached);
    REQUIRE(GraphEditor().detachGuideCurveFromTrimeshVertexParameter(
            graph, "waveMesh", 2, "amp").succeeded());
    REQUIRE(graph.getGuideAssignments().empty());
}

TEST_CASE("Trimesh guide attachment target resolves vertex owners directly", "[cycle-v2][nodes][trimesh]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const Node* mesh = graph.findNode("waveMesh");
    REQUIRE(mesh != nullptr);

    const auto targets = TrimeshGuideAttachmentTarget::cubeTargetsForVertex(*mesh, 2, "phase");
    REQUIRE(targets.size() == 1);
    REQUIRE(targets.front().cubeIndex == 0);
    REQUIRE(targets.front().field == GuideCurveField::Phase);
    REQUIRE(TrimeshGuideAttachmentTarget::guideField("amp") == GuideCurveField::Amplitude);
    REQUIRE(TrimeshGuideAttachmentTarget::guideField("curve") == GuideCurveField::Curve);
}

TEST_CASE("Trimesh node model selects vertices by phase and amplitude", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const int vertexIndex = model.findNearestVertexIndexForPhaseAmp(0.18f, 0.82f);

    REQUIRE(vertexIndex >= 0);

    node.editorState = selectedVertexEditorState(vertexIndex);
    model.syncFromNode(node);
    REQUIRE(model.getSelectedVertexIndex() == vertexIndex);

    const auto vertexMarkers = model.getVertexMarkers();
    REQUIRE(vertexMarkers[(size_t) vertexIndex].selected);
}

TEST_CASE("Trimesh node model resolves a default selected vertex for parameter edits", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {
                    { "yellow", "Yellow", "0.35" },
                    { "red", "Red", "0.55" },
                    { "blue", "Blue", "0.65" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const int resolvedVertexIndex = model.getResolvedSelectedVertexIndex();

    REQUIRE(resolvedVertexIndex >= 0);
    REQUIRE(model.getSelectedVertexIndex() == -1);

    node.editorState = selectedVertexEditorState(resolvedVertexIndex);
    model.syncFromNode(node);

    REQUIRE(model.getResolvedSelectedVertexIndex() == resolvedVertexIndex);
    REQUIRE(model.getSelectedVertexIndex() == resolvedVertexIndex);
}

TEST_CASE("Trimesh gridwise DSP renders independent morph columns", "[cycle-v2][nodes][trimesh]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshGridwiseDsp dsp;

    const auto columns = dsp.renderColumns(
            *mesh,
            MorphPosition(0.5f, 0.5f, 0.5f),
            Vertex::Time,
            4,
            8,
            PortDomain::SpectralMagnitudeSignal,
            ChannelLayout::Mono);

    REQUIRE(columns.size() == 4);
    REQUIRE(columns.front().signal.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(columns.front().signal.block.samples.size() == 8);
    REQUIRE(columns.front().morph.time.getCurrentValue() == Catch::Approx(0.f));
    REQUIRE(columns.back().morph.time.getCurrentValue() == Catch::Approx(1.f));
    REQUIRE(columns.front().signal.block.samples != columns.back().signal.block.samples);

    mesh->destroy();
}

TEST_CASE("Trimesh spectral traversal columns receive deterministic distinct Guide noise",
        "[cycle-v2][nodes][trimesh][guide][grid]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh("NoisyGuideGrid");
    REQUIRE(mesh != nullptr);
    mesh->getCubes().front()->guideCurveAt(Vertex::Amp) = 0;
    mesh->getCubes().front()->guideCurveGainAt(Vertex::Amp) = 0.25f;

    GuideCurveResource guide;
    guide.id = "noise";
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({
            { 1, 0.05f, 0.5f, 1.f },
            { 2, 0.95f, 0.5f, 1.f }
    }));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    guide.noise = 0.4f;

    GuideCurveSnapshotProvider provider;
    REQUIRE(provider.addGuide(guide));

    constexpr int columnCount = 4;
    constexpr int rowCount = 64;
    std::vector<float> plainValues(columnCount * rowCount);
    std::vector<float> firstValues(columnCount * rowCount);
    std::vector<float> repeatedValues(columnCount * rowCount);
    TrimeshGridwiseDsp plain;
    TrimeshGridwiseDsp guided;
    guided.setGuideCurveProvider(&provider);

    const MorphPosition center(0.5f, 0.5f, 0.5f);
    REQUIRE(plain.renderColumnsInto(
            *mesh,
            center,
            Vertex::Red,
            columnCount,
            Buffer<float>(plainValues.data(), (int) plainValues.size()),
            PortDomain::SpectralMagnitudeSignal));
    REQUIRE(guided.renderColumnsInto(
            *mesh,
            center,
            Vertex::Red,
            columnCount,
            Buffer<float>(firstValues.data(), (int) firstValues.size()),
            PortDomain::SpectralMagnitudeSignal));
    REQUIRE(guided.renderColumnsInto(
            *mesh,
            center,
            Vertex::Red,
            columnCount,
            Buffer<float>(repeatedValues.data(), (int) repeatedValues.size()),
            PortDomain::SpectralMagnitudeSignal));

    REQUIRE(firstValues == repeatedValues);

    double adjacentNoiseDifference {};
    for (int row = 0; row < rowCount; ++row) {
        const float firstNoise = firstValues[(size_t) row] - plainValues[(size_t) row];
        const float secondNoise = firstValues[(size_t) rowCount + (size_t) row]
                - plainValues[(size_t) rowCount + (size_t) row];
        adjacentNoiseDifference += std::abs(firstNoise - secondNoise);
    }
    REQUIRE(adjacentNoiseDifference > 0.01);

    mesh->destroy();
}

TEST_CASE(
        "Trimesh gridwise DSP renders directly into prepared traversal storage",
        "[cycle-v2][nodes][trimesh][complexity]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    const MorphPosition center(0.5f, 0.5f, 0.5f);
    TrimeshGridwiseDsp owningDsp;
    TrimeshGridwiseDsp directDsp;
    owningDsp.setCyclic(false);
    directDsp.setCyclic(false);

    const auto columns = owningDsp.renderColumns(
            *mesh,
            center,
            Vertex::Time,
            4,
            8,
            PortDomain::SpectralMagnitudeSignal,
            ChannelLayout::Mono);
    std::vector<float> directValues(32);
    directDsp.prepare(
            *mesh,
            center,
            Vertex::Time,
            4,
            8,
            PortDomain::SpectralMagnitudeSignal);
    REQUIRE(directDsp.counters().sliceCount == 0);
    REQUIRE(directDsp.renderColumnsInto(
            *mesh,
            center,
            Vertex::Time,
            4,
            Buffer<float>(directValues.data(), (int) directValues.size()),
            PortDomain::SpectralMagnitudeSignal));

    std::vector<float> owningValues;
    for (const auto& column : columns) {
        owningValues.insert(
                owningValues.end(),
                column.signal.block.samples.begin(),
                column.signal.block.samples.end());
    }

    REQUIRE(directValues == owningValues);
    REQUIRE(directDsp.counters().sliceCount == 4);
    REQUIRE(directDsp.counters().bakeCount == 4);
    mesh->destroy();
}

TEST_CASE("Trimesh panel data source adapts node grid data to Panel3D columns", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {
                    { "yellow", "Yellow", "0.25" },
                    { "red", "Red", "0.50" },
                    { "blue", "Blue", "0.75" },
                    { "primaryAxis", "Primary Axis", "yellow" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;
    TrimeshPanelDataSource source;

    model.syncFromNode(node);
    source.rebuild(model, 16, 5);

    const auto& columns = source.getColumns();
    Buffer<float> columnArray = source.getColumnArray();

    REQUIRE(columns.size() == 5);
    REQUIRE(columnArray.size() == 80);
    REQUIRE(columns.front().size() == 16);
    REQUIRE(columns.front().x == Catch::Approx(0.f));
    REQUIRE(columns.back().x == Catch::Approx(1.f));
    REQUIRE(columns.front().get() == columnArray.get());
    REQUIRE(columns.back().get() == columnArray.get() + 64);
}

TEST_CASE("Trimesh Panel3D reads node-backed columns through lib data retriever", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    SingletonRepo repo;
    TrimeshNodeModel model;
    TrimeshPanelDataSource source;
    TrimeshPanel3D panel(&repo, source);

    model.syncFromNode(node);
    source.rebuild(model, 12, 4);

    REQUIRE(panel.shouldDrawGrid());
    REQUIRE(panel.getColumns().size() == 4);
    REQUIRE(panel.getColumns().front().size() == 12);
    REQUIRE(&panel.getGridLock() == &source.getGridLock());
}

TEST_CASE("Trimesh panel bridge binds Panel3D interactor and rasterizer", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {
                    { "yellow", "Yellow", "0.15" },
                    { "red", "Red", "0.35" },
                    { "blue", "Blue", "0.65" },
                    { "primaryAxis", "Primary Axis", "blue" }
            },
            {},
            {}
    };
    TrimeshPanelBridge bridge;

    bridge.syncFromNode(node, 10, 3);

    REQUIRE(bridge.getPanel3D().getInteractor().get() == &bridge.getInteractor3D());
    REQUIRE(bridge.getInteractor3D().hasRasterizer());
    REQUIRE(bridge.getDataSource().getColumns().size() == 3);
    REQUIRE(bridge.getDataSource().getColumns().front().size() == 10);
}

TEST_CASE("Trimesh panel bridge clears interaction pointers only for mesh replacement",
        "[cycle-v2][nodes][trimesh][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshPanelBridge bridge;

    bridge.syncFromNode(node, 10, 3);
    Vertex* liveVertex = bridge.getModel().getMeshForPanel().getVerts().front();
    bridge.getInteractor2D().getSelected().push_back(liveVertex);
    node.model = TrimeshNodeModelState::copyOf(bridge.getModel().getMeshForPanel(), 2);

    bridge.syncFromNode(node, 10, 3);
    REQUIRE(bridge.getInteractor2D().getSelected().size() == 1);
    REQUIRE(bridge.getInteractor2D().getSelected().front() == liveVertex);

    auto changedMesh = TrimeshMeshFactory::createDefaultMesh("Cycle2TrimeshNode");
    changedMesh->getVerts().front()->values[Vertex::Amp] = 0.17f;
    node.model = TrimeshNodeModelState::copyOf(*changedMesh, 3);
    changedMesh->destroy();

    bridge.syncFromNode(node, 10, 3);
    REQUIRE(bridge.getInteractor2D().getSelected().empty());
}

TEST_CASE("Hosted Trimesh point drag defers mesh replacement across publications",
        "[cycle-v2][nodes][trimesh][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshPanelBridge bridge;
    bridge.syncFromNode(node, 320, 96);

    Component* host = bridge.getPanel2DHostComponent();
    host->setBounds(0, 0, 640, 280);
    host->addToDesktop(ComponentPeer::windowIsTemporary);
    host->setVisible(true);
    bridge.syncFromNode(node, 320, 96);

    const auto preparedGuides = [&bridge] {
        PreparedTrimeshGuides guides;
        guides.mesh = std::shared_ptr<Mesh>(new Mesh(), [](Mesh* mesh) {
            mesh->destroy();
            delete mesh;
        });
        guides.mesh->deepCopy(&bridge.getModel().getMeshForPanel());
        guides.provider = std::make_shared<GuideCurveSnapshotProvider>();
        return guides;
    };
    int publicationCount = 0;
    int replacementCount = 0;
    bridge.setMeshEditedCallback([&](TrimeshMeshEditEvent) {
        ++publicationCount;
        replacementCount += bridge.applyPreparedGuides(preparedGuides()) ? 1 : 0;
    });

    auto snapshot = bridge.getInteractor2D().rasterizerSnapshot();
    REQUIRE(snapshot.intercepts().size() >= 2);
    const Intercept& intercept = snapshot.intercepts()[snapshot.intercepts().size() / 2];
    const Point<float> source(
            bridge.getPanel2D().sx(intercept.x),
            bridge.getPanel2D().sy(intercept.y));
    auto move = panelMouseEvent(*host, source, {}, source, false);
    host->mouseMove(move);
    auto down = panelMouseEvent(
            *host,
            source,
            ModifierKeys::leftButtonModifier,
            source,
            false);
    host->mouseDown(down);

    auto& selected = bridge.getInteractor2D().getSelected();
    REQUIRE(selected.size() == 1);
    Vertex* gestureVertex = selected.front();
    const float initialPhase = gestureVertex->values[Vertex::Phase];
    const int selectedVertexIndex = bridge.selectedVertexIndexForPanel();
    Point<float> destination = source;
    for (int step = 1; step <= 12; ++step) {
        destination = source.translated(3.f * step, -2.f * step);
        auto drag = panelMouseEvent(
                *host,
                destination,
                ModifierKeys::leftButtonModifier,
                source,
                true);
        host->mouseDrag(drag);
        MessageManager::getInstance()->runDispatchLoopUntil(40);

        REQUIRE(selected.size() == 1);
        REQUIRE(selected.front() == gestureVertex);
        REQUIRE(bridge.selectedVertexIndexForPanel() == selectedVertexIndex);
        REQUIRE(replacementCount == 0);
    }
    auto up = panelMouseEvent(*host, destination, {}, source, true);
    host->mouseUp(up);

    REQUIRE(publicationCount >= 3);
    REQUIRE(selected.size() == 1);
    REQUIRE(selected.front() == gestureVertex);
    REQUIRE(gestureVertex->values[Vertex::Phase] != Catch::Approx(initialPhase));
    REQUIRE(bridge.applyPreparedGuides(preparedGuides()));
}

TEST_CASE("Trimesh hover follows the closest intercept without changing selection",
        "[cycle-v2][nodes][trimesh][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshPanelBridge bridge;
    bridge.syncFromNode(node, 320, 96);

    Component* host = bridge.getPanel2DHostComponent();
    host->setBounds(0, 0, 640, 280);
    bridge.syncFromNode(node, 320, 96);
    auto& interactor = bridge.getInteractor2D();
    const auto snapshot = interactor.rasterizerSnapshot();
    REQUIRE(snapshot.intercepts().size() >= 2);

    const auto hoverIntercept = [&] (const Intercept& intercept) {
        const Point<float> position(
                bridge.getPanel2D().sx(intercept.x),
                bridge.getPanel2D().sy(intercept.y));
        auto move = panelMouseEvent(*host, position, {}, position, false);
        interactor.mouseMove(move);
        return interactor.state.currentIcpt;
    };

    const int selectedVertexIndex = bridge.selectedVertexIndexForPanel();
    const int firstIntercept = hoverIntercept(snapshot.intercepts().front());
    const int lastIntercept = hoverIntercept(snapshot.intercepts().back());

    REQUIRE(firstIntercept != lastIntercept);
    REQUIRE(bridge.selectedVertexIndexForPanel() == selectedVertexIndex);
    REQUIRE(interactor.state.currentVertex != nullptr);
}

TEST_CASE("Trimesh highlighted curve wins gesture routing over a nearby intercept",
        "[cycle-v2][nodes][trimesh][interaction]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshPanelBridge bridge;
    bridge.syncFromNode(node, 320, 96);

    Component* host = bridge.getPanel2DHostComponent();
    host->setBounds(0, 0, 640, 280);
    bridge.syncFromNode(node, 320, 96);
    auto& interactor = bridge.getInteractor2D();
    const auto snapshot = interactor.rasterizerSnapshot();
    REQUIRE(snapshot.intercepts().size() >= 2);
    const size_t leftIndex = snapshot.intercepts().size() / 2 - 1;
    const Intercept& left = snapshot.intercepts()[leftIndex];
    const Intercept& right = snapshot.intercepts()[leftIndex + 1];
    const float centreX = 0.5f * (left.x + right.x);
    const Buffer<Float32> waveX = snapshot.waveX();
    const Buffer<Float32> waveY = snapshot.waveY();
    const int centreIndex = Arithmetic::binarySearch(centreX, waveX);
    const Point<float> position(
            bridge.getPanel2D().sx(waveX[centreIndex]),
            bridge.getPanel2D().sy(waveY[centreIndex]));

    auto move = panelMouseEvent(*host, position, {}, position, false);
    interactor.mouseMove(move);
    REQUIRE(interactor.state.mouseFlags[PanelState::WithinReshapeThresh]);
    auto down = panelMouseEvent(
            *host,
            position,
            ModifierKeys::leftButtonModifier,
            position,
            false);
    interactor.mouseDown(down);

    REQUIRE(interactor.state.actionState == PanelState::ReshapingCurve);

    Vertex* curveVertex = interactor.state.currentVertex;
    REQUIRE(curveVertex != nullptr);
    const float initialCurve = curveVertex->values[Vertex::Curve];
    const Point<float> destination = position.translated(0.f, -24.f);
    auto drag = panelMouseEvent(
            *host,
            destination,
            ModifierKeys::leftButtonModifier,
            position,
            true);
    interactor.mouseDrag(drag);
    auto up = panelMouseEvent(*host, destination, {}, position, true);
    interactor.mouseUp(up);

    REQUIRE(interactor.getSelected().size() == 1);
    REQUIRE(interactor.getSelected().front() == curveVertex);
    const int selectedVertexIndex = bridge.getModel().getResolvedSelectedVertexIndex();
    REQUIRE(selectedVertexIndex >= 0);
    REQUIRE(bridge.getModel().currentMesh().getVerts()[(size_t) selectedVertexIndex]
            == curveVertex);
    REQUIRE(curveVertex->values[Vertex::Curve] != Catch::Approx(initialCurve));
}

TEST_CASE("Spectral Trimesh panels share pitch-dependent LogRegions coordinates",
        "[cycle-v2][nodes][trimesh][spectral][integration]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    TrimeshPanelBridge bridge;
    bridge.setRenderProfile(TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralMagnitudeSignal));

    for (const int midiNote : { 48, 72 }) {
        bridge.setPreviewMidiNote(midiNote);
        bridge.syncFromNode(node, 10, 3);

        const auto& renderData = bridge.getRenderData();
        const auto& columns = bridge.getDataSource().getColumns();
        const int expectedRows = LogRegionMapping(midiNote).regionSize();

        CAPTURE(midiNote);
        REQUIRE(renderData.rows == expectedRows);
        REQUIRE(renderData.surface.size() == (size_t) expectedRows * 3);
        REQUIRE(renderData.linearFrequencySurface.size()
                == renderData.surface.size());
        REQUIRE(columns.size() == 3);
        REQUIRE(columns.front().size() == expectedRows);
        REQUIRE(columns.front().midiKey == midiNote);
    }
}

TEST_CASE("Trimesh preview pitch positions whichever morph axis owns key scale",
        "[cycle-v2][nodes][trimesh][spectral][key-scale]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshPanelBridge bridge;

    bridge.setPreviewKeyScaleAxis(Vertex::Time);
    bridge.setPreviewMidiNote(48);
    bridge.syncFromNode(node, 10, 3);
    const float c3Position = ModulationSource::normalizeKey(
            48,
            Constants::LowestMidiNote,
            Constants::HighestMidiNote);
    REQUIRE(bridge.getModel().getMorphPosition().time.getCurrentValue()
            == Catch::Approx(c3Position));
    REQUIRE(bridge.getModel().getMorphPosition().red.getCurrentValue()
            == Catch::Approx(0.5f));

    bridge.setPreviewKeyScaleAxis(Vertex::Blue);
    bridge.setPreviewMidiNote(72);
    bridge.syncFromNode(node, 10, 3);
    const float c5Position = ModulationSource::normalizeKey(
            72,
            Constants::LowestMidiNote,
            Constants::HighestMidiNote);
    REQUIRE(bridge.getModel().getMorphPosition().time.getCurrentValue()
            == Catch::Approx(0.5f));
    REQUIRE(bridge.getModel().getMorphPosition().blue.getCurrentValue()
            == Catch::Approx(c5Position));

    bridge.setPreviewMidiNote(Constants::HighestMidiNote);
    bridge.syncFromNode(node, 10, 3);
    REQUIRE(bridge.getModel().getMorphPosition().blue.getCurrentValue()
            == Catch::Approx(1.f));
}

TEST_CASE("Spectral Trimesh columns span pitch only on the key-scale primary axis",
        "[cycle-v2][nodes][trimesh][spectral][key-scale][grid]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshPanelBridge bridge;
    bridge.setRenderProfile(TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralMagnitudeSignal));
    bridge.setPreviewMidiNote(72);
    bridge.setPreviewKeyScaleAxis(Vertex::Time);
    bridge.syncFromNode(node, 10, 5);

    const auto& yellowColumns = bridge.getDataSource().getColumns();
    REQUIRE(bridge.getPanel3D().willAdjustSurfaceColumns());
    REQUIRE((int) yellowColumns.front().midiKey == Constants::LowestMidiNote);
    REQUIRE((int) yellowColumns.back().midiKey == Constants::HighestMidiNote);
    REQUIRE(yellowColumns.front().size()
            == LogRegionMapping(Constants::LowestMidiNote).regionSize());
    REQUIRE(yellowColumns.back().size()
            == LogRegionMapping(Constants::HighestMidiNote).regionSize());

    bridge.setPreviewKeyScaleAxis(Vertex::Red);
    bridge.syncFromNode(node, 10, 5);
    const auto& nonKeyColumns = bridge.getDataSource().getColumns();
    REQUIRE_FALSE(bridge.getPanel3D().willAdjustSurfaceColumns());
    REQUIRE(std::all_of(
            nonKeyColumns.begin(),
            nonKeyColumns.end(),
            [](const Column& column) { return (int) column.midiKey == 72; }));

    for (auto& parameter : node.parameters) {
        if (parameter.id == "primaryAxis") {
            parameter.value = "red";
        }
    }
    bridge.syncFromNode(node, 10, 5);
    const auto& redColumns = bridge.getDataSource().getColumns();
    REQUIRE(bridge.getPanel3D().willAdjustSurfaceColumns());
    REQUIRE((int) redColumns.front().midiKey == Constants::LowestMidiNote);
    REQUIRE((int) redColumns.back().midiKey == Constants::HighestMidiNote);
}

TEST_CASE("Trimesh panel bridge hosts panel cores without legacy OpenGL leaves", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    TrimeshPanelBridge bridge;

    Component* panel3DHost = bridge.getPanel3DHostComponent();
    Component* panel2DHost = bridge.getPanel2DHostComponent();

    REQUIRE(panel3DHost != nullptr);
    REQUIRE(panel2DHost != nullptr);
    REQUIRE(bridge.getPanel3D().getComponent() == panel3DHost);
    REQUIRE(bridge.getPanel2D().getComponent() == panel2DHost);
    REQUIRE(bridge.getPanel3D().getOpenglPanel() == nullptr);
    REQUIRE(bridge.getPanel2D().getOpenglPanel() == nullptr);
}

TEST_CASE("Trimesh link parameters drive mature linked-vertex interaction",
        "[cycle-v2][nodes][trimesh][links]") {
    ScopedJuceInitialiser_GUI juce;
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(
            NodeKind::TrilinearMesh,
            "mesh",
            {}));
    TrimeshPanelBridge bridge;
    GraphEditor editor;

    bridge.syncFromNode(*graph.findNode("mesh"), 32, 8);
    VertCube* cube = bridge.getModel().getMeshForPanel().getCubes().front();
    Vertex* vertex = cube->getVertex(0);
    REQUIRE(bridge.getInteractor2D().getVerticesToMove(cube, vertex).size() == 2);

    REQUIRE(editor.setNodeParameter(
            graph, "mesh", "link.red", "Link Red", "1").succeeded());
    bridge.syncFromNode(*graph.findNode("mesh"), 32, 8);
    REQUIRE(bridge.getInteractor2D().getVerticesToMove(cube, vertex).size() == 4);

    REQUIRE(editor.setNodeParameter(
            graph, "mesh", "link.blue", "Link Blue", "1").succeeded());
    bridge.syncFromNode(*graph.findNode("mesh"), 32, 8);
    REQUIRE(bridge.getInteractor2D().getVerticesToMove(cube, vertex).size() == 8);
}

TEST_CASE("Trimesh panel hosts use component cursors and delegated repaint",
        "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    TrimeshPanelBridge bridge;
    RecordingTrimeshPanelHostDelegate delegate;

    bridge.setPanelHostDelegate(&delegate);
    Component* panel3DHost = bridge.getPanel3DHostComponent();
    Component* panel2DHost = bridge.getPanel2DHostComponent();

    bridge.getPanel3D().setPanelMouseCursor(MouseCursor::PointingHandCursor);
    REQUIRE(panel3DHost->getMouseCursor() == MouseCursor::PointingHandCursor);
    REQUIRE(delegate.lastCursor == MouseCursor::PointingHandCursor);

    bridge.getPanel2D().setPanelMouseCursor(MouseCursor::LeftRightResizeCursor);
    REQUIRE(panel2DHost->getMouseCursor() == MouseCursor::LeftRightResizeCursor);
    REQUIRE(delegate.lastCursor == MouseCursor::LeftRightResizeCursor);

    bridge.getPanel2D().requestRepaint(PanelDirtyState::Flag::Overlay);
    REQUIRE(delegate.repaintCount == 1);

    bridge.clearPanelHostDelegate(&delegate);
    bridge.getPanel3D().setPanelMouseCursor(MouseCursor::NormalCursor);
    bridge.getPanel3D().requestRepaint(PanelDirtyState::Flag::Overlay);
    MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE(panel3DHost->getMouseCursor() == MouseCursor::NormalCursor);
    REQUIRE(delegate.repaintCount == 1);
}

TEST_CASE("Trimesh controls component mounts expanded editor control regions", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    TrimeshWidget widget;
    std::array<String, 6> guideLabels;
    guideLabels[4] = "1";
    widget.setGuideAttachmentLabels(guideLabels);
    TrimeshControlsComponent controls(widget);
    controls.setBounds(0, 0, 1400, 760);

    controls.setNode(node);
    controls.setContentBounds({ 10.f, 42.f, 1380.f, 710.f });

    REQUIRE(controls.getControlRegionCount() == 21);
    REQUIRE(controls.getMorphSliderCount() == 3);
    REQUIRE(controls.getSpectralRangeSliderCount() == 0);
    REQUIRE(controls.getPrimaryAxisButtonCount() == 3);
    REQUIRE(controls.getLinkToggleButtonCount() == 3);
    REQUIRE(controls.getVertexParameterSliderCount() == 6);
    REQUIRE(controls.getVertexGuideGainKnobCount() == 3);
    REQUIRE(controls.getVertexGuideAttachmentButtonCount() == 3);
    REQUIRE(controls.getNumChildComponents() == 21);
}

TEST_CASE("Trimesh controls own expanded pointer interaction", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshWidget widget;
    std::array<String, 6> guideLabels;
    guideLabels[4] = "1";
    widget.setGuideAttachmentLabels(guideLabels);
    widget.setRenderProfile(TrimeshRenderProfile::fromDomain(
            PortDomain::SpectralPhaseSignal));
    TrimeshControlsComponent controls(widget);
    RecordingTrimeshControlsDelegate delegate;
    const Rectangle<float> content { 10.f, 42.f, 1380.f, 710.f };

    controls.setBounds(0, 0, 1400, 760);
    controls.setDelegate(&delegate);
    controls.setNode(node);
    controls.setContentBounds(content);

    const auto regions = widget.expandedControlHitRegions(content, true);
    const auto findRegion = [&regions](TrimeshExpandedHitRegionKind kind) -> const TrimeshExpandedHitRegion& {
        const auto found = std::find_if(
                regions.begin(),
                regions.end(),
                [kind](const TrimeshExpandedHitRegion& region) {
                    return region.kind == kind;
                });
        REQUIRE(found != regions.end());
        return *found;
    };

    const auto& primaryAxis = findRegion(TrimeshExpandedHitRegionKind::PrimaryAxis);
    controls.beginPointerInteraction(primaryAxis.bounds.getCentre(), {});
    REQUIRE(delegate.primaryAxis == primaryAxis.axisValue);
    REQUIRE(controls.cursorFor(primaryAxis.bounds.getCentre()) == MouseCursor::PointingHandCursor);

    const auto& linkToggle = findRegion(TrimeshExpandedHitRegionKind::LinkToggle);
    controls.beginPointerInteraction(linkToggle.bounds.getCentre(), {});
    REQUIRE(delegate.linkedAxis == linkToggle.axisValue);
    REQUIRE(controls.cursorFor(linkToggle.bounds.getCentre()) == MouseCursor::PointingHandCursor);
    auto* keyboardLink = controls.findChildWithID(
            "trimesh.link." + linkToggle.axisValue);
    REQUIRE(keyboardLink != nullptr);
    delegate.linkedAxis = {};
    REQUIRE(keyboardLink->keyPressed(KeyPress(KeyPress::spaceKey)));
    REQUIRE(delegate.linkedAxis == linkToggle.axisValue);

    const auto& morph = findRegion(TrimeshExpandedHitRegionKind::MorphControl);
    controls.beginPointerInteraction(morph.bounds.getCentre(), {});
    controls.continuePointerInteraction({ morph.bounds.getRight(), morph.bounds.getCentreY() });
    controls.endPointerInteraction();

    REQUIRE(delegate.morphBeginCount == 1);
    REQUIRE(delegate.morphEndCount == 1);
    REQUIRE(delegate.activeParameter == morph.parameterId);
    REQUIRE(delegate.updateValue > delegate.beginValue);
    REQUIRE(controls.cursorFor(morph.bounds.getCentre()) == MouseCursor::LeftRightResizeCursor);

    const auto& range = findRegion(TrimeshExpandedHitRegionKind::SpectralRange);
    controls.beginPointerInteraction(range.bounds.getCentre(), {});
    controls.continuePointerInteraction({ range.bounds.getRight(), range.bounds.getCentreY() });
    controls.endPointerInteraction();
    REQUIRE(delegate.rangeBeginCount == 1);
    REQUIRE(delegate.rangeEndCount == 1);
    REQUIRE(delegate.updateValue > delegate.beginValue);
    REQUIRE(controls.cursorFor(range.bounds.getCentre()) == MouseCursor::LeftRightResizeCursor);
    Component* morphTarget {};
    for (auto* child : controls.getChildren()) {
        if (child->getBounds().contains(morph.bounds.getCentre().roundToInt())) {
            morphTarget = child;
            break;
        }
    }
    REQUIRE(morphTarget != nullptr);
    REQUIRE(morphTarget != &controls);
    REQUIRE(morphTarget->getMouseCursor() == MouseCursor::LeftRightResizeCursor);

    const auto guideGain = std::find_if(
            regions.begin(),
            regions.end(),
            [](const TrimeshExpandedHitRegion& region) {
                return region.kind == TrimeshExpandedHitRegionKind::VertexGuideGain
                        && region.parameterId == "guideGain.amp";
            });
    REQUIRE(guideGain != regions.end());
    controls.beginPointerInteraction(guideGain->bounds.getCentre(), {});
    controls.continuePointerInteraction(
            guideGain->bounds.getCentre().translated(0.f, -24.f));
    controls.endPointerInteraction();
    REQUIRE(delegate.activeParameter == "guideGain.amp");
    REQUIRE(delegate.updateValue > delegate.beginValue);
    REQUIRE(controls.cursorFor(guideGain->bounds.getCentre())
            == MouseCursor::UpDownResizeCursor);

    auto* keyboardGuideGain = controls.findChildWithID("trimesh.guideGain.amp");
    REQUIRE(keyboardGuideGain != nullptr);
    REQUIRE(keyboardGuideGain->keyPressed(KeyPress(KeyPress::upKey)));
    REQUIRE(delegate.updateValue > delegate.beginValue);

    int expectedSelection {};
    const Point<float> selectionPoint = TrimeshWidget::expandedWavePanelContentBounds(content).getCentre();
    REQUIRE(widget.findVertexSelectionAt(node, content, selectionPoint, expectedSelection));

    controls.beginPointerInteraction(selectionPoint, {});
    REQUIRE(delegate.selectedVertex == expectedSelection);
}

TEST_CASE("Trimesh panel bridge maps spectral grids by signal domain",
        "[cycle-v2][nodes][trimesh][expanded][spectral]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    TrimeshPanelBridge bridge;

    bridge.setRenderProfile(TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal));
    bridge.syncFromNode(node, 12, 4);
    REQUIRE(bridge.getDataSource().getRenderData().cyclic);
    REQUIRE(bridge.rasterizerWrapsVertices());

    bridge.setRenderProfile(TrimeshRenderProfile::fromDomain(PortDomain::SpectralMagnitudeSignal));
    bridge.syncFromNode(node, 12, 4);
    REQUIRE_FALSE(bridge.getDataSource().getRenderData().cyclic);
    REQUIRE_FALSE(bridge.rasterizerWrapsVertices());
    const TrimeshRenderData magnitude = bridge.getDataSource().getRenderData();

    bridge.setRenderProfile(TrimeshRenderProfile::fromSemantic({
            PortDomain::SpectralMagnitudeSignal,
            RenderScalePolicy::Bipolar,
            RenderSemanticRole::SpectralMagnitudeMultiplicative
    }));
    bridge.syncFromNode(node, 12, 4);
    const TrimeshRenderData multiplicativeMagnitude = bridge.getDataSource().getRenderData();

    REQUIRE(magnitude.surface.size() == multiplicativeMagnitude.surface.size());
    for (size_t index = 0; index < magnitude.surface.size(); ++index) {
        REQUIRE(multiplicativeMagnitude.surface[index]
                == Catch::Approx(magnitude.surface[index]));
    }

    bridge.setRenderProfile(TrimeshRenderProfile::fromDomain(PortDomain::SpectralPhaseSignal));
    bridge.syncFromNode(node, 12, 4);
    const TrimeshRenderData phase = bridge.getDataSource().getRenderData();

    REQUIRE(magnitude.slice.size() == phase.slice.size());
    REQUIRE(magnitude.surface.size() == phase.surface.size());
    REQUIRE(magnitude.slice != phase.slice);
    REQUIRE(magnitude.surface != phase.surface);
    REQUIRE(*std::min_element(magnitude.slice.begin(), magnitude.slice.end()) >= 0.f);
    REQUIRE(*std::max_element(magnitude.slice.begin(), magnitude.slice.end()) <= 1.f);
    REQUIRE(*std::min_element(phase.slice.begin(), phase.slice.end()) >= 0.f);
    REQUIRE(*std::max_element(phase.slice.begin(), phase.slice.end()) <= 1.f);
    REQUIRE(*std::min_element(magnitude.surface.begin(), magnitude.surface.end()) >= 0.f);
    REQUIRE(*std::max_element(magnitude.surface.begin(), magnitude.surface.end()) <= 1.f);
    REQUIRE(*std::min_element(phase.surface.begin(), phase.surface.end()) >= 0.f);
    REQUIRE(*std::max_element(phase.surface.begin(), phase.surface.end()) <= 1.f);
}

TEST_CASE("Compact and expanded Trimesh views share mapped magnitude data",
        "[cycle-v2][nodes][trimesh][compact][expanded][spectral]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            {},
            {},
            {},
            {},
            {}
    };
    const TrimeshRenderProfile profile = TrimeshRenderProfile::fromSemantic({
            PortDomain::SpectralMagnitudeSignal,
            RenderScalePolicy::Bipolar,
            RenderSemanticRole::SpectralMagnitudeMultiplicative
    });
    TrimeshWidget widget;
    Image compactImage(Image::ARGB, 220, 180, true);
    Graphics compactGraphics(compactImage);
    widget.paintCompact(
            compactGraphics,
            node,
            compactImage.getBounds().toFloat(),
            1.f,
            profile);
    const TrimeshRenderData compact = widget.renderDataForAutomation();

    widget.setRenderProfile(profile);
    Image expandedImage(Image::ARGB, 900, 650, true);
    Graphics expandedGraphics(expandedImage);
    widget.paintExpanded(expandedGraphics, node, expandedImage.getBounds().toFloat());
    const TrimeshRenderData expanded = widget.renderDataForAutomation();

    REQUIRE(compact.rows == expanded.rows);
    REQUIRE(compact.columns == expanded.columns);
    REQUIRE(compact.slice == expanded.slice);
    REQUIRE(compact.surface == expanded.surface);
}
