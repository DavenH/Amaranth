#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCompiler.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphEditor.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Trimesh/Model/TrimeshMeshFactory.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Runtime/GraphPresentationModel.h"
#include "UI/SignalProbeCanvas.h"
#include "UI/WorkspaceDockInteractionController.h"

using namespace CycleV2;

TEST_CASE("Signal probe cable annotations scale with canvas zoom",
        "[cycle-v2][ui][signal-probe][zoom]") {
    const float reference = SignalProbeCanvas::cableAnnotationDiameter(0.58f);

    REQUIRE(reference == Catch::Approx(8.4f));
    REQUIRE(SignalProbeCanvas::cableAnnotationDiameter(1.16f)
            == Catch::Approx(reference * 2.f));
    REQUIRE(SignalProbeCanvas::cableAnnotationDiameter(0.29f)
            == Catch::Approx(reference * 0.5f));
}

TEST_CASE("Spy tethers leave cables perpendicularly toward the card",
        "[cycle-v2][ui][signal-probe][tether]") {
    const auto checkDeparture = [](Point<float> start,
            Point<float> end, Point<float> target) {
        Path cable;
        cable.startNewSubPath(start);
        cable.lineTo(end);
        const Path tether = SignalProbeCanvas::tetherPath(cable, 0.5f, target);
        const Point<float> anchor = cable.getPointAlongPath(cable.getLength() * 0.5f);
        const Point<float> departure = tether.getPointAlongPath(1.f) - anchor;
        const Point<float> tangent = end - start;
        REQUIRE(tether.getLength() > 0.f);
        const float parallelFraction = (departure.x * tangent.x
                + departure.y * tangent.y)
                / (departure.getDistanceFromOrigin() * tangent.getDistanceFromOrigin());
        REQUIRE(parallelFraction == Catch::Approx(0.f).margin(0.05f));
        REQUIRE(departure.x * (target.x - anchor.x)
                        + departure.y * (target.y - anchor.y) > 0.f);
    };

    checkDeparture({ 0.f, 0.f }, { 100.f, 0.f }, { 50.f, -100.f });
    checkDeparture({ 0.f, 0.f }, { 100.f, 0.f }, { 50.f, 100.f });
    checkDeparture({ 0.f, 0.f }, { 0.f, 100.f }, { 100.f, 50.f });
    checkDeparture({ 0.f, 0.f }, { 0.f, 100.f }, { -100.f, 50.f });
}

namespace {

NodeGraph probeGraph() {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 400.f, 0.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    return graph;
}

}

TEST_CASE("Default output spy has a canvas card without becoming graph state",
        "[cycle-v2][ui][probe][default-output]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());

    const auto ids = SignalProbeCanvas::orderedProbeIds(graph);
    REQUIRE(ids.size() == 2);
    REQUIRE(ids.front() == graph.getSignalProbes().front().id);
    REQUIRE(ids.back() == DefaultOutputProbeResolver::probeId);
    REQUIRE(SignalProbeCanvas::ordinalForProbe(
            graph, DefaultOutputProbeResolver::probeId) == 2);
    REQUIRE(SignalProbeCanvas::ordinalForProbe(graph, ids.front()) == 1);

    SignalProbeCanvasState state;
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 800.f, 600.f });
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport, 1, 1);
    const auto card = SignalProbeCanvas::cardBoundsFor(
            DefaultOutputProbeResolver::probeId, graph, snapshot, viewport, state);
    REQUIRE_FALSE(card.isEmpty());
    REQUIRE(SignalProbeCanvas::cardAt(
            card.getCentre(), graph, snapshot, viewport, state)
            == DefaultOutputProbeResolver::probeId);
    REQUIRE(graph.getSignalProbes().size() == 1);
}

TEST_CASE("Canvas Spy positions serialize and undo as a small graph delta",
        "[cycle-v2][probe][canvas][undo]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    const String probeId = graph.getSignalProbes().front().id;
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);

    REQUIRE(commands.moveSignalProbe(probeId, { 125.f, -80.f }).succeeded());
    REQUIRE(document.graph().findSignalProbe(probeId)->canvasPosition
            == Point<float>(125.f, -80.f));
    REQUIRE(GraphSerializer().fromJsonString(document.toJson())
                    .findSignalProbe(probeId)->canvasPosition
            == Point<float>(125.f, -80.f));

    REQUIRE(document.undo());
    REQUIRE_FALSE(document.graph().findSignalProbe(probeId)->canvasPosition.has_value());
    REQUIRE(document.redo());
    REQUIRE(document.graph().findSignalProbe(probeId)->canvasPosition
            == Point<float>(125.f, -80.f));
}

TEST_CASE("Output Spy card position persists as preset presentation",
        "[cycle-v2][probe][canvas][default-output]") {
    GraphDocument document(probeGraph());
    GraphCommandDispatcher commands(document);

    REQUIRE(commands.moveDefaultOutputSpy({ 320.f, 180.f }));
    REQUIRE(document.isDirty());
    REQUIRE(document.presentation().outputSpyPosition == Point<float>(320.f, 180.f));
    REQUIRE(GraphSerializer().loadJsonString(document.toJson())
                    .presentation.outputSpyPosition
            == Point<float>(320.f, 180.f));
}

TEST_CASE("Removing the output Spy hides its card but preserves output capture",
        "[cycle-v2][probe][canvas][default-output][preview]") {
    GraphDocument document(probeGraph());
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;

    REQUIRE(commands.setDefaultOutputSpyVisible(false));
    REQUIRE_FALSE(document.presentation().outputSpyVisible);
    REQUIRE_FALSE(GraphSerializer().loadJsonString(document.toJson())
                    .presentation.outputSpyVisible);
    REQUIRE(presentation.refresh(document.graph(), document.revision()));
    REQUIRE(presentation.previewResult().defaultOutput.has_value());

    SignalProbeCanvasState state;
    state.outputSpyVisible = document.presentation().outputSpyVisible;
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 800.f, 600.f });
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(document.graph(), viewport, 1, 1);
    REQUIRE(SignalProbeCanvas::cardBoundsFor(
            DefaultOutputProbeResolver::probeId,
            document.graph(), snapshot, viewport, state).isEmpty());
}

TEST_CASE("Spy display domains persist with the preset",
        "[cycle-v2][probe][domain][serialization]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    const String probeId = graph.getSignalProbes().front().id;
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);

    REQUIRE(commands.setSignalProbeFrequencyView(probeId, true).succeeded());
    REQUIRE(commands.setDefaultOutputSpyFrequencyView(false));
    const GraphLoadResult loaded = GraphSerializer().loadJsonString(document.toJson());
    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.graph.findSignalProbe(probeId)->frequencyView);
    REQUIRE_FALSE(loaded.presentation.outputSpyFrequencyView);
    REQUIRE(document.undo());
    REQUIRE_FALSE(document.graph().findSignalProbe(probeId)->frequencyView);
}

TEST_CASE("Time Spy capture publishes a matching frequency view",
        "[cycle-v2][probe][preview][spectrum]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    GraphPresentationModel presentation;

    REQUIRE(presentation.refresh(graph, 1));
    const auto& preview = presentation.previewResult();
    REQUIRE(preview.probes.size() == 1);
    REQUIRE(preview.probes.front().connected);
    REQUIRE(preview.probes.front().domain == PortDomain::TimeSignal);
    REQUIRE(preview.probeSpectra.size() == 1);
    REQUIRE(preview.probeSpectra.front().has_value());
    REQUIRE(preview.probeSpectra.front()->domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(preview.probeSpectra.front()->gridColumns == preview.probes.front().gridColumns);
}

TEST_CASE("Signal probes toggle once per source output without changing execution", "[cycle-v2][probe]") {
    NodeGraph graph = probeGraph();
    const auto before = GraphCompiler().compile(graph);
    REQUIRE(before.succeeded());

    const auto added = GraphEditor().toggleSignalProbe(graph, 0, 0.25f);
    REQUIRE(added.changes.probesChanged);
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

TEST_CASE("Signal probe labels and rail positions remain unique after deletion",
        "[cycle-v2][probe]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "firstWave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "firstOut", {}));
    graph.addNode(factory.createNode(NodeKind::WaveSource, "secondWave", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "secondOut", {}));
    graph.addEdge({ "firstWave", "out", "firstOut", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "secondWave", "out", "secondOut", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addSignalProbe({
            "probe2", "firstWave", "out", "firstOut", "time", "Spy 2", 0.5f, 1
    });

    REQUIRE(GraphEditor().toggleSignalProbe(graph, 1, 0.5f).succeeded());
    REQUIRE(graph.getSignalProbes().size() == 2);
    REQUIRE(graph.getSignalProbes().back().label == "Spy 3");
    REQUIRE(graph.getSignalProbes().back().railOrder == 2);
}

TEST_CASE("Signal probes reject nonsignal cables", "[cycle-v2][probe]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 400.f, 0.f }));
    graph.addEdge({ "env", "env", "multiply", "right", PortDomain::EnvelopeSignal, ConnectionKind::Signal });

    REQUIRE_FALSE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    REQUIRE(graph.getSignalProbes().empty());
}

TEST_CASE("Signal probes serialize independently from processing nodes", "[cycle-v2][probe]") {
    NodeGraph graph = probeGraph();
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.4f).succeeded());

    const String json = GraphSerializer().toJsonString(graph);
    REQUIRE(json.contains("\"probes\""));
    REQUIRE(json.contains("\"id\": \"probe\""));

    const NodeGraph restored = GraphSerializer().fromJsonString(json);
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
    graph.addEdge({ "wave", "out", "first", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({ "wave", "out", "second", "time", PortDomain::TimeSignal, ConnectionKind::Signal });

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

TEST_CASE("Pinned Spy batch matches targeted capture at one comparison note",
        "[cycle-v2][probe][comparison][batch]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal,
            ConnectionKind::Signal });
    graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal,
            ConnectionKind::Signal });
    graph.addEdge({ "add", "out", "out", "time", PortDomain::TimeSignal,
            ConnectionKind::Signal });
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 0, 0.5f).succeeded());
    REQUIRE(GraphEditor().toggleSignalProbe(graph, 2, 0.5f).succeeded());
    GraphPresentationModel presentation;
    REQUIRE(presentation.refresh(graph, graph.getRevision()));

    std::vector<String> ids;
    for (const auto& probe : graph.getSignalProbes()) {
        ids.push_back(probe.id);
    }
    ids.push_back(DefaultOutputProbeResolver::probeId);
    constexpr int note = 57;
    const size_t rows = GraphPreviewExecutor::sourceFrameCountForMidiNote(note);
    const auto batch = presentation.captureProbePreviews(graph, ids, rows, note);
    REQUIRE(batch.size() == ids.size());
    for (size_t index = 0; index < ids.size(); ++index) {
        const auto single = presentation.captureProbePreview(graph, ids[index], rows, note);
        REQUIRE(batch[index].has_value() == single.has_value());
        if (single.has_value()) {
            REQUIRE(batch[index]->values == single->values);
            REQUIRE(batch[index]->domain == single->domain);
        }
    }

    const auto& plan = presentation.compileResult().plan;
    AudioVoiceContext voice;
    voice.controls.noteNumber = note;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    GraphAudioExecutor executor;
    const auto batched = executor.processProbes(
            plan, plan.signalProbes, rows, {}, voice, 512);
    size_t separateStepVisits {};
    for (const auto& probe : plan.signalProbes) {
        separateStepVisits += executor.processProbe(
                plan, probe, rows, {}, voice, 512).processedStepCount;
    }
    REQUIRE(batched.processedStepCount <= separateStepVisits);
}

TEST_CASE("Comparison morph capture preparation ignores unrelated graph and mesh content",
        "[cycle-v2][probe][comparison][complexity]") {
    const auto verify = [](int unrelatedNodes, int unrelatedCubes) {
        GraphNodeFactory factory;
        NodeGraph graph;
        graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
        graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", {}));
        graph.addNode(factory.createNode(NodeKind::Add, "add", {}));
        graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
        graph.addEdge({ "wave", "out", "add", "left", PortDomain::TimeSignal,
                ConnectionKind::Signal });
        graph.addEdge({ "mesh", "out", "add", "right", PortDomain::ControlSignal,
                ConnectionKind::Signal });
        graph.addEdge({ "add", "out", "out", "time", PortDomain::TimeSignal,
                ConnectionKind::Signal });
        for (int index = 0; index < unrelatedNodes; ++index) {
            graph.addNode(factory.createNode(
                    NodeKind::Delay, "unrelated" + String(index), {}));
        }
        auto mesh = TrimeshMeshFactory::createDefaultMesh("comparison-complexity");
        for (int index = 0; index < unrelatedCubes; ++index) {
            TrimeshMeshFactory::addVoiceCube(
                    *mesh, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f);
        }
        REQUIRE(graph.replaceNodeModel(
                "mesh", TrimeshNodeModelState::copyOf(*mesh, 2)));
        mesh->destroy();

        GraphDocument document(std::move(graph));
        GraphPresentationModel presentation;
        REQUIRE(presentation.refresh(document.graph(), document.revision()));
        const std::vector<String> selected { DefaultOutputProbeResolver::probeId };
        const auto before = presentation.captureProbePreviews(
                document.graph(), selected, 512, 57);
        REQUIRE(before.size() == 1);
        REQUIRE(before.front().has_value());
        GraphCommandDispatcher commands(document);
        REQUIRE(commands.setNodeParameter("mesh", "red", "Red", "0.73").succeeded());
        InteractionComplexityDiagnostics::reset();
        REQUIRE(presentation.prepareProbeCaptureForNode(document.graph(), "mesh"));
        const auto counts = InteractionComplexityDiagnostics::counts();
        REQUIRE(counts.graphCopies == 0);
        REQUIRE(counts.meshCopies == 0);
        REQUIRE(counts.modelSerializations == 0);
        REQUIRE(counts.nodeLinearScans == 0);
        REQUIRE(counts.assignmentLinearScans == 0);
        const auto after = presentation.captureProbePreviews(
                document.graph(), selected, 512, 57);
        REQUIRE(after.front().has_value());
        REQUIRE(after.front()->values != before.front()->values);

        NodeGraph modelEdited = document.graph();
        const auto previousModel = std::dynamic_pointer_cast<
                const TrimeshNodeModelState>(modelEdited.findNode("mesh")->model);
        REQUIRE(previousModel != nullptr);
        REQUIRE(modelEdited.replaceNodeModel(
                "mesh", TrimeshNodeModelState::copyOf(previousModel->mesh(), 3)));
        InteractionComplexityDiagnostics::reset();
        REQUIRE_FALSE(presentation.prepareProbeCaptureForNode(modelEdited, "mesh"));
        const auto modelChangeCounts = InteractionComplexityDiagnostics::counts();
        REQUIRE(modelChangeCounts.graphCopies == 0);
        REQUIRE(modelChangeCounts.meshCopies == 0);
        REQUIRE(modelChangeCounts.modelSerializations == 0);
        return counts;
    };

    const auto small = verify(0, 0);
    const auto large = verify(128, 128);
    REQUIRE(large.graphCopies == small.graphCopies);
    REQUIRE(large.meshCopies == small.meshCopies);
    REQUIRE(large.modelSerializations == small.modelSerializations);
    REQUIRE(large.nodeLinearScans == small.nodeLinearScans);
    REQUIRE(large.assignmentLinearScans == small.assignmentLinearScans);
}
