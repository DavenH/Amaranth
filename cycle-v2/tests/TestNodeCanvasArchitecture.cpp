#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphCommandDispatcher.h"
#include "../src/Graph/GraphDocument.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/UI/NodeCanvasScene.h"
#include "../src/UI/NodeAutomationFacade.h"
#include "../src/UI/NodeCanvasViewport.h"
#include "../src/UI/NodeViewModule.h"
#include "../src/Runtime/GraphPresentationModel.h"

using namespace CycleV2;

TEST_CASE("Node canvas viewport transforms round trip and preserve zoom anchors", "[cycle-v2][canvas]") {
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1200.f, 800.f });
    viewport.setTransform({ 34.f, 38.f }, 0.58f);
    const Point<float> world { 372.f, 218.f };

    REQUIRE(viewport.toWorld(viewport.toScreen(world)).x == Catch::Approx(world.x));
    REQUIRE(viewport.toWorld(viewport.toScreen(world)).y == Catch::Approx(world.y));

    const Point<float> anchor { 640.f, 360.f };
    const Point<float> before = viewport.toWorld(anchor);
    viewport.zoomAround(anchor, 1.4f);
    const Point<float> after = viewport.toWorld(anchor);
    REQUIRE(after.x == Catch::Approx(before.x));
    REQUIRE(after.y == Catch::Approx(before.y));
    REQUIRE(viewport.centreWorld() == viewport.toWorld(Point<float>(600.f, 400.f)));
}

TEST_CASE("Node canvas viewport snapping is deterministic", "[cycle-v2][canvas]") {
    NodeCanvasViewport viewport;
    REQUIRE(viewport.snap({ 24.f, 26.f }, 10.f) == Point<float>(20.f, 30.f));
    REQUIRE(viewport.snap({ 24.f, 26.f }, 0.f) == Point<float>(24.f, 26.f));
}

TEST_CASE("Node canvas scene shares geometry with typed hit testing", "[cycle-v2][canvas]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 100.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 500.f, 80.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    NodeCanvasViewport viewport;
    viewport.setTransform({ 20.f, 30.f }, 1.f);
    NodeCanvasScene sceneBuilder;
    const auto& scene = sceneBuilder.build(graph, viewport);
    const auto outputTarget = std::find_if(scene.targets.begin(), scene.targets.end(), [](const auto& target) {
        return target.semanticId == "output:wave.out";
    });
    REQUIRE(outputTarget != scene.targets.end());

    const auto hit = NodeCanvasHitTester().hitTest(scene, outputTarget->bounds.getCentre());
    REQUIRE(hit.has_value());
    REQUIRE(hit->kind == NodeSceneTargetKind::OutputPort);
    REQUIRE(hit->portAddress().nodeId == "wave");
    REQUIRE(hit->portAddress().portId == "out");
}

TEST_CASE("Node canvas scene invalidates only for relevant revisions", "[cycle-v2][canvas]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Envelope, "env", {}));
    NodeCanvasViewport viewport;
    NodeCanvasScene scene;

    const auto* firstTargets = scene.build(graph, viewport, 4).targets.data();
    const auto* cachedTargets = scene.build(graph, viewport, 4).targets.data();
    REQUIRE(cachedTargets == firstTargets);

    viewport.panBy({ 1.f, 0.f });
    REQUIRE(scene.build(graph, viewport, 4).viewportRevision == viewport.getRevision());
    REQUIRE(scene.build(graph, viewport, 5).presentationRevision == 5);
    REQUIRE(scene.build(graph, viewport, 5, 99).graphRevision == 99);
}

TEST_CASE("Graph document rejects failed loads without replacing active state", "[cycle-v2][canvas][document]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    const String before = document.toXml();

    REQUIRE_FALSE(document.loadXml("<not-a-graph/>", true));
    REQUIRE(document.toXml() == before);
    REQUIRE_FALSE(document.canUndo());
}

TEST_CASE("Graph command dispatcher records semantic edits and undo", "[cycle-v2][canvas][document]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    const size_t initialNodeCount = document.graph().getNodes().size();
    const uint64_t initialRevision = document.revision();

    const auto added = commands.addNode(NodeKind::Envelope, { 100.f, 140.f });
    REQUIRE(added.succeeded());
    REQUIRE(document.graph().getNodes().size() == initialNodeCount + 1);
    REQUIRE(document.revision() == initialRevision + 1);
    REQUIRE(document.lastChange().topologyChanged);
    REQUIRE(document.canUndo());

    REQUIRE(document.undo());
    REQUIRE(document.graph().getNodes().size() == initialNodeCount);
    REQUIRE(document.redo());
    REQUIRE(document.graph().getNodes().size() == initialNodeCount + 1);
}

TEST_CASE("Graph command dispatcher coalesces a drag into one undo entry", "[cycle-v2][canvas][document]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Envelope, "env", { 10.f, 20.f }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);

    commands.beginCompoundEdit();
    REQUIRE(commands.moveNode("env", { 40.f, 50.f }).succeeded());
    REQUIRE(commands.moveNode("env", { 80.f, 90.f }).succeeded());
    commands.commitCompoundEdit();
    REQUIRE(document.graph().findNode("env")->bounds.getPosition() == Point<float>(80.f, 90.f));

    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("env")->bounds.getPosition() == Point<float>(10.f, 20.f));
    REQUIRE_FALSE(document.undo());
}

TEST_CASE("Canvas and automation command requests share the same dispatcher", "[cycle-v2][canvas][document]") {
    GraphDocument canvasDocument(NodeGraph::createDemoGraph());
    GraphDocument automationDocument(NodeGraph::createDemoGraph());
    GraphCommandDispatcher canvasCommands(canvasDocument);
    GraphCommandDispatcher automationCommands(automationDocument);

    REQUIRE(canvasCommands.setNodeParameter("voice", "voices", "Voices", "4").succeeded());
    REQUIRE(automationCommands.setNodeParameter("voice", "voices", "Voices", "4").succeeded());
    REQUIRE(canvasDocument.toXml() == automationDocument.toXml());
}

TEST_CASE("Graph presentation schedules work from semantic change impacts", "[cycle-v2][canvas][presentation]") {
    const NodeGraph graph = NodeGraph::createDemoGraph();
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(graph, 1, topology));
    const size_t compilationCount = presentation.compilationCount();
    const size_t previewCount = presentation.previewRenderCount();

    GraphChangeSet layout;
    layout.layoutChanged = true;
    REQUIRE(presentation.refresh(graph, 2, layout));
    REQUIRE(presentation.compilationCount() == compilationCount);
    REQUIRE(presentation.previewRenderCount() == previewCount);

    GraphChangeSet preview;
    preview.parameterImpacts = ParameterImpact::Preview;
    REQUIRE(presentation.refresh(graph, 3, preview));
    REQUIRE(presentation.compilationCount() == compilationCount);
    REQUIRE(presentation.previewRenderCount() == previewCount + 1);
}

TEST_CASE("Graph presentation rejects stale revision results", "[cycle-v2][canvas][presentation]") {
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(NodeGraph::createDemoGraph(), 7, topology));

    GraphPresentationSnapshot stale;
    stale.graphRevision = 6;
    REQUIRE_FALSE(presentation.acceptSnapshot(std::move(stale)));
    REQUIRE(presentation.snapshot().graphRevision == 7);
}

TEST_CASE("Graph presentation preserves configuration revision history across recompiles",
        "[cycle-v2][canvas][presentation]") {
    const File defaultGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");
    NodeGraph graph = GraphSerializer().fromXmlString(defaultGraph.loadFileAsString());
    GraphNodeFactory factory;
    graph.replaceNodeParameters("waveshaper",
            factory.createNode(NodeKind::Waveshaper, "defaults", {}).parameters);
    graph.replaceNodeParameters("ir",
            factory.createNode(NodeKind::ImpulseResponse, "defaults", {}).parameters);
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    GraphChangeSet topology;
    topology.topologyChanged = true;
    REQUIRE(presentation.refresh(document.graph(), document.revision(), topology));
    REQUIRE(presentation.compileResult().succeeded());
    const auto initialAudio = presentation.captureAudio(document.graph(), 128).output.block.samples;
    REQUIRE(presentation.captureAudio(document.graph(), 128).output.block.samples == initialAudio);

    const auto revisionFor = [&](const String& nodeId) {
        const auto& steps = presentation.compileResult().plan.steps;
        String stepIds;
        for (const auto& step : steps) {
            stepIds << step.nodeId << " ";
        }
        const auto found = std::find_if(steps.begin(), steps.end(), [&](const auto& step) {
            return step.nodeId == nodeId;
        });
        INFO("configuration revision requested for " << nodeId << "; steps: " << stepIds);
        REQUIRE(found != steps.end());
        return found->configuration.revision;
    };

    const uint64_t initialRevision = revisionFor("waveshaper");
    const uint64_t initialIrRevision = revisionFor("ir");
    const uint64_t initialEnvelopeRevision = revisionFor("env");
    REQUIRE(commands.setNodeParameter("waveshaper", "pre", "Pre", "0.75").succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(revisionFor("waveshaper") == initialRevision + 1);
    REQUIRE(presentation.captureAudio(document.graph(), 128).output.block.samples != initialAudio);

    REQUIRE(commands.setNodeParameter("ir", "post", "Post", "0.75").succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(revisionFor("ir") == initialIrRevision + 1);
    const auto irAudio = presentation.captureAudio(document.graph(), 128).output.block.samples;

    REQUIRE(commands.setNodeParameter("env", "red", "Red", "0.75").succeeded());
    REQUIRE(presentation.refresh(document.graph(), document.revision(), document.lastChange()));
    REQUIRE(revisionFor("env") == initialEnvelopeRevision + 1);
    REQUIRE(presentation.captureAudio(document.graph(), 128).output.block.samples != irAudio);
}

TEST_CASE("Rich node views are selected through the view module registry", "[cycle-v2][canvas][view]") {
    const auto& registry = NodeViewModuleRegistry::instance();
    REQUIRE(registry.moduleFor(NodeKind::Envelope).capabilities().hostedEditor);
    REQUIRE(registry.moduleFor(NodeKind::TrilinearMesh).capabilities().outputSideControl);
    REQUIRE(registry.moduleFor(NodeKind::Add).capabilities().operationLayoutControl);
    REQUIRE_FALSE(registry.moduleFor(NodeKind::Output).capabilities().hostedEditor);
    REQUIRE(registry.moduleFor(NodeKind::Envelope).editorFactory() != nullptr);
    REQUIRE(registry.moduleFor(NodeKind::TrilinearMesh).editorFactory() != nullptr);
    REQUIRE(registry.moduleFor(NodeKind::Output).editorFactory() == nullptr);

    const auto bounds = registry.moduleFor(NodeKind::ImpulseResponse)
            .expandedEditorBounds({ 0.f, 0.f, 1400.f, 800.f }, 18.f);
    REQUIRE(bounds.getWidth() == Catch::Approx(1050.f));
    REQUIRE(bounds.getHeight() == Catch::Approx(470.f));

    const auto meshBounds = registry.moduleFor(NodeKind::TrilinearMesh)
            .expandedEditorBounds({ 0.f, 0.f, 1200.f, 800.f }, 18.f);
    REQUIRE(meshBounds.getWidth() == Catch::Approx(1080.f));
    REQUIRE(meshBounds.getHeight() == Catch::Approx(720.f));
}

TEST_CASE("Automation facade mutates through commands and inspects the shared scene", "[cycle-v2][canvas][automation]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    NodeAutomationFacade automation(document, commands);
    REQUIRE(automation.setParameter("voice", "voices", "Voices", "6").succeeded());
    REQUIRE(document.canUndo());

    NodeCanvasViewport viewport;
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(document.graph(), viewport);
    const auto voice = std::find_if(snapshot.targets.begin(), snapshot.targets.end(), [](const auto& target) {
        return target.semanticId == "node:voice";
    });
    REQUIRE(voice != snapshot.targets.end());
    const auto hit = automation.inspectPointer(snapshot, voice->bounds.getCentre());
    REQUIRE(hit.has_value());
    REQUIRE(hit->semanticId == "node:voice");
}

TEST_CASE("Registered view modules contribute dynamic attachment geometry", "[cycle-v2][canvas][scene]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GuideCurve, "guide", { 40.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 420.f, 80.f }));
    graph.addEdge({ "guide", "guide", "mesh", "guide.vertex.0.red",
            PortDomain::ControlSignal, true });

    NodeCanvasViewport viewport;
    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    REQUIRE(snapshot.edges.size() == 1);
    REQUIRE(snapshot.edges.front().destination.y
            == Catch::Approx(viewport.toScreen(graph.findNode("mesh")->bounds.getTopLeft()).y));
}
