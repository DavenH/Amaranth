#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphCompiler.h"
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
    const auto& wave = restored.getNodes()[1];

    REQUIRE(wave.id == "wave");
    REQUIRE(wave.kind == NodeKind::TrilinearMesh);
    REQUIRE(wave.bounds.getWidth() >= 260.f);
    REQUIRE(wave.inputs.size() == 3);
    REQUIRE(wave.inputs[2].id == "scratch");
    REQUIRE(wave.inputs[2].purpose == PortPurpose::ScratchAttachment);
    REQUIRE(wave.outputs[0].domain == PortDomain::TimeSignal);
    REQUIRE(wave.outputs[0].channelLayout == ChannelLayout::LinkedStereo);
}

TEST_CASE("Graph serializer preserves attachment edges", "[cycle-v2][graph]") {
    const NodeGraph source = NodeGraph::createDemoGraph();
    const GraphSerializer serializer;
    const NodeGraph restored = serializer.fromValueTree(serializer.toValueTree(source));

    bool foundScratchAttachment = false;
    for (const auto& edge : restored.getEdges()) {
        if (edge.sourceNodeId == "scratchEnv" && edge.destNodeId == "wave" && edge.destPortId == "scratch") {
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
