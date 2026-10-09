#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphSerializer.h"
#include "UI/NodeCanvasInteraction.h"
#include "UI/NodeCanvasScene.h"

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
