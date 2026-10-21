#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphNodeFactory.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/GraphValidator.h"
#include "UI/CanvasChromePalette.h"
#include "UI/GroupInformationCardPresentation.h"
#include "UI/NodeCanvasScene.h"
#include "UI/NodeGroupPresentation.h"

using namespace CycleV2;

namespace {

NodeGraph informationCardGraph() {
    NodeGraph graph = NodeGraph::createDemoGraph();
    GraphNodeFactory factory;
    graph.addNode(factory.createNode(NodeKind::Add, "a", { 100.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "b", { 400.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "c", { 800.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "d", { 1100.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "outside", { 1800.f, 100.f }));
    REQUIRE(graph.addNodeGroup({ "first", "Timbre", { "a", "b" }, false }));
    REQUIRE(graph.addNodeGroup({ "second", "Motion", { "c", "d" }, false }));
    return graph;
}

void updateCards(GroupInformationCardPresentation& cards, const GraphDocument& document) {
    const GraphEdgeIndex edges(document.graph().getEdges());
    cards.update(document.graph(), edges, document.revision());
}

void showCard(GraphCommandDispatcher& commands, const String& groupId,
        const String& text, Point<float> offset = { 0.f, 300.f }) {
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, groupId }, text).succeeded());
    REQUIRE(commands.setGroupInformationCard(groupId, offset).succeeded());
}

}

TEST_CASE("Information cards are optional views of the group's single explanation",
        "[cycle-v2][groups][explanations][cards][presentation]") {
    const juce::ScopedJuceInitialiser_GUI gui;
    GraphDocument document(informationCardGraph());
    GraphCommandDispatcher commands(document);
    GroupInformationCardPresentation cards;
    updateCards(cards, document);
    REQUIRE(cards.visibleCards().empty());
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, "first" }, "Shared body").changed);
    updateCards(cards, document);
    REQUIRE(cards.visibleCards().empty());
    REQUIRE(commands.setGroupInformationCard("first", Point<float>(0.f, 300.f)).changed);
    updateCards(cards, document);
    REQUIRE(cards.visibleCards().size() == 1);
    REQUIRE(cards.find("first")->text == document.graph().findNodeGroup("first")->rationale);
    REQUIRE(cards.find("first")->title == "Timbre");
    REQUIRE(commands.setExplanationText({ ExplanationScope::Group, "first" }, {}).changed);
    updateCards(cards, document);
    REQUIRE(cards.visibleCards().empty());
    REQUIRE(document.graph().findNodeGroup("first")->informationCardOffset.has_value());
    REQUIRE(document.undo());
    updateCards(cards, document);
    REQUIRE(cards.find("first")->text == "Shared body");
    REQUIRE(commands.setGroupInformationCard("first", std::nullopt).changed);
    updateCards(cards, document);
    REQUIRE(cards.find("first") == nullptr);
    REQUIRE(document.graph().findNodeGroup("first")->rationale == "Shared body");
}

TEST_CASE("Information cards retain world placement through collapse, viewport changes and group movement",
        "[cycle-v2][groups][explanations][cards][geometry]") {
    const juce::ScopedJuceInitialiser_GUI gui;
    GraphDocument document(informationCardGraph());
    GraphCommandDispatcher commands(document);
    showCard(commands, "first", "Connected harmonic body\nTry gentle expression.");
    GroupInformationCardPresentation cards;
    updateCards(cards, document);
    NodeCanvasViewport viewport;
    viewport.setBounds({ 0.f, 0.f, 1200.f, 800.f });
    viewport.setTransform({}, 1.f);
    const auto original = GroupInformationCardPresentation::bounds(*cards.find("first"), viewport).card;
    const auto savedOffset = document.graph().findNodeGroup("first")->informationCardOffset;
    REQUIRE(commands.setNodeGroupCollapsed("first", true).changed);
    updateCards(cards, document);
    REQUIRE(cards.visibleCards().size() == 1);
    for (const float zoom : { 0.43f, 1.f, 1.5f }) {
        viewport.setTransform({ 50.f, -40.f }, zoom);
        const auto screen = GroupInformationCardPresentation::bounds(*cards.find("first"), viewport).card;
        const auto world = viewport.toWorld(screen);
        REQUIRE(world.getX() == Catch::Approx(original.getX()));
        REQUIRE(world.getY() == Catch::Approx(original.getY()));
        REQUIRE(world.getWidth() == Catch::Approx(original.getWidth()));
        REQUIRE(world.getHeight() == Catch::Approx(original.getHeight()));
    }
    REQUIRE(commands.translateNodes({ "a", "b" }, { 40.f, -20.f }).succeeded());
    updateCards(cards, document);
    const auto moved = viewport.toWorld(
            GroupInformationCardPresentation::bounds(*cards.find("first"), viewport).card);
    REQUIRE(moved.getX() == Catch::Approx(original.getX() + 40.f));
    REQUIRE(moved.getY() == Catch::Approx(original.getY() - 20.f));
    REQUIRE(document.graph().findNodeGroup("first")->informationCardOffset == savedOffset);
    REQUIRE(document.undo());
    updateCards(cards, document);
    REQUIRE(cards.find("first")->origin == Point<float>(100.f, 100.f));
}

TEST_CASE("Information card geometry gives short notes natural height and separates controls from text",
        "[cycle-v2][groups][explanations][cards][geometry]") {
    const juce::ScopedJuceInitialiser_GUI gui;
    GraphDocument document(informationCardGraph());
    GraphCommandDispatcher commands(document);
    showCard(commands, "first", "Warm body");
    showCard(commands, "second", String::repeatedString(
            "The transient supplies brightness before a soft sustained body.\n", 24));
    GroupInformationCardPresentation cards;
    updateCards(cards, document);
    const auto& run = *cards.find("first")->layout.getLine(0).runs.getFirst();
    REQUIRE(run.font.getHeight() == Catch::Approx(14.f));
    REQUIRE(run.colour == CanvasChromePalette::text);
    REQUIRE(cards.find("first")->height < cards.find("second")->height);
    REQUIRE(cards.find("first")->height < 100.f);
    REQUIRE(cards.find("second")->height < cards.find("second")->layout.getHeight());
    REQUIRE(cards.find("second")->height - 46.f == Catch::Approx(
            cards.find("second")->layout.getLine(7).getLineBounds().getBottom()));
    NodeCanvasViewport viewport;
    for (const float zoom : { 0.43f, 1.f, 1.5f }) {
        viewport.setTransform({}, zoom);
        for (const auto* card : cards.visibleCards()) {
            const auto area = GroupInformationCardPresentation::bounds(*card, viewport);
            REQUIRE(area.card.contains(area.header));
            REQUIRE(area.card.contains(area.body));
            REQUIRE(area.header.contains(area.close));
            REQUIRE_FALSE(area.header.intersects(area.body));
            REQUIRE_FALSE(area.close.intersects(area.body));
            const auto dragHeader = area.header.withTrimmedRight(area.close.getWidth());
            REQUIRE_FALSE(dragHeader.intersects(area.close));
            REQUIRE(area.body.getX() - area.card.getX() == Catch::Approx(10.f * zoom));
            REQUIRE(area.card.getRight() - area.body.getRight() == Catch::Approx(10.f * zoom));
            REQUIRE(area.body.getY() > area.header.getBottom());
        }
    }
}

TEST_CASE("Card hits follow paint order and local placement previews leave the document untouched",
        "[cycle-v2][groups][explanations][cards][hit-testing]") {
    const juce::ScopedJuceInitialiser_GUI gui;
    GraphDocument document(informationCardGraph());
    GraphCommandDispatcher commands(document);
    showCard(commands, "first", "First card");
    showCard(commands, "second", "Second card", { -700.f, 300.f });
    GroupInformationCardPresentation cards;
    updateCards(cards, document);
    NodeCanvasViewport viewport;
    viewport.setTransform({}, 1.f);
    const auto area = GroupInformationCardPresentation::bounds(*cards.find("first"), viewport);
    REQUIRE(cards.cardAt(area.body.getCentre(), viewport, {})->groupId == "second");
    const auto revision = document.revision();
    const auto savedOffset = document.graph().findNodeGroup("first")->informationCardOffset;
    for (const Point<float> offset : { Point<float>(30.f, 320.f), Point<float>(70.f, 340.f) }) {
        const GroupInformationCardPreview preview { "first", offset };
        const auto moved = GroupInformationCardPresentation::bounds(*cards.find("first"), viewport, preview);
        REQUIRE(moved.card.getPosition() == cards.find("first")->origin + offset);
        REQUIRE(GroupInformationCardPresentation::bounds(*cards.find("second"), viewport, preview).card
                == GroupInformationCardPresentation::bounds(*cards.find("second"), viewport).card);
    }
    REQUIRE(document.revision() == revision);
    REQUIRE(document.graph().findNodeGroup("first")->informationCardOffset == savedOffset);
    REQUIRE(GroupInformationCardPresentation::bounds(*cards.find("first"), viewport).card == area.card);
}

TEST_CASE("Card text layout stays cached when unrelated graph content moves",
        "[cycle-v2][groups][explanations][cards][complexity]") {
    const juce::ScopedJuceInitialiser_GUI gui;
    GraphDocument document(informationCardGraph());
    GraphCommandDispatcher commands(document);
    showCard(commands, "first", "Bright attack\nThe harmonic body remains restrained.");
    GroupInformationCardPresentation cards;
    updateCards(cards, document);
    const auto* original = cards.find("first");
    REQUIRE(original->layout.getNumLines() >= 2);
    const auto* line = &original->layout.getLine(0);
    REQUIRE(commands.moveNode("outside", { 1900.f, 220.f }).succeeded());
    updateCards(cards, document);
    REQUIRE(cards.find("first") == original);
    REQUIRE(&cards.find("first")->layout.getLine(0) == line);
    REQUIRE(cards.find("first")->origin == Point<float>(100.f, 100.f));
}

TEST_CASE("Card anchors use inline-pan presentation and structural history removes stale cards",
        "[cycle-v2][groups][explanations][cards][lifecycle]") {
    const juce::ScopedJuceInitialiser_GUI gui;
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 100.f, 100.f }));
    graph.addNode(factory.createNode(NodeKind::SpectralLayer, "pan", { -2000.f, -800.f }));
    graph.addNode(factory.createNode(NodeKind::Ifft, "ifft", { 600.f, 100.f }));
    REQUIRE(GraphNodeStateEditor().setNodeParameter(
            graph, "mesh", "signalType", {}, "spectralMagnitude").succeeded());
    graph.addEdge({ "mesh", "out", "pan", "in", PortDomain::ControlSignal });
    graph.addEdge({ "pan", "out", "ifft", "mag", PortDomain::ControlSignal });
    REQUIRE(graph.addNodeGroup({ "first", "Stereo placement", { "mesh", "pan" }, false }));
    REQUIRE(GraphValidator().isValid(graph));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    showCard(commands, "first", "Keep the stereo image stable.");
    GroupInformationCardPresentation cards;
    updateCards(cards, document);
    REQUIRE(cards.find("first")->origin.x >= 100.f);
    REQUIRE(cards.find("first")->origin.y >= 50.f);
    const File file = File(String(CYCLE_V2_TEST_OUTPUT_DIR)).getChildFile("information-card.cyclegraph");
    REQUIRE(document.save(file));
    REQUIRE(commands.removeCable({ 0 }).succeeded());
    updateCards(cards, document);
    REQUIRE(cards.find("first") == nullptr);
    REQUIRE(cards.visibleCards().empty());
    REQUIRE(document.undo());
    updateCards(cards, document);
    REQUIRE(cards.find("first") != nullptr);
    REQUIRE(cards.find("first")->text == "Keep the stereo image stable.");
    REQUIRE(commands.ungroupNodes("first").succeeded());
    updateCards(cards, document);
    REQUIRE(cards.visibleCards().empty());
    REQUIRE(document.load(file));
    updateCards(cards, document);
    REQUIRE(cards.find("first") != nullptr);
    REQUIRE(cards.find("first")->offset == Point<float>(0.f, 300.f));
}
