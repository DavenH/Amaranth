#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"

using namespace CycleV2;

namespace {

NodeGraph probeGraph() {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 400.f, 0.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });
    return graph;
}

}

TEST_CASE("Signal probes toggle once per source output without changing execution", "[cycle-v2][probe]") {
    NodeGraph graph = probeGraph();
    const auto before = GraphCompiler().compile(graph);
    REQUIRE(before.succeeded());

    const auto added = GraphEditor().toggleSignalProbe(graph, 0, 0.25f);
    REQUIRE(added.succeeded());
    REQUIRE(graph.getSignalProbes().size() == 1);
    REQUIRE(graph.getSignalProbes().front().sourceNodeId == "wave");
    REQUIRE(graph.getSignalProbes().front().sourcePortId == "out");
    REQUIRE(graph.getSignalProbes().front().tapPosition == 0.25f);

    const auto after = GraphCompiler().compile(graph);
    REQUIRE(after.succeeded());
    REQUIRE(after.plan.nodeOrder == before.plan.nodeOrder);
    REQUIRE(after.plan.steps.size() == before.plan.steps.size());
    REQUIRE(after.plan.buffers.size() == before.plan.buffers.size());
    REQUIRE(after.plan.signalEdges.size() == before.plan.signalEdges.size());

    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.75f).succeeded());
    REQUIRE(graph.getSignalProbes().empty());
}

TEST_CASE("Signal probes reject nonsignal cables", "[cycle-v2][probe]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 400.f, 0.f }));
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, false });

    REQUIRE_FALSE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    REQUIRE(graph.getSignalProbes().empty());
}

TEST_CASE("Signal probes serialize independently from processing nodes", "[cycle-v2][probe]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.4f).succeeded());

    const String xml = GraphSerializer().toXmlString(graph);
    REQUIRE(xml.contains("<probe"));
    REQUIRE_FALSE(xml.contains("kind=\"spy\""));

    const NodeGraph restored = GraphSerializer().fromXmlString(xml);
    REQUIRE(restored.getSignalProbes().size() == 1);
    const auto& probe = restored.getSignalProbes().front();
    REQUIRE(probe.sourceNodeId == "wave");
    REQUIRE(probe.sourcePortId == "out");
    REQUIRE(probe.anchorDestNodeId == "out");
    REQUIRE(probe.tapPosition == 0.4f);
}

TEST_CASE("Deleting a probe source preserves a disconnected rail record", "[cycle-v2][probe]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());

    graph.removeNode("wave");

    REQUIRE(graph.getSignalProbes().size() == 1);
    REQUIRE(graph.getSignalProbes().front().sourceNodeId.isEmpty());
    REQUIRE(graph.getSignalProbes().front().sourcePortId.isEmpty());
}

TEST_CASE("Signal probe commands reattach remove and restore through undo", "[cycle-v2][probe]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "first", { 400.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "second", { 400.f, 240.f }));
    graph.addEdge({ "wave", "out", "first", "time", PortDomain::TimeSignal, false });
    graph.addEdge({ "wave", "out", "second", "time", PortDomain::TimeSignal, false });

    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.toggleSignalProbe(0, 0.25f).succeeded());
    const String probeId = document.graph().getSignalProbes().front().id;

    REQUIRE(commands.reattachSignalProbe(probeId, 1, 0.8f).succeeded());
    REQUIRE(document.graph().findSignalProbe(probeId)->anchorDestNodeId == "second");
    REQUIRE(document.graph().findSignalProbe(probeId)->tapPosition == 0.8f);

    REQUIRE(document.undo());
    REQUIRE(document.graph().findSignalProbe(probeId)->anchorDestNodeId == "first");
    REQUIRE(document.redo());
    REQUIRE(document.graph().findSignalProbe(probeId)->anchorDestNodeId == "second");

    REQUIRE(commands.removeSignalProbe(probeId).succeeded());
    REQUIRE(document.graph().getSignalProbes().empty());
    REQUIRE(document.undo());
    REQUIRE(document.graph().findSignalProbe(probeId) != nullptr);
}
