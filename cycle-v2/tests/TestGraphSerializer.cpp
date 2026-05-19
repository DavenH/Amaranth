#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"

#include <algorithm>

using namespace CycleV2;

namespace {

const Edge& findSignalEdge(const GraphExecutionPlan& plan, const String& sourceNodeId, const String& destNodeId) {
    const auto found = std::find_if(
            plan.signalEdges.begin(),
            plan.signalEdges.end(),
            [&](const Edge& edge) {
                return edge.sourceNodeId == sourceNodeId && edge.destNodeId == destNodeId;
            });

    REQUIRE(found != plan.signalEdges.end());
    return *found;
}

}

TEST_CASE("Graph serializer round trips demo graph", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));

    REQUIRE(restored.getNodes().size() == source.getNodes().size());
    REQUIRE(restored.getEdges().size() == source.getEdges().size());
    REQUIRE(GraphValidator().isValid(restored));
    REQUIRE(GraphCompiler().compile(restored).succeeded());
}

TEST_CASE("Graph serializer preserves node and port metadata", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));
    const auto& voice = restored.getNodes()[0];
    const auto& waveMesh = restored.getNodes()[1];

    REQUIRE(voice.id == "voice");
    REQUIRE(voice.kind == NodeKind::VoiceContext);
    REQUIRE(voice.outputs[0].domain == PortDomain::DomainContext);
    REQUIRE(parameterValueForNode(voice, "domain") == "waveform");
    REQUIRE(parameterValueForNode(voice, "voices") == "6");

    REQUIRE(waveMesh.id == "waveMesh");
    REQUIRE(waveMesh.kind == NodeKind::TrilinearMesh);
    REQUIRE(waveMesh.bounds.getWidth() >= 260.f);
    REQUIRE(waveMesh.inputs.size() == 3);
    REQUIRE(waveMesh.inputs[1].id == "in");
    REQUIRE(waveMesh.inputs[1].domain == PortDomain::ControlSignal);
    REQUIRE(waveMesh.inputs[2].id == "scratch");
    REQUIRE(waveMesh.inputs[2].purpose == PortPurpose::ScratchAttachment);
    REQUIRE(waveMesh.inputs[2].side == PortSide::Left);
    REQUIRE(waveMesh.outputs[0].domain == PortDomain::ControlSignal);
    REQUIRE(waveMesh.outputs[0].channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(waveMesh.outputs[0].side == PortSide::Right);
}

TEST_CASE("Graph serializer preserves node parameters", "[cycle-v2][graph]") {
    NodeGraph source = NodeGraph::createDemoGraph();
    source.getNodesForEditing()[0].parameters.push_back({ "mode", "Mode", "spectral" });

    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));
    const auto& voice = restored.getNodes()[0];

    REQUIRE(parameterValueForNode(voice, "mode") == "spectral");
    REQUIRE(parameterValueForNode(voice, "missing", "fallback") == "fallback");
}

TEST_CASE("Graph serializer preserves attachment edges", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));

    bool foundScratchAttachment = false;
    for (const auto& edge : restored.getEdges()) {
        if (edge.sourceNodeId == "scratchEnv" && edge.destNodeId == "waveMesh" && edge.destPortId == "scratch") {
            foundScratchAttachment = edge.attachment;
        }
    }

    REQUIRE(foundScratchAttachment);
}

TEST_CASE("Graph serializer round trips XML", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const String xml = serializer.toXmlString(source);
    const NodeGraph restored = serializer.fromXmlString(xml);

    REQUIRE(xml.contains("cycleV2Graph"));
    REQUIRE(restored.getNodes().size() == source.getNodes().size());
    REQUIRE(restored.getEdges().size() == source.getEdges().size());
    REQUIRE(GraphValidator().isValid(restored));
}

TEST_CASE("Graph serializer migrates legacy start nodes to voice context domains", "[cycle-v2][graph]") {
    GraphNodeFactory factory;
    NodeGraph graph;

    graph.addNode(factory.createNode(NodeKind::VoiceContext, "voice", { 0.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::TrilinearMesh, "mesh", { 240.f, 0.f }));
    graph.addNode(factory.createNode(NodeKind::Add, "add", { 520.f, 0.f }));
    graph.addEdge({ "voice", "context", "mesh", "context", PortDomain::DomainContext, false });
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, false });

    const GraphSerializer serializer;
    const String xml = serializer.toXmlString(graph).replace("kind=\"voiceContext\"", "kind=\"spectralStart\"");
    const NodeGraph restored = serializer.fromXmlString(xml);
    const auto compileResult = GraphCompiler().compile(restored);

    REQUIRE(restored.getNodes().front().kind == NodeKind::VoiceContext);
    REQUIRE(parameterValueForNode(restored.getNodes().front(), "domain") == "spectral");
    REQUIRE(compileResult.succeeded());
    REQUIRE(findSignalEdge(compileResult.plan, "mesh", "add").domain == PortDomain::SpectralMagnitudeSignal);
}
