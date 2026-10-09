#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "UI/NodeCanvasPresentation.h"
#include "UI/NodePortLayout.h"

using namespace CycleV2;

TEST_CASE("Operation port layouts share authored, painted, and hit geometry",
        "[cycle-v2][canvas][presentation][layout]") {
    struct LayoutExpectation {
        OperationPortLayout layout;
        PortSide firstInput;
        PortSide secondInput;
        bool reversed;
    };
    const std::array<LayoutExpectation, 5> expectations {{
            { OperationPortLayout::Side, PortSide::Left, PortSide::Left, false },
            { OperationPortLayout::SideReversed, PortSide::Left, PortSide::Left, true },
            { OperationPortLayout::Uptack, PortSide::Left, PortSide::Top, false },
            { OperationPortLayout::Vertical, PortSide::Top, PortSide::Bottom, false },
            { OperationPortLayout::Tee, PortSide::Left, PortSide::Bottom, false }
    }};

    NodeCanvasViewport viewport;
    viewport.setTransform({ 31.f, 47.f }, 0.73f);
    for (size_t index = 0; index < expectations.size(); ++index) {
        const LayoutExpectation& expectation = expectations[index];
        Node node = GraphNodeFactory().createNode(NodeKind::Add, "add", { 120.f, 80.f });
        applyOperationPortLayout(node, expectation.layout);

        REQUIRE(operationPortLayout(node) == expectation.layout);
        REQUIRE(nextOperationPortLayout(expectation.layout)
                == expectations[(index + 1) % expectations.size()].layout);
        REQUIRE(node.inputs[0].side == expectation.firstInput);
        REQUIRE(node.inputs[1].side == expectation.secondInput);
        REQUIRE(node.outputs[0].side == PortSide::Right);
        REQUIRE(node.reverseInputPortOrder == expectation.reversed);
        if (expectation.layout == OperationPortLayout::Side
                || expectation.layout == OperationPortLayout::SideReversed) {
            const float firstY = NodeCanvasScene::portWorldCentre(node, node.inputs[0]).y;
            const float secondY = NodeCanvasScene::portWorldCentre(node, node.inputs[1]).y;
            REQUIRE((firstY > secondY) == expectation.reversed);
        }

        NodeGraph graph;
        graph.addNode(node);
        const Node& stored = *graph.findNode("add");
        NodeCanvasScene sceneBuilder;
        const NodeCanvasSceneSnapshot& scene = sceneBuilder.build(graph, viewport);
        const auto checkPort = [&](const Port& port) {
            const NodePortPresentation painted = NodeCanvasPresentation::portPresentation(
                    viewport, stored, port);
            const auto target = std::find_if(
                    scene.targets.begin(),
                    scene.targets.end(),
                    [&](const NodeSceneTarget& candidate) {
                        return candidate.nodeId == stored.id
                                && candidate.portId == port.id
                                && candidate.isPort();
                    });
            REQUIRE(target != scene.targets.end());
            REQUIRE(target->bounds.contains(painted.centre));
            REQUIRE(target->bounds.getCentreX() == Catch::Approx(painted.centre.x));
            REQUIRE(target->bounds.getCentreY() == Catch::Approx(painted.centre.y));
        };
        checkPort(stored.inputs[0]);
        checkPort(stored.inputs[1]);
        checkPort(stored.outputs[0]);
    }
}

TEST_CASE("Reversed side layout moves cable endpoints without changing input identities",
        "[cycle-v2][canvas][presentation][layout]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "upper", { 20.f, 20.f }));
    graph.addNode(factory.createNode(NodeKind::ModulationSource, "lower", { 20.f, 240.f }));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 320.f, 100.f }));
    const Node* multiply = graph.findNode("multiply");
    REQUIRE(multiply != nullptr);
    const String firstId = multiply->inputs[0].id;
    const String secondId = multiply->inputs[1].id;
    const String sourcePort = graph.findNode("upper")->outputs[0].id;
    graph.addEdge({ "upper", sourcePort, "multiply", firstId,
            PortDomain::ControlSignal, ConnectionKind::Signal });
    graph.addEdge({ "lower", sourcePort, "multiply", secondId,
            PortDomain::ControlSignal, ConnectionKind::Signal });
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);

    NodeCanvasViewport viewport;
    viewport.setTransform({}, 1.f);
    NodeCanvasScene sceneBuilder;
    const auto& before = sceneBuilder.build(document.graph(), viewport);
    REQUIRE(before.edges.size() == 2);
    const float firstBeforeY = before.edges[0].destination.y;
    const float secondBeforeY = before.edges[1].destination.y;
    REQUIRE(firstBeforeY < secondBeforeY);

    REQUIRE(commands.editNodePresentation("multiply", [](Node& edited) {
        applyOperationPortLayout(edited, OperationPortLayout::SideReversed);
    }).succeeded());
    const auto& after = sceneBuilder.build(document.graph(), viewport);
    REQUIRE(after.edges.size() == 2);
    REQUIRE(after.edges[0].destination.y == Catch::Approx(secondBeforeY));
    REQUIRE(after.edges[1].destination.y == Catch::Approx(firstBeforeY));
    REQUIRE(document.graph().getEdges()[0].destPortId == firstId);
    REQUIRE(document.graph().getEdges()[1].destPortId == secondId);
    const Node* edited = document.graph().findNode("multiply");
    REQUIRE(edited != nullptr);
    REQUIRE(edited->inputs[0].id == firstId);
    REQUIRE(edited->inputs[1].id == secondId);
}

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

TEST_CASE("Node canvas marks only authored global processing",
        "[cycle-v2][canvas][presentation][audio-scope]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", {}));
    graph.addNode(factory.createNode(NodeKind::VoiceOutput, "voiceOut", {}));
    graph.addNode(factory.createNode(NodeKind::GlobalInput, "global", {}));
    graph.addNode(factory.createNode(NodeKind::GenericProcessor, "route", {}));
    graph.addNode(factory.createNode(NodeKind::Delay, "delay", {}));
    graph.addNode(factory.createNode(NodeKind::Output, "out", {}));
    graph.addEdge({ "wave", "out", "voiceOut", "time", PortDomain::TimeSignal, ConnectionKind::Signal });
    graph.addEdge({
            "global", "time", "route", "in",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "route", "out", "delay", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    graph.addEdge({
            "delay", "time", "out", "time",
            PortDomain::TimeSignal, ConnectionKind::Signal
    });
    const auto compiled = GraphCompiler().compile(graph);
    REQUIRE(compiled.succeeded());
    GraphPresentationSnapshot snapshot;
    GraphPresentationFacts facts(graph, snapshot);
    const auto isGlobal = [&](const String& nodeId) {
        return graph.findNode(nodeId) != nullptr
                && facts.audioScopeAnalysis().scopeFor(nodeId)
                        == AuthoredAudioScope::Global;
    };

    REQUIRE_FALSE(isGlobal("wave"));
    REQUIRE(isGlobal("global"));
    REQUIRE(isGlobal("route"));
    REQUIRE(isGlobal("delay"));
    REQUIRE(isGlobal("out"));
    REQUIRE_FALSE(isGlobal("missing"));

    graph.addNode(factory.createNode(NodeKind::Equalizer, "invalidGlobalEq", {}));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph,
            "invalidGlobalEq",
            "processingScope",
            "Processing",
            "global").succeeded());
    REQUIRE_FALSE(GraphCompiler().compile(graph).succeeded());
    GraphPresentationFacts invalidFacts(graph, snapshot);
    REQUIRE(invalidFacts.audioScopeAnalysis().scopeFor("invalidGlobalEq")
            == AuthoredAudioScope::Global);

    const Rectangle<float> header { 20.f, 30.f, 180.f, 42.f };
    const auto clear = NodeCanvasPresentation::globalProcessingIndicatorBounds(
            header,
            1.f,
            false);
    const auto besideAction = NodeCanvasPresentation::globalProcessingIndicatorBounds(
            header,
            1.f,
            true);
    REQUIRE(clear.getWidth() == 24.f);
    REQUIRE(clear.getHeight() == 24.f);
    REQUIRE(besideAction.getWidth() == 24.f);
    REQUIRE(besideAction.getHeight() == 24.f);
    REQUIRE(clear.getRight() <= header.getRight());
    REQUIRE(besideAction.getRight() < clear.getX());
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

TEST_CASE("Guide column and Spy row retain independent scroll room",
        "[cycle-v2][canvas][presentation][guide-dock]") {
    const Rectangle<float> workspace(0.f, 0.f, 1000.f, 700.f);
    SignalProbeRailState dockState;
    GuideCurveShelfState guideState;
    const Rectangle<float> guides = GuideCurveShelf::guideWorkspace(workspace);
    const Rectangle<float> spies = GuideCurveShelf::spyWorkspace(workspace);

    REQUIRE(guides.getWidth() == Catch::Approx(274.f));
    REQUIRE(guides.getX() == Catch::Approx(workspace.getX()));
    REQUIRE(guides.getY() == Catch::Approx(workspace.getY()));
    REQUIRE(spies.getWidth() > guides.getWidth());
    REQUIRE(guides.getRight() < spies.getX());
    REQUIRE(GuideCurveShelf::maximumVerticalOffset(
            workspace,
            dockState,
            guideState,
            1) == Catch::Approx(0.f));
    REQUIRE(GuideCurveShelf::maximumVerticalOffset(
            workspace,
            dockState,
            guideState,
            8) > 0.f);

    guideState.minimized = true;
    REQUIRE(GuideCurveShelf::boundsFor(workspace, dockState, guideState).getWidth()
            == Catch::Approx(GuideCurveShelf::minimizedWidth));
    REQUIRE(GuideCurveShelf::spyWorkspace(workspace, true, false).getWidth()
            > spies.getWidth());

    guideState.minimized = false;
    dockState.minimized = true;
    REQUIRE(GuideCurveShelf::guideWorkspace(workspace, false, true).getWidth()
            == Catch::Approx(guides.getWidth()));
    REQUIRE(GuideCurveShelf::spyWorkspace(workspace, false, true).getWidth()
            == Catch::Approx(GuideCurveShelf::minimizedWidth));
    REQUIRE(SignalProbeRail::minimizeButtonBoundsFor(spies, dockState).isEmpty());
}
