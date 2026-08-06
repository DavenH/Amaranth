#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Nodes/Effect2D/CurveNodeModels.h"
#include "../src/Nodes/Guide/GuideCurveSnapshotProvider.h"
#include "../src/Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshControlsComponent.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../src/Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"
#include "../src/Nodes/Trimesh/TrimeshMeshState.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshNodeModel.h"
#include "../src/Nodes/Trimesh/TrimeshPanelBridge.h"
#include "../src/Nodes/Trimesh/TrimeshPanel3D.h"
#include "../src/Nodes/Trimesh/TrimeshPanelDataSource.h"
#include "../src/Nodes/Trimesh/TrimeshRenderProfile.h"
#include "../src/Nodes/Trimesh/TrimeshSidePanelRenderer.h"
#include "../src/Nodes/Trimesh/TrimeshSurfaceRenderer.h"
#include "../src/Nodes/Trimesh/TrimeshWidget.h"

#include <App/SingletonRepo.h>
#include <Curve/Mesh/Intercept.h>

#include <algorithm>
#include <array>
#include <cmath>

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

    void setTrimeshPanelCursor(const MouseCursor& nextCursor) override {
        cursor = nextCursor;
        ++cursorCount;
    }

    void handleMouseOutsideTrimeshPanels(Point<float> screenPosition) override {
        outsidePosition = screenPosition;
        ++outsideCount;
    }

    int repaintCount {};
    int cursorCount {};
    int outsideCount {};
    MouseCursor cursor;
    Point<float> outsidePosition;
};

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
    Node guide = GraphNodeFactory().createNode(NodeKind::GuideCurve, "guide", {});
    std::vector<FlatCurveVertex> vertices {
            { 1, 0.05f, 0.25f, 1.f },
            { 2, 0.95f, 0.75f, 1.f }
    };
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices(std::move(vertices)));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    for (auto& parameter : guide.parameters) {
        if (parameter.id == "noise" || parameter.id == "dcOffset" || parameter.id == "phase") {
            parameter.value = "0";
        }
    }

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
    Node guide = GraphNodeFactory().createNode(NodeKind::GuideCurve, "guide", {});
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({
            { 1, 0.05f, 0.2f, 1.f },
            { 2, 0.95f, 0.8f, 1.f }
    }));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    for (auto& parameter : guide.parameters) {
        if (parameter.id == "noise") {
            parameter.value = "0.4";
        } else if (parameter.id == "dcOffset") {
            parameter.value = "0.3";
        } else if (parameter.id == "phase") {
            parameter.value = "0.2";
        }
    }

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
    Node guide = GraphNodeFactory().createNode(NodeKind::GuideCurve, "guide", {});
    FlatCurveModel curve;
    REQUIRE(curve.replaceVertices({
            { 1, 0.05f, 1.f, 1.f },
            { 2, 0.95f, 1.f, 1.f }
    }));
    guide.model = CurveNodeModelState::copyOf(curve, 2);
    for (auto& parameter : guide.parameters) {
        if (parameter.id == "noise" || parameter.id == "dcOffset" || parameter.id == "phase") {
            parameter.value = "0";
        }
    }
    Node trimesh = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    trimesh.model = TrimeshNodeModelState::copyOf(*mesh, 2);
    graph.addNode(std::move(guide));
    graph.addNode(std::move(trimesh));
    graph.addEdge({
            "guide",
            "guide",
            "mesh",
            TrimeshGuideAttachmentTarget::portIdForCube(0, "amp"),
            PortDomain::EnvelopeSignal,
            ConnectionKind::ProcessingAttachment,
            AttachmentType::GuideCurve
    });
    graph.addEdge({
            "guide",
            "guide",
            "mesh",
            TrimeshGuideAttachmentTarget::portIdForCube(0, "time"),
            PortDomain::EnvelopeSignal,
            ConnectionKind::ProcessingAttachment,
            AttachmentType::GuideCurve
    });

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
    REQUIRE(timeSliceStyle.panel2DTitle != magSliceStyle.panel2DTitle);
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

TEST_CASE("Trimesh side panel renderer keeps all control surfaces in panel bounds", "[cycle-v2][nodes][trimesh]") {
    const Rectangle<float> sideArea { 100.f, 50.f, 240.f, 420.f };

    REQUIRE(sideArea.contains(TrimeshSidePanelRenderer::morphCubeBounds(sideArea)));

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> primary = TrimeshSidePanelRenderer::primaryAxisBounds(sideArea, i);
        const Rectangle<float> link = TrimeshSidePanelRenderer::linkToggleBounds(sideArea, i);

        REQUIRE(sideArea.contains(TrimeshSidePanelRenderer::morphRailBounds(sideArea, i)));
        REQUIRE(sideArea.contains(primary));
        REQUIRE(sideArea.contains(link));
        REQUIRE(primary.getWidth() == Catch::Approx(link.getWidth()));
        REQUIRE(primary.getHeight() == Catch::Approx(link.getHeight()));
    }

    const Rectangle<float> parameterArea =
            TrimeshSidePanelRenderer::vertexParameterPanelBounds(sideArea);

    REQUIRE(sideArea.contains(parameterArea));

    for (int i = 0; i < 3; ++i) {
        const Rectangle<float> row = TrimeshSidePanelRenderer::vertexParameterRowBounds(parameterArea, i);
        REQUIRE(parameterArea.contains(row));
    }
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

TEST_CASE(
        "Trimesh DSP preserves unipolar magnitude and bipolar phase domains",
        "[cycle-v2][nodes][trimesh][dsp][domains]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshBlockwiseDsp dsp;
    dsp.setMesh(mesh.get());
    dsp.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    dsp.setCyclic(false);

    SignalPayload magnitude;
    SignalPayload phase;
    SignalPayload time;
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

    REQUIRE(magnitude.block.samples.size() == phase.block.samples.size());
    REQUIRE(magnitude.block.samples.size() == time.block.samples.size());
    for (size_t index = 0; index < magnitude.block.samples.size(); ++index) {
        REQUIRE(magnitude.block.samples[index]
                == Catch::Approx(phase.block.samples[index] * 0.5f + 0.5f));
        REQUIRE(magnitude.block.samples[index]
                == Catch::Approx(time.block.samples[index] * 0.5f + 0.5f));
    }

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

TEST_CASE("Trimesh guide attachment menu lists new item and numbered guide nodes", "[cycle-v2][nodes][trimesh]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().addNode(graph, NodeKind::GuideCurve, { 10.f, 10.f }).succeeded());
    REQUIRE(GraphEditor().addNode(graph, NodeKind::GuideCurve, { 20.f, 20.f }).succeeded());
    REQUIRE(GraphEditor().attachGuideCurveToTrimeshVertexParameter(
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

    REQUIRE(items.size() == 3);
    REQUIRE(items[0].label == "new...");
    REQUIRE(items[0].createNew);
    REQUIRE(items[1].label == "1");
    REQUIRE(items[1].guideNodeId == "guide");
    REQUIRE_FALSE(items[1].attached);
    REQUIRE(items[2].label == "2");
    REQUIRE(items[2].guideNodeId == "guide2");
    REQUIRE(items[2].attached);
}

TEST_CASE("Trimesh guide attachment target parses and formats cube components", "[cycle-v2][nodes][trimesh]") {
    const auto cubeTarget = TrimeshGuideAttachmentTarget::parse("guide.cube.4.phase");
    REQUIRE(cubeTarget.isValid());
    REQUIRE(cubeTarget.isCubeTarget());
    REQUIRE(cubeTarget.cubeIndex == 4);
    REQUIRE(cubeTarget.field == "phase");
    REQUIRE(TrimeshGuideAttachmentTarget::portIdForCube(4, "phase")
            == "guide.cube.4.phase");
    REQUIRE_FALSE(TrimeshGuideAttachmentTarget::parse("guide.vertex.12.amp").isValid());
    REQUIRE_FALSE(TrimeshGuideAttachmentTarget::parse("guide.cube.12.unknown").isValid());
    REQUIRE_FALSE(TrimeshGuideAttachmentTarget::parse("scratch").isValid());
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

TEST_CASE("Trimesh panel hosts report lifecycle through one delegate", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    TrimeshPanelBridge bridge;
    RecordingTrimeshPanelHostDelegate delegate;

    bridge.setPanelHostDelegate(&delegate);
    Component* panel3DHost = bridge.getPanel3DHostComponent();
    Component* panel2DHost = bridge.getPanel2DHostComponent();

    bridge.getPanel3D().setPanelMouseCursor(MouseCursor::PointingHandCursor);
    REQUIRE(delegate.cursorCount == 1);
    REQUIRE(delegate.cursor == MouseCursor::PointingHandCursor);
    REQUIRE(panel3DHost->getMouseCursor() == MouseCursor::PointingHandCursor);

    bridge.getPanel2D().setPanelMouseCursor(MouseCursor::LeftRightResizeCursor);
    REQUIRE(delegate.cursorCount == 2);
    REQUIRE(delegate.cursor == MouseCursor::LeftRightResizeCursor);
    REQUIRE(panel2DHost->getMouseCursor() == MouseCursor::LeftRightResizeCursor);

    bridge.getPanel2D().requestRepaint(PanelDirtyState::Flag::Overlay);
    MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE(delegate.repaintCount == 1);

    bridge.clearPanelHostDelegate(&delegate);
    bridge.getPanel3D().setPanelMouseCursor(MouseCursor::NormalCursor);
    bridge.getPanel3D().requestRepaint(PanelDirtyState::Flag::Overlay);
    MessageManager::getInstance()->runDispatchLoopUntil(20);
    REQUIRE(delegate.cursorCount == 2);
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
    TrimeshControlsComponent controls(widget);
    controls.setBounds(0, 0, 900, 620);

    controls.setNode(node);
    controls.setContentBounds({ 10.f, 42.f, 880.f, 570.f });

    REQUIRE(controls.getControlRegionCount() == 21);
    REQUIRE(controls.getMorphSliderCount() == 3);
    REQUIRE(controls.getPrimaryAxisButtonCount() == 3);
    REQUIRE(controls.getLinkToggleButtonCount() == 3);
    REQUIRE(controls.getVertexParameterSliderCount() == 6);
    REQUIRE(controls.getVertexGuideAttachmentButtonCount() == 6);
    REQUIRE(controls.getNumChildComponents() == 21);
}

TEST_CASE("Trimesh controls own expanded pointer interaction", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    TrimeshWidget widget;
    TrimeshControlsComponent controls(widget);
    RecordingTrimeshControlsDelegate delegate;
    const Rectangle<float> content { 10.f, 42.f, 880.f, 570.f };

    controls.setBounds(0, 0, 900, 620);
    controls.setDelegate(&delegate);
    controls.setNode(node);
    controls.setContentBounds(content);

    const auto regions = widget.expandedControlHitRegions(content);
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

    const auto& morph = findRegion(TrimeshExpandedHitRegionKind::MorphControl);
    controls.beginPointerInteraction(morph.bounds.getCentre(), {});
    controls.continuePointerInteraction({ morph.bounds.getRight(), morph.bounds.getCentreY() });
    controls.endPointerInteraction();

    REQUIRE(delegate.morphBeginCount == 1);
    REQUIRE(delegate.morphEndCount == 1);
    REQUIRE(delegate.activeParameter == morph.parameterId);
    REQUIRE(delegate.updateValue > delegate.beginValue);
    REQUIRE(controls.cursorFor(morph.bounds.getCentre()) == MouseCursor::LeftRightResizeCursor);
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
    for (size_t index = 0; index < magnitude.slice.size(); ++index) {
        REQUIRE(magnitude.slice[index] == Catch::Approx(phase.slice[index]));
    }
    REQUIRE(magnitude.surface != phase.surface);
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
