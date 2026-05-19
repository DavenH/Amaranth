#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

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

TEST_CASE("Graph audio executor routes multi-output node buffers by port", "[cycle-v2][runtime]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", { 260.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 520.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 780.f, 0.f }));
    graph.addEdge({ "wave", "out", "fft", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "fft", "mag", "ifft", "mag", PortDomain::SpectralMagnitudeSignal, false });
    graph.addEdge({ "fft", "phase", "ifft", "phase", PortDomain::SpectralPhaseSignal, false });
    graph.addEdge({ "ifft", "time", "out", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 4);
    const auto& fft = findNodeAudio(result, "fft");

    REQUIRE(fft.outputs.size() == 2);
    REQUIRE(fft.outputs[0].first == "mag");
    REQUIRE(fft.outputs[0].second.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(fft.outputs[1].first == "phase");
    REQUIRE(fft.outputs[1].second.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(result.output.samples[0] == Catch::Approx(-0.5f).margin(1.0e-5f));
    REQUIRE(result.output.samples[1] == Catch::Approx(-1.f / 6.f).margin(1.0e-5f));
    REQUIRE(result.output.samples[2] == Catch::Approx(1.f / 6.f).margin(1.0e-5f));
    REQUIRE(result.output.samples[3] == Catch::Approx(0.5f).margin(1.0e-5f));
}

TEST_CASE("Graph audio executor renders the demo graph through resolved mesh operands", "[cycle-v2][runtime]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    const auto result = GraphAudioExecutor().process(graph, compileResult.plan, 4);

    REQUIRE(findNodeAudio(result, "waveMesh").output.domain == PortDomain::TimeSignal);
    REQUIRE(findNodeAudio(result, "magMesh").output.domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(findNodeAudio(result, "phaseMesh").output.domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(findNodeAudio(result, "fft").outputs.size() == 2);
    REQUIRE(result.output.domain == PortDomain::TimeSignal);
    REQUIRE(result.output.samples.size() == 4);
    REQUIRE(result.output.samples != std::vector<float> { 0.f, 0.f, 0.f, 0.f });
}
