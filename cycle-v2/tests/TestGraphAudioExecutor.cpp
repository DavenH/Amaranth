#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Runtime/GraphAudioExecutor.h"

#include <algorithm>

using namespace CycleV2;

namespace {

const NodeAudioResult& findNodeAudio(const GraphAudioResult& result, const String& nodeId) {
    const auto found = std::find_if(
            result.nodes.begin(),
            result.nodes.end(),
            [&](const NodeAudioResult& node) {
                return node.nodeId == nodeId;
            });

    REQUIRE(found != result.nodes.end());
    return *found;
}

}

TEST_CASE("Graph audio executor renders source through envelope multiply to output", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 240.f, 180.f }));
    graph.addNode(factory.createNode(NodeKind::Multiply, "mul", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 760.f, 0.f }));

    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "wave", "out", "mul", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "env", "env", "mul", "right", PortDomain::EnvelopeSignal, false });
    graph.addEdge({ "mul", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 5);

    REQUIRE(result.output.samples == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
    REQUIRE(findNodeAudio(result, "wave").output.samples == std::vector<float> { 0.f, 0.25f, 0.5f, 0.75f, 1.f });
    REQUIRE(findNodeAudio(result, "mul").output.samples == result.output.samples);
}

TEST_CASE("Graph audio executor passes parameters to node processors", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.getNodesForEditing().back().parameters = { { "level", "Level", "0.5" } };
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 260.f, 0.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 3);

    REQUIRE(result.output.samples == std::vector<float> { 0.f, 0.25f, 0.5f });
}

TEST_CASE("Graph audio executor returns silence for disconnected output", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::Output, "out", { 0.f, 0.f }));

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 3);

    REQUIRE(result.output.samples == std::vector<float> { 0.f, 0.f, 0.f });
}
