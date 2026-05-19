#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"

using namespace CycleV2;

TEST_CASE("Graph node factory creates canonical envelope nodes", "[cycle-v2][graph]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", { 10.f, 20.f });

    REQUIRE(node.id == "env");
    REQUIRE(node.kind == NodeKind::Envelope);
    REQUIRE(node.bounds.getX() == 10.f);
    REQUIRE(node.bounds.getY() == 20.f);
    REQUIRE(node.outputs.size() == 1);
    REQUIRE(node.outputs.front().domain == PortDomain::EnvelopeSignal);
}

TEST_CASE("Graph node factory creates canonical FFT nodes", "[cycle-v2][graph]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::Fft, "fft", { 0.f, 0.f });

    REQUIRE(node.inputs.size() == 1);
    REQUIRE(node.inputs.front().domain == PortDomain::TimeSignal);
    REQUIRE(node.inputs.front().channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(node.outputs.size() == 2);
    REQUIRE(node.outputs[0].domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(node.outputs[1].domain == PortDomain::SpectralPhaseSignal);
}

TEST_CASE("Graph node factory creates mesh and arithmetic nodes", "[cycle-v2][graph]") {
    const Node mesh = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    const Node add = GraphNodeFactory().createNode(NodeKind::Add, "add", {});

    REQUIRE(mesh.title == "Trilinear Mesh");
    REQUIRE(mesh.outputs.size() == 1);
    REQUIRE(mesh.outputs.front().domain == PortDomain::MeshField);
    REQUIRE(mesh.inputs.front().purpose == PortPurpose::ScratchAttachment);

    REQUIRE(add.title == "Add");
    REQUIRE(add.inputs.size() == 2);
    REQUIRE(add.outputs.size() == 1);
    REQUIRE(add.outputs.front().domain == PortDomain::ControlSignal);
}

TEST_CASE("Graph editor adds nodes with unique ids", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().addNode(graph, NodeKind::Envelope, { 100.f, 100.f });

    REQUIRE(result.succeeded());
    REQUIRE(result.nodeId == "env2");
    REQUIRE(graph.getNodes().back().id == "env2");
    REQUIRE(graph.getNodes().back().kind == NodeKind::Envelope);
}

TEST_CASE("Graph node factory creates stereo split and join nodes", "[cycle-v2][graph]") {
    const Node split = GraphNodeFactory().createNode(NodeKind::StereoSplit, "split", {});
    const Node join = GraphNodeFactory().createNode(NodeKind::StereoJoin, "join", {});

    REQUIRE(split.inputs.size() == 1);
    REQUIRE(split.inputs.front().channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(split.outputs.size() == 2);
    REQUIRE(split.outputs[0].channelLayout == ChannelLayout::Left);
    REQUIRE(split.outputs[1].channelLayout == ChannelLayout::Right);

    REQUIRE(join.inputs.size() == 2);
    REQUIRE(join.inputs[0].channelLayout == ChannelLayout::Left);
    REQUIRE(join.inputs[1].channelLayout == ChannelLayout::Right);
    REQUIRE(join.outputs.front().channelLayout == ChannelLayout::LinkedStereo);
}

TEST_CASE("Graph node factory sizes nodes from their content", "[cycle-v2][graph]") {
    const Node wave = GraphNodeFactory().createNode(NodeKind::TrilinearWaveSurface, "wave", {});
    const Node env = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});

    REQUIRE(wave.bounds.getWidth() >= 380.f);
    REQUIRE(wave.bounds.getHeight() >= 300.f);
    REQUIRE(env.bounds.getWidth() >= 240.f);
    REQUIRE(env.bounds.getHeight() >= 160.f);
    REQUIRE(wave.bounds.getHeight() > env.bounds.getHeight());
}
