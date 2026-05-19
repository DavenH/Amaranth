#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphEditor.h"
#include "../src/Graph/GraphNodeFactory.h"

using namespace CycleV2;

TEST_CASE("Graph node factory creates canonical envelope nodes", "[cycle-v2][graph]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::Envelope, "env", { 10.f, 20.f });

    REQUIRE(node.id == "env");
    REQUIRE(node.kind == NodeKind::Envelope);
    REQUIRE(node.bounds.getX() == 10.f);
    REQUIRE(node.bounds.getY() == 20.f);
    REQUIRE(node.outputs.size() == 1);
    REQUIRE(node.outputs.front().domain == PortDomain::EnvelopeSignal);
}

TEST_CASE("Graph node factory creates canonical FFT nodes", "[cycle-v2][graph]") {
    const Node node = GraphNodeFactory().createNode(NodeKind::Fft, "fft", { 0.f, 0.f });

    REQUIRE(node.inputs.size() == 1);
    REQUIRE(node.inputs.front().domain == PortDomain::TimeSignal);
    REQUIRE(node.inputs.front().channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(node.outputs.size() == 2);
    REQUIRE(node.outputs[0].domain == PortDomain::SpectralMagnitudeSignal);
    REQUIRE(node.outputs[1].domain == PortDomain::SpectralPhaseSignal);
    REQUIRE(parameterValueForNode(node, "cycleFrames") == "2048");
    REQUIRE(parameterValueForNode(node, "window") == "blackmanHarris");
}

TEST_CASE("Graph node factory creates mesh and arithmetic nodes", "[cycle-v2][graph]") {
    const Node mesh = GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {});
    const Node add = GraphNodeFactory().createNode(NodeKind::Add, "add", {});

    REQUIRE(mesh.title == "Trilinear Mesh");
    REQUIRE(mesh.outputs.size() == 1);
    REQUIRE(mesh.inputs[0].domain == PortDomain::DomainContext);
    REQUIRE(mesh.inputs[1].id == "in");
    REQUIRE(mesh.inputs[1].domain == PortDomain::ControlSignal);
    REQUIRE(mesh.outputs.front().domain == PortDomain::ControlSignal);
    REQUIRE(mesh.inputs[2].purpose == PortPurpose::ScratchAttachment);

    REQUIRE(add.title == "Add");
    REQUIRE(add.inputs.size() == 2);
    REQUIRE(add.outputs.size() == 1);
    REQUIRE(add.inputs[1].side == PortSide::Left);
    REQUIRE(add.outputs.front().domain == PortDomain::ControlSignal);
    REQUIRE(add.bounds.getWidth() == 150.f);
    REQUIRE(add.bounds.getHeight() == 118.f);

    const Node multiply = GraphNodeFactory().createNode(NodeKind::Multiply, "mul", {});
    REQUIRE(multiply.inputs[0].domain == PortDomain::ControlSignal);
    REQUIRE(multiply.inputs[1].side == PortSide::Left);
    REQUIRE(multiply.outputs.front().domain == PortDomain::ControlSignal);
    REQUIRE(multiply.bounds.getWidth() == add.bounds.getWidth());
    REQUIRE(multiply.bounds.getHeight() == add.bounds.getHeight());
}

TEST_CASE("Graph node factory creates menu node families", "[cycle-v2][graph]") {
    const Node voice = GraphNodeFactory().createNode(NodeKind::VoiceContext, "voice", {});
    const Node wave = GraphNodeFactory().createNode(NodeKind::WaveSource, "wave", {});
    const Node image = GraphNodeFactory().createNode(NodeKind::ImageSource, "image", {});
    const Node guide = GraphNodeFactory().createNode(NodeKind::GuideCurve, "guide", {});
    const Node ir = GraphNodeFactory().createNode(NodeKind::ImpulseResponse, "ir", {});

    REQUIRE(voice.outputs.front().domain == PortDomain::DomainContext);
    REQUIRE(parameterValueForNode(voice, "domain") == "waveform");
    REQUIRE(parameterValueForNode(voice, "voices") == "1");
    REQUIRE(wave.inputs.front().domain == PortDomain::DomainContext);
    REQUIRE(image.inputs.front().domain == PortDomain::DomainContext);
    REQUIRE(guide.outputs.front().domain == PortDomain::EnvelopeSignal);
    REQUIRE(ir.inputs.front().domain == PortDomain::TimeSignal);
    REQUIRE(ir.outputs.front().domain == PortDomain::TimeSignal);
}

TEST_CASE("Graph editor adds nodes with unique ids", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    const auto result = GraphEditor().addNode(graph, NodeKind::Envelope, { 100.f, 100.f });

    REQUIRE(result.succeeded());
    REQUIRE(result.nodeId == "env2");
    REQUIRE(graph.getNodes().back().id == "env2");
    REQUIRE(graph.getNodes().back().kind == NodeKind::Envelope);
}

TEST_CASE("Graph node factory creates stereo split and join nodes", "[cycle-v2][graph]") {
    const Node split = GraphNodeFactory().createNode(NodeKind::StereoSplit, "split", {});
    const Node join = GraphNodeFactory().createNode(NodeKind::StereoJoin, "join", {});

    REQUIRE(split.inputs.size() == 1);
    REQUIRE(split.inputs.front().channelLayout == ChannelLayout::LinkedStereo);
    REQUIRE(split.outputs.size() == 2);
    REQUIRE(split.outputs[0].channelLayout == ChannelLayout::Left);
    REQUIRE(split.outputs[1].channelLayout == ChannelLayout::Right);

    REQUIRE(join.inputs.size() == 2);
    REQUIRE(join.inputs[0].channelLayout == ChannelLayout::Left);
    REQUIRE(join.inputs[1].channelLayout == ChannelLayout::Right);
    REQUIRE(join.outputs.front().channelLayout == ChannelLayout::LinkedStereo);
}

TEST_CASE("Graph node factory sizes nodes from their content", "[cycle-v2][graph]") {
    const Node wave = GraphNodeFactory().createNode(NodeKind::TrilinearWaveSurface, "wave", {});
    const Node env = GraphNodeFactory().createNode(NodeKind::Envelope, "env", {});

    REQUIRE(wave.bounds.getWidth() >= 380.f);
    REQUIRE(wave.bounds.getHeight() >= 300.f);
    REQUIRE(env.bounds.getWidth() >= 240.f);
    REQUIRE(env.bounds.getHeight() >= 160.f);
    REQUIRE(wave.bounds.getHeight() > env.bounds.getHeight());
}
