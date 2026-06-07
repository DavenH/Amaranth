#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"

using namespace CycleV2;

TEST_CASE("Graph editor connects compatible ports", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("multiply", "factor");

    const auto result = GraphEditor().connect(
            graph,
            { "env", "env", false },
            { "multiply", "factor", true });

    REQUIRE(result.succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    const auto& edge = graph.getEdges().back();
    REQUIRE(edge.sourceNodeId == "env");
    REQUIRE(edge.destNodeId == "multiply");
    REQUIRE(edge.destPortId == "factor");
    REQUIRE_FALSE(edge.attachment);
}

TEST_CASE("Graph editor orients input to output connections", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("multiply", "factor");

    const auto result = GraphEditor().connect(
            graph,
            { "multiply", "factor", true },
            { "env", "env", false });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().sourceNodeId == "env");
    REQUIRE(graph.getEdges().back().destNodeId == "multiply");
}

TEST_CASE("Graph editor marks scratch connections as attachments", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.removeEdgesToInput("waveMesh", "scratch");

    const auto result = GraphEditor().connect(
            graph,
            { "scratchEnv", "env", false },
            { "waveMesh", "scratch", true });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().attachment);
    REQUIRE(graph.getEdges().back().destPortId == "scratch");
}

TEST_CASE("Graph editor creates targeted Trimesh guide attachments", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().createAndAttachGuideCurveToTrimeshVertexParameter(
            graph,
            "waveMesh",
            2,
            "amp",
            { 100.f, 100.f });

    REQUIRE(result.succeeded());
    REQUIRE(result.nodeId == "guide");
    REQUIRE(GraphValidator().isValid(graph));

    const auto& edge = graph.getEdges().back();
    REQUIRE(edge.attachment);
    REQUIRE(edge.sourceNodeId == "guide");
    REQUIRE(edge.sourcePortId == "guide");
    REQUIRE(edge.destNodeId == "waveMesh");
    REQUIRE(edge.destPortId == "guide.vertex.2.amp");
}

TEST_CASE("Graph editor shares guide curves across multiple Trimesh targets", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().addNode(graph, NodeKind::GuideCurve, { 100.f, 100.f }).succeeded());

    const auto waveResult = GraphEditor().attachGuideCurveToTrimeshVertexParameter(
            graph,
            "guide",
            "waveMesh",
            1,
            "phase");
    const auto magResult = GraphEditor().attachGuideCurveToTrimeshVertexParameter(
            graph,
            "guide",
            "magMesh",
            3,
            "amp");

    REQUIRE(waveResult.succeeded());
    REQUIRE(magResult.succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    int guideAttachments {};
    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment && edge.sourceNodeId == "guide") {
            ++guideAttachments;
        }
    }

    REQUIRE(guideAttachments == 2);
}

TEST_CASE("Graph editor replaces existing Trimesh guide attachment target", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    REQUIRE(GraphEditor().addNode(graph, NodeKind::GuideCurve, { 100.f, 100.f }).succeeded());
    REQUIRE(GraphEditor().addNode(graph, NodeKind::GuideCurve, { 180.f, 100.f }).succeeded());
    REQUIRE(GraphEditor().attachGuideCurveToTrimeshVertexParameter(
            graph,
            "guide",
            "waveMesh",
            2,
            "amp").succeeded());

    const auto result = GraphEditor().attachGuideCurveToTrimeshVertexParameter(
            graph,
            "guide2",
            "waveMesh",
            2,
            "amp");

    REQUIRE(result.succeeded());
    REQUIRE(GraphValidator().isValid(graph));

    int targetAttachments {};
    String attachedGuideId;

    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment
                && edge.destNodeId == "waveMesh"
                && edge.destPortId == "guide.vertex.2.amp") {
            ++targetAttachments;
            attachedGuideId = edge.sourceNodeId;
        }
    }

    REQUIRE(targetAttachments == 1);
    REQUIRE(attachedGuideId == "guide2");
}

TEST_CASE("Graph editor colours universal output edges from typed destinations", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Multiply, "mul", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Output, "out", { 260.f, 0.f }));

    const auto result = GraphEditor().connect(
            graph,
            { "mul", "out", false },
            { "out", "time", true });

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().back().domain == PortDomain::TimeSignal);
}

TEST_CASE("Graph editor rejects incompatible connections", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addNode({
            "pitch",
            NodeKind::GenericProcessor,
            "Pitch",
            {},
            {},
            {},
            {},
            { { "out", "Pitch", PortDomain::PitchSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().connect(
            graph,
            { "pitch", "out", false },
            { "multiply", "audio", true });

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE_FALSE(result.validationIssues.empty());
    REQUIRE(graph.getEdges().size() == edgeCount);
}

TEST_CASE("Graph editor rejects context outputs on ordinary signal inputs", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", {}));
    graph.addNode(factory.createNode(NodeKind::Multiply, "multiply", { 260.f, 0.f }));
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().connect(
            graph,
            { "voice", "context", false },
            { "multiply", "left", true });

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::ValidationRejected);
    REQUIRE(graph.getEdges().size() == edgeCount);
}

TEST_CASE("Graph editor removes nodes and incident edges", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().removeNode(graph, "fft");

    REQUIRE(result.succeeded());

    for (const auto& node : graph.getNodes()) {
        REQUIRE(node.id != "fft");
    }

    for (const auto& edge : graph.getEdges()) {
        REQUIRE(edge.sourceNodeId != "fft");
        REQUIRE(edge.destNodeId != "fft");
    }
}

TEST_CASE("Graph editor reports missing node removal", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().removeNode(graph, "missing");

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::MissingNode);
}

TEST_CASE("Graph editor removes edges by index", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto edgeCount = graph.getEdges().size();

    const auto result = GraphEditor().removeEdgeAt(graph, 0);

    REQUIRE(result.succeeded());
    REQUIRE(graph.getEdges().size() == edgeCount - 1);
}

TEST_CASE("Graph editor reports missing edge removal", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().removeEdgeAt(graph, graph.getEdges().size());

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::MissingEdge);
}

TEST_CASE("Graph editor updates node parameters", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    GraphEditor editor;

    const auto updateResult = editor.setNodeParameter(
            graph,
            "voice",
            "domain",
            "Start Domain",
            "spectral");

    REQUIRE(updateResult.succeeded());
    REQUIRE(updateResult.nodeId == "voice");
    REQUIRE(parameterValueForNode(graph.getNodes().front(), "domain") == "spectral");

    const auto addResult = editor.setNodeParameter(
            graph,
            "voice",
            "tempoSync",
            "Tempo Sync",
            "true");

    REQUIRE(addResult.succeeded());
    REQUIRE(parameterValueForNode(graph.getNodes().front(), "tempoSync") == "true");
    REQUIRE(graph.getNodes().front().parameters.size() == 3);
}

TEST_CASE("Graph editor reports missing node parameter updates", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().setNodeParameter(
            graph,
            "missing",
            "domain",
            "Start Domain",
            "spectral");

    REQUIRE_FALSE(result.succeeded());
    REQUIRE(result.code == GraphEditCode::MissingNode);
}
