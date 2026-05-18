#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"

using namespace CycleV2;

TEST_CASE("Graph editor connects compatible ports", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("multiply", "factor");

    const auto result = GraphEditor().connect(
            graph,
            { "env", "env", false },
            { "multiply", "factor", true });

    REQUIRE(result.succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    const auto& edge = graph.getEdges().back();
    REQUIRE(edge.sourceNodeId == "env");
    REQUIRE(edge.destNodeId == "multiply");
    REQUIRE(edge.destPortId == "factor");
    REQUIRE_FALSE(edge.attachment);
}

TEST_CASE("Graph editor orients input to output connections", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("multiply", "factor");

    const auto result = GraphEditor().connect(
            graph,
            { "multiply", "factor", true },
            { "env", "env", false });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().sourceNodeId == "env");
    REQUIRE(graph.getEdges().back().destNodeId == "multiply");
}

TEST_CASE("Graph editor marks scratch connections as attachments", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("wave", "scratch");

    const auto result = GraphEditor().connect(
            graph,
            { "scratchEnv", "env", false },
            { "wave", "scratch", true });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().attachment);
    REQUIRE(graph.getEdges().back().destPortId == "scratch");
}

TEST_CASE("Graph editor rejects incompatible connections", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().connect(
            graph,
            { "voice", "pitch", false },
            { "multiply", "audio", true });

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE_FALSE(result.validationIssues.empty());
    REQUIRE(graph.getEdges().size() == edgeCount);
}
