#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"

using namespace CycleV2;

TEST_CASE("Graph commands commit compound changes with one undo step", "[cycle-v2][graph][transaction]") {
    GraphDocument document;
    GraphCommandDispatcher commands(document);
    commands.beginCompoundEdit();
    const auto wave = commands.addNode(NodeKind::WaveSource, {});
    const auto output = commands.addNode(NodeKind::Output, { 300.f, 0.f });

    REQUIRE(wave.succeeded());
    REQUIRE(output.succeeded());
    REQUIRE(commands.connect(
            { wave.nodeId, "out", false },
            { output.nodeId, "time", true }).succeeded());
    REQUIRE(commands.commitCompoundEdit());

    REQUIRE(document.lastChange().topologyChanged);
    REQUIRE(document.graph().getNodes().size() == 2);
    REQUIRE(document.graph().getEdges().size() == 1);
    REQUIRE(document.undo());
    REQUIRE(document.graph().getNodes().empty());
    REQUIRE(document.redo());
    REQUIRE(document.graph().getEdges().size() == 1);
}

TEST_CASE("Cancelled graph commands leave the document unchanged", "[cycle-v2][graph][transaction]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    const uint64_t revision = document.revision();
    commands.beginCompoundEdit();

    const auto valid = commands.setNodeParameter("voice", "octave", "Octave", "1");
    REQUIRE(valid.succeeded());
    const auto invalid = commands.setNodeParameter("voice", "octave", "Octave", "99");
    REQUIRE_FALSE(invalid.succeeded());
    commands.cancelCompoundEdit();
    REQUIRE(document.revision() == revision);
    REQUIRE(parameterValueForNode(*document.graph().findNode("voice"), "octave") == "0");
    REQUIRE_FALSE(document.canUndo());
}
