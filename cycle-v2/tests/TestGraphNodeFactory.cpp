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

TEST_CASE("Graph editor adds nodes with unique ids", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().addNode(graph, NodeKind::Envelope, { 100.f, 100.f });

    REQUIRE(result.succeeded());
    REQUIRE(result.nodeId == "env2");
    REQUIRE(graph.getNodes().back().id == "env2");
    REQUIRE(graph.getNodes().back().kind == NodeKind::Envelope);
}
