#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphNodeFactory.h"
#include "UI/NodeCanvasQueryModel.h"

using namespace CycleV2;

TEST_CASE("Node canvas queries expose graph execution and presentation semantics",
        "[cycle-v2][canvas][queries]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 20.f, 40.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 340.f, 40.f }));
    graph.addEdge({ "wave", "out", "output", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

    const auto compileResult = GraphCompiler().compile(graph);
    REQUIRE(compileResult.succeeded());

    RuntimeProcessTrace runtimeTrace;
    RuntimeNodeTrace waveTrace;
    waveTrace.nodeId = "wave";
    waveTrace.signalOutputs.push_back({ "out", PortDomain::TimeSignal, {} });
    runtimeTrace.nodes.push_back(std::move(waveTrace));

    GraphPreviewResult previewResult;
    previewResult.nodes.push_back({ "wave", PreviewModuleRole::Waveform });

    NodeCanvasQueryModel queries(graph, compileResult, runtimeTrace, previewResult);
    const Node* wave = queries.findNode("wave");
    REQUIRE(wave != nullptr);
    REQUIRE(queries.findNodeAt({ 25.f, 45.f }) == wave);
    REQUIRE(queries.findPort(*wave, "out", false) != nullptr);
    REQUIRE(queries.findRuntimeTrace("wave") == &runtimeTrace.nodes.front());
    REQUIRE(queries.findPreviewResult("wave") == &previewResult.nodes.front());
    REQUIRE(queries.displayDomainForEdge(graph.getEdges().front()) == PortDomain::TimeSignal);
    REQUIRE(queries.displayDomainForNodeOutput(*wave, "out") == PortDomain::TimeSignal);
    REQUIRE(queries.executionIndexForNode("wave") >= 0);
    REQUIRE(queries.attachmentCount() == 0);

    const String portHelp = queries.hoverTextForPort({ "wave", "out", false });
    const String nodeHelp = queries.hoverTextForNode(*wave);
    const String edgeHelp = queries.hoverTextForEdge(graph.getEdges().front());
    REQUIRE(portHelp == "Audio leaves Wave here.");
    REQUIRE(nodeHelp == "Generates a waveform for the current voice.");
    REQUIRE_FALSE(nodeHelp.contains("input"));
    REQUIRE_FALSE(nodeHelp.contains("output"));
    REQUIRE_FALSE(nodeHelp.contains("produces"));
    REQUIRE(edgeHelp == "Audio flows from Wave to Output.");
    REQUIRE_FALSE(portHelp.contains(" / "));
    REQUIRE_FALSE(nodeHelp.contains(" / "));
    REQUIRE_FALSE(edgeHelp.contains(" / "));
}

TEST_CASE("Node hover help describes musical intent in plain ASCII",
        "[cycle-v2][canvas][queries][help]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
    graph.addNode(factory.createNode(NodeKind::Reverb, "reverb", {}));
    graph.addNode(factory.createNode(NodeKind::Fft, "fft", {}));

    NodeCanvasQueryModel queries(graph, {}, {}, {});
    REQUIRE(queries.hoverTextForNode(*graph.findNode("shape"))
            == "Shapes the waveform with a custom transfer curve.");
    REQUIRE(queries.hoverTextForNode(*graph.findNode("reverb"))
            == "Adds a sense of space and room around the sound.");
    REQUIRE(queries.hoverTextForNode(*graph.findNode("fft"))
            == "Opens a waveform into magnitude and phase for spectral editing.");

    for (const auto& node : graph.getNodes()) {
        const String help = queries.hoverTextForNode(node);
        for (const auto character : help) {
            REQUIRE(character < 128);
        }
    }
}
