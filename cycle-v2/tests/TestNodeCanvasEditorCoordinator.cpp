#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/NodeCanvasEditorCoordinator.h"

using namespace CycleV2;

TEST_CASE("Expanded editor routing captures hosted editors before canvas gestures",
        "[cycle-v2][canvas][editor-coordinator]") {
    const Node envelope = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});
    const Rectangle<float> canvas(0.f, 0.f, 1200.f, 800.f);

    const ExpandedEditorClick click = NodeCanvasEditorCoordinator::routeClick(
            &envelope,
            canvas,
            { 4.f, 4.f });

    REQUIRE(click.kind == ExpandedEditorClickKind::Captured);
    REQUIRE(NodeCanvasEditorCoordinator::blocksCanvas(&envelope));
}

TEST_CASE("Expanded editor routing distinguishes close chrome from content",
        "[cycle-v2][canvas][editor-coordinator]") {
    const Node transform = GraphNodeFactory().createNode(NodeKind::Fft, "fft", {});
    const Rectangle<float> canvas(0.f, 0.f, 1200.f, 800.f);
    const Rectangle<float> panel = NodeCanvasEditorCoordinator::boundsFor(&transform, canvas);
    const Point<float> close { panel.getRight() - 18.f, panel.getY() + 15.f };

    REQUIRE(NodeCanvasEditorCoordinator::routeClick(&transform, canvas, close).kind
            == ExpandedEditorClickKind::Close);
    REQUIRE(NodeCanvasEditorCoordinator::routeClick(&transform, canvas, { 2.f, 2.f }).kind
            == ExpandedEditorClickKind::Unclaimed);
    REQUIRE_FALSE(NodeCanvasEditorCoordinator::blocksCanvas(&transform));
}
