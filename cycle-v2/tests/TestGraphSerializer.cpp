#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphSerializer.h"

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
    REQUIRE(waveMesh.inputs.size() == 2);
    REQUIRE(waveMesh.inputs[1].id == "scratch");
    REQUIRE(waveMesh.inputs[1].purpose == PortPurpose::ScratchAttachment);
    REQUIRE(waveMesh.inputs[1].side == PortSide::Left);
    REQUIRE(waveMesh.outputs[0].domain == PortDomain::ControlSignal);
    REQUIRE(waveMesh.outputs[0].channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(waveMesh.outputs[0].side == PortSide::Right);
}

TEST_CASE("Graph serializer preserves node parameters", "[cycle-v2][graph]") {
    NodeGraph source = NodeGraph::createDemoGraph();
    source.getNodesForEditing()[0].parameters.push_back({ "mode", "Mode", "spectral" });
    source.getNodesForEditing()[1].parameters.push_back({ "mesh.vertex.2.curve", "Sharpness", "0.75" });

    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));
    const auto& voice = restored.getNodes()[0];
    const auto& waveMesh = restored.getNodes()[1];

    REQUIRE(parameterValueForNode(voice, "mode") == "spectral");
    REQUIRE(parameterValueForNode(voice, "octave") == "0");
    REQUIRE(parameterValueForNode(voice, "oversampling") == "1x");
    REQUIRE(parameterValueForNode(voice, "missing", "fallback") == "fallback");
    REQUIRE(parameterValueForNode(waveMesh, "mesh.vertex.2.curve") == "0.75");
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

    for (auto& node : source.getNodesForEditing()) {
        if (node.id == "waveMesh") {
            node.outputs[0].side = PortSide::Bottom;
        }
    }

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
    REQUIRE(restored.getNodes().size() == source.getNodes().size());
    REQUIRE(restored.getEdges().size() == source.getEdges().size());
    REQUIRE(GraphValidator().isValid(restored));
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
