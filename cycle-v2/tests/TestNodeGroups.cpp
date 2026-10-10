#include <algorithm>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "UI/GraphEndpointNodeRenderer.h"
#include "UI/NodeCanvasInteraction.h"
#include "UI/NodeCanvasScene.h"
#include "UI/NodeGroupPresentation.h"
#include "UI/SignalProbeCanvas.h"

using namespace CycleV2;

namespace {

bool hasNodeTarget(const NodeCanvasSceneSnapshot& scene, const String& nodeId) {
    return std::any_of(scene.targets.begin(), scene.targets.end(),
            [&](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::Node
                        && target.nodeId == nodeId;
            });
}

}

TEST_CASE("Node groups persist and undo without changing graph connectivity",
        "[cycle-v2][graph][groups]") {
    GraphDocument document(NodeGraph::createDemoGraph());
    GraphCommandDispatcher commands(document);
    const auto& nodes = document.graph().getNodes();
    REQUIRE(nodes.size() >= 2);
    const std::vector<String> memberIds { nodes[0].id, nodes[1].id };
    const size_t edgeCount = document.graph().getEdges().size();

    const GraphEditResult created = commands.groupNodes(memberIds, "Effects");
    REQUIRE(created.succeeded());
    const String groupId = created.nodeId;
    REQUIRE(document.graph().findNodeGroup(groupId) != nullptr);
    REQUIRE(document.graph().getEdges().size() == edgeCount);

    REQUIRE(commands.renameNodeGroup(groupId, "Global FX").succeeded());
    REQUIRE(document.graph().findNodeGroup(groupId)->title == "Global FX");

    REQUIRE(commands.setNodeGroupCollapsed(groupId, true).succeeded());
    REQUIRE(document.graph().findNodeGroup(groupId)->collapsed);
    const GraphLoadResult loaded = GraphSerializer().readJSON(
            GraphSerializer().writeJSON(document.graph()));
    REQUIRE(loaded.succeeded());
    REQUIRE(loaded.graph.findNodeGroup(groupId) != nullptr);
    REQUIRE(loaded.graph.findNodeGroup(groupId)->collapsed);
    REQUIRE(loaded.graph.getEdges().size() == edgeCount);

    REQUIRE(document.undo());
    REQUIRE_FALSE(document.graph().findNodeGroup(groupId)->collapsed);
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNodeGroup(groupId)->title == "Effects");
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNodeGroup(groupId) == nullptr);
    REQUIRE(document.redo());
    REQUIRE(document.graph().findNodeGroup(groupId) != nullptr);
    REQUIRE(document.redo());
    REQUIRE(document.graph().findNodeGroup(groupId)->title == "Global FX");
    REQUIRE(document.redo());
    REQUIRE(document.graph().findNodeGroup(groupId)->collapsed);
    REQUIRE(commands.ungroupNodes(groupId).succeeded());
    REQUIRE(document.graph().findNodeGroup(groupId) == nullptr);
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNodeGroup(groupId)->collapsed);
}

TEST_CASE("Collapsed groups hide member targets and internal cables",
        "[cycle-v2][ui][groups]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node first = factory.createNode(NodeKind::WaveSource, "first", { 0.f, 0.f });
    Node second = factory.createNode(NodeKind::Multiply, "second", { 260.f, 0.f });
    Node outside = factory.createNode(NodeKind::Output, "outside", { 530.f, 0.f });
    REQUIRE_FALSE(first.outputs.empty());
    REQUIRE_FALSE(second.inputs.empty());
    REQUIRE_FALSE(second.outputs.empty());
    REQUIRE_FALSE(outside.inputs.empty());
    graph.addNode(first);
    graph.addNode(second);
    graph.addNode(outside);
    graph.addEdge({ first.id, first.outputs.front().id,
            second.id, second.inputs.front().id, first.outputs.front().domain });
    graph.addEdge({ second.id, second.outputs.front().id,
            outside.id, outside.inputs.front().id, second.outputs.front().domain });
    REQUIRE(graph.addNodeGroup({ "group", "Source", { first.id, second.id }, false }));

    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1000.f, 600.f });
    viewport.setTransform({}, 1.f);
    NodeCanvasScene scene;
    const auto& expanded = scene.build(graph, viewport);
    REQUIRE(hasNodeTarget(expanded, first.id));
    REQUIRE(hasNodeTarget(expanded, second.id));
    REQUIRE(expanded.edges.size() == 2);

    NodeGroup collapsed = *graph.findNodeGroup("group");
    collapsed.collapsed = true;
    REQUIRE(graph.replaceNodeGroup(collapsed));
    const auto& compact = scene.build(graph, viewport);
    REQUIRE_FALSE(hasNodeTarget(compact, first.id));
    REQUIRE_FALSE(hasNodeTarget(compact, second.id));
    REQUIRE(hasNodeTarget(compact, outside.id));
    REQUIRE(compact.edges.size() == 1);
    REQUIRE(compact.edges.front().edgeIndex == 1);
    const auto selected = NodeCanvasInteraction().nodeIdsIntersecting(
            graph, viewport, { -100.f, -100.f, 1000.f, 500.f },
            GraphEdgeIndex(graph.getEdges()));
    REQUIRE(std::find(selected.begin(), selected.end(), first.id) == selected.end());
    REQUIRE(std::find(selected.begin(), selected.end(), second.id) == selected.end());
    REQUIRE(std::find(selected.begin(), selected.end(), outside.id) != selected.end());
    REQUIRE(std::any_of(compact.targets.begin(), compact.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::NodeGroupToggle;
            }));
}

TEST_CASE("Collapsed group geometry follows an inline pan on its cable",
        "[cycle-v2][ui][groups][pan]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 0.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::SpectralLayer, "pan", { 2000.f, 800.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 520.f, 80.f }));
    graph.addEdge({
            "mesh", "out", "pan", "in",
            PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({
            "pan", "out", "ifft", "mag",
            PortDomain::ControlSignal, ConnectionKind::Signal });
    REQUIRE(graph.addNodeGroup({ "group", "Timbre", { "mesh", "pan" }, true }));
    const GraphEdgeIndex edgeIndex(graph.getEdges());
    const NodeGroup& group = *graph.findNodeGroup("group");
    const auto panBounds = NodeCanvasScene::presentationWorldBounds(
            graph, *graph.findNode("pan"), edgeIndex);
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1000.f, 600.f });
    viewport.setTransform({}, 1.f);
    const auto groupBounds = NodeGroupPresentation::worldBounds(
            graph, group, viewport, edgeIndex);

    REQUIRE(panBounds.getCentreX() < 520.f);
    REQUIRE(groupBounds.getRight() < 1000.f);
    REQUIRE(groupBounds.getBottom() < 500.f);

    NodeCanvasScene scene;
    const auto& snapshot = scene.build(graph, viewport);
    REQUIRE(snapshot.edges.size() == 1);
    REQUIRE(snapshot.edges.front().source.x == Catch::Approx(groupBounds.getRight()));
}

TEST_CASE("Expanded group header clears endpoint nodes and their captions",
        "[cycle-v2][ui][groups]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "input", { 200.f, 200.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 700.f, 200.f }));
    REQUIRE(graph.addNodeGroup({ "group", "Global FX", { "input", "output" }, false }));
    const GraphEdgeIndex edgeIndex(graph.getEdges());
    const NodeGroup& group = *graph.findNodeGroup("group");

    for (const float zoom : { 1.f, 0.4f, 0.18f }) {
        NodeCanvasViewport viewport;
        viewport.setBounds({ 0.f, 0.f, 1200.f, 800.f });
        viewport.setTransform({}, zoom);
        const auto frame = viewport.toScreen(NodeGroupPresentation::worldBounds(
                graph, group, viewport, edgeIndex));
        const auto toggle = NodeGroupPresentation::toggleBounds(
                graph, group, viewport, edgeIndex);
        const Node& input = *graph.findNode("input");
        const auto inputVisual = GraphEndpointNodeRenderer::visualBounds(
                input.kind, viewport.toScreen(input.bounds),
                viewport.getBounds(), viewport.getZoom());
        const auto outputVisual = viewport.toScreen(graph.findNode("output")->bounds);

        REQUIRE(frame.contains(inputVisual));
        REQUIRE(frame.contains(outputVisual));
        REQUIRE(toggle.getBottom() + 6.f <= juce::jmin(
                inputVisual.getY(), outputVisual.getY()));
    }
}

TEST_CASE("Group toggles stay anchored while their Spy cards collapse",
        "[cycle-v2][ui][groups][spy]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "input", { 100.f, 150.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 450.f, 150.f }));
    graph.addEdge({ "input", "time", "output", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addSignalProbe({ "spy", "input", "time", "output", "time",
            "Spy", 0.5f, 0, juce::Point<float>(230.f, 400.f) });
    REQUIRE(graph.addNodeGroup({ "group", "Global FX", { "input", "output" }, false }));
    SignalProbeCanvasState probeState;
    probeState.outputCanvasPosition = juce::Point<float>(550.f, 400.f);
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1100.f, 700.f });
    viewport.setTransform({}, 0.8f);
    NodeCanvasScene scene;
    const auto& expanded = scene.build(graph, viewport);
    const GraphEdgeIndex edgeIndex(graph.getEdges());
    const auto before = NodeGroupPresentation::toggleBounds(
            graph, *graph.findNodeGroup("group"), viewport, edgeIndex);
    REQUIRE_FALSE(SignalProbeCanvas::cardBoundsFor(
            "spy", graph, expanded, viewport, probeState).isEmpty());
    REQUIRE_FALSE(SignalProbeCanvas::cardBoundsFor(
            DefaultOutputProbeResolver::probeId,
            graph, expanded, viewport, probeState).isEmpty());

    NodeGroup collapsed = *graph.findNodeGroup("group");
    collapsed.collapsed = true;
    REQUIRE(graph.replaceNodeGroup(collapsed));
    const auto& compact = scene.build(graph, viewport);
    const auto after = NodeGroupPresentation::toggleBounds(
            graph, collapsed, viewport, edgeIndex);
    REQUIRE(after.getCentreX() == Catch::Approx(before.getCentreX()));
    REQUIRE(after.getCentreY() == Catch::Approx(before.getCentreY()));
    REQUIRE(SignalProbeCanvas::cardBoundsFor(
            "spy", graph, compact, viewport, probeState).isEmpty());
    REQUIRE(SignalProbeCanvas::cardBoundsFor(
            DefaultOutputProbeResolver::probeId,
            graph, compact, viewport, probeState).isEmpty());
    REQUIRE(std::any_of(compact.targets.begin(), compact.targets.end(),
            [](const NodeSceneTarget& target) {
                return target.kind == NodeSceneTargetKind::NodeGroupGrabber;
            }));
}

TEST_CASE("Group grabber moves members through one undoable gesture",
        "[cycle-v2][ui][groups][gesture]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Add, "first", { 100.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "second", { 300.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "outside", { 900.f, 100.f }));
    REQUIRE(graph.addNodeGroup({ "group", "Pair", { "first", "second" }, true }));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    NodeCanvasInteraction interaction;
    NodeCanvasViewport viewport;
    viewport.setTransform({}, 1.f);
    NodeCanvasScene scene;
    interaction.beginNodeDrag(document.graph(), "first", { "first", "second" },
            document.graph().findNode("first")->bounds, false);
    REQUIRE(interaction.gestureValidationContext() == nullptr);
    commands.beginCompoundEdit();
    for (float offset : { 20.f, 50.f }) {
        const auto update = interaction.drag(
                document.graph(), viewport, scene.build(document.graph(), viewport),
                { 100.f + offset, 100.f }, { offset, 0.f });
        const auto& movement = std::get<NodeDragUpdate>(update);
        const auto current = commands.editingGraph().findNode("first")->bounds;
        REQUIRE(commands.translateNodes(movement.nodeIds,
                movement.bounds.getPosition() - current.getPosition()).succeeded());
    }
    REQUIRE(commands.commitCompoundEdit());
    REQUIRE(document.graph().findNode("first")->bounds.getX() == Catch::Approx(150.f));
    REQUIRE(document.graph().findNode("second")->bounds.getX() == Catch::Approx(350.f));
    REQUIRE(document.graph().findNode("outside")->bounds.getX() == Catch::Approx(900.f));
    REQUIRE(document.undo());
    REQUIRE(document.graph().findNode("first")->bounds.getX() == Catch::Approx(100.f));
    REQUIRE(document.graph().findNode("second")->bounds.getX() == Catch::Approx(300.f));
}

TEST_CASE("Deleting a group member clears the remaining membership",
        "[cycle-v2][graph][groups]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Add, "first", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "second", { 200.f, 0.f }));
    REQUIRE(graph.addNodeGroup({ "group", "Pair", { "first", "second" }, true }));

    graph.removeNode("first");

    REQUIRE(graph.findNodeGroup("group") == nullptr);
    REQUIRE(graph.groupForNode("second") == nullptr);
    REQUIRE(graph.isNodeVisible("second"));
}

TEST_CASE("Group deltas restore membership after a member is restored",
        "[cycle-v2][graph][groups]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::Add, "first", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "second", { 200.f, 0.f }));
    Node third = factory.createNode(NodeKind::Add, "third", { 400.f, 0.f });
    graph.addNode(third);
    REQUIRE(graph.addNodeGroup({ "group", "Triple", { "first", "second", "third" }, true }));
    GraphDeltaBuilder delta;
    delta.captureNodeGroup(graph, "group");

    graph.removeNode("third");
    REQUIRE(graph.findNodeGroup("group")->nodeIds.size() == 2);
    graph.addNode(third);
    delta.restore(graph);

    REQUIRE(graph.findNodeGroup("group")->nodeIds.size() == 3);
    REQUIRE(graph.groupForNode("third") != nullptr);
    REQUIRE_FALSE(graph.isNodeVisible("third"));
}
