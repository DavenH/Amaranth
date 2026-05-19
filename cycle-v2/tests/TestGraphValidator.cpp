#include <catch2/catch_test_macros.hpp>

#include "../src/Graph/GraphNodeFactory.h"
#include "../src/Graph/GraphValidator.h"

#include <algorithm>

using namespace CycleV2;

TEST_CASE("Demo graph validates", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();

    REQUIRE(GraphValidator().isValid(graph));
}

TEST_CASE("Demo graph exposes stable node kinds", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    const auto& nodes = graph.getNodes();

    const auto findKind = [&](const String& nodeId) {
        const auto found = std::find_if(
                nodes.begin(),
                nodes.end(),
                [&](const Node& node) {
                    return node.id == nodeId;
                });
        REQUIRE(found != nodes.end());
        return found->kind;
    };

    REQUIRE(findKind("voice") == NodeKind::VoiceContext);
    REQUIRE(findKind("waveMesh") == NodeKind::TrilinearMesh);
    REQUIRE(findKind("fft") == NodeKind::Fft);
    REQUIRE(findKind("addMag") == NodeKind::Add);
    REQUIRE(findKind("addPhase") == NodeKind::Add);
    REQUIRE(findKind("ifft") == NodeKind::Ifft);
    REQUIRE(findKind("multiply") == NodeKind::Multiply);
    REQUIRE(findKind("out") == NodeKind::Output);
}

TEST_CASE("Channel layouts have stable short labels", "[cycle-v2][graph]") {
    REQUIRE(labelForChannelLayout(ChannelLayout::Mono).isEmpty());
    REQUIRE(labelForChannelLayout(ChannelLayout::LinkedStereo) == "L/R");
    REQUIRE(labelForChannelLayout(ChannelLayout::Left) == "L");
    REQUIRE(labelForChannelLayout(ChannelLayout::Right) == "R");
    REQUIRE(labelForChannelLayout(ChannelLayout::StereoPair) == "Pair");
}

TEST_CASE("Universal ports accept typed graph operands", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode(GraphNodeFactory().createNode(NodeKind::TrilinearMesh, "mesh", {}));
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Add, "add", { 320.f, 0.f }));
    graph.addEdge({ "mesh", "out", "add", "left", PortDomain::ControlSignal, false });

    REQUIRE(GraphValidator().isValid(graph));
    REQUIRE(labelForDomain(PortDomain::ControlSignal) == "Universal");
}

TEST_CASE("Operation nodes reject mixed concrete signal domains", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode({
            "time",
            NodeKind::GenericProcessor,
            "Time",
            {},
            {},
            {},
            { { "out", "Out", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode({
            "mag",
            NodeKind::GenericProcessor,
            "Magnitude",
            {},
            {},
            {},
            { { "out", "Out", PortDomain::SpectralMagnitudeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false } }
    });
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Add, "add", { 320.f, 0.f }));
    graph.addEdge({ "time", "out", "add", "left", PortDomain::TimeSignal, false });
    graph.addEdge({ "mag", "out", "add", "right", PortDomain::SpectralMagnitudeSignal, false });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::MixedOperationDomains;
            }));
}

TEST_CASE("Scratch ports require attachment routing", "[cycle-v2][graph]") {
    NodeGraph graph = NodeGraph::createDemoGraph();
    graph.addEdge({ "env", "env", "waveMesh", "scratch", PortDomain::EnvelopeSignal, false });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ScratchPortRequiresAttachment;
            }));
}

TEST_CASE("Pitch cannot feed non voice-aware processors", "[cycle-v2][graph]") {
    NodeGraph graph;
    graph.addNode({
            "pitch",
            NodeKind::GenericProcessor,
            "Pitch",
            {},
            {},
            {},
            { { "out", "Pitch", PortDomain::PitchSignal, ChannelLayout::Mono, PortPurpose::Signal, false } }
    });
    graph.addNode(GraphNodeFactory().createNode(NodeKind::Multiply, "multiply", {}));
    graph.addEdge({ "pitch", "out", "multiply", "left", PortDomain::PitchSignal, false });

    auto issues = GraphValidator().validate(graph);

    REQUIRE_FALSE(issues.empty());
    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::DomainMismatch
                    || issue.code == GraphValidationCode::PitchRequiresVoiceAwareDestination;
            }));
}

TEST_CASE("Audio signal edges require compatible channel layouts", "[cycle-v2][graph]") {
    NodeGraph graph;

    graph.addNode({
            "source",
            NodeKind::GenericProcessor,
            "Source",
            {},
            {},
            {},
            {
                    { "time", "Time L/R", PortDomain::TimeSignal, ChannelLayout::LinkedStereo, PortPurpose::Signal, false }
            }
    });
    graph.addNode({
            "dest",
            NodeKind::GenericProcessor,
            "Dest",
            {},
            {},
            {
                    { "time", "Time Mono", PortDomain::TimeSignal, ChannelLayout::Mono, PortPurpose::Signal, true }
            },
            {}
    });
    graph.addEdge({ "source", "time", "dest", "time", PortDomain::TimeSignal, false });

    const auto issues = GraphValidator().validate(graph);

    REQUIRE(std::any_of(
            issues.begin(),
            issues.end(),
            [](const GraphValidationIssue& issue) {
                return issue.code == GraphValidationCode::ChannelLayoutMismatch;
            }));
}
