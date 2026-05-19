#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"

#include <algorithm>

using namespace CycleV2;

namespace {

Port input(String id, PortDomain domain) {
    return { id, id, domain, ChannelLayout::Mono, PortPurpose::Signal, true };
}

Port output(String id, PortDomain domain) {
    return { id, id, domain, ChannelLayout::Mono, PortPurpose::Signal, false };
}

Node graphNode(String id, std::vector<Port> inputs, std::vector<Port> outputs) {
    return {
        id,
        NodeKind::GenericProcessor,
        id,
        {},
        {},
        std::move(inputs),
        std::move(outputs)
    };
}

int orderIndex(const GraphExecutionPlan& plan, const String& nodeId) {
    const auto found = std::find(plan.nodeOrder.begin(), plan.nodeOrder.end(), nodeId);
    REQUIRE(found != plan.nodeOrder.end());
    return static_cast<int>(std::distance(plan.nodeOrder.begin(), found));
}

}

TEST_CASE("Demo graph compiles to a stable execution order", "[cycle-v2][graph]") {
    const auto result = GraphCompiler().compile(NodeGraph::createDemoGraph());

    REQUIRE(result.succeeded());
    REQUIRE(result.plan.attachments.size() == 2);
    REQUIRE(result.plan.signalEdges.size() == 11);

    const auto& plan = result.plan;
    REQUIRE(orderIndex(plan, "voice") < orderIndex(plan, "waveMesh"));
    REQUIRE(orderIndex(plan, "scratchEnv") < orderIndex(plan, "waveMesh"));
    REQUIRE(orderIndex(plan, "waveMesh") < orderIndex(plan, "fft"));
    REQUIRE(orderIndex(plan, "scratchEnv") < orderIndex(plan, "magMesh"));
    REQUIRE(orderIndex(plan, "fft") < orderIndex(plan, "addMag"));
    REQUIRE(orderIndex(plan, "magMesh") < orderIndex(plan, "addMag"));
    REQUIRE(orderIndex(plan, "fft") < orderIndex(plan, "addPhase"));
    REQUIRE(orderIndex(plan, "phaseMesh") < orderIndex(plan, "addPhase"));
    REQUIRE(orderIndex(plan, "addMag") < orderIndex(plan, "ifft"));
    REQUIRE(orderIndex(plan, "addPhase") < orderIndex(plan, "ifft"));
    REQUIRE(orderIndex(plan, "ifft") < orderIndex(plan, "multiply"));
    REQUIRE(orderIndex(plan, "env") < orderIndex(plan, "multiply"));
    REQUIRE(orderIndex(plan, "multiply") < orderIndex(plan, "out"));
}

TEST_CASE("Invalid graphs do not compile", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "voice", "context", "multiply", "audio", PortDomain::DomainContext, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE_FALSE(result.validationIssues.empty());
    REQUIRE(result.compileIssues.empty());
    REQUIRE(result.plan.nodeOrder.empty());
}

TEST_CASE("Compiler rejects processing cycles", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(graphNode(
            "a",
            { input("in", PortDomain::TimeSignal) },
            { output("out", PortDomain::TimeSignal) }));
    graph.addNode(graphNode(
            "b",
            { input("in", PortDomain::TimeSignal) },
            { output("out", PortDomain::TimeSignal) }));
    graph.addEdge({ "a", "out", "b", "in", PortDomain::TimeSignal, false });
    graph.addEdge({ "b", "out", "a", "in", PortDomain::TimeSignal, false });

    const auto result = GraphCompiler().compile(graph);

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.validationIssues.empty());
    REQUIRE(result.compileIssues.size() == 1);
    REQUIRE(result.compileIssues.front().code == GraphCompileCode::CycleDetected);
    REQUIRE(result.plan.nodeOrder.empty());
}
