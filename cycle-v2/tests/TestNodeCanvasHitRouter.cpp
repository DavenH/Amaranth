#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/NodeCanvasHitRouter.h"

using namespace CycleV2;

TEST_CASE("Node canvas hit routing preserves action edge and palette placement semantics",
        "[cycle-v2][canvas][hit-router]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 40.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", { 40.f, 520.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "output", { 500.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 620.f, 520.f }));
    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 500.f, 300.f }));
    graph.addEdge({ "wave", "out", "output", "time", PortDomain::TimeSignal, false });

    const auto compileResult = GraphCompiler().compile(graph);
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
    NodeCanvasQueryModel queries(graph, compileResult, runtimeTrace, previewResult);
    NodePalette palette;
    NodeCanvasHitRouter router(graph, palette, queries);
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 900.f, 700.f });
    viewport.setTransform({}, 1.f);

    const Node* multiply = graph.findNode("multiply");
    REQUIRE(multiply != nullptr);
    const Point<float> actionPoint {
            multiply->bounds.getRight() - 21.f,
            multiply->bounds.getY() + 21.f
    };
    const auto action = router.nodeActionAt(viewport, actionPoint);
    REQUIRE(action.has_value());
    REQUIRE(action->kind == CanvasNodeActionKind::CycleOperationLayout);
    REQUIRE(action->nodeId == "multiply");
    REQUIRE(router.hoverTextFor(viewport, {}, actionPoint).contains("port layout"));

    NodeCanvasScene sceneBuilder;
    const auto& scene = sceneBuilder.build(graph, viewport, 1, 1);
    REQUIRE(scene.edges.size() == 1);
    const auto& sceneEdge = scene.edges.front();
    const Point<float> edgePoint = sceneEdge.cablePath.getPointAlongPath(
            sceneEdge.cablePath.getLength() * 0.5f);
    REQUIRE(router.edgeAt(scene, edgePoint) == 0);
    REQUIRE(router.spliceTargetEdgeAt(scene, edgePoint, "delay") == 0);
    REQUIRE(router.spliceTargetEdgeAt(scene, edgePoint, "wave") == -1);
    REQUIRE(router.hoverTextFor(viewport, scene, edgePoint).contains("Signal edge"));

    const Point<float> paletteClick { 80.f, 420.f };
    const Point<float> creationPosition = router.paletteCreationWorldPosition(
            viewport,
            { 0.f, 0.f, 900.f, 700.f },
            NodeKind::Multiply,
            paletteClick);
    const Node created = factory.createNode(NodeKind::Multiply, "created", creationPosition);
    REQUIRE(!created.inputs.empty());
    REQUIRE(NodeCanvasScene::portWorldCentre(created, created.inputs.front()).y
            == Catch::Approx(paletteClick.y));

    const Rectangle<float> dragBounds = router.paletteDragBounds(
            viewport,
            created,
            { 300.f, 500.f });
    Node dragged = created;
    dragged.bounds = dragBounds;
    REQUIRE(NodeCanvasScene::portWorldCentre(dragged, dragged.inputs.front()).y
            == Catch::Approx(500.f));
}
