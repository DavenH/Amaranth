#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Nodes/Trimesh/TrimeshBlockwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshGridwiseDsp.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"

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
