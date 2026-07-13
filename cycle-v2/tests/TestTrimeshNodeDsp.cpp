#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshControlsComponent.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../src/Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"
#include "../src/Nodes/Trimesh/TrimeshMeshEditState.h"
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

using namespace CycleV2;

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
    REQUIRE(magMid == phaseMid);
    REQUIRE(timeMid.getFloatAlpha() == Catch::Approx(0.82f).margin(0.01f));
    REQUIRE(magLow.getFloatAlpha() > 0.10f);
    REQUIRE(magHigh.getFloatAlpha() > 0.80f);
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

TEST_CASE("Trimesh node model renders compact grid data from node parameters", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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

TEST_CASE("Trimesh node model applies legacy selected vertex parameter overrides", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "vertex.time", "time", "0.10" },
                    { "vertex.red", "red", "0.20" },
                    { "vertex.blue", "blue", "0.30" },
                    { "vertex.phase", "phase", "0.40" },
                    { "vertex.amp", "amp", "0.50" },
                    { "vertex.curve", "curve", "0.60" },
                    { "vertexOverrideIndex", "Vertex Override Index", "-1" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const auto vertexParameters = model.getSelectedVertexParameters();

    REQUIRE(vertexParameters.size() == 6);
    REQUIRE(vertexParameters[0].value == Catch::Approx(0.10f));
    REQUIRE(vertexParameters[1].value == Catch::Approx(0.20f));
    REQUIRE(vertexParameters[2].value == Catch::Approx(0.30f));
    REQUIRE(vertexParameters[3].value == Catch::Approx(0.40f));
    REQUIRE(vertexParameters[4].value == Catch::Approx(0.50f));
    REQUIRE(vertexParameters[5].value == Catch::Approx(0.60f));

    node.parameters.push_back({ "selectedVertexIndex", "Selected Vertex", "2" });
    model.syncFromNode(node);
    const auto changedSelectionParameters = model.getSelectedVertexParameters();

    REQUIRE(changedSelectionParameters[1].value != Catch::Approx(0.20f));
}

TEST_CASE("Trimesh node model exposes explicit derived revisions", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {},
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const TrimeshDerivedRevisions initial = model.getDerivedRevisions();

    node.parameters.push_back({ "selectedVertexIndex", "Selected Vertex", "2" });
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

    node.parameters.push_back({ "mesh.vertex.2.amp", "Amplitude", "0.17" });
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

TEST_CASE("Trimesh node model applies serialized mesh edits for multiple vertices", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "selectedVertexIndex", "Selected Vertex", "0" },
                    { "mesh.vertex.0.time", "time", "0.03" },
                    { "mesh.vertex.0.red", "red", "0.04" },
                    { "mesh.vertex.0.blue", "blue", "0.05" },
                    { "mesh.vertex.0.amp", "Amplitude", "0.11" },
                    { "mesh.vertex.0.phase", "Phase", "0.22" },
                    { "mesh.vertex.2.amp", "Amplitude", "0.77" },
                    { "mesh.vertex.2.curve", "Sharpness", "0.88" }
            },
            {},
            {}
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

    node.parameters[0].value = "2";
    model.syncFromNode(node);
    const auto secondVertexParameters = model.getSelectedVertexParameters();

    REQUIRE(secondVertexParameters.size() == 6);
    REQUIRE(secondVertexParameters[4].value == Catch::Approx(0.77f));
    REQUIRE(secondVertexParameters[5].value == Catch::Approx(0.88f));
}

TEST_CASE("Trimesh node model still reads legacy indexed vertex edit parameters", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "selectedVertexIndex", "Selected Vertex", "1" },
                    { "vertex.1.amp", "Amplitude", "0.31" },
                    { "vertex.1.phase", "Phase", "0.41" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const auto vertexParameters = model.getSelectedVertexParameters();

    REQUIRE(vertexParameters.size() == 6);
    REQUIRE(vertexParameters[3].value == Catch::Approx(0.41f));
    REQUIRE(vertexParameters[4].value == Catch::Approx(0.31f));
}

TEST_CASE("Trimesh mesh edit state formats canonical vertex edit parameters", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "mesh.vertex.4.amp", "Amplitude", "0.61" },
                    { "vertex.3.phase", "Phase", "0.42" },
                    { "vertex.phase", "Legacy Selected Phase", "0.99" },
                    { "mesh.vertex.x.amp", "Invalid", "0.2" }
            },
            {},
            {}
    };

    const TrimeshMeshEditState state = TrimeshMeshEditState::fromNode(node);
    const auto& edits = state.getVertexEdits();

    REQUIRE(TrimeshMeshEditState::canonicalVertexParameterId(4, "amp") == "mesh.vertex.4.amp");
    REQUIRE(edits.size() == 2);
    REQUIRE(edits[0].sourceId == "mesh.vertex.4.amp");
    REQUIRE(edits[0].vertexIndex == 4);
    REQUIRE(edits[1].sourceId == "vertex.3.phase");
    REQUIRE(edits[1].vertexIndex == 3);
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

TEST_CASE("Trimesh guide attachment target parses and formats vertex fields", "[cycle-v2][nodes][trimesh]") {
    const auto target = TrimeshGuideAttachmentTarget::parse("guide.vertex.12.amp");

    REQUIRE(target.isValid());
    REQUIRE(target.vertexIndex == 12);
    REQUIRE(target.field == "amp");
    REQUIRE(target.fieldIndex() == 4);
    REQUIRE(TrimeshGuideAttachmentTarget::fields()[(size_t) target.fieldIndex()] == "amp");
    REQUIRE(TrimeshGuideAttachmentTarget::portIdFor(12, "amp") == "guide.vertex.12.amp");
    REQUIRE_FALSE(TrimeshGuideAttachmentTarget::parse("guide.vertex.x.amp").isValid());
    REQUIRE_FALSE(TrimeshGuideAttachmentTarget::parse("guide.vertex.12.unknown").isValid());
    REQUIRE_FALSE(TrimeshGuideAttachmentTarget::parse("scratch").isValid());
}

TEST_CASE("Trimesh node model selects vertices by phase and amplitude", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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

    node.parameters.push_back({ "selectedVertexIndex", "Selected Vertex", String(vertexIndex) });
    model.syncFromNode(node);
    REQUIRE(model.getSelectedVertexIndex() == vertexIndex);

    const auto vertexMarkers = model.getVertexMarkers();
    REQUIRE(vertexMarkers[(size_t) vertexIndex].selected);
}

TEST_CASE("Trimesh node model resolves a default selected vertex for parameter edits", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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

    node.parameters.push_back({ "selectedVertexIndex", "Selected Vertex", String(resolvedVertexIndex) });
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

TEST_CASE("Trimesh panel data source adapts node grid data to Panel3D columns", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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
            "Trilinear Mesh",
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
            "Trilinear Mesh",
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

TEST_CASE("Trimesh controls component mounts expanded editor control regions", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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

TEST_CASE("Trimesh panel bridge publishes rasterizer intercepts from the shared rasterizer", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "yellow", "Yellow", "0.5" },
                    { "red", "Red", "0.5" },
                    { "blue", "Blue", "0.5" },
                    { "primaryAxis", "Primary Axis", "yellow" }
            },
            {},
            {}
    };
    TrimeshPanelBridge bridge;

    bridge.syncFromNode(node, 12, 4);

    const auto& intercepts = bridge.getRasterizerIntercepts();
    REQUIRE_FALSE(intercepts.empty());
    REQUIRE(std::any_of(intercepts.begin(), intercepts.end(), [](const Intercept& intercept) {
        return intercept.cube != nullptr;
    }));
}

TEST_CASE("Trimesh panel bridge disables cyclic rasterizer wrapping for spectral profiles", "[cycle-v2][nodes][trimesh]") {
    ScopedJuceInitialiser_GUI juce;
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
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
}
