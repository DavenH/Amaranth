#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphEditor.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/GraphNodeStateEditor.h"
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

    GraphPresentationSnapshot snapshot;
    snapshot.compileResult = compileResult;
    snapshot.runtimeTrace = runtimeTrace;
    snapshot.previewResult = previewResult;
    NodeCanvasQueryModel queries(graph, snapshot);
    const Node* wave = queries.findNode("wave");
    REQUIRE(wave != nullptr);
    REQUIRE(queries.findNodeAt({ 25.f, 45.f }) == wave);
    REQUIRE(queries.findPort(*wave, "out", false) != nullptr);
    REQUIRE(queries.findRuntimeTrace("wave") == &snapshot.runtimeTrace.nodes.front());
    REQUIRE(queries.findPreviewResult("wave") == &snapshot.previewResult.nodes.front());
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

    GraphPresentationSnapshot snapshot;
    NodeCanvasQueryModel queries(graph, snapshot);
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

TEST_CASE("Scratch attachment help distinguishes defaults inheritance and exclusion",
        "[cycle-v2][canvas][queries][voice-context][scratch]") {
    GraphNodeFactory factory;
    GraphEditor editor;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Envelope, "scratch", {}));
    graph.addNode(factory.createNode(NodeKind::ScratchDefaultOverride, "voiceTime", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "inherited", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "excluded", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "scratch", "purpose", "Purpose", "scratch").succeeded());
    REQUIRE(editor.connect(
            graph,
            { "scratch", "env", false },
            { "voice", "scratch", true }).succeeded());
    for (const String& target : { "inherited", "excluded" }) {
        REQUIRE(editor.connect(
                graph,
                { "voice", "context", false },
                { target, "context", true }).succeeded());
    }
    REQUIRE(editor.connect(
            graph,
            { "voiceTime", "scratch", false },
            { "excluded", "scratch", true }).succeeded());

    const GraphCompileResult compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphPresentationSnapshot snapshot;
    snapshot.compileResult = compiled;
    NodeCanvasQueryModel queries(graph, snapshot);

    REQUIRE(queries.hoverTextForPort({ "voice", "scratch", true }).contains("default"));
    REQUIRE(queries.hoverTextForPort({ "inherited", "scratch", true }).contains("Inherits"));
    REQUIRE(queries.hoverTextForPort({ "excluded", "scratch", true }).contains("voice time"));
    REQUIRE(queries.hoverTextForPort({ "voiceTime", "scratch", false }).contains("instead"));
    REQUIRE(queries.hoverTextForEdge(graph.getEdges().front()).contains("default"));
    REQUIRE(queries.hoverTextForEdge(graph.getEdges().back()).startsWith("Stops"));
}

TEST_CASE("Indexed canvas presentation queries ignore disconnected graph scale",
        "[cycle-v2][canvas][queries][complexity]") {
    for (const int unrelatedNodeCount : { 0, 128 }) {
        GraphNodeFactory factory;
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
        graph.addEdge({
                "wave", "out", "output", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        for (int index = 0; index < unrelatedNodeCount; ++index) {
            graph.addNode(factory.createNode(
                    NodeKind::Multiply,
                    "unrelated" + String(index),
                    {}));
        }

        GraphPresentationSnapshot snapshot;
        snapshot.compileResult = GraphCompiler().compile(graph);
        snapshot.previewResult.nodes.push_back({
                "wave", PreviewModuleRole::Waveform
        });
        snapshot.facts = std::make_shared<const GraphPresentationFacts>(graph, snapshot);
        NodeCanvasQueryModel queries(graph, snapshot);
        const Node* wave = graph.findNode("wave");
        REQUIRE(wave != nullptr);

        InteractionComplexityDiagnostics::reset();
        for (int repetition = 0; repetition < 16; ++repetition) {
            REQUIRE(queries.displayDomainForEdge(graph.getEdges().front())
                    == PortDomain::TimeSignal);
            REQUIRE(queries.findPreviewResult("wave") != nullptr);
            REQUIRE(queries.renderProfileForNodeOutput(*wave, "out").getDomain()
                    == PortDomain::TimeSignal);
        }
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.domainTransfers == 0);
        REQUIRE(counts.validationEdgeVisits == 0);
        REQUIRE(counts.nodeLinearScans == 0);
    }
}
