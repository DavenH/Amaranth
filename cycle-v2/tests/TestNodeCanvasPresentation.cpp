#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/ModulationCableBundle.h"
#include "../src/UI/NodeCanvasPresentation.h"
#include "../src/UI/NodePortGeometry.h"
#include "../src/UI/NodePortIconRenderer.h"
#include "../src/UI/NodePortVisualResolver.h"
#include "../src/UI/NodePreviewRenderer.h"

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

TEST_CASE("Port visual semantics resolve from typed graph metadata",
        "[cycle-v2][canvas][presentation][ports][icons]") {
    const GraphNodeFactory factory;
    const Node voice = factory.createNode(NodeKind::VoiceContext, "voice", {});
    const Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});

    REQUIRE(NodePortVisualResolver::semanticFor(voice.inputs[0])
            == PortVisualSemantic::ModulationYrb);
    REQUIRE(NodePortVisualResolver::semanticFor(voice.inputs[1])
            == PortVisualSemantic::PitchEnvelope);
    REQUIRE(NodePortVisualResolver::semanticFor(voice.inputs[2])
            == PortVisualSemantic::UnisonConfiguration);
    REQUIRE(NodePortVisualResolver::semanticFor(voice.outputs[0])
            == PortVisualSemantic::None);
    REQUIRE(NodePortVisualResolver::semanticFor(mesh.inputs[1])
            == PortVisualSemantic::ScratchAttachment);
    REQUIRE(NodePortVisualResolver::modulationSemantic(false)
            == PortVisualSemantic::ModulationRb);
}

TEST_CASE("Every semantic port icon is parseable",
        "[cycle-v2][canvas][presentation][ports][icons]") {
    const PortVisualSemantic semantics[] {
            PortVisualSemantic::ModulationYrb,
            PortVisualSemantic::ModulationRb,
            PortVisualSemantic::PitchEnvelope,
            PortVisualSemantic::UnisonConfiguration,
            PortVisualSemantic::VoiceContext,
            PortVisualSemantic::ScratchAttachment
    };

    for (const PortVisualSemantic semantic : semantics) {
        REQUIRE(NodePortIconRenderer::hasIcon(semantic));
    }
    REQUIRE_FALSE(NodePortIconRenderer::hasIcon(PortVisualSemantic::None));
}

TEST_CASE("Only primary processing domains retain port colour",
        "[cycle-v2][canvas][presentation][ports][colour]") {
    const Colour neutral = colourForDomain(PortDomain::ControlSignal);

    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::TimeSignal)
            == colourForDomain(PortDomain::TimeSignal));
    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::SpectralMagnitudeSignal)
            == colourForDomain(PortDomain::SpectralMagnitudeSignal));
    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::SpectralPhaseSignal)
            == colourForDomain(PortDomain::SpectralPhaseSignal));
    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::DomainContext) == neutral);
    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::EnvelopeSignal) == neutral);
    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::PitchSignal) == neutral);
    REQUIRE(NodePortVisualResolver::colourFor(PortDomain::VoiceControlSignal) == neutral);
}

TEST_CASE("Input sockets precede icons attached to the node edge",
        "[cycle-v2][canvas][presentation][ports][layout]") {
    const Node voice = GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {});
    NodeCanvasViewport viewport;
    viewport.setTransform({}, NodePortGeometry::referenceZoom);

    const auto input = NodeCanvasPresentation::portPresentation(
            viewport,
            voice,
            voice.inputs[1]);
    const Rectangle<float> nodeBounds = viewport.toScreen(voice.bounds);
    REQUIRE(input.bounds.getRight() < input.iconBounds.getX());
    REQUIRE(input.iconBounds.getRight() == Catch::Approx(nodeBounds.getX()));
    REQUIRE(input.iconBounds.getWidth() == Catch::Approx(NodePortGeometry::iconSize));

    const auto nextInput = NodeCanvasPresentation::portPresentation(
            viewport,
            voice,
            voice.inputs[2]);
    const float verticalSpacing = nextInput.centre.y - input.centre.y;
    REQUIRE(verticalSpacing == Catch::Approx(
            NodePortGeometry::sidePortSpacing * NodePortGeometry::referenceZoom));
    REQUIRE(NodePortGeometry::sidePortSpacing >= 44.f * 1.2f);
    REQUIRE(nextInput.iconBounds.getBottom() < nodeBounds.getBottom());

    viewport.setTransform({}, NodePortGeometry::referenceZoom * 0.5f);
    const auto reduced = NodeCanvasPresentation::portPresentation(
            viewport,
            voice,
            voice.inputs[1]);
    REQUIRE(reduced.iconBounds.getWidth() == Catch::Approx(NodePortGeometry::iconSize * 0.5f));
    REQUIRE(reduced.bounds.getRight() < reduced.iconBounds.getX());
    REQUIRE(reduced.iconBounds.getRight()
            == Catch::Approx(viewport.toScreen(voice.bounds).getX()));
}

TEST_CASE("Bundled modulation uses the same side-port row grid",
        "[cycle-v2][canvas][presentation][ports][layout]") {
    const Node mesh = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    const Point<float> context = NodeCanvasScene::portWorldCentre(mesh, mesh.inputs[0]);
    const Point<float> scratch = NodeCanvasScene::portWorldCentre(mesh, mesh.inputs[1]);
    const Point<float> modulation = ModulationCableBundle::worldCentre(mesh, true);

    REQUIRE(scratch.y - context.y == Catch::Approx(NodePortGeometry::sidePortSpacing));
    REQUIRE(modulation.y - scratch.y == Catch::Approx(NodePortGeometry::sidePortSpacing));
}

TEST_CASE("Input icon semantics do not reduce preview content",
        "[cycle-v2][canvas][presentation][ports][layout]") {
    const GraphNodeFactory factory;
    const Node mesh = factory.createNode(NodeKind::TrilinearMesh, "mesh", {});
    const Node reverb = factory.createNode(NodeKind::Reverb, "reverb", {});
    const Rectangle<float> nodeBounds { 10.f, 20.f, 240.f, 160.f };
    const float zoom = NodePortGeometry::referenceZoom;
    const Rectangle<float> expected = nodeBounds.withTrimmedTop(42.f * zoom).reduced(8.f * zoom);

    REQUIRE(NodePreviewRenderer::boundsFor(mesh, nodeBounds, zoom) == expected);
    REQUIRE(NodePreviewRenderer::boundsFor(reverb, nodeBounds, zoom) == expected);
}
