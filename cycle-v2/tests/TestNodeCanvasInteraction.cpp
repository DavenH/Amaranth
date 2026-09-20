#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphNodeFactory.h"
#include "UI/NodeCanvasInteraction.h"

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

    interaction.beginConnection(graph, source, { 100.f, 100.f });
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

TEST_CASE("Node canvas viewport fits graph bounds inside dock-safe screen bounds",
        "[cycle-v2][ui][viewport][global-audio]") {
    NodeCanvasViewport viewport;
    const Rectangle<float> graphBounds { -200.f, 100.f, 1600.f, 900.f };
    const Rectangle<float> available { 40.f, 40.f, 860.f, 560.f };

    viewport.fit(graphBounds, available);

    const Rectangle<float> fitted = viewport.toScreen(graphBounds);
    REQUIRE(fitted.getX() == Catch::Approx(available.getX()).margin(0.001f));
    REQUIRE(fitted.getY() >= available.getY() - 0.001f);
    REQUIRE(fitted.getRight() <= available.getRight() + 0.001f);
    REQUIRE(fitted.getBottom() <= available.getBottom() + 0.001f);
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
    interaction.beginNodeDrag(graph, node.id, { node.id, "peer" }, node.bounds);

    auto first = interaction.drag(graph, viewport, {}, {}, { 10.f, 5.f });
    const auto* firstDrag = std::get_if<NodeDragUpdate>(&first);
    REQUIRE(firstDrag != nullptr);
    REQUIRE(firstDrag->beginTransaction);
    REQUIRE(firstDrag->moved);
    REQUIRE(firstDrag->nodeIds == std::vector<String> { node.id, "peer" });
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
    REQUIRE(nodeCompletion->nodeIds == std::vector<String> { node.id, "peer" });
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

TEST_CASE("Node canvas interaction owns inline and probe gesture state",
        "[cycle-v2][ui][interaction]") {
    NodeCanvasInteraction interaction;

    interaction.beginSpectralPan("spectral", 0.25f);
    REQUIRE(interaction.spectralPan() != nullptr);
    REQUIRE(interaction.spectralPan()->nodeId == "spectral");
    REQUIRE(interaction.spectralPan()->startValue == Catch::Approx(0.25f));

    interaction.beginOutputGain("output", 0.75f);
    REQUIRE(interaction.spectralPan() == nullptr);
    REQUIRE(interaction.outputGain() != nullptr);
    REQUIRE(interaction.outputGain()->nodeId == "output");
    REQUIRE(interaction.outputGain()->startValue == Catch::Approx(0.75f));

    interaction.beginProbeDrag("probe");
    REQUIRE(interaction.outputGain() == nullptr);
    REQUIRE(interaction.probeDrag() != nullptr);
    REQUIRE(interaction.probeDrag()->probeId == "probe");

    interaction.reset();
    REQUIRE(interaction.isIdle());
    REQUIRE(interaction.probeDrag() == nullptr);
}

TEST_CASE("Node canvas interaction keeps one screen-space area selection rectangle",
        "[cycle-v2][ui][interaction][selection]") {
    NodeCanvasInteraction interaction;
    interaction.beginAreaSelection({ 80.f, 70.f });

    const auto first = interaction.drag({}, {}, {}, { 40.f, 120.f }, { -40.f, 50.f });
    const auto* firstSelection = std::get_if<AreaSelectionDragUpdate>(&first);
    REQUIRE(firstSelection != nullptr);
    REQUIRE(firstSelection->moved);
    REQUIRE(firstSelection->bounds == Rectangle<float>(40.f, 70.f, 40.f, 50.f));

    const auto second = interaction.drag({}, {}, {}, { 120.f, 30.f }, { 40.f, -40.f });
    const auto* secondSelection = std::get_if<AreaSelectionDragUpdate>(&second);
    REQUIRE(secondSelection != nullptr);
    REQUIRE(secondSelection->bounds == Rectangle<float>(80.f, 30.f, 40.f, 40.f));

    const auto completion = interaction.finish({}, {}, { 120.f, 30.f });
    const auto* finished = std::get_if<AreaSelectionCompletion>(&completion);
    REQUIRE(finished != nullptr);
    REQUIRE(finished->moved);
    REQUIRE(finished->bounds == secondSelection->bounds);
    REQUIRE(interaction.isIdle());

    interaction.beginAreaSelection({ 20.f, 20.f });
    const auto tiny = interaction.drag({}, {}, {}, { 22.f, 21.f }, { 2.f, 1.f });
    REQUIRE_FALSE(std::get<AreaSelectionDragUpdate>(tiny).moved);
    REQUIRE_FALSE(std::get<AreaSelectionCompletion>(
            interaction.finish({}, {}, { 22.f, 21.f })).moved);

    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "inside", { 50.f, 60.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "outside", { 500.f, 600.f }));
    NodeCanvasViewport viewport;
    viewport.setTransform({ 10.f, 20.f }, 0.5f);
    const GraphEdgeIndex edgeIndex(graph.getEdges());
    const Rectangle<float> insideBounds = viewport.toScreen(
            NodeCanvasScene::presentationWorldBounds(
                    graph,
                    *graph.findNode("inside"),
                    edgeIndex));
    REQUIRE(interaction.nodeIdsIntersecting(
            graph,
            viewport,
            insideBounds.reduced(2.f),
            edgeIndex)
            == std::vector<String> { "inside" });
}
