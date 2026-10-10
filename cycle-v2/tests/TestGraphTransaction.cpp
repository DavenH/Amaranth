#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/InteractionComplexityDiagnostics.h"

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

TEST_CASE("Connection deltas preserve edge order through undo and redo",
        "[cycle-v2][graph][transaction][complexity]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave-a", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave-b", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave-c", {}));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay-a", {}));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay-b", {}));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay-c", {}));
    graph.addEdge({ "wave-a", "out", "delay-b", "time", PortDomain::TimeSignal });
    graph.addEdge({ "wave-b", "out", "delay-a", "time", PortDomain::TimeSignal });
    graph.addEdge({ "wave-c", "out", "delay-c", "time", PortDomain::TimeSignal });
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    InteractionComplexityDiagnostics::reset();

    REQUIRE(commands.connect(
            { "wave-c", "out", false },
            { "delay-a", "time", true }).succeeded());
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    REQUIRE(document.graph().getEdges()[0].destNodeId == "delay-b");
    REQUIRE(document.graph().getEdges()[1].destNodeId == "delay-c");
    REQUIRE(document.graph().getEdges()[2].sourceNodeId == "wave-c");

    REQUIRE(document.undo());
    REQUIRE(document.graph().getEdges()[0].sourceNodeId == "wave-a");
    REQUIRE(document.graph().getEdges()[1].sourceNodeId == "wave-b");
    REQUIRE(document.graph().getEdges()[2].sourceNodeId == "wave-c");
    REQUIRE(document.redo());
    REQUIRE(document.graph().getEdges()[2].sourceNodeId == "wave-c");
    REQUIRE(document.graph().getEdges()[2].destNodeId == "delay-a");
}

TEST_CASE("Splice commands capture only affected edge inputs",
        "[cycle-v2][graph][transaction][complexity]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addEdge({ "wave", "out", "output", "time", PortDomain::TimeSignal });
    AudioSampleResource audio { "unrelated-audio", "Unrelated.wav", 48000.0, {} };
    audio.samples.resize(16384);
    graph.addAudioResource(std::move(audio));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    InteractionComplexityDiagnostics::reset();

    REQUIRE(commands.spliceNodeIntoEdge(0, "delay").succeeded());
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().audioSamplesCopied == 0);
    REQUIRE(document.graph().getEdges().size() == 2);
    REQUIRE(document.graph().getEdges()[0].destNodeId == "delay");
    REQUIRE(document.graph().getEdges()[1].sourceNodeId == "delay");
    REQUIRE(document.undo());
    REQUIRE(document.graph().getEdges().size() == 1);
    REQUIRE(document.graph().getEdges()[0].sourceNodeId == "wave");
    REQUIRE(document.graph().getEdges()[0].destNodeId == "output");
    REQUIRE(document.redo());
    REQUIRE(document.graph().getEdges().size() == 2);
}
