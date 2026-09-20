#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphValidationContext.h"
#include "Graph/NodeParameterMap.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Runtime/PresentationGestureSession.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Curve/Panel/FlatCurvePanelAdapter.h"
#include "Nodes/Envelope/Editor/EnvelopePanelAdapter.h"

#include <Curve/Mesh/Vertex.h>

namespace CycleV2 {

namespace {

NodeGraph scaledGraph(int unrelatedNodeCount, size_t audioSampleCount) {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
    for (int index = 0; index < unrelatedNodeCount; ++index) {
        graph.addNode(factory.createNode(
                NodeKind::Add,
                "unrelated" + String(index),
                { (float) index, (float) index }));
    }

    AudioSampleResource audio { "audio", "Unrelated.wav", 48000.0, {} };
    audio.samples.resize(audioSampleCount);
    graph.addAudioResource(std::move(audio));
    return graph;
}

}

TEST_CASE("Edge index lookups ignore unrelated graph scale",
        "[cycle-v2][complexity][connection][index]") {
    GraphNodeFactory factory;
    for (const int unrelatedNodes : { 0, 128 }) {
        NodeGraph graph = scaledGraph(unrelatedNodes, 16384);
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addEdge({
                "wave", "out", "output", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        for (int index = 0; index < unrelatedNodes; ++index) {
            graph.addEdge({
                    "wave", "out", "unrelated" + String(index), "left",
                    PortDomain::TimeSignal, ConnectionKind::Signal
            });
        }

        const GraphEdgeIndex edgeIndex(graph.getEdges());
        InteractionComplexityDiagnostics::reset();

        REQUIRE(edgeIndex.edgesToInput("output", "time")
                == std::vector<size_t> { 0 });
        REQUIRE(edgeIndex.incomingEdges("output")
                == std::vector<size_t> { 0 });
        REQUIRE(edgeIndex.outgoingEdges("wave").size()
                == (size_t) unrelatedNodes + 1);
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.validationEdgeVisits == 0);
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);

        const GraphEdgeView proposedEdges(
                graph.getEdges(),
                { 0 },
                {{
                        "wave", "out", "output", "time",
                        PortDomain::TimeSignal, ConnectionKind::Signal
                }});
        const GraphEdgeIndex rebuiltIndex(proposedEdges);
        const GraphEdgeIndexOverlay overlayIndex(edgeIndex, proposedEdges);
        InteractionComplexityDiagnostics::reset();

        REQUIRE(overlayIndex.edgesToInput("output", "time")
                == rebuiltIndex.edgesToInput("output", "time"));
        REQUIRE(overlayIndex.incomingEdges("output")
                == rebuiltIndex.incomingEdges("output"));
        REQUIRE(overlayIndex.outgoingEdges("wave")
                == rebuiltIndex.outgoingEdges("wave"));
        REQUIRE(InteractionComplexityDiagnostics::counts().validationEdgeVisits == 0);
    }
}

TEST_CASE("Proposed domain resolution ignores disconnected graph scale",
        "[cycle-v2][complexity][domains][index]") {
    GraphNodeFactory factory;
    uint64_t expectedTransfers {};
    for (const int unrelatedBranches : { 0, 128 }) {
        NodeGraph graph;
        Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
        const auto signalType = std::find_if(
                mesh.parameters.begin(),
                mesh.parameters.end(),
                [](const NodeParameter& parameter) {
                    return parameter.id == "signalType";
                });
        REQUIRE(signalType != mesh.parameters.end());
        signalType->value = "spectralMagnitude";
        graph.addNode(std::move(mesh));
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addNode(factory.createNode(NodeKind::SpectralLayer, "layer", {}));
        graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", {}));
        graph.addNode(factory.createNode(NodeKind::Add, "final", {}));
        graph.addEdge({
                "mesh", "out", "layer", "in",
                PortDomain::ControlSignal, ConnectionKind::Signal
        });
        graph.addEdge({
                "layer", "out", "multiply", "right",
                PortDomain::ControlSignal, ConnectionKind::Signal
        });
        graph.addEdge({
                "multiply", "out", "final", "left",
                PortDomain::ControlSignal, ConnectionKind::Signal
        });

        for (int index = 0; index < unrelatedBranches; ++index) {
            const String sourceId = "source" + String(index);
            const String destinationId = "destination" + String(index);
            graph.addNode(factory.createNode(NodeKind::Add, sourceId, {}));
            graph.addNode(factory.createNode(NodeKind::Multiply, destinationId, {}));
            graph.addEdge({
                    sourceId, "out", destinationId, "left",
                    PortDomain::ControlSignal, ConnectionKind::Signal
            });
        }

        const GraphDomainResolver resolver;
        const auto baseline = resolver.resolve(graph);
        const GraphEdgeIndex baseIndex(graph.getEdges());
        const Edge replacement {
                "wave", "out", "layer", "in",
                PortDomain::TimeSignal, ConnectionKind::Signal
        };
        const GraphEdgeView proposed(graph.getEdges(), { 0 }, { replacement });
        const GraphEdgeIndexOverlay proposedIndex(baseIndex, proposed);
        const auto expected = resolver.resolve(graph, proposed);
        InteractionComplexityDiagnostics::reset();

        const auto resolved = resolver.resolve(
                graph,
                proposed,
                proposedIndex,
                baseline);

        REQUIRE(resolved.domains == expected.domains);
        REQUIRE(resolved.channelLayouts == expected.channelLayouts);
        REQUIRE(graph.getEdges()[0].sourceNodeId == "mesh");
        const auto counts = InteractionComplexityDiagnostics::counts();
        if (unrelatedBranches == 0) {
            expectedTransfers = counts.domainTransfers;
        }
        REQUIRE(counts.domainTransfers > 0);
        REQUIRE(counts.domainTransfers == expectedTransfers);
        REQUIRE(counts.validationNodeVisits == 0);
        REQUIRE(counts.validationEdgeVisits == 0);
        REQUIRE(counts.graphCopies == 0);
    }
}

TEST_CASE("Proposed audio scope analysis ignores disconnected graph scale",
        "[cycle-v2][complexity][audio-scope][index]") {
    GraphNodeFactory factory;
    uint64_t expectedNodeVisits {};
    uint64_t expectedEdgeVisits {};
    for (const int unrelatedBranches : { 0, 128 }) {
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addNode(factory.createNode(NodeKind::GenericProcessor, "route", {}));
        graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
        graph.addEdge({
                "globalIn", "time", "route", "in",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        graph.addEdge({
                "route", "out", "out", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });

        for (int index = 0; index < unrelatedBranches; ++index) {
            const String sourceId = "scopeSource" + String(index);
            const String destinationId = "scopeDestination" + String(index);
            graph.addNode(factory.createNode(NodeKind::WaveSource, sourceId, {}));
            graph.addNode(factory.createNode(
                    NodeKind::GenericProcessor,
                    destinationId,
                    {}));
            graph.addEdge({
                    sourceId, "out", destinationId, "in",
                    PortDomain::TimeSignal, ConnectionKind::Signal
            });
        }

        const GraphAudioScopeAnalyzer analyzer;
        const auto baseline = analyzer.analyze(graph);
        const GraphEdgeIndex baseIndex(graph.getEdges());
        const Edge replacement {
                "wave", "out", "route", "in",
                PortDomain::TimeSignal, ConnectionKind::Signal
        };
        const GraphEdgeView proposed(graph.getEdges(), { 0 }, { replacement });
        const GraphEdgeIndexOverlay proposedIndex(baseIndex, proposed);
        const auto expected = analyzer.analyze(graph, proposed);
        InteractionComplexityDiagnostics::reset();

        const auto resolved = analyzer.analyze(
                graph,
                proposed,
                proposedIndex,
                baseline);

        REQUIRE(resolved.nodes == expected.nodes);
        REQUIRE(resolved.conflictingNeutralNodeIds
                == expected.conflictingNeutralNodeIds);
        REQUIRE(resolved.scopeFor("route") == AuthoredAudioScope::Voice);
        REQUIRE(resolved.hasConflict("route"));
        const auto counts = InteractionComplexityDiagnostics::counts();
        if (unrelatedBranches == 0) {
            expectedNodeVisits = counts.validationNodeVisits;
            expectedEdgeVisits = counts.validationEdgeVisits;
        }
        REQUIRE(counts.validationNodeVisits > 0);
        REQUIRE(counts.validationNodeVisits == expectedNodeVisits);
        REQUIRE(counts.validationEdgeVisits == expectedEdgeVisits);
        REQUIRE(counts.domainTransfers == 0);
        REQUIRE(counts.graphCopies == 0);
    }
}

TEST_CASE("Proposed validation ignores disconnected graph scale",
        "[cycle-v2][complexity][validation][index]") {
    GraphNodeFactory factory;
    uint64_t expectedNodeVisits {};
    uint64_t expectedEdgeVisits {};
    uint64_t expectedDomainTransfers {};
    for (const int unrelatedNodes : { 0, 128 }) {
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::Output, "output", {}));
        Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
        const auto signalType = std::find_if(
                mesh.parameters.begin(),
                mesh.parameters.end(),
                [](const NodeParameter& parameter) {
                    return parameter.id == "signalType";
                });
        REQUIRE(signalType != mesh.parameters.end());
        signalType->value = "spectralMagnitude";
        graph.addNode(std::move(mesh));
        for (int index = 0; index < unrelatedNodes; ++index) {
            graph.addNode(factory.createNode(
                    NodeKind::Add,
                    "unrelated" + String(index),
                    {}));
        }

        const GraphValidationContext context(graph);
        const Edge proposedEdge {
                "mesh", "out", "output", "time",
                PortDomain::ControlSignal, ConnectionKind::Signal
        };
        InteractionComplexityDiagnostics::reset();

        const auto issues = context.validateProposal(graph, {}, { proposedEdge });

        REQUIRE(issues.size() == 1);
        REQUIRE(issues.front().code == GraphValidationCode::DomainMismatch);
        REQUIRE(issues.front().sourceNodeId == "mesh");
        REQUIRE(issues.front().destNodeId == "output");
        const auto counts = InteractionComplexityDiagnostics::counts();
        if (unrelatedNodes == 0) {
            expectedNodeVisits = counts.validationNodeVisits;
            expectedEdgeVisits = counts.validationEdgeVisits;
            expectedDomainTransfers = counts.domainTransfers;
        }
        REQUIRE(counts.validationNodeVisits == expectedNodeVisits);
        REQUIRE(counts.validationEdgeVisits == expectedEdgeVisits);
        REQUIRE(counts.domainTransfers == expectedDomainTransfers);
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
    }
}

TEST_CASE("Explicit audio proposal validation ignores disconnected graph scale",
        "[cycle-v2][complexity][validation][audio-scope][index]") {
    GraphNodeFactory factory;
    uint64_t expectedNodeVisits {};
    uint64_t expectedEdgeVisits {};
    uint64_t expectedDomainTransfers {};
    for (const int unrelatedBranches : { 0, 128 }) {
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
        graph.addNode(factory.createNode(NodeKind::GlobalInput, "globalIn", {}));
        graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addNode(factory.createNode(NodeKind::GenericProcessor, "route", {}));
        graph.addEdge({
                "globalIn", "time", "route", "in",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        graph.addEdge({
                "route", "out", "out", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        graph.addEdge({
                "wave", "out", "voiceOut", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        for (int index = 0; index < unrelatedBranches; ++index) {
            const String sourceId = "unrelatedSource" + String(index);
            const String routeId = "unrelatedRoute" + String(index);
            graph.addNode(factory.createNode(NodeKind::WaveSource, sourceId, {}));
            graph.addNode(factory.createNode(NodeKind::GenericProcessor, routeId, {}));
            graph.addEdge({
                    sourceId, "out", routeId, "in",
                    PortDomain::TimeSignal, ConnectionKind::Signal
            });
        }

        const GraphValidationContext context(graph);
        const Edge replacement {
                "wave", "out", "route", "in",
                PortDomain::TimeSignal, ConnectionKind::Signal
        };
        InteractionComplexityDiagnostics::reset();

        const auto issues = context.validateProposal(graph, { 0 }, { replacement });

        REQUIRE_FALSE(issues.empty());
        const auto counts = InteractionComplexityDiagnostics::counts();
        if (unrelatedBranches == 0) {
            expectedNodeVisits = counts.validationNodeVisits;
            expectedEdgeVisits = counts.validationEdgeVisits;
            expectedDomainTransfers = counts.domainTransfers;
        }
        REQUIRE(counts.validationNodeVisits == expectedNodeVisits);
        REQUIRE(counts.validationEdgeVisits == expectedEdgeVisits);
        REQUIRE(counts.domainTransfers == expectedDomainTransfers);
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
    }
}

TEST_CASE("Connection commit does not copy unrelated graph or audio resources",
        "[cycle-v2][complexity][connection]") {
    GraphNodeFactory factory;
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        NodeGraph graph = scaledGraph(scale.first, scale.second);
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        InteractionComplexityDiagnostics::reset();

        const auto result = GraphEditor().connect(
                graph,
                { "wave", "out", false },
                { "output", "time", true });

        REQUIRE(result.succeeded());
        REQUIRE(graph.getEdges().size() == 1);
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
    }
}

TEST_CASE("Splice commit does not copy unrelated graph or audio resources",
        "[cycle-v2][complexity][connection]") {
    GraphNodeFactory factory;
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        NodeGraph graph = scaledGraph(scale.first, scale.second);
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addNode(factory.createNode(NodeKind::Waveshaper, "shape", {}));
        graph.addEdge({
                "wave", "out", "output", "time",
                PortDomain::TimeSignal, ConnectionKind::Signal
        });
        InteractionComplexityDiagnostics::reset();

        const auto result = GraphEditor().spliceNodeIntoEdge(graph, 0, "shape");

        REQUIRE(result.succeeded());
        REQUIRE(graph.getEdges().size() == 2);
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
    }
}

TEST_CASE("Scalar gesture cost is independent of unrelated graph and audio data",
        "[cycle-v2][complexity][gesture][parameter]") {
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        GraphDocument document(scaledGraph(scale.first, scale.second));
        GraphCommandDispatcher commands(document);
        int causalPublications {};
        GraphChangeSet causalChange;
        document.setListener([&](uint64_t, const GraphChangeSet& change) {
            ++causalPublications;
            causalChange = change;
        });
        InteractionComplexityDiagnostics::reset();

        commands.beginTransientEdit();
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.6").succeeded());
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.7").succeeded());
        const auto noChange = commands.setNodeParameter("output", "gain", "Gain", "0.7");
        REQUIRE(noChange.succeeded());
        REQUIRE_FALSE(noChange.changed);
        REQUIRE(document.graph().findNodeParameter("output", "gain")->value != "0.7");
        REQUIRE(commands.editingGraph().findNodeParameter("output", "gain")->value == "0.7");
        const auto& editingNodes = commands.editingGraph().getNodes();
        const auto output = std::find_if(editingNodes.begin(), editingNodes.end(), [](const auto& node) {
            return node.id == "output";
        });
        REQUIRE(output != editingNodes.end());
        REQUIRE(parameterValueForNode(*output, "gain") == "0.7");
        commands.commitTransientEdit();
        REQUIRE(causalPublications == 1);
        REQUIRE(causalChange.nodeIds == std::vector<String> { "output" });
        REQUIRE(causalChange.parameterImpacts != ParameterImpact::None);

        REQUIRE(document.undo());
        REQUIRE(document.redo());
        REQUIRE(causalPublications == 3);
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.parameterLinearScans == 0);
    }
}

TEST_CASE("Preview morph publication is independent of unrelated graph content",
        "[cycle-v2][complexity][preview-morph]") {
    GraphNodeFactory factory;
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        NodeGraph graph = scaledGraph(scale.first, scale.second);
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher commands(document);
        InteractionComplexityDiagnostics::reset();

        REQUIRE(commands.setPreviewMorph(0.2f, 0.8f).succeeded());
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.parameterLinearScans == 0);
    }
}

TEST_CASE("Two transient preview morph movements commit once and undo together",
        "[cycle-v2][complexity][preview-morph][gesture]") {
    GraphNodeFactory factory;
    for (const int unrelatedNodes : { 0, 128 }) {
        NodeGraph graph = scaledGraph(unrelatedNodes, 16384);
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher commands(document);
        const uint64_t baseRevision = document.revision();
        const float initialBlue = NodeParameterMap(
                *document.graph().findNode("mesh")).floatValue("blue");
        auto stableBase = std::make_shared<const NodeGraph>(document.graph());
        InteractionComplexityDiagnostics::reset();

        commands.beginTransientEdit();
        REQUIRE(commands.setPreviewMorph(0.2f, 0.3f).changed);
        const NodeGraph firstSnapshot = commands.editingGraph().snapshotNodeEdits(stableBase);
        REQUIRE(commands.setPreviewMorph(0.2f, 0.8f).changed);
        const NodeGraph finalSnapshot = commands.editingGraph().snapshotNodeEdits(stableBase);
        REQUIRE_FALSE(commands.setPreviewMorph(0.2f, 0.8f).changed);
        REQUIRE(document.revision() == baseRevision);
        REQUIRE(NodeParameterMap(*commands.editingGraph().findNode("mesh"))
                .floatValue("blue") == 0.8f);
        REQUIRE(NodeParameterMap(*firstSnapshot.findNode("mesh")).floatValue("blue") == 0.3f);
        REQUIRE(NodeParameterMap(*finalSnapshot.findNode("mesh")).floatValue("blue") == 0.8f);
        REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
                .floatValue("blue") == initialBlue);

        commands.commitTransientEdit();
        REQUIRE(document.revision() > baseRevision);
        REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
                .floatValue("blue") == 0.8f);
        REQUIRE(NodeParameterMap(*document.graph().findNode("env"))
                .floatValue("blue") == 0.8f);
        REQUIRE(document.undo());
        stableBase.reset();
        REQUIRE(NodeParameterMap(*finalSnapshot.findNode("mesh")).floatValue("blue") == 0.8f);
        REQUIRE(NodeParameterMap(*document.graph().findNode("mesh"))
                .floatValue("blue") == initialBlue);

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
    }
}

TEST_CASE("Shared parameter gesture snapshots scale with its edited node",
        "[cycle-v2][complexity][gesture][presentation-session]") {
    for (const int unrelatedNodes : { 0, 128 }) {
        GraphDocument document(scaledGraph(unrelatedNodes, 16384));
        GraphCommandDispatcher commands(document);
        PresentationGestureSession session;
        REQUIRE(session.beginGraphGesture(
                "editor:output",
                commands,
                document,
                ProbeRefreshMode::LiveLatest,
                0,
                true));
        InteractionComplexityDiagnostics::reset();

        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.6").changed);
        REQUIRE(session.recordGraphMovement("editor:output", 6).has_value());
        const auto first = session.snapshotGraphGesture("editor:output", commands);
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.7").changed);
        REQUIRE(session.recordGraphMovement("editor:output", 7).has_value());
        const auto last = session.snapshotGraphGesture("editor:output", commands);
        REQUIRE(first != nullptr);
        REQUIRE(last != nullptr);
        REQUIRE(parameterValueForNode(*first->findNode("output"), "gain") == "0.6");
        REQUIRE(parameterValueForNode(*last->findNode("output"), "gain") == "0.7");

        const auto finished = session.finishGraphGesture("editor:output", commands, document);
        REQUIRE(finished.durableChanged);
        REQUIRE(document.undo());
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.parameterLinearScans == 0);
    }
}

TEST_CASE("On release parameter gesture starts without copying the graph",
        "[cycle-v2][complexity][gesture][presentation-session]") {
    for (const int unrelatedNodes : { 0, 128 }) {
        GraphDocument document(scaledGraph(unrelatedNodes, 16384));
        GraphCommandDispatcher commands(document);
        PresentationGestureSession session;
        InteractionComplexityDiagnostics::reset();

        REQUIRE(session.beginGraphGesture(
                "editor:output",
                commands,
                document,
                ProbeRefreshMode::OnGestureCommit,
                0,
                true));
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.6").changed);
        REQUIRE(session.recordGraphMovement("editor:output", 6).has_value());
        REQUIRE(commands.setNodeParameter("output", "gain", "Gain", "0.7").changed);
        REQUIRE(session.recordGraphMovement("editor:output", 7).has_value());
        REQUIRE_FALSE(session.snapshotGraphGesture("editor:output", commands));
        REQUIRE(session.finishGraphGesture("editor:output", commands, document).durableChanged);
        REQUIRE(document.undo());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.parameterLinearScans == 0);
    }
}

TEST_CASE("Local primary morph gesture avoids a live graph snapshot",
        "[cycle-v2][complexity][gesture][presentation-session]") {
    GraphNodeFactory factory;
    for (const int unrelatedNodes : { 0, 128 }) {
        NodeGraph graph = scaledGraph(unrelatedNodes, 16384);
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        GraphDocument document(std::move(graph));
        GraphCommandDispatcher commands(document);
        PresentationGestureSession session;
        InteractionComplexityDiagnostics::reset();

        REQUIRE(session.beginGraphGesture(
                "editor:mesh", commands, document,
                ProbeRefreshMode::LiveLatest, 0, true, false));
        REQUIRE(commands.setNodeParameter("mesh", "yellow", "Yellow", "0.2").changed);
        REQUIRE(session.recordGraphMovement("editor:mesh", 2).has_value());
        REQUIRE(commands.setNodeParameter("mesh", "yellow", "Yellow", "0.8").changed);
        REQUIRE(session.recordGraphMovement("editor:mesh", 8).has_value());
        REQUIRE_FALSE(session.snapshotGraphGesture("editor:mesh", commands));
        REQUIRE(session.finishGraphGesture("editor:mesh", commands, document).durableChanged);
        REQUIRE(document.undo());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.parameterLinearScans == 0);
    }
}

TEST_CASE("Canvas translation iterates changed identities without graph snapshots",
        "[cycle-v2][complexity][gesture][canvas]") {
    for (const auto& scale : std::vector<std::pair<int, size_t>> {
            { 0, 0 }, { 128, 16384 } }) {
        CAPTURE(scale.first, scale.second);
        GraphDocument document(scaledGraph(scale.first, scale.second));
        GraphCommandDispatcher commands(document);
        const Point<float> original = document.graph().findNode("output")->bounds.getPosition();
        InteractionComplexityDiagnostics::reset();

        commands.beginCompoundEdit();
        REQUIRE(commands.translateNodes({ "output" }, { 4.f, 3.f }).succeeded());
        REQUIRE(commands.translateNodes({ "output" }, { 2.f, 1.f }).succeeded());
        commands.commitCompoundEdit();
        REQUIRE(document.graph().findNode("output")->bounds.getPosition()
                == original.translated(6.f, 4.f));

        REQUIRE(document.undo());
        REQUIRE(document.graph().findNode("output")->bounds.getPosition() == original);
        REQUIRE(document.redo());
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.nodeLinearScans == 0);
    }
}

TEST_CASE("Guide curve gesture resolves consumers from the relationship index",
        "[cycle-v2][complexity][gesture][guide]") {
    for (const int unrelatedNodeCount : { 0, 128 }) {
        CAPTURE(unrelatedNodeCount);
        NodeGraph graph = scaledGraph(unrelatedNodeCount, unrelatedNodeCount == 0 ? 0 : 16384);
        const NodeModelStatePtr model = createDefaultGuideCurveModel();
        REQUIRE(graph.addGuideCurve({
                "guide", "G", "Guide", 0, 0, true,
                0.f, 0.f, 0.f, -1, model, {}, 1
        }));
        if (unrelatedNodeCount == 0) {
            REQUIRE(graph.assignGuideCurve({
                    "guide", "output", { 0, GuideCurveField::Amplitude }
            }));
        } else {
            for (int index = 0; index < unrelatedNodeCount; ++index) {
                REQUIRE(graph.assignGuideCurve({
                        "guide",
                        "unrelated" + String(index),
                        { index, GuideCurveField::Amplitude }
                }));
            }
        }

        GraphDocument document(std::move(graph));
        GraphCommandDispatcher commands(document);
        const std::vector<NodeParameter> firstControls {
                { "enabled", "Enabled", "1" },
                { "noise", "Noise", "0.25" },
                { "dcOffset", "DC Offset", "0" },
                { "phase", "Phase", "0" }
        };
        auto secondControls = firstControls;
        secondControls[1].value = "0.5";
        InteractionComplexityDiagnostics::reset();

        commands.beginTransientEdit();
        REQUIRE(commands.publishGuideCurveState({
                "guide", 1, model, firstControls
        }).succeeded());
        REQUIRE(commands.publishGuideCurveState({
                "guide", 1, model, secondControls
        }).succeeded());
        const auto noChange = commands.publishGuideCurveState({
                "guide", 1, model, secondControls
        });
        REQUIRE(noChange.succeeded());
        REQUIRE_FALSE(noChange.changed);
        commands.commitTransientEdit();
        REQUIRE(document.undo());
        REQUIRE(document.redo());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.audioSamplesCopied == 0);
        REQUIRE(counts.assignmentLinearScans == 0);
    }
}

TEST_CASE("Flat curve movement notifications do not serialize or snapshot the model",
        "[cycle-v2][complexity][gesture][curve]") {
    for (const int extraVertexCount : { 0, 128 }) {
        CAPTURE(extraVertexCount);
        FlatCurvePanelAdapter adapter(NodeKind::Waveshaper);
        adapter.initialiseDefaultMesh();
        for (int index = 0; index < extraVertexCount; ++index) {
            adapter.mesh().addVertex(new Vertex(
                    (float) index / 128.f,
                    (float) index / 256.f));
        }
        InteractionComplexityDiagnostics::reset();

        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.25f;
        REQUIRE(adapter.registerMeshEdit());
        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.5f;
        REQUIRE(adapter.registerMeshEdit());
        REQUIRE(adapter.registerMeshEdit());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.meshCopies == 0);
    }
}

TEST_CASE("Envelope movement notifications do not compare or snapshot the mesh",
        "[cycle-v2][complexity][gesture][envelope]") {
    for (const int extraVertexCount : { 0, 128 }) {
        CAPTURE(extraVertexCount);
        EnvelopePanelAdapter adapter;
        adapter.initialiseDefaultMesh();
        REQUIRE_FALSE(adapter.mesh().getVerts().empty());
        for (int index = 0; index < extraVertexCount; ++index) {
            adapter.mesh().addVertex(new Vertex(
                    (float) index / 128.f,
                    (float) index / 256.f));
        }
        InteractionComplexityDiagnostics::reset();

        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.25f;
        REQUIRE(adapter.registerMeshEdit());
        adapter.mesh().getVerts().front()->values[Vertex::Amp] = 0.5f;
        REQUIRE(adapter.registerMeshEdit());
        REQUIRE(adapter.registerMeshEdit());

        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.meshCopies == 0);
    }
}

}
