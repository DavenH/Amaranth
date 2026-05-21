#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshNodeModel.h"

#include <algorithm>

using namespace CycleV2;

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

    REQUIRE(model.getPrimaryViewAxis() == Vertex::Red);
    REQUIRE(model.getMorphPosition().time.getCurrentValue() == Catch::Approx(0.25f));
    REQUIRE(model.getMorphPosition().red.getCurrentValue() == Catch::Approx(0.50f));
    REQUIRE(model.getMorphPosition().blue.getCurrentValue() == Catch::Approx(0.75f));
    REQUIRE(renderData.rows == 12);
    REQUIRE(renderData.columns == 6);
    REQUIRE(renderData.slice.size() == 12);
    REQUIRE(renderData.surface.size() == 72);
    REQUIRE(renderData.canDrawSurface());

    const auto vertexParameters = model.getSelectedVertexParameters();
    REQUIRE(vertexParameters.size() == 3);
    REQUIRE(vertexParameters[0].id == "amp");
    REQUIRE(vertexParameters[1].id == "phase");
    REQUIRE(vertexParameters[2].id == "curve");

    for (const auto& parameter : vertexParameters) {
        REQUIRE(parameter.value >= parameter.minimum);
        REQUIRE(parameter.value <= parameter.maximum);
    }
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
