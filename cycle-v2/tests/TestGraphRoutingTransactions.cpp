#include <catch2/catch_test_macros.hpp>

#include <functional>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphValidator.h"
#include "Graph/InteractionComplexityDiagnostics.h"

using namespace CycleV2;

namespace {

NodeGraph routingGraph() {
    NodeGraph graph;
    GraphNodeFactory factory;
    for (const String id : { "env-a", "env-b" }) {
        graph.addNode(factory.createNode(NodeKind::Envelope, id, {}));
        REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, id, "purpose", {}, "volume").succeeded());
        REQUIRE(GraphNodeStateEditor().setNodeParameter(graph, id, "logarithmic", {}, "1").succeeded());
    }
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply-a", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply-b", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({ "wave", "out", "multiply-a", "left", PortDomain::TimeSignal });
    graph.addEdge({ "env-a", "env", "multiply-b", "right", PortDomain::EnvelopeSignal });
    graph.addEdge({ "voice", "context", "wave", "context", PortDomain::DomainContext });
    graph.addEdge({ "env-b", "env", "multiply-a", "right", PortDomain::EnvelopeSignal });
    graph.addEdge({ "multiply-a", "out", "multiply-b", "left", PortDomain::TimeSignal });
    graph.addEdge({ "multiply-b", "out", "output", "time", PortDomain::TimeSignal });
    REQUIRE(GraphValidator().isValid(graph));
    return graph;
}

String routingOrder(const NodeGraph& graph) {
    StringArray routes;
    for (const auto& edge : graph.getEdges()) {
        routes.add(edge.sourceNodeId + "." + edge.sourcePortId + ">"
                + edge.destNodeId + "." + edge.destPortId);
    }
    return routes.joinIntoString("\n");
}

}

TEST_CASE("Compound routing deltas preserve mixed explicit edits and implicit removals",
        "[cycle-v2][graph][transaction][routing][rollback]") {
    std::function<void(GraphCommandDispatcher&)> edit;
    String expectedRoutes;
    SECTION("Connecting an Envelope before purpose removes the newly connected edge") {
        edit = [](GraphCommandDispatcher& commands) {
            REQUIRE(commands.connect({ "env-a", "env", false }, { "multiply-a", "right", true }).succeeded());
            REQUIRE(commands.setNodeParameter("env-a", "purpose", {}, "scratch").succeeded());
        };
        expectedRoutes = "wave.out>multiply-a.left\nvoice.context>wave.context\n"
                "multiply-a.out>multiply-b.left\nmultiply-b.out>output.time";
    }
    SECTION("Changing purpose before connecting keeps the subsequent replacement") {
        edit = [](GraphCommandDispatcher& commands) {
            REQUIRE(commands.setNodeParameter("env-a", "purpose", {}, "scratch").succeeded());
            REQUIRE(commands.connect({ "env-b", "env", false }, { "multiply-b", "right", true }).succeeded());
        };
        expectedRoutes = "wave.out>multiply-a.left\nvoice.context>wave.context\n"
                "env-b.env>multiply-a.right\nmultiply-a.out>multiply-b.left\n"
                "multiply-b.out>output.time\nenv-b.env>multiply-b.right";
    }
    SECTION("A middle removal shifts an unrelated input replacement") {
        edit = [](GraphCommandDispatcher& commands) {
            REQUIRE(commands.setNodeParameter("env-a", "purpose", {}, "scratch").succeeded());
            REQUIRE(commands.connect({ "wave", "out", false }, { "multiply-a", "right", true }).succeeded());
        };
        expectedRoutes = "wave.out>multiply-a.left\nvoice.context>wave.context\n"
                "multiply-a.out>multiply-b.left\nmultiply-b.out>output.time\n"
                "wave.out>multiply-a.right";
    }

    GraphDocument document(routingGraph());
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setNodeParameter("voice", "octave", {}, "1").succeeded());
    REQUIRE(document.undo());
    const String baseline = document.toJson();
    const uint64_t documentRevision = document.revision();
    const uint64_t graphRevision = document.graph().getRevision();
    const bool dirty = document.isDirty();
    int publications = 0;
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    InteractionComplexityDiagnostics::reset();

    commands.beginCompoundEdit();
    edit(commands);
    REQUIRE(routingOrder(document.graph()) == expectedRoutes);
    REQUIRE(parameterValueForNode(*document.graph().findNode("env-a"), "logarithmic") == "0");
    commands.cancelCompoundEdit();
    REQUIRE(document.toJson() == baseline);
    REQUIRE(document.revision() == documentRevision);
    REQUIRE(document.graph().getRevision() == graphRevision);
    REQUIRE(document.isDirty() == dirty);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.canRedo());
    REQUIRE(publications == 0);
    REQUIRE(parameterValueForNode(*document.graph().findNode("env-a"), "logarithmic") == "1");

    commands.beginCompoundEdit();
    edit(commands);
    REQUIRE(commands.commitCompoundEdit());
    REQUIRE(GraphValidator().isValid(document.graph()));
    REQUIRE(routingOrder(document.graph()) == expectedRoutes);
    const String candidate = document.toJson();
    REQUIRE(candidate != baseline);
    REQUIRE(publications == 1);
    REQUIRE(document.revision() == documentRevision + 1);
    REQUIRE_FALSE(document.canRedo());
    REQUIRE(document.undo());
    REQUIRE(document.toJson() == baseline);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.redo());
    REQUIRE(document.toJson() == candidate);
    REQUIRE(routingOrder(document.graph()) == expectedRoutes);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
}
