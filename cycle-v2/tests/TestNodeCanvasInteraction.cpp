#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/NodeCanvasInteraction.h"

using namespace CycleV2;

namespace {

NodeSceneTarget portTarget(
        const PortAddress& address,
        Point<float> centre,
        int zOrder = 1) {
    NodeSceneTarget target;
    target.kind = address.input
            ? NodeSceneTargetKind::InputPort
            : NodeSceneTargetKind::OutputPort;
    target.nodeId = address.nodeId;
    target.portId = address.portId;
    target.bounds = Rectangle<float>(10.f, 10.f).withCentre(centre);
    target.zOrder = zOrder;
    return target;
}

bool sameAddress(const PortAddress& left, const PortAddress& right) {
    return left.nodeId == right.nodeId
            && left.portId == right.portId
            && left.input == right.input;
}

}

TEST_CASE("Node canvas interaction resolves compatible connection targets by proximity",
        "[cycle-v2][ui][interaction]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 300.f, 0.f }));

    const PortAddress source { "wave", "out", false };
    const PortAddress target { "output", "time", true };
    NodeCanvasSceneSnapshot scene;
    scene.targets.push_back(portTarget(source, { 100.f, 100.f }));
    scene.targets.push_back(portTarget(target, { 200.f, 100.f }));

    NodeCanvasInteraction interaction;
    const auto port = interaction.portAt(scene, { 100.f, 100.f });
    REQUIRE(port.has_value());
    REQUIRE(sameAddress(*port, source));
    const auto resolvedTarget = interaction.connectionTargetAt(
            graph,
            scene,
            source,
            { 196.f, 101.f });
    REQUIRE(resolvedTarget.has_value());
    REQUIRE(sameAddress(*resolvedTarget, target));
    REQUIRE_FALSE(interaction.connectionTargetAt(graph, scene, source, { 100.f, 100.f }).has_value());

    interaction.beginConnection(source, { 100.f, 100.f });
    const auto update = interaction.drag(
            graph,
            {},
            scene,
            { 196.f, 101.f },
            {});
    const auto* connection = std::get_if<ConnectionDragUpdate>(&update);
    REQUIRE(connection != nullptr);
    REQUIRE(connection->target.has_value());
    REQUIRE(sameAddress(*connection->target, target));
    REQUIRE(connection->endpoint == Point<float>(200.f, 100.f));

    const auto completion = interaction.finish(graph, scene, { 196.f, 101.f });
    const auto* finished = std::get_if<ConnectionCompletion>(&completion);
    REQUIRE(finished != nullptr);
    REQUIRE(sameAddress(finished->source, source));
    REQUIRE(finished->target.has_value());
    REQUIRE(sameAddress(*finished->target, target));
    REQUIRE(interaction.isIdle());
}

TEST_CASE("Node canvas interaction snaps dragged ports and exposes guide positions",
        "[cycle-v2][ui][interaction]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node dragged = factory.createNode(NodeKind::Add, "dragged", { 0.f, 0.f });
    Node stationary = factory.createNode(NodeKind::Multiply, "stationary", { 400.f, 240.f });
    graph.addNode(dragged);
    graph.addNode(stationary);

    const Point<float> stationaryPort = NodeCanvasScene::portWorldCentre(
            stationary,
            stationary.inputs.front());
    Rectangle<float> proposed = dragged.bounds.translated(400.f, 240.f);
    Node proposedNode = dragged;
    proposedNode.bounds = proposed;
    const Point<float> proposedPort = NodeCanvasScene::portWorldCentre(
            proposedNode,
            proposedNode.inputs.front());
    proposed = proposed.translated(
            stationaryPort.x - proposedPort.x + 5.f,
            stationaryPort.y - proposedPort.y + 4.f);

    NodeCanvasInteraction interaction;
    const auto snapped = interaction.snapNode(graph, dragged, proposed);
    Node snappedNode = dragged;
    snappedNode.bounds = snapped.bounds;
    const Point<float> snappedPort = NodeCanvasScene::portWorldCentre(
            snappedNode,
            snappedNode.inputs.front());

    REQUIRE(snapped.guides.x.has_value());
    REQUIRE(snapped.guides.y.has_value());
    REQUIRE(snappedPort.x == Catch::Approx(stationaryPort.x));
    REQUIRE(snappedPort.y == Catch::Approx(stationaryPort.y));
}

TEST_CASE("Node canvas interaction models node drag transaction and completion states",
        "[cycle-v2][ui][interaction]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    Node node = factory.createNode(NodeKind::WaveSource, "wave", { 20.f, 30.f });
    graph.addNode(node);

    NodeCanvasViewport viewport;
    viewport.setTransform({}, 0.5f);
    NodeCanvasInteraction interaction;
    interaction.beginNodeDrag(node.id, node.bounds);

    auto first = interaction.drag(graph, viewport, {}, {}, { 10.f, 5.f });
    const auto* firstDrag = std::get_if<NodeDragUpdate>(&first);
    REQUIRE(firstDrag != nullptr);
    REQUIRE(firstDrag->beginTransaction);
    REQUIRE(firstDrag->moved);
    REQUIRE(firstDrag->bounds.getX() == Catch::Approx(node.bounds.getX() + 20.f));
    REQUIRE(firstDrag->bounds.getY() == Catch::Approx(node.bounds.getY() + 10.f));

    auto second = interaction.drag(graph, viewport, {}, {}, { 12.f, 6.f });
    const auto* secondDrag = std::get_if<NodeDragUpdate>(&second);
    REQUIRE(secondDrag != nullptr);
    REQUIRE_FALSE(secondDrag->beginTransaction);

    const auto completion = interaction.finish(graph, {}, {});
    const auto* nodeCompletion = std::get_if<NodeDragCompletion>(&completion);
    REQUIRE(nodeCompletion != nullptr);
    REQUIRE(nodeCompletion->nodeId == node.id);
    REQUIRE(nodeCompletion->moved);
    REQUIRE(interaction.isIdle());
}

TEST_CASE("Node canvas interaction distinguishes pan and expanded editor capture",
        "[cycle-v2][ui][interaction]") {
    NodeCanvasInteraction interaction;
    interaction.beginPan({ 30.f, 40.f });

    const auto update = interaction.drag({}, {}, {}, {}, { 5.f, -3.f });
    const auto* pan = std::get_if<PanDragUpdate>(&update);
    REQUIRE(pan != nullptr);
    REQUIRE(pan->pan == Point<float>(35.f, 37.f));

    interaction.captureExpandedEditor();
    REQUIRE(std::holds_alternative<ExpandedEditorGesture>(interaction.gesture()));
    REQUIRE(std::holds_alternative<std::monostate>(
            interaction.drag({}, {}, {}, {}, {})));
}
