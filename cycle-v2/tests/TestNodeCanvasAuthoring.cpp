#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/UI/NodeCanvasAuthoring.h"

using namespace CycleV2;

namespace {

class NullEditorCommands final : public NodeEditorCommands {
public:
    bool publishCurveState(
            const String&,
            const String&,
            uint64_t,
            const std::vector<NodeParameter>&) override {
        return true;
    }

    void beginCurveTransaction() override {}
    void commitCurveTransaction() override {}
    bool setTrimeshPrimaryAxisValue(const String&, const String&) override { return true; }
    bool toggleTrimeshLinkAxisValue(const String&, const String&) override { return true; }
    bool beginTrimeshMorphEdit(const String&, const String&, float) override { return true; }
    bool updateTrimeshMorphEditValue(float) override { return true; }
    void endTrimeshMorphEdit() override {}
    bool beginTrimeshVertexParameterEdit(const String&, const String&, float) override { return true; }
    bool updateTrimeshVertexParameterEditValue(float) override { return true; }
    void endTrimeshVertexParameterEdit() override {}
    bool showTrimeshGuideAttachmentMenu(
            const String&,
            const String&,
            Rectangle<int>) override {
        return true;
    }
    bool selectTrimeshVertexIndex(const String&, int) override { return true; }
    void persistTrimeshMeshEdits(const String&) override {}
};

NodeCanvasAuthoring makeAuthoring(
        GraphDocument& document,
        GraphCommandDispatcher& commands,
        GraphPresentationModel& presentation,
        NodeEditorCommands& editorCommands) {
    presentation.refresh(document.graph(), document.revision());
    return { document, commands, presentation, editorCommands };
}

}

TEST_CASE("Node canvas authoring preserves graph and layout semantics",
        "[cycle-v2][canvas][authoring]") {
    GraphNodeFactory factory;
    NodeGraph graph;
    graph.addNode(factory.createNode(NodeKind::WaveSource, "wave", { 20.f, 80.f }));
    graph.addNode(factory.createNode(NodeKind::Output, "out", { 260.f, 80.f }));
    graph.addEdge({ "wave", "out", "out", "time", PortDomain::TimeSignal, false });

    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    NullEditorCommands editorCommands;
    auto authoring = makeAuthoring(document, commands, presentation, editorCommands);

    const auto added = authoring.addNode(NodeKind::Delay, { 90.f, 80.f });
    REQUIRE(added.succeeded);
    REQUIRE(added.graphChanged);
    REQUIRE(authoring.session().selectedNodeId == added.nodeId);

    const auto inserted = authoring.spliceSelectedNodeIntoEdge(0);
    REQUIRE(inserted.succeeded);
    REQUIRE(document.graph().getEdges().size() == 2);

    const Node* wave = document.graph().findNode("wave");
    const Node* delay = document.graph().findNode(added.nodeId);
    const Node* output = document.graph().findNode("out");
    REQUIRE(wave != nullptr);
    REQUIRE(delay != nullptr);
    REQUIRE(output != nullptr);
    REQUIRE(delay->bounds.getX() >= wave->bounds.getRight() + 56.f);
    REQUIRE(output->bounds.getX() >= delay->bounds.getRight() + 56.f);

    const uint64_t revisionAfterInsert = document.revision();
    REQUIRE(authoring.undo().succeeded);
    REQUIRE(document.graph().getEdges().size() == 1);
    REQUIRE(authoring.redo().succeeded);
    REQUIRE(document.revision() > revisionAfterInsert);
    REQUIRE(document.graph().getEdges().size() == 2);

    REQUIRE(authoring.deleteNode(added.nodeId).succeeded);
    REQUIRE(document.graph().findNode(added.nodeId) == nullptr);
}

TEST_CASE("Node canvas authoring keeps voice parameter and subtitle atomic",
        "[cycle-v2][canvas][authoring]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {}));
    GraphDocument document(std::move(graph));
    GraphCommandDispatcher commands(document);
    GraphPresentationModel presentation;
    NullEditorCommands editorCommands;
    auto authoring = makeAuthoring(document, commands, presentation, editorCommands);

    const auto edited = authoring.applyVoiceContextEdit(
            "voice",
            { VoiceContextEdit::Control::Domain, "spectral" });
    REQUIRE(edited.succeeded);
    REQUIRE(document.canUndo());

    String domain;
    REQUIRE(authoring.getNodeParameter("voice", "domain", domain));
    REQUIRE(domain == "spectral");
    REQUIRE(document.graph().findNode("voice")->subtitle == "spectral start");
    REQUIRE(authoring.session().statusMessage == "Voice start domain: spectral");

    REQUIRE(authoring.undo().succeeded);
    REQUIRE(document.graph().findNode("voice")->subtitle != "spectral start");
}
