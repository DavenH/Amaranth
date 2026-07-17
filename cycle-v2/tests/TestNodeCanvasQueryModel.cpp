#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/NodeCanvasQueryModel.h"

using namespace CycleV2;

TEST_CASE("Node canvas queries expose graph execution and presentation semantics",
        "[cycle-v2][canvas][queries]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 20.f, 40.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 340.f, 40.f }));
    graph.addEdge({ "wave", "out", "output", "time", PortDomain::TimeSignal, false });

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

    REQUIRE(queries.hoverTextForPort({ "wave", "out", false }).contains("Output port"));
    REQUIRE(queries.hoverTextForNode(*wave).contains("emits out=Time"));
    REQUIRE(queries.hoverTextForEdge(graph.getEdges().front()).contains("Signal edge"));
}
