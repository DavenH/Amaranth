#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/NodeCanvasPresentation.h"

using namespace CycleV2;

TEST_CASE("Node canvas presentation shares port centres with the scene model",
        "[cycle-v2][canvas][presentation]") {
    Node node = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", { 120.f, 80.f });
    NodeCanvasViewport viewport;
    viewport.setTransform({ 31.f, 47.f }, 0.73f);

    for (const auto& port : node.inputs) {
        const auto presentation = NodeCanvasPresentation::portPresentation(viewport, node, port);
        const Point<float> expected = viewport.toScreen(NodeCanvasScene::portWorldCentre(node, port));

        REQUIRE(presentation.centre.x == Catch::Approx(expected.x));
        REQUIRE(presentation.centre.y == Catch::Approx(expected.y));
        REQUIRE(presentation.bounds.getCentreX() == Catch::Approx(expected.x));
        REQUIRE(presentation.bounds.getCentreY() == Catch::Approx(expected.y));
    }
}

TEST_CASE("Node canvas presentation scales port hit geometry with canvas zoom",
        "[cycle-v2][canvas][presentation]") {
    Node node = GraphNodeFactory().createNode(NodeKind::WaveSource, "wave", { 100.f, 90.f });
    REQUIRE_FALSE(node.outputs.empty());

    NodeCanvasViewport viewport;
    viewport.setTransform({}, 0.58f);
    const auto reference = NodeCanvasPresentation::portPresentation(viewport, node, node.outputs.front());

    REQUIRE(reference.bounds.getWidth() == Catch::Approx(8.4f));

    viewport.setTransform({}, 1.16f);
    const auto doubled = NodeCanvasPresentation::portPresentation(viewport, node, node.outputs.front());

    REQUIRE(doubled.bounds.getWidth() == Catch::Approx(reference.bounds.getWidth() * 2.f));
    REQUIRE(doubled.bounds.getHeight() == Catch::Approx(reference.bounds.getHeight() * 2.f));
}

TEST_CASE("Signal and attachment sockets share one presentation diameter",
        "[cycle-v2][canvas][presentation][ports]") {
    const Node voice = GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {});
    NodeCanvasViewport viewport;
    viewport.setTransform({}, 0.58f);

    const auto modulation = NodeCanvasPresentation::portPresentation(
            viewport,
            voice,
            voice.inputs[0]);
    const auto pitch = NodeCanvasPresentation::portPresentation(
            viewport,
            voice,
            voice.inputs[1]);
    const auto unison = NodeCanvasPresentation::portPresentation(
            viewport,
            voice,
            voice.inputs[2]);

    REQUIRE(modulation.bounds.getWidth() == Catch::Approx(8.4f));
    REQUIRE(pitch.bounds.getWidth() == Catch::Approx(8.4f));
    REQUIRE(unison.bounds.getWidth() == Catch::Approx(8.4f));
}

TEST_CASE("Guide and Spy shelves divide the dock and retain scroll room",
        "[cycle-v2][canvas][presentation][guide-dock]") {
    const Rectangle<float> workspace(0.f, 0.f, 1000.f, 700.f);
    SignalProbeRailState dockState;
    GuideCurveShelfState guideState;
    const Rectangle<float> guides = GuideCurveShelf::guideWorkspace(workspace, 0.5f);
    const Rectangle<float> spies = GuideCurveShelf::spyWorkspace(workspace, 0.5f);

    REQUIRE(guides.getWidth() == Catch::Approx(500.f));
    REQUIRE(spies.getWidth() == Catch::Approx(500.f));
    REQUIRE(guides.getRight() == Catch::Approx(spies.getX()));
    REQUIRE(GuideCurveShelf::maximumHorizontalOffset(
            workspace,
            dockState,
            0.5f,
            guideState,
            1) == Catch::Approx(0.f));
    REQUIRE(GuideCurveShelf::maximumHorizontalOffset(
            workspace,
            dockState,
            0.5f,
            guideState,
            8) > 0.f);

    guideState.minimized = true;
    REQUIRE(GuideCurveShelf::boundsFor(workspace, dockState, 0.5f, guideState).getWidth()
            == Catch::Approx(GuideCurveShelf::minimizedWidth));
}
