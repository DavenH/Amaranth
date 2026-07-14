#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphTransaction.h"

using namespace CycleV2;

TEST_CASE("Graph transactions commit compound changes atomically", "[cycle-v2][graph][transaction]") {
    NodeGraph graph;
    GraphTransaction transaction(graph);
    const auto wave = transaction.addNode(NodeKind::WaveSource, {});
    const auto output = transaction.addNode(NodeKind::Output, { 300.f, 0.f });

    REQUIRE(wave.succeeded());
    REQUIRE(output.succeeded());
    REQUIRE(transaction.connect(
            { wave.nodeId, "out", false },
            { output.nodeId, "time", true }).succeeded());
    const auto committed = transaction.commit();

    REQUIRE(committed.succeeded());
    REQUIRE(committed.changes.topologyChanged);
    REQUIRE(graph.getNodes().size() == 2);
    REQUIRE(graph.getEdges().size() == 1);
}

TEST_CASE("Failed graph transactions leave their target unchanged", "[cycle-v2][graph][transaction]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
    const uint64_t revision = graph.getRevision();
    GraphTransaction transaction(graph);

    const auto invalid = transaction.setNodeParameter("voice", "octave", "Octave", "99");
    REQUIRE_FALSE(invalid.succeeded());
    REQUIRE_FALSE(transaction.commit().succeeded());
    REQUIRE(graph.getRevision() == revision);
    REQUIRE(parameterValueForNode(*graph.findNode("voice"), "octave") == "0");
}
