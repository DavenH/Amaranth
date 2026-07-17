#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"
#include "../src/Nodes/Trimesh/TrimeshMeshFactory.h"
#include "../src/Nodes/Trimesh/TrimeshMeshState.h"

#include <Curve/Mesh/Mesh.h>

using namespace CycleV2;

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
    REQUIRE(parameterValueForNode(voice, "octave") == "0");
    REQUIRE(parameterValueForNode(voice, "pitch") == "0");
    REQUIRE(parameterValueForNode(voice, "portamento") == "0");
    REQUIRE(parameterValueForNode(voice, "oversampling") == "1x");

    REQUIRE(waveMesh.id == "waveMesh");
    REQUIRE(waveMesh.kind == NodeKind::TrilinearMesh);
    REQUIRE(waveMesh.bounds.getWidth() >= 260.f);
    REQUIRE(waveMesh.inputs.size() == 5);
    REQUIRE(waveMesh.inputs[1].id == "scratch");
    REQUIRE(waveMesh.inputs[1].purpose == PortPurpose::ScratchAttachment);
    REQUIRE(waveMesh.inputs[1].side == PortSide::Left);
    REQUIRE(waveMesh.inputs[2].id == "yellow");
    REQUIRE(waveMesh.inputs[3].id == "red");
    REQUIRE(waveMesh.inputs[4].id == "blue");
    REQUIRE(waveMesh.outputs[0].domain == PortDomain::ControlSignal);
    REQUIRE(waveMesh.outputs[0].channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(waveMesh.outputs[0].side == PortSide::Right);
}

TEST_CASE("Graph serializer preserves node parameters", "[cycle-v2][graph]") {
    NodeGraph source = NodeGraph::createDemoGraph();
    source.findNodeForEditing("voice")->parameters.push_back({ "mode", "Mode", "spectral" });
    auto authoredMesh = TrimeshMeshFactory::createDefaultMesh("SerializedTopology");
    const String topology = TrimeshMeshState::serialize(*authoredMesh);
    authoredMesh->destroy();
    source.findNodeForEditing("waveMesh")->parameters.push_back({
            TrimeshMeshState::parameterId(),
            "Mesh Topology",
            topology
    });

    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));
    const auto& voice = restored.getNodes()[0];
    const auto& waveMesh = restored.getNodes()[1];

    REQUIRE(parameterValueForNode(voice, "mode") == "spectral");
    REQUIRE(parameterValueForNode(voice, "octave") == "0");
    REQUIRE(parameterValueForNode(voice, "oversampling") == "1x");
    REQUIRE(parameterValueForNode(voice, "missing", "fallback") == "fallback");
    REQUIRE(parameterValueForNode(waveMesh, TrimeshMeshState::parameterId()) == topology);
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

TEST_CASE("Graph serializer preserves mesh output port side", "[cycle-v2][graph]") {
    NodeGraph source = NodeGraph::createDemoGraph();

    source.findNodeForEditing("waveMesh")->outputs[0].side = PortSide::Bottom;

    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));
    const auto found = std::find_if(
            restored.getNodes().begin(),
            restored.getNodes().end(),
            [](const Node& node) {
                return node.id == "waveMesh";
            });

    REQUIRE(found != restored.getNodes().end());
    REQUIRE(found->outputs[0].side == PortSide::Bottom);
}

TEST_CASE("Graph serializer round trips XML", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const String xml = serializer.toXmlString(source);
    const NodeGraph restored = serializer.fromXmlString(xml);

    REQUIRE(xml.contains("cycleV2Graph"));
    REQUIRE(xml.contains("formatVersion=\"2\""));
    REQUIRE_FALSE(xml.contains(" w=\""));
    REQUIRE_FALSE(xml.contains(" h=\""));
    REQUIRE(restored.getNodes().size() == source.getNodes().size());
    REQUIRE(restored.getEdges().size() == source.getEdges().size());
    REQUIRE(GraphValidator().isValid(restored));
}

TEST_CASE("Graph serializer reports unsupported and unknown graph data", "[cycle-v2][graph][migration]") {
    GraphSerializer serializer;
    ValueTree future("cycleV2Graph");
    future.setProperty("formatVersion", GraphSerializer::currentFormatVersion + 1, nullptr);
    REQUIRE(serializer.loadValueTree(future).issues.front().code == GraphLoadCode::UnsupportedVersion);

    ValueTree unknown("cycleV2Graph");
    unknown.setProperty("formatVersion", GraphSerializer::currentFormatVersion, nullptr);
    ValueTree node("node");
    node.setProperty("id", "mystery", nullptr);
    node.setProperty("kind", "not-installed", nullptr);
    unknown.addChild(node, -1, nullptr);
    REQUIRE(serializer.loadValueTree(unknown).issues.front().code == GraphLoadCode::UnknownNodeType);
}

TEST_CASE("Bundled default graph loads", "[cycle-v2][graph]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File defaultGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("default.cyclegraph");

    REQUIRE(defaultGraph.existsAsFile());

    const NodeGraph graph = GraphSerializer().fromXmlString(defaultGraph.loadFileAsString());

    REQUIRE_FALSE(graph.getNodes().empty());
    REQUIRE(GraphValidator().isValid(graph));
    REQUIRE(GraphCompiler().compile(graph).succeeded());
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}

TEST_CASE("Bundled spy graph loads", "[cycle-v2][graph]") {
  #if defined(CYCLE_V2_SOURCE_DIR)
    const File spyGraph = File(String(CYCLE_V2_SOURCE_DIR))
            .getChildFile("resources")
            .getChildFile("with-spies.cyclegraph");

    REQUIRE(spyGraph.existsAsFile());

    const NodeGraph graph = GraphSerializer().fromXmlString(spyGraph.loadFileAsString());

    REQUIRE_FALSE(graph.getNodes().empty());
    REQUIRE(GraphValidator().isValid(graph));
    REQUIRE(GraphCompiler().compile(graph).succeeded());
  #else
    SUCCEED("CYCLE_V2_SOURCE_DIR is not defined");
  #endif
}
