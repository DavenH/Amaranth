#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <Curve/Mesh/Mesh.h>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

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

TEST_CASE("Cancelled compound edits restore implicit removals in serialized order",
        "[cycle-v2][graph][transaction][rollback]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    for (const String id : { "env-a", "env-b" }) {
        graph.addNode(factory.createNode(NodeKind::Envelope, id, {}));
        REQUIRE(GraphNodeStateEditor().setNodeParameter(
                graph, id, "purpose", "Purpose", "volume").succeeded());
        REQUIRE(GraphNodeStateEditor().setNodeParameter(
                graph, id, "logarithmic", "Logarithmic", "1").succeeded());
    }
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply-a", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply-b", {}));
    graph.addEdge({ "wave", "out", "multiply-a", "left", PortDomain::TimeSignal });
    graph.addEdge({ "env-a", "env", "multiply-a", "right", PortDomain::TimeSignal });
    graph.addEdge({ "wave", "out", "multiply-b", "left", PortDomain::TimeSignal });
    graph.addEdge({ "env-b", "env", "multiply-b", "right", PortDomain::TimeSignal });
    graph.addEdge({ "wave", "out", "output", "time", PortDomain::TimeSignal });

    auto mesh = TrimeshMeshFactory::createDefaultMesh();
    TrimeshMeshFactory::addVoiceCube(*mesh, 0.2f, 0.8f, 0.2f, 0.8f, 0.5f);
    TrimeshMeshFactory::addVoiceCube(*mesh, 0.3f, 0.7f, 0.3f, 0.7f, 0.5f);
    for (const String id : { "mesh-a", "mesh-b" }) {
        Node node = factory.createNode(NodeKind::TrilinearMesh, id, {});
        node.model = TrimeshNodeModelState::copyOf(*mesh, 1);
        graph.addNode(std::move(node));
    }
    GuideCurveResource guide;
    guide.id = "guide";
    guide.name = "Guide";
    guide.model = createDefaultGuideCurveModel();
    REQUIRE(graph.addGuideCurve(std::move(guide)));
    for (const auto& item : std::vector<std::pair<String, int>> {
            { "mesh-a", 0 }, { "mesh-a", 8 }, { "mesh-b", 0 },
            { "mesh-a", 9 }, { "mesh-b", 9 }, { "mesh-a", 1 }, { "mesh-b", 8 } }) {
        REQUIRE(graph.assignGuideCurve({ "guide", item.first,
                { item.second, GuideCurveField::Amplitude } }));
    }
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    REQUIRE(commands.setNodeParameter("wave", "level", "Level", "0.1").succeeded());
    REQUIRE(document.undo());
    const String before = document.toJson();
    const uint64_t revision = document.revision();
    const uint64_t graphRevision = document.graph().getRevision();
    const bool dirty = document.isDirty();
    int publications = 0;
    document.setListener([&](uint64_t, const GraphChangeSet&) { ++publications; });
    InteractionComplexityDiagnostics::reset();

    commands.beginCompoundEdit();
    REQUIRE(commands.setNodeParameter("env-a", "purpose", "Purpose", "scratch").succeeded());
    REQUIRE(commands.setNodeParameter("env-b", "purpose", "Purpose", "scratch").succeeded());
    auto smaller = TrimeshMeshFactory::createDefaultMesh();
    REQUIRE(commands.replaceNodeModel("mesh-a", 1,
            TrimeshNodeModelState::copyOf(*smaller, 2)).succeeded());
    REQUIRE(commands.replaceNodeModel("mesh-b", 1,
            TrimeshNodeModelState::copyOf(*smaller, 2)).succeeded());
    REQUIRE(document.graph().getGuideAssignments().size() == 3);
    commands.cancelCompoundEdit();

    REQUIRE(document.toJson() == before);
    REQUIRE(document.revision() == revision);
    REQUIRE(document.graph().getRevision() == graphRevision);
    REQUIRE(document.isDirty() == dirty);
    REQUIRE_FALSE(document.canUndo());
    REQUIRE(document.canRedo());
    REQUIRE(publications == 0);
    REQUIRE(InteractionComplexityDiagnostics::counts().graphCopies == 0);

    commands.beginCompoundEdit();
    REQUIRE(commands.setNodeParameter("env-a", "purpose", "Purpose", "scratch").succeeded());
    REQUIRE(commands.replaceNodeModel("mesh-a", 1,
            TrimeshNodeModelState::copyOf(*smaller, 2)).succeeded());
    REQUIRE(commands.commitCompoundEdit());
    const String after = document.toJson();
    REQUIRE(document.undo());
    REQUIRE(document.toJson() == before);
    REQUIRE(document.redo());
    REQUIRE(document.toJson() == after);
    mesh->destroy();
    smaller->destroy();
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
