#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshControlsComponent.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshNodeModel.h"
#include "../src/Nodes/Trimesh/TrimeshPanelBridge.h"
#include "../src/Nodes/Trimesh/TrimeshPanel3D.h"
#include "../src/Nodes/Trimesh/TrimeshPanelDataSource.h"
#include "../src/Nodes/Trimesh/TrimeshRenderProfile.h"
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
    const Colour timeMid = timeProfile.surfaceColour(0.55f);
    const Colour magMid = magProfile.surfaceColour(0.55f);
    const Colour phaseMid = phaseProfile.surfaceColour(0.55f);
    const Colour magLow = magProfile.surfaceColour(0.01f);
    const Colour magHigh = magProfile.surfaceColour(0.90f);

    REQUIRE(timeMid != magMid);
    REQUIRE(magMid == phaseMid);
    REQUIRE(timeMid.getFloatAlpha() == Catch::Approx(0.82f).margin(0.01f));
    REQUIRE(magLow.getFloatAlpha() < 0.02f);
    REQUIRE(magHigh.getFloatAlpha() > 0.80f);
    REQUIRE_FALSE(timeProfile.surfaceTextureUsesAlpha());
    REQUIRE(magProfile.surfaceTextureUsesAlpha());
    REQUIRE(phaseProfile.surfaceTextureUsesAlpha());
    REQUIRE(timeProfile.getSliceBackground() == TrimeshSliceBackground::Waveform);
    REQUIRE(magProfile.getSliceBackground() == TrimeshSliceBackground::SpectrumMagnitude);
    REQUIRE(phaseProfile.getSliceBackground() == TrimeshSliceBackground::SpectrumPhase);
    REQUIRE(timeProfile.panel2DTitle() != magProfile.panel2DTitle());
    REQUIRE(timeProfile.curveIsBipolar());
    REQUIRE_FALSE(magProfile.curveIsBipolar());
    REQUIRE(phaseProfile.curveIsBipolar());
    REQUIRE(timeProfile.curveIsCyclic());
    REQUIRE_FALSE(magProfile.curveIsCyclic());
    REQUIRE_FALSE(phaseProfile.curveIsCyclic());
    REQUIRE(timeProfile.positiveCurveColour() == timeProfile.negativeCurveColour());
    REQUIRE(magProfile.positiveCurveColour() == magProfile.negativeCurveColour());
    REQUIRE_FALSE(phaseProfile.positiveCurveColour() == phaseProfile.negativeCurveColour());
    REQUIRE(phaseProfile.positiveCurveColour().toColour() == colourForDomain(PortDomain::SpectralPhaseSignal).withAlpha(0.84f));
    REQUIRE_FALSE(phaseProfile.negativeCurveColour() == magProfile.negativeCurveColour());
    REQUIRE_FALSE(phaseProfile.negativeCurveColour() == magProfile.positiveCurveColour());
}

TEST_CASE("Trimesh blockwise DSP renders a source cycle from a trilinear mesh", "[cycle-v2][nodes][trimesh]") {
    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshBlockwiseDsp dsp;

    AudioProcessBlock output;
    dsp.setMesh(mesh.get());
    dsp.setMorphPosition(MorphPosition(0.5f, 0.5f, 0.5f));
    dsp.renderCycle(8, PortDomain::TimeSignal, ChannelLayout::LinkedStereo, output);

    REQUIRE(output.domain == PortDomain::TimeSignal);
    REQUIRE(output.channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(output.samples.size() == 8);
    REQUIRE(output.samples[0] == Catch::Approx(0.f).margin(0.35f));
    REQUIRE(*std::min_element(output.samples.begin(), output.samples.end())
            < *std::max_element(output.samples.begin(), output.samples.end()));

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
    REQUIRE(vertexParameters.size() == 3);
    REQUIRE(vertexMarkers.size() >= vertexParameters.size());
    REQUIRE(vertexParameters[0].id == "vertex.amp");
    REQUIRE(vertexParameters[1].id == "vertex.phase");
    REQUIRE(vertexParameters[2].id == "vertex.curve");

    for (const auto& parameter : vertexParameters) {
        REQUIRE(parameter.value >= parameter.minimum);
        REQUIRE(parameter.value <= parameter.maximum);
    }
}

TEST_CASE("Trimesh node model applies serialized vertex parameter overrides", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "vertex.amp", "Amplitude", "0.20" },
                    { "vertex.phase", "Phase", "0.30" },
                    { "vertex.curve", "Sharpness", "0.40" },
                    { "vertexOverrideIndex", "Vertex Override Index", "-1" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const auto vertexParameters = model.getSelectedVertexParameters();

    REQUIRE(vertexParameters.size() == 3);
    REQUIRE(vertexParameters[0].value == Catch::Approx(0.20f));
    REQUIRE(vertexParameters[1].value == Catch::Approx(0.30f));
    REQUIRE(vertexParameters[2].value == Catch::Approx(0.40f));

    node.parameters.push_back({ "selectedVertexIndex", "Selected Vertex", "2" });
    model.syncFromNode(node);
    const auto changedSelectionParameters = model.getSelectedVertexParameters();

    REQUIRE(changedSelectionParameters[0].value != Catch::Approx(0.20f));
}

TEST_CASE("Trimesh node model applies persistent edits for multiple vertices", "[cycle-v2][nodes][trimesh]") {
    Node node {
            "mesh",
            NodeKind::TrilinearMesh,
            "Trilinear Mesh",
            {},
            {},
            {
                    { "selectedVertexIndex", "Selected Vertex", "0" },
                    { "vertex.0.amp", "Amplitude", "0.11" },
                    { "vertex.0.phase", "Phase", "0.22" },
                    { "vertex.2.amp", "Amplitude", "0.77" },
                    { "vertex.2.curve", "Sharpness", "0.88" }
            },
            {},
            {}
    };
    TrimeshNodeModel model;

    model.syncFromNode(node);
    const auto firstVertexParameters = model.getSelectedVertexParameters();

    REQUIRE(firstVertexParameters.size() == 3);
    REQUIRE(firstVertexParameters[0].value == Catch::Approx(0.11f));
    REQUIRE(firstVertexParameters[1].value == Catch::Approx(0.22f));

    node.parameters[0].value = "2";
    model.syncFromNode(node);
    const auto secondVertexParameters = model.getSelectedVertexParameters();

    REQUIRE(secondVertexParameters.size() == 3);
    REQUIRE(secondVertexParameters[0].value == Catch::Approx(0.77f));
    REQUIRE(secondVertexParameters[2].value == Catch::Approx(0.88f));
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
    REQUIRE(columns.front().signal.samples.size() == 8);
    REQUIRE(columns.front().morph.time.getCurrentValue() == Catch::Approx(0.f));
    REQUIRE(columns.back().morph.time.getCurrentValue() == Catch::Approx(1.f));
    REQUIRE(columns.front().signal.samples != columns.back().signal.samples);

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

    REQUIRE(controls.getControlRegionCount() == 9);
    REQUIRE(controls.getPrimaryAxisButtonCount() == 3);
    REQUIRE(controls.getNumChildComponents() == 9);
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
